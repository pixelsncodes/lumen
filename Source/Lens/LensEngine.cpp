#include "Lens/LensEngine.h"

#include <juce_cryptography/juce_cryptography.h>
#include <juce_dsp/juce_dsp.h>

#include <cmath>

namespace lumen::lens
{
namespace
{
    constexpr double kTwoPi = 2.0 * juce::MathConstants<double>::pi;

    // FNV-1a 64-bit (SPEC 13.2).
    constexpr uint64_t kFnvOffset = 14695981039346656037ull;
    constexpr uint64_t kFnvPrime = 1099511628211ull;

    uint64_t fnv1a (uint64_t hash, const uint8_t* bytes, size_t count) noexcept
    {
        for (size_t i = 0; i < count; ++i)
        {
            hash ^= bytes[i];
            hash *= kFnvPrime;
        }
        return hash;
    }

    // xorshift64*: the one PRNG all seeded phases come from.
    uint64_t nextRandom (uint64_t& state) noexcept
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 2685821657736338717ull;
    }

    double uniform01 (uint64_t& state) noexcept
    {
        return static_cast<double> (nextRandom (state) >> 11) * 0x1.0p-53;
    }

    float lumaOf (juce::Colour c) noexcept
    {
        return 0.2126f * c.getFloatRed() + 0.7152f * c.getFloatGreen()
             + 0.0722f * c.getFloatBlue();
    }

    // DC removal + silence guard + peak normalization shared by both modes
    // (SPEC 13.3; the guard also protects spectral frames from all-dark
    // columns, DECISIONS.md).
    void finishFrame (float* frame)
    {
        double mean = 0.0;
        for (int j = 0; j < kFrameLength; ++j)
            mean += frame[j];
        mean /= kFrameLength;

        float peak = 0.0f;
        for (int j = 0; j < kFrameLength; ++j)
        {
            frame[j] -= static_cast<float> (mean);
            peak = juce::jmax (peak, std::abs (frame[j]));
        }

        if (peak < 1.0e-4f)
        {
            for (int j = 0; j < kFrameLength; ++j)
                frame[j] = 0.05f * static_cast<float> (
                    std::sin (kTwoPi * j / kFrameLength));
            return;
        }

        const float gain = kPeakTarget / peak;
        for (int j = 0; j < kFrameLength; ++j)
            frame[j] *= gain;
    }

    std::vector<float> buildScanFrames (const juce::Image& img)
    {
        std::vector<float> frames (static_cast<size_t> (kNumFrames) * kFrameLength, 0.0f);
        const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);
        const int w = data.width, h = data.height;

        std::vector<float> row (static_cast<size_t> (w), 0.0f);

        for (int i = 0; i < kNumFrames; ++i)
        {
            // Row y = round((i + 0.5) / 64 * (h - 1)) — morph travels down.
            const int y = static_cast<int> (std::lround (
                (i + 0.5) / kNumFrames * (h - 1)));

            for (int x = 0; x < w; ++x)
                row[static_cast<size_t> (x)] = lumaOf (data.getPixelColour (x, y));

            float* frame = frames.data() + static_cast<size_t> (i) * kFrameLength;
            for (int j = 0; j < kFrameLength; ++j)
            {
                // Linear resample of the row to 2048 points.
                const double xf = w > 1 ? static_cast<double> (j) * (w - 1) / (kFrameLength - 1) : 0.0;
                const int x0 = static_cast<int> (xf);
                const int x1 = juce::jmin (w - 1, x0 + 1);
                const float frac = static_cast<float> (xf - x0);
                const float luma = row[static_cast<size_t> (x0)]
                                 + frac * (row[static_cast<size_t> (x1)] - row[static_cast<size_t> (x0)]);
                frame[j] = 2.0f * luma - 1.0f;
            }
            finishFrame (frame);
        }
        return frames;
    }

    std::vector<float> buildSpectralFrames (const juce::Image& img, uint64_t seed)
    {
        std::vector<float> frames (static_cast<size_t> (kNumFrames) * kFrameLength, 0.0f);
        const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);
        const int w = data.width, h = data.height;

        // Seeded phases, one per harmonic, shared by all frames so morphing
        // across columns stays phase-coherent (DECISIONS.md).
        uint64_t rng = seed != 0 ? seed : kFnvOffset;
        double phases[kMaxHarmonics + 1] {};
        for (int harmonic = 1; harmonic <= kMaxHarmonics; ++harmonic)
            phases[harmonic] = uniform01 (rng) * kTwoPi;

        // 3x3 mean luma around (x, y), clamped at the edges.
        auto brightnessAt = [&data, w, h] (int cx, int cy)
        {
            float sum = 0.0f;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    sum += lumaOf (data.getPixelColour (juce::jlimit (0, w - 1, cx + dx),
                                                        juce::jlimit (0, h - 1, cy + dy)));
            return sum / 9.0f;
        };

        constexpr int fftOrder = 11; // 2^11 = kFrameLength
        juce::dsp::FFT fft (fftOrder);
        std::vector<float> spectrum (2 * kFrameLength, 0.0f);

        const double logMax = std::log (static_cast<double> (kMaxHarmonics));

        for (int i = 0; i < kNumFrames; ++i)
        {
            const int x = juce::jmin (w - 1, static_cast<int> (
                (i + 0.5) / kNumFrames * w));

            std::fill (spectrum.begin(), spectrum.end(), 0.0f);
            for (int harmonic = 1; harmonic <= kMaxHarmonics; ++harmonic)
            {
                // y maps h logarithmically, image top = high frequencies.
                const double yNorm = 1.0 - std::log (static_cast<double> (harmonic)) / logMax;
                const int y = static_cast<int> (std::lround (yNorm * (h - 1)));

                const float amplitude = std::pow (brightnessAt (x, y), 1.5f);
                const auto phase = static_cast<float> (phases[harmonic]);
                const float re = amplitude * std::cos (phase);
                const float im = amplitude * std::sin (phase);

                spectrum[2 * static_cast<size_t> (harmonic)] = re;
                spectrum[2 * static_cast<size_t> (harmonic) + 1] = im;
                const int mirror = kFrameLength - harmonic;
                spectrum[2 * static_cast<size_t> (mirror)] = re;
                spectrum[2 * static_cast<size_t> (mirror) + 1] = -im;
            }

            fft.performRealOnlyInverseTransform (spectrum.data());

            float* frame = frames.data() + static_cast<size_t> (i) * kFrameLength;
            std::copy_n (spectrum.data(), kFrameLength, frame);
            finishFrame (frame);
        }
        return frames;
    }
} // namespace

