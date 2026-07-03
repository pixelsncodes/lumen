#pragma once

#include <cmath>

// Modulation destinations (SPEC section 9): every continuous engine-facing
// parameter. Combination happens in NORMALIZED space, so each destination
// carries the same {min, max, skew-centre} its APVTS range uses —
// State/Parameters.cpp builds its NormalisableRanges from this table, which
// keeps engine and host mapping identical by construction.
//
// Pure data + math, no JUCE.

namespace lumen::mod
{
enum class Dest
{
    filterCutoff = 0, filterRes, filterDrive, filterKeytrack, filterEnvAmount,
    oscAMorph, oscALevel, oscAPan, oscAFine, oscADetune, oscAWidth, oscABlend,
    oscBMorph, oscBLevel, oscBPan, oscBFine, oscBDetune, oscBWidth, oscBBlend,
    subLevel, noiseLevel,
    env1Attack, env1Decay, env1Sustain, env1Release, env1Curve,
    env2Attack, env2Decay, env2Sustain, env2Release, env2Curve,
    env3Attack, env3Decay, env3Sustain, env3Release, env3Curve,
    // Phase 4 (append-only): FX destinations. Global-only — poly-source
    // routings to these are inactive (the FX bus has no per-voice identity).
    delayMix, reverbMix, masterGain,
    driveAmount, driveTone,
    chorusRate, chorusDepth, chorusMix,
    delayTime, delayFeedback, delayDamp,
    reverbSize, reverbDamp, reverbWidth,
    // Phase 5 (append-only): LFO rates (SPEC 9 — every continuous knob).
    // Global-only, like the FX bus: poly-source routings are inactive, and
    // the modulated rate lands one engine chunk late (LFO values are read
    // before the sums that would retune them exist). Free-rate only; a
    // host-synced LFO ignores its rate modulation.
    lfo1Rate, lfo2Rate, lfo3Rate,
    count
};

constexpr int kNumDests = static_cast<int> (Dest::count);

struct DestRange
{
    float min;
    float max;
    float skewCentre; // 0 = linear; else the value that maps to normalized 0.5
};

// Order must match Dest. Centres mirror Parameters.cpp exactly.
inline constexpr DestRange kRanges[kNumDests] = {
    { 20.0f, 20000.0f, 632.5f },   // filterCutoff (logHzRange centre)
    { 0.0f, 1.0f, 0.0f },          // filterRes
    { 0.0f, 24.0f, 0.0f },         // filterDrive
    { 0.0f, 1.0f, 0.0f },          // filterKeytrack
    { -1.0f, 1.0f, 0.0f },         // filterEnvAmount
    { 0.0f, 1.0f, 0.0f },          // oscAMorph
    { 0.0f, 1.0f, 0.0f },          // oscALevel
    { -1.0f, 1.0f, 0.0f },         // oscAPan
    { -100.0f, 100.0f, 0.0f },     // oscAFine
    { 0.0f, 50.0f, 0.0f },         // oscADetune
    { 0.0f, 1.0f, 0.0f },          // oscAWidth
    { 0.0f, 1.0f, 0.0f },          // oscABlend
    { 0.0f, 1.0f, 0.0f },          // oscBMorph
    { 0.0f, 1.0f, 0.0f },          // oscBLevel
    { -1.0f, 1.0f, 0.0f },         // oscBPan
    { -100.0f, 100.0f, 0.0f },     // oscBFine
    { 0.0f, 50.0f, 0.0f },         // oscBDetune
    { 0.0f, 1.0f, 0.0f },          // oscBWidth
    { 0.0f, 1.0f, 0.0f },          // oscBBlend
    { 0.0f, 1.0f, 0.0f },          // subLevel
    { -60.0f, 0.0f, 0.0f },        // noiseLevel (dB)
    { 0.001f, 10.0f, 0.1f },       // env1Attack  (sqrt(min*max))
    { 0.001f, 10.0f, 0.1f },       // env1Decay
    { 0.0f, 1.0f, 0.0f },          // env1Sustain
    { 0.005f, 15.0f, 0.27386128f },// env1Release (sqrt(min*max))
    { -1.0f, 1.0f, 0.0f },         // env1Curve
    { 0.001f, 10.0f, 0.1f },       // env2Attack
    { 0.001f, 10.0f, 0.1f },       // env2Decay
    { 0.0f, 1.0f, 0.0f },          // env2Sustain
    { 0.005f, 15.0f, 0.27386128f },// env2Release
    { -1.0f, 1.0f, 0.0f },         // env2Curve
    { 0.001f, 10.0f, 0.1f },       // env3Attack
    { 0.001f, 10.0f, 0.1f },       // env3Decay
    { 0.0f, 1.0f, 0.0f },          // env3Sustain
    { 0.005f, 15.0f, 0.27386128f },// env3Release
    { -1.0f, 1.0f, 0.0f },         // env3Curve
    { 0.0f, 1.0f, 0.0f },          // delayMix
    { 0.0f, 1.0f, 0.0f },          // reverbMix
    { -60.0f, 6.0f, 0.0f },        // masterGain (dB)
    { 0.0f, 24.0f, 0.0f },         // driveAmount (dB)
    { -1.0f, 1.0f, 0.0f },         // driveTone
    { 0.05f, 5.0f, 0.5f },         // chorusRate (sqrt(min*max))
    { 0.0f, 1.0f, 0.0f },          // chorusDepth
    { 0.0f, 1.0f, 0.0f },          // chorusMix
    { 1.0f, 2000.0f, 44.7213595f },// delayTime (ms, sqrt(min*max))
    { 0.0f, 0.95f, 0.0f },         // delayFeedback
    { 1000.0f, 16000.0f, 4000.0f },// delayDamp (Hz, sqrt(min*max))
    { 0.0f, 1.0f, 0.0f },          // reverbSize
    { 0.0f, 1.0f, 0.0f },          // reverbDamp
    { 0.0f, 1.0f, 0.0f },          // reverbWidth
    { 0.01f, 40.0f, 0.63245553f }, // lfo1Rate (Hz, sqrt(min*max))
    { 0.01f, 40.0f, 0.63245553f }, // lfo2Rate
    { 0.01f, 40.0f, 0.63245553f }, // lfo3Rate
};

// JUCE skew math: normalized p -> value = min + (max-min) * p^e where
// 0.5^e = (centre-min)/(max-min). Exponents are cheap to compute; callers
// that care cache them (see SynthEngine).
inline float skewExponent (Dest dest) noexcept
{
    const auto& r = kRanges[static_cast<int> (dest)];
    if (r.skewCentre == 0.0f)
        return 1.0f;
    return std::log ((r.skewCentre - r.min) / (r.max - r.min)) / std::log (0.5f);
}

inline float denormalize (Dest dest, float p, float exponent) noexcept
{
    const auto& r = kRanges[static_cast<int> (dest)];
    p = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    const float shaped = exponent == 1.0f ? p : std::pow (p, exponent);
    return r.min + (r.max - r.min) * shaped;
}

inline float normalize (Dest dest, float value, float exponent) noexcept
{
    const auto& r = kRanges[static_cast<int> (dest)];
    float p = (value - r.min) / (r.max - r.min);
    p = p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    return exponent == 1.0f ? p : std::pow (p, 1.0f / exponent);
}
} // namespace lumen::mod
