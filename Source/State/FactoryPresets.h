#pragma once

#include "Engine/EngineParams.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace lumen::presets
{
// The 32-preset factory bank (SPEC section 16), defined once in code and
// applied through two paths that the unit tests hold bit-identical:
//   - applyToEngine():  EngineParams for lumen_render / tests (headless);
//   - buildState():     a complete APVTS state tree for the plugin
//                       (replaceState), including matrix/macro trees and,
//                       for the two Lens presets, the generated wavetable
//                       frames + thumbnail (never the source image).
//
// Settings are deltas against the EngineParams member-initializer defaults
// (the single source of default values, mirrored by the APVTS layout), in
// natural units. Values must sit exactly on the parameter grid (interval
// steps) so both paths agree bit-exactly.

inline constexpr const char* kFactoryAuthor = "Lumen Audio";
inline constexpr int kNumPresets = 32;

struct Setting { const char* id; float value; };

// Mod-matrix route, tokens as in modstate (depth in normalized units).
struct Route { const char* source; const char* dest; float depth; };

// Macro map defined by its total normalized span + response curve; min/max
// derive from the preset's stored macro position p so the knob rests
// sound-neutral (offset 0 at p) and sweeps away from the designed patch:
//   min = -span * p^curve, max = span - span * p^curve.
// Negative span inverts the direction of travel.
struct MacroMap { int macro; const char* dest; float span; float curve = 1.0f; };

// Lens-built preset: the wavetable comes from a bundled procedural image
// (lens::testimages name) through the Phase 6 engine, deterministically.
struct LensSpec { const char* image = nullptr; int mode = 0; int targetOsc = 0; };

struct FactoryPreset
{
    const char* name;
    const char* category;   // Bass | Leads | Pads | Keys | Textures
    std::vector<Setting> settings;
    std::vector<Route> routes;
    std::vector<MacroMap> maps;
    LensSpec lens {};
    bool initMods = false;  // Init only: seed the stock modulation defaults

    bool hasLens() const noexcept { return lens.image != nullptr; }
};

const std::vector<FactoryPreset>& bank();
const juce::StringArray& categories();                // display order
const FactoryPreset* find (const juce::String& name); // case-insensitive; null if unknown

// The preset's stored position of macro 0..3 (settings override, else the
// frozen defaults 0.5/0.5/0.3/0.2) — the neutral point of its macro maps.
float macroPosition (const FactoryPreset& preset, int macroIndex);

// Resolved normalized min/max of one macro map (see MacroMap).
void macroMapRange (const FactoryPreset& preset, const MacroMap& map,
                    float& rangeMin, float& rangeMax);

// Headless path: settings + matrix/macros onto EngineParams (mod config is
// replaced, not merged). The Lens table, if any, is NOT installed here —
// callers build it from buildLensFrames() and own the Wavetable.
void applyToEngine (const FactoryPreset& preset, EngineParams& params);

// The deterministic Lens frames for a Lens preset (empty vector otherwise).
std::vector<float> buildLensFrames (const FactoryPreset& preset);

// Plugin/tests path: a complete state tree for APVTS::replaceState — every
// parameter (defaults + snapped overrides), MODMATRIX/MACROS, LENS (with
// frames + thumbnail for Lens presets), stateVersion and preset metadata
// (presetName/presetCategory/presetAuthor properties).
juce::ValueTree buildState (const FactoryPreset& preset,
                            const juce::AudioProcessorValueTreeState& apvts);
} // namespace lumen::presets