Analysis analyzeImage (const juce::Image& source)
{
    Analysis result;
    if (! source.isValid() || source.getWidth() < 4 || source.getHeight() < 4)
        return result;

    // Downscale to fit 512x512, bilinear, never upscale (SPEC 13.1).
    juce::Image working = source.convertedToFormat (juce::Image::ARGB);
    const int w = working.getWidth(), h = working.getHeight();
    if (w > 512 || h > 512)
    {
        const double scale = juce::jmin (512.0 / w, 512.0 / h);
        working = working.rescaled (juce::jmax (4, static_cast<int> (std::lround (w * scale))),
                                    juce::jmax (4, static_cast<int> (std::lround (h * scale))),
                                    juce::Graphics::mediumResamplingQuality);
    }

    result.working = working;
    result.chromaCopy = working.rescaled (256, 256, juce::Graphics::mediumResamplingQuality);

    // Seed: FNV-1a over dimensions + downscaled ARGB bytes (SPEC 13.2).
    const juce::Image::BitmapData data (working, juce::Image::BitmapData::readOnly);
    uint64_t hash = kFnvOffset;
    const uint32_t dims[2] = { static_cast<uint32_t> (data.width),
                               static_cast<uint32_t> (data.height) };
    hash = fnv1a (hash, reinterpret_cast<const uint8_t*> (dims), sizeof (dims));
    for (int y = 0; y < data.height; ++y)
        hash = fnv1a (hash, data.getLinePointer (y),
                      static_cast<size_t> (data.width) * static_cast<size_t> (data.pixelStride));

    result.seed = hash;
    result.valid = true;
    return result;
}

Analysis analyzeImageFile (const juce::File& file)
{
    return analyzeImage (juce::ImageFileFormat::loadFrom (file));
}

Analysis analyzeImageData (const void* data, size_t size)
{
    return analyzeImage (juce::ImageFileFormat::loadFrom (data, size));
}

std::vector<float> buildFrames (const Analysis& analysis, Mode mode)
{
    if (! analysis.valid)
        return {};
    return mode == Mode::scan ? buildScanFrames (analysis.working)
                              : buildSpectralFrames (analysis.working, analysis.seed);
}

