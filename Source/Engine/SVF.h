#pragma once

#include "Engine/EngineParams.h"

#include <algorithm>
#include <cmath>

namespace lumen
{
// Trapezoidal (TPT) state-variable filter, Andrew Simper's form — stable
// under audio-rate cutoff modulation (SPEC section 6). One instance filters
// a stereo pair with shared coefficients. LP24 cascades a second stage.
// Resonance 0..1 maps to Q = 0.5 * 24^res (~0.5..12), and k is floored so
// self-oscillation stays controlled.
class SVF
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        maxCutoff = static_cast<float> (0.49 * sr);
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : state)
            s = {};
    }

    // Call once per sample before processSample when cutoff/res move.
    void setPerSample (float cutoffHz, float res) noexcept
    {
        const float fc = std::clamp (cutoffHz, 20.0f, std::min (20000.0f, maxCutoff));
        const float g  = std::tan (kPi * fc / static_cast<float> (sampleRate));
        const float q  = 0.5f * std::exp2 (4.585f * std::clamp (res, 0.0f, 1.0f)); // 0.5 * 24^res
        k  = std::max (1.0f / 12.0f, 1.0f / q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;

        // LP24's second stage runs Butterworth damping (k = sqrt 2) so the
        // resonance peak matches LP12 instead of doubling in dB (DECISIONS.md).
        b1 = 1.0f / (1.0f + g * (g + kDamped));
        b2 = g * b1;
        b3 = g * b2;
    }

    float processSample (float v0, int mode, int channel) noexcept
    {
        float out = tick (v0, state[static_cast<size_t> (channel)], mode, k, a1, a2, a3);
        if (mode == static_cast<int> (FilterMode::lp24))
            out = tick (out, state[2 + static_cast<size_t> (channel)], mode, kDamped, b1, b2, b3);
        return out;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    static constexpr float kDamped = 1.41421356f;

    struct State { float ic1eq = 0.0f, ic2eq = 0.0f; };

    static float tick (float v0, State& s, int mode,
                       float kc, float c1, float c2, float c3) noexcept
    {
        const float v3 = v0 - s.ic2eq;
        const float v1 = c1 * s.ic1eq + c2 * v3;
        const float v2 = s.ic2eq + c2 * s.ic1eq + c3 * v3;
        s.ic1eq = 2.0f * v1 - s.ic1eq;
        s.ic2eq = 2.0f * v2 - s.ic2eq;

        switch (static_cast<FilterMode> (mode))
        {
            case FilterMode::lp12:
            case FilterMode::lp24:  return v2;
            case FilterMode::hp12:  return v0 - kc * v1 - v2;
            case FilterMode::bp12:  return kc * v1;          // unity-gain bandpass
            case FilterMode::notch: return v0 - kc * v1;
        }
        return v2;
    }

    double sampleRate = 48000.0;
    float maxCutoff = 20000.0f;
    float k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float b1 = 0.0f, b2 = 0.0f, b3 = 0.0f;
    State state[4]; // stage 1 L/R, stage 2 L/R (LP24)
};
} // namespace lumen
