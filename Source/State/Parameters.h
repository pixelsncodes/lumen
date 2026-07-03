#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace lumen::params
{
inline constexpr int kStateVersion = 1;

// Frozen first 8 (SPEC section 12, Maschine knob page 1).
// NEVER reorder, remove, or insert before these.
inline constexpr const char* macro1       = "macro1";
inline constexpr const char* macro2       = "macro2";
inline constexpr const char* macro3       = "macro3";
inline constexpr const char* macro4       = "macro4";
inline constexpr const char* filterCutoff = "filterCutoff";
inline constexpr const char* filterRes    = "filterRes";
inline constexpr const char* delayMix     = "delayMix";
inline constexpr const char* reverbMix    = "reverbMix";

// Parameters after the frozen block are append-only.
inline constexpr const char* masterGain   = "masterGain";

// --- Phase 2: core engine ---------------------------------------------
// Osc A/B ids are "oscA"/"oscB" + suffix (see EngineBindings.cpp).
inline constexpr const char* oscAEnabled   = "oscAEnabled";
inline constexpr const char* oscATable     = "oscATable";
inline constexpr const char* oscAMorph     = "oscAMorph";
inline constexpr const char* oscALevel     = "oscALevel";
inline constexpr const char* oscAPan       = "oscAPan";
inline constexpr const char* oscASemi      = "oscASemi";
inline constexpr const char* oscAFine      = "oscAFine";
inline constexpr const char* oscAUnison    = "oscAUnison";
inline constexpr const char* oscADetune    = "oscADetune";
inline constexpr const char* oscAWidth     = "oscAWidth";
inline constexpr const char* oscABlend     = "oscABlend";
inline constexpr const char* oscAPhaseRand = "oscAPhaseRand";

inline constexpr const char* oscBEnabled   = "oscBEnabled";
inline constexpr const char* oscBTable     = "oscBTable";
inline constexpr const char* oscBMorph     = "oscBMorph";
inline constexpr const char* oscBLevel     = "oscBLevel";
inline constexpr const char* oscBPan       = "oscBPan";
inline constexpr const char* oscBSemi      = "oscBSemi";
inline constexpr const char* oscBFine      = "oscBFine";
inline constexpr const char* oscBUnison    = "oscBUnison";
inline constexpr const char* oscBDetune    = "oscBDetune";
inline constexpr const char* oscBWidth     = "oscBWidth";
inline constexpr const char* oscBBlend     = "oscBBlend";
inline constexpr const char* oscBPhaseRand = "oscBPhaseRand";

inline constexpr const char* subWave       = "subWave";
inline constexpr const char* subOctave     = "subOctave";
inline constexpr const char* subLevel      = "subLevel";
inline constexpr const char* noiseType     = "noiseType";
inline constexpr const char* noiseLevel    = "noiseLevel";

inline constexpr const char* filterMode      = "filterMode";
inline constexpr const char* filterDrive     = "filterDrive";
inline constexpr const char* filterKeytrack  = "filterKeytrack";
inline constexpr const char* filterEnvAmount = "filterEnvAmount";

inline constexpr const char* env1Attack  = "env1Attack";
inline constexpr const char* env1Decay   = "env1Decay";
inline constexpr const char* env1Sustain = "env1Sustain";
inline constexpr const char* env1Release = "env1Release";
inline constexpr const char* env1Curve   = "env1Curve";
inline constexpr const char* env2Attack  = "env2Attack";
inline constexpr const char* env2Decay   = "env2Decay";
inline constexpr const char* env2Sustain = "env2Sustain";
inline constexpr const char* env2Release = "env2Release";
inline constexpr const char* env2Curve   = "env2Curve";
inline constexpr const char* env3Attack  = "env3Attack";
inline constexpr const char* env3Decay   = "env3Decay";
inline constexpr const char* env3Sustain = "env3Sustain";
inline constexpr const char* env3Release = "env3Release";
inline constexpr const char* env3Curve   = "env3Curve";

// --- Phase 3: LFOs (ids are "lfo1".."lfo3" + suffix) --------------------
inline constexpr const char* lfo1Shape   = "lfo1Shape";
inline constexpr const char* lfo1Sync    = "lfo1Sync";
inline constexpr const char* lfo1Rate    = "lfo1Rate";
inline constexpr const char* lfo1SyncDiv = "lfo1SyncDiv";
inline constexpr const char* lfo1Phase   = "lfo1Phase";
inline constexpr const char* lfo1Fade    = "lfo1Fade";
inline constexpr const char* lfo1Mode    = "lfo1Mode";
inline constexpr const char* lfo2Shape   = "lfo2Shape";
inline constexpr const char* lfo2Sync    = "lfo2Sync";
inline constexpr const char* lfo2Rate    = "lfo2Rate";
inline constexpr const char* lfo2SyncDiv = "lfo2SyncDiv";
inline constexpr const char* lfo2Phase   = "lfo2Phase";
inline constexpr const char* lfo2Fade    = "lfo2Fade";
inline constexpr const char* lfo2Mode    = "lfo2Mode";
inline constexpr const char* lfo3Shape   = "lfo3Shape";
inline constexpr const char* lfo3Sync    = "lfo3Sync";
inline constexpr const char* lfo3Rate    = "lfo3Rate";
inline constexpr const char* lfo3SyncDiv = "lfo3SyncDiv";
inline constexpr const char* lfo3Phase   = "lfo3Phase";
inline constexpr const char* lfo3Fade    = "lfo3Fade";
inline constexpr const char* lfo3Mode    = "lfo3Mode";

// --- Phase 4: effects (delayMix/reverbMix live in the frozen first 8) ---
inline constexpr const char* driveEnabled  = "driveEnabled";
inline constexpr const char* driveAmount   = "driveAmount";
inline constexpr const char* driveTone     = "driveTone";
inline constexpr const char* chorusEnabled = "chorusEnabled";
inline constexpr const char* chorusRate    = "chorusRate";
inline constexpr const char* chorusDepth   = "chorusDepth";
inline constexpr const char* chorusMix     = "chorusMix";
inline constexpr const char* delayEnabled  = "delayEnabled";
inline constexpr const char* delaySync     = "delaySync";
inline constexpr const char* delayTime     = "delayTime";
inline constexpr const char* delayDiv      = "delayDiv";
inline constexpr const char* delayFeedback = "delayFeedback";
inline constexpr const char* delayDamp     = "delayDamp";
inline constexpr const char* delayPingPong = "delayPingPong";
inline constexpr const char* reverbEnabled = "reverbEnabled";
inline constexpr const char* reverbSize    = "reverbSize";
inline constexpr const char* reverbDamp    = "reverbDamp";
inline constexpr const char* reverbWidth   = "reverbWidth";

// --- Phase 7: voice modes + glide (SPEC section 11) ----------------------
inline constexpr const char* voiceMode = "voiceMode";
inline constexpr const char* glideTime = "glideTime";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace lumen::params