ChromaStats chromaStats (const Analysis& analysis)
{
    ChromaStats stats;
    if (! analysis.valid)
        return stats;

    const juce::Image::BitmapData data (analysis.chromaCopy, juce::Image::BitmapData::readOnly);
    const int w = data.width, h = data.height;
    const auto count = static_cast<double> (w) * h;

    std::vector<float> luma (static_cast<size_t> (w) * static_cast<size_t> (h), 0.0f);

    double sumSat = 0.0, sumVal = 0.0, sumLuma = 0.0, sumLumaSq = 0.0;
    double hueX = 0.0, hueY = 0.0, hueWeight = 0.0;

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const auto c = data.getPixelColour (x, y);
            const float s = c.getSaturation();
            const float v = c.getBrightness();
            const float l = lumaOf (c);

            sumSat += s;
            sumVal += v;
            sumLuma += l;
            sumLumaSq += static_cast<double> (l) * l;
            luma[static_cast<size_t> (y) * static_cast<size_t> (w) + static_cast<size_t> (x)] = l;

            // Saturation-weighted hue vectors: gray pixels have no hue and
            // must not drag the circular mean toward red (DECISIONS.md).
            const double angle = c.getHue() * kTwoPi;
            hueX += s * std::cos (angle);
            hueY += s * std::sin (angle);
            hueWeight += s;
        }
    }

    stats.satMean = static_cast<float> (sumSat / count);
    stats.valMean = static_cast<float> (sumVal / count);

    const double lumaMean = sumLuma / count;
    const double lumaVar = juce::jmax (0.0, sumLumaSq / count - lumaMean * lumaMean);
    stats.lumaSigma = juce::jlimit (0.0f, 1.0f, 2.0f * static_cast<float> (std::sqrt (lumaVar)));

    if (hueWeight > 1.0e-9)
    {
        double hm = std::atan2 (hueY, hueX) / kTwoPi * 360.0;
        if (hm < 0.0)
            hm += 360.0;
        stats.hueMeanDeg = static_cast<float> (hm);
        const double r = std::sqrt (hueX * hueX + hueY * hueY) / hueWeight;
        stats.hueSigma = juce::jlimit (0.0f, 1.0f, static_cast<float> (1.0 - r));
    }

    // E: mean Sobel magnitude over the interior, normalized by the per-axis
    // kernel maximum (4), clamped 0..1.
    double edgeSum = 0.0;
    auto at = [&luma, w] (int x, int y) { return static_cast<double> (
        luma[static_cast<size_t> (y) * static_cast<size_t> (w) + static_cast<size_t> (x)]); };
    for (int y = 1; y < h - 1; ++y)
    {
        for (int x = 1; x < w - 1; ++x)
        {
            const double gx = -at (x - 1, y - 1) + at (x + 1, y - 1)
                              - 2.0 * at (x - 1, y) + 2.0 * at (x + 1, y)
                              - at (x - 1, y + 1) + at (x + 1, y + 1);
            const double gy = -at (x - 1, y - 1) - 2.0 * at (x, y - 1) - at (x + 1, y - 1)
                              + at (x - 1, y + 1) + 2.0 * at (x, y + 1) + at (x + 1, y + 1);
            edgeSum += std::sqrt (gx * gx + gy * gy);
        }
    }
    const double interior = static_cast<double> (w - 2) * (h - 2);
    stats.edgeMean = juce::jlimit (0.0f, 1.0f,
                                   static_cast<float> (edgeSum / interior / 4.0));
    return stats;
}

PatchTargets patchTargetsFor (const ChromaStats& c)
{
    PatchTargets t;
    // Hue in [0,70) or [310,360) -> LP24 (warm); [70,170) -> BP12; else LP12.
    t.filterMode = (c.hueMeanDeg < 70.0f || c.hueMeanDeg >= 310.0f) ? 1
                 : (c.hueMeanDeg < 170.0f ? 3 : 0);
    t.cutoffHz = 250.0f * std::exp2 (5.5f * c.valMean);
    t.res = 0.05f + 0.55f * c.satMean;
    t.detuneCents = 3.0f + 30.0f * c.satMean;
    t.unison = 1 + static_cast<int> (std::lround (5.0f * c.satMean));
    t.attackSeconds = 0.4f * std::pow (2.0f / 400.0f, c.valMean);
    t.releaseSeconds = (150.0f + 1350.0f * (1.0f - c.valMean)) * 0.001f;
    t.driveDb = 20.0f * c.edgeMean;
    t.noiseDb = -60.0f + 42.0f * c.edgeMean;
    t.lfoDepth = 0.5f * c.hueSigma;
    t.lfoRateHz = 0.15f + 4.0f * c.lumaSigma;
    t.reverbMix = 0.10f + 0.35f * (1.0f - c.edgeMean);
    t.macro4 = c.edgeMean;
    return t;
}

juce::Image makeThumbnail (const Analysis& analysis)
{
    if (! analysis.valid)
        return {};
    return analysis.working.rescaled (kThumbSize, kThumbSize,
                                      juce::Graphics::mediumResamplingQuality);
}

juce::MemoryBlock encodePng (const juce::Image& image)
{
    juce::MemoryBlock block;
    if (image.isValid())
    {
        juce::MemoryOutputStream stream (block, false);
        juce::PNGImageFormat().writeImageToStream (image, stream);
    }
    return block;
}

juce::String sha256Hex (const void* data, size_t size)
{
    return juce::SHA256 (data, size).toHexString();
}
} // namespace lumen::lens
