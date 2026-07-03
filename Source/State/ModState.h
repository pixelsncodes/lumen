#pragma once

#include "Engine/ModMatrix.h"

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace lumen::modstate
{
// Matrix + macro-map persistence (SPEC section 9: stored in plugin state,
// not host-automatable). Layout inside the APVTS state tree:
//
//   <MODMATRIX> <SLOT source="lfo1" dest="filterCutoff" depth="0.4" enabled="1"/> x24 </MODMATRIX>
//   <MACROS> <MACRO index="0"> <MAP dest="oscAMorph" min="0" max="0.5"/> ... </MACRO> x4 </MACROS>
//
// Sources and destinations are stored as string tokens so states stay
// readable and survive enum growth.

// CLI/state tokens, indexed by mod::Source.
const juce::StringArray& sourceTokens();
// Human-readable names for the UI, indexed by mod::Source.
const juce::StringArray& sourceNames();
// Destination tokens are the APVTS parameter ids, indexed by mod::Dest.
const juce::StringArray& destTokens();

int sourceFromToken (const juce::String& token); // -1 if unknown
int destFromToken (const juce::String& token);   // -1 if unknown

// Creates MODMATRIX (24 slots) / MACROS (4 x 8) children if missing. A
// freshly created tree (= the Init patch) gets the default modulation set —
// SPEC pillar 1 / section 14: the four macros always do something musical —
// while states loaded from a preset/host are left exactly as saved.
void ensureTrees (juce::ValueTree& state);

// The same Init defaults written straight into the engine POD, so
// lumen_render --preset init renders the patch the plugin actually ships.
void applyInitModDefaults (mod::Config& out);

// Parses the trees into the engine POD (missing/invalid entries disabled).
void buildConfig (const juce::ValueTree& state, mod::Config& out);
} // namespace lumen::modstate
