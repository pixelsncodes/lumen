#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

#include <vector>

namespace lumen::lensstate
{
// Lens persistence (SPEC section 13): presets and DAW state store the
// GENERATED WAVETABLE DATA plus a 64x64 PNG thumbnail — never a path to the
// original file — so projects recall bit-exactly on any machine. Layout
// inside the APVTS state tree:
//
//   <LENS mode="0|1" target="0|1">
//     <IMAGE osc="0" frames="<base64 float32 LE, 64*2048>"
//            thumb="<base64 PNG>" seed="<hex64>" source="cat.png"/>
//     <IMAGE osc="1" .../>
//   </LENS>
//
// mode: 0 = Scan, 1 = Spectral. target: 0 = Osc A, 1 = Osc B. `source` is a
// display name only (informational). Frames are raw little-endian float32
// (x64 Windows only, SPEC section 2) encoded with standard Base64.
//
// The COLORS ("Set patch from colors") toggle is NOT stored here: it is a
// live Lens driving mode, session-global on the LensController, never
// recalled from or written into preset/DAW state. Older states may carry a
// chroma="0|1" attribute on LENS — it is simply ignored on load.

inline constexpr int kFrameFloats = 64 * 2048;

juce::ValueTree ensureTree (juce::ValueTree& state); // create-if-missing
juce::ValueTree getTree (const juce::ValueTree& state);

int  mode (const juce::ValueTree& state);    // 0 scan, 1 spectral
int  target (const juce::ValueTree& state);  // 0 A, 1 B
void setMode (juce::ValueTree& state, int newMode);
void setTarget (juce::ValueTree& state, int osc);

void storeImage (juce::ValueTree& state, int osc,
                 const std::vector<float>& frames,
                 const juce::MemoryBlock& thumbPng,
                 juce::uint64 seed, const juce::String& sourceName);

// Drops the osc's IMAGE node (frames + thumbnail + name) from the state.
void removeImage (juce::ValueTree& state, int osc);

// True + fills `out` (kFrameFloats floats) if osc has stored frames.
bool loadImageFrames (const juce::ValueTree& state, int osc, std::vector<float>& out);
juce::Image loadThumbnail (const juce::ValueTree& state, int osc);
juce::String sourceName (const juce::ValueTree& state, int osc);
bool hasImage (const juce::ValueTree& state, int osc);
} // namespace lumen::lensstate
