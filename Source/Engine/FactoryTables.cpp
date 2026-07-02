#include "Engine/FactoryTables.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>

namespace lumen::factory
{
namespace
{
    constexpr int kMaxHarm = 1023;
    constexpr int kN = Wavetable::kFrameLength;

    using Harmonics = std::vector<float>; // index h = harmonic h, [0] unused

    // Render a sine-phase harmonic series to one 2048-sample frame via IFFT.
    // All factory waves share sine phase so time-domain crossfades between
    // canonical shapes behave (no cancellation).
    void renderFrame (const Harmonics& amps, float* dest, juce::dsp::FFT& fft,
                      std::vector<float>& work)
    {
        std::fill (work.begin(), work.end(), 0.0f);
        const int top = std::min<int> (kMaxHarm, static_cast<int> (amps.size()) - 1);

        for (int h = 1; h <= top; ++h)
        {
            const float a = amps[static_cast<size_t> (h)];
            if (a == 0.0f)
                continue;

            // sin series: X[h] = -i * a * N/2, X[N-h] = conj(X[h])
            const float im = -0.5f * a * static_cast<float> (kN);
            work[2 * static_cast<size_t> (h) + 1]        = im;
            work[2 * static_cast<size_t> (kN - h) + 1]   = -im;
        }

        fft.performRealOnlyInverseTransform (work.data());
        std::copy_n (work.begin(), kN, dest);
    }

    Harmonics sineWave()
    {
        Harmonics a (2, 0.0f);
        a[1] = 1.0f;
        return a;
    }

    Harmonics triangleWave()
    {
        Harmonics a (kMaxHarm + 1, 0.0f);
        constexpr float k = 8.0f / (juce::MathConstants<float>::pi * juce::MathConstants<float>::pi);
        float sign = 1.0f;
        for (int h = 1; h <= kMaxHarm; h += 2)
        {
            a[static_cast<size_t> (h)] = sign * k / static_cast<float> (h * h);
            sign = -sign;
        }
        return a;
    }

    Harmonics sawWave()
    {
        Harmonics a (kMaxHarm + 1, 0.0f);
        constexpr float k = 2.0f / juce::MathConstants<float>::pi;
        for (int h = 1; h <= kMaxHarm; ++h)
            a[static_cast<size_t> (h)] = k / static_cast<float> (h);
        return a;
    }

    Harmonics squareWave()
    {
        Harmonics a (kMaxHarm + 1, 0.0f);
        constexpr float k = 4.0f / juce::MathConstants<float>::pi;
        for (int h = 1; h <= kMaxHarm; h += 2)
            a[static_cast<size_t> (h)] = k / static_cast<float> (h);
        return a;
    }

    Harmonics mix (const Harmonics& x, const Harmonics& y, float t)
    {
        Harmonics out (std::max (x.size(), y.size()), 0.0f);
        for (size_t h = 1; h < out.size(); ++h)
        {
            const float xv = h < x.size() ? x[h] : 0.0f;
            const float yv = h < y.size() ? y[h] : 0.0f;
            out[h] = (1.0f - t) * xv + t * yv;
        }
        return out;
    }

    Wavetable buildFromHarmonicFrames (const std::vector<Harmonics>& frames)
    {
        juce::dsp::FFT fft (11);
        std::vector<float> work (2 * kN, 0.0f);
        std::vector<float> timeFrames (static_cast<size_t> (frames.size()) * kN);

        for (size_t f = 0; f < frames.size(); ++f)
            renderFrame (frames[f], timeFrames.data() + f * kN, fft, work);

        Wavetable table;
        table.build (timeFrames.data(), static_cast<int> (frames.size()));
        return table;
    }

    Wavetable buildBasic()
    {
        const auto sine = sineWave(), tri = triangleWave(), saw = sawWave(), square = squareWave();

        std::vector<Harmonics> frames;
        frames.reserve (kNumFrames);
        for (int f = 0; f < kNumFrames; ++f)
        {
            if (f <= 21)      frames.push_back (mix (sine, tri,    static_cast<float> (f)      / 21.0f));
            else if (f <= 42) frames.push_back (mix (tri,  saw,    static_cast<float> (f - 21) / 21.0f));
            else              frames.push_back (mix (saw,  square, static_cast<float> (f - 42) / 21.0f));
        }
        return buildFromHarmonicFrames (frames);
    }

