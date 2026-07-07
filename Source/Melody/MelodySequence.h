#pragma once

#include <juce_core/juce_core.h>

#include <vector>

// The hand-off type between the Lumena-driven melody generator (message thread)
// and the audio-thread sequencer / the UI. Deliberately free of any Lumena
// type so it can be included by the engine-adjacent player and the plain UI
// without pulling the library's headers into those translation units.
namespace lumen::melody
{
// One note of a generated melody. `velocity` is 0..1 (SynthEngine::noteOn's
// scale, not raw MIDI); `col`/`row` are the brightness-grid cell the note was
// sampled from (Lumena's Melody::cells), used to trace the melody across the
// image. Timing is in beats from the sequence start.
struct Step
{
    int    note        = 60;
    float  velocity    = 0.8f;
    double startBeats  = 0.0;
    double lengthBeats = 1.0;
    int    col         = 0;
    int    row         = 0;
};

// A whole generated melody plus the grid it was sampled on and the key it was
// built in (for the UI's "Detected: ..." readout). A pure value type; copies
// and moves are cheap enough for the infrequent generate/persist paths.
struct Sequence
{
    std::vector<Step> steps;
    int         gridCols  = 8;
    int         gridRows  = 8;
    double      totalBeats = 0.0;
    juce::String keyName;   // e.g. "D Major Pentatonic" — never read on the audio thread
};
} // namespace lumen::melody
