#pragma once

#include "Engine/Lfo.h"
#include "Engine/ModDestinations.h"

#include <cmath>

namespace lumen::mod
{
// Modulation sources (SPEC section 9).
enum class Source
{
    env1 = 0, env2, env3, lfo1, lfo2, lfo3,
    macro1, macro2, macro3, macro4,
    velocity, modWheel, aftertouch, pitchBend, keytrack, randomPerNote,
    count
};

constexpr int kNumSources = static_cast<int> (Source::count);
constexpr int kNumSlots = 24;
constexpr int kNumMacros = 4;
constexpr int kMaxMacroMaps = 8;

struct Slot
{
    int source = 0;     // Source
    int dest = -1;      // Dest, -1 = unassigned
    float depth = 0.0f; // -1..+1
    bool enabled = false;
};

struct MacroMapping
{
    int dest = -1;              // Dest, -1 = unused
    float rangeMin = 0.0f;      // normalized offset added at macro = 0
    float rangeMax = 1.0f;      // normalized offset added at macro = 1
    float curve = 1.0f;         // response exponent: offset follows macro^curve
};

// Plain data handed to the engine each block (part of EngineParams).
struct Config
{
    Slot slots[kNumSlots];
    MacroMapping macroMaps[kNumMacros][kMaxMacroMaps];
};

// Whether a source is per-voice (poly) or global. LFO polarity depends on
// the LFO's own mono switch, resolved by the caller.
inline bool isLfoSource (Source s) noexcept
{
    return s == Source::lfo1 || s == Source::lfo2 || s == Source::lfo3;
}

inline bool isPolySource (Source s, const LfoParams* lfoParams) noexcept
{
    switch (s)
    {
        case Source::env1: case Source::env2: case Source::env3:
        case Source::velocity: case Source::keytrack: case Source::randomPerNote:
            return true;
        case Source::lfo1: return ! lfoParams[0].mono;
        case Source::lfo2: return ! lfoParams[1].mono;
        case Source::lfo3: return ! lfoParams[2].mono;
        default:
            return false;
    }
}

// SPEC section 9 combination rule, one destination at a time:
// final = clamp(baseNorm + sum(depth_i * source_i)) in normalized space.
// `values` is indexed by Source; sources that don't apply in this pass
// (poly during the global pass and vice versa) must be zero.
inline float sumForDest (const Config& config, int dest,
                         const float* values, const bool* sourceActive) noexcept
{
    float sum = 0.0f;
    for (const auto& slot : config.slots)
        if (slot.enabled && slot.dest == dest && sourceActive[slot.source])
            sum += slot.depth * values[slot.source];
    return sum;
}

// One macro mapping's contribution: offset = min + (max-min) * macro^curve.
// curve 1 = linear; curve > 1 packs the offset's growth into the top of the
// knob travel (the map still spans exactly min..max end to end).
inline float macroMapOffset (const MacroMapping& map, float macro) noexcept
{
    const float shaped = map.curve == 1.0f ? macro : std::pow (macro, map.curve);
    return map.rangeMin + (map.rangeMax - map.rangeMin) * shaped;
}

// Macro mapping lists contribute globally.
inline float macroSumForDest (const Config& config, int dest, const float* macroValues) noexcept
{
    float sum = 0.0f;
    for (int m = 0; m < kNumMacros; ++m)
        for (const auto& map : config.macroMaps[m])
            if (map.dest == dest)
                sum += macroMapOffset (map, macroValues[m]);
    return sum;
}

inline float clampNorm (float v) noexcept
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
} // namespace lumen::mod
