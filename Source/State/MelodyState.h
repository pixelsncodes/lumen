#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "Melody/MelodySequence.h"

namespace lumen::melodystate
{
// Melody persistence. The musical controls (key mode, sequence length,
// brightness bias, phrase mode, ornaments) are ordinary APVTS parameters and
// serialize with the rest of the patch; this sub-tree carries the two
// non-parameter bits of state — the RNG seed and the lock toggle — plus the
// generated note sequence itself.
//
// The sequence is stored explicitly (not just regenerated from the seed)
// because Lens persists only a 64x64 thumbnail of the source image, never the
// full-resolution pixels (SPEC 13). Regenerating from the seed after a reload
// would therefore sample a different brightness grid and produce a *different*
// melody; storing the notes guarantees a saved project recalls the exact same
// melody it played when saved. Layout inside the APVTS state tree:
//
//   <MELODY seed="<hex64>" locked="0|1">
//     <SEQ cols="8" rows="8" beats="32.0" key="D Major Pentatonic"
//          steps="note,vel,start,len,col,row;..."/>
//   </MELODY>
juce::ValueTree ensureTree (juce::ValueTree& state); // create-if-missing
juce::ValueTree getTree (const juce::ValueTree& state);

juce::uint64 seed (const juce::ValueTree& state);
bool         locked (const juce::ValueTree& state);
void         setSeed (juce::ValueTree& state, juce::uint64 seed);
void         setLocked (juce::ValueTree& state, bool locked);

// Store / load the generated note sequence. hasSequence() is false until a
// melody has been generated (or one was restored from a saved state).
void storeSequence (juce::ValueTree& state, const melody::Sequence& seq);
bool hasSequence (const juce::ValueTree& state);
bool loadSequence (const juce::ValueTree& state, melody::Sequence& out);
} // namespace lumen::melodystate