    Wavetable buildPwm()
    {
        // Pulse-width sweep 50% -> 95%. b_h = (4 / h pi) * sin(h pi d); the
        // DC term of the pulse is dropped (tables are DC-free by contract).
        std::vector<Harmonics> frames;
        frames.reserve (kNumFrames);
        for (int f = 0; f < kNumFrames; ++f)
        {
            const float d = 0.5f + 0.45f * static_cast<float> (f) / (kNumFrames - 1.0f);
            Harmonics a (kMaxHarm + 1, 0.0f);
            for (int h = 1; h <= kMaxHarm; ++h)
                a[static_cast<size_t> (h)] = (4.0f / (static_cast<float> (h) * juce::MathConstants<float>::pi))
                                             * std::sin (static_cast<float> (h) * juce::MathConstants<float>::pi * d);
            frames.push_back (std::move (a));
        }
        return buildFromHarmonicFrames (frames);
    }

    Wavetable buildHarmonicRise()
    {
        // Frame f carries harmonics 1..f+1 at saw-style 1/h amplitudes.
        std::vector<Harmonics> frames;
        frames.reserve (kNumFrames);
        for (int f = 0; f < kNumFrames; ++f)
        {
            Harmonics a (static_cast<size_t> (f) + 2, 0.0f);
            for (int h = 1; h <= f + 1; ++h)
                a[static_cast<size_t> (h)] = 1.0f / static_cast<float> (h);
            frames.push_back (std::move (a));
        }
        return buildFromHarmonicFrames (frames);
    }

    Wavetable buildFormant()
    {
        // Two resonant peaks sweeping up in log-harmonic space over a faint
        // 1/h^1.5 bed so low morph positions still carry a fundamental.
        std::vector<Harmonics> frames;
        frames.reserve (kNumFrames);
        constexpr float sigma = 0.35f;

        for (int f = 0; f < kNumFrames; ++f)
        {
            const float t = static_cast<float> (f) / (kNumFrames - 1.0f);
            const float c1 = 2.0f  * std::pow (12.0f, t); // 2 -> 24
            const float c2 = 6.0f  * std::pow (15.0f, t); // 6 -> 90

            Harmonics a (257, 0.0f);
            for (int h = 1; h <= 256; ++h)
            {
                const float lh = std::log (static_cast<float> (h));
                const float g1 = std::exp (-0.5f * juce::square ((lh - std::log (c1)) / sigma));
                const float g2 = std::exp (-0.5f * juce::square ((lh - std::log (c2)) / sigma));
                const float bed = 0.15f / std::pow (static_cast<float> (h), 1.5f);
                a[static_cast<size_t> (h)] = g1 + 0.6f * g2 + bed;
            }
            frames.push_back (std::move (a));
        }
        return buildFromHarmonicFrames (frames);
    }
} // namespace

const Wavetable& get (TableChoice choice)
{
    // Magic statics: built once, thread-safe, deterministic.
    static const Wavetable basic        = buildBasic();
    static const Wavetable pwm          = buildPwm();
    static const Wavetable harmonicRise = buildHarmonicRise();
    static const Wavetable formant      = buildFormant();

    switch (choice)
    {
        case TableChoice::pwm:          return pwm;
        case TableChoice::harmonicRise: return harmonicRise;
        case TableChoice::formant:      return formant;
        case TableChoice::image:        return basic; // Lens lands in Phase 6
        case TableChoice::basic:        break;
    }
    return basic;
}

const Wavetable& forIndex (int tableParamValue)
{
    const int clamped = tableParamValue < 0 ? 0 : (tableParamValue > 4 ? 4 : tableParamValue);
    return get (static_cast<TableChoice> (clamped));
}
} // namespace lumen::factory
