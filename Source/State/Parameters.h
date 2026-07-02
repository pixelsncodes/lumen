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

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace lumen::params
