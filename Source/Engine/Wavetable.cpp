#include "Engine/Wavetable.h"

#include <juce_dsp/juce_dsp.h>

namespace lumen
{
void Wavetable::build (const float* sourceFrames, int numFramesToUse,
                       int harmonicCap, float peakTarget)
{
    numFrames = numFramesToUse;
    data.assign (static_cast<size_t> (numFrames) * kNumMips * kStride, 0.0f);

    constexpr int fftOrder = 11; // 2^11 = kFrameLength
    juce::dsp::FFT fft (fftOrder);

    std::vector<float> spectrum (2 * kFrameLength, 0.0f);
    std::vector<float> work (2 * kFrameLength, 0.0f);

    for (int frame = 0; frame < numFrames; ++frame)
    {
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        std::copy_n (sourceFrames + static_cast<size_t> (frame) * kFrameLength,
                     kFrameLength, spectrum.begin());
        fft.performRealOnlyForwardTransform (spectrum.data());

        // Interleaved complex bins; bin h == harmonic h. Kill DC, Nyquist,
        // and anything above the cap (mirror bins too for a clean inverse).
        auto zeroBin = [&work] (int bin)
        {
            work[2 * static_cast<size_t> (bin)]     = 0.0f;
            work[2 * static_cast<size_t> (bin) + 1] = 0.0f;
            const int mirror = kFrameLength - bin;
            if (mirror > 0 && mirror < kFrameLength)
            {
                work[2 * static_cast<size_t> (mirror)]     = 0.0f;
                work[2 * static_cast<size_t> (mirror) + 1] = 0.0f;
            }
        };

        float normalization = 1.0f;

        for (int level = 0; level < kNumMips; ++level)
        {
            std::copy (spectrum.begin(), spectrum.end(), work.begin());

            const int keep = std::min (maxHarmonic (level), harmonicCap);
            zeroBin (0);
            for (int bin = keep + 1; bin <= kFrameLength / 2; ++bin)
                zeroBin (bin);

            fft.performRealOnlyInverseTransform (work.data());

            if (level == 0)
            {
                float peak = 0.0f;
                for (int i = 0; i < kFrameLength; ++i)
                    peak = std::max (peak, std::abs (work[static_cast<size_t> (i)]));
                normalization = peak > 1.0e-9f ? peakTarget / peak : 0.0f;
            }

            float* dest = data.data() + (static_cast<size_t> (frame) * kNumMips
                                         + static_cast<size_t> (level)) * kStride;
            for (int i = 0; i < kFrameLength; ++i)
                dest[i] = work[static_cast<size_t> (i)] * normalization;
            dest[kFrameLength] = dest[0]; // wrap sample
        }
    }
}
} // namespace lumen
