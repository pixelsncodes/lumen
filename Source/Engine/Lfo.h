#pragma once

#include <cmath>
#include <cstdint>

namespace lumen
{
// LFO (SPEC section 8): six shapes, free or host-synced rate, phase offset,
// fade-in, poly (retriggers, per voice) or mono (global, free-running).
// Bipolar output -1..+1. S&H randomness comes from a seeded xorshift32 —
// deterministic, no rand().

enum class LfoShape { sine = 0, triangle, sawUp, sawDown, square, sampleHold };

struct LfoParams
{
    int   shape = 0;          // LfoShape
    bool  sync = false;
    float rateHz = 1.0f;      // 0.01..40, used when !sync
    int   syncDiv = 12;       // index into kSyncBeats/kSyncDivNames ("1/4")
    float phaseDeg = 0.0f;    // 0..360
    float fadeSeconds = 0.0f; // 0..5, poly mode only
    bool  mono = false;
};

// "4/1","4/1 D","4/1 T","2/1",... straight/dotted/triplet per base division.
inline constexpr int kNumSyncDivs = 24;

inline double lfoSyncBeats (int divIndex) noexcept
{
    constexpr double base[8] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
    const int clamped = divIndex < 0 ? 0 : (divIndex >= kNumSyncDivs ? kNumSyncDivs - 1 : divIndex);
    const double beats = base[clamped / 3];
    const int mod = clamped % 3;
    return mod == 0 ? beats : (mod == 1 ? beats * 1.5 : beats * (2.0 / 3.0));
}

inline double lfoRateHz (const LfoParams& p, double bpm) noexcept
{
    if (! p.sync)
        return static_cast<double> (p.rateHz);
    const double beatsPerCycle = lfoSyncBeats (p.syncDiv);
    return bpm / (60.0 * beatsPerCycle);
}

class Lfo
{
public:
    void prepare (double sr, uint32_t seed) noexcept
    {
        sampleRate = sr;
        rng = seed != 0 ? seed : 1;
        phase = 0.0;
        ageSeconds = 0.0;
        drawSample();
    }

    // Poly retrigger: restart phase and fade, draw a fresh S&H value.
    void retrigger() noexcept
    {
        phase = 0.0;
        ageSeconds = 0.0;
        drawSample();
    }

    float value (const LfoParams& p) const noexcept
    {
        double x = phase + static_cast<double> (p.phaseDeg) / 360.0;
        x -= std::floor (x);

        float v;
        switch (static_cast<LfoShape> (p.shape))
        {
            case LfoShape::sine:     v = std::sin (2.0f * 3.14159265f * static_cast<float> (x)); break;
            case LfoShape::triangle: v = x < 0.25 ? 4.0f * static_cast<float> (x)
                                       : x < 0.75 ? 2.0f - 4.0f * static_cast<float> (x)
                                                  : 4.0f * static_cast<float> (x) - 4.0f;       break;
            case LfoShape::sawUp:    v = 2.0f * static_cast<float> (x) - 1.0f;                   break;
            case LfoShape::sawDown:  v = 1.0f - 2.0f * static_cast<float> (x);                   break;
            case LfoShape::square:   v = x < 0.5 ? 1.0f : -1.0f;                                 break;
            case LfoShape::sampleHold: default: v = held;                                        break;
        }

        if (p.fadeSeconds > 0.001f && ! p.mono) // fade applies to poly retriggers
            v *= static_cast<float> (ageSeconds >= p.fadeSeconds ? 1.0 : ageSeconds / p.fadeSeconds);
        return v;
    }

    void advance (const LfoParams& p, double bpm, int numSamples) noexcept
    {
        const double inc = lfoRateHz (p, bpm) / sampleRate;
        phase += inc * numSamples;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            drawSample(); // new S&H target each cycle
        }
        ageSeconds += numSamples / sampleRate;
    }

    // Raw 0..1 phase (before the phase-offset parameter) for UI position markers.
    double currentPhase() const noexcept { return phase; }

private:
    void drawSample() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        held = static_cast<float> (static_cast<int32_t> (rng)) * (1.0f / 2147483648.0f);
    }

    double sampleRate = 48000.0;
    double phase = 0.0;
    double ageSeconds = 0.0;
    float held = 0.0f;
    uint32_t rng = 1;
};
} // namespace lumen
