#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

#include <vector>

namespace lumen::lensstate
{
// Lens persistence (SPEC section 13, extended): presets and DAW state store
// the GENERATED WAVETABLE DATA, a 64x64 PNG thumbnail, and the ORIGINAL
// source-encoded image bytes (PNG/JPEG exactly as loaded — never a raw
// bitmap, never a path) so a reloaded project shows and re-analyzes the
// full-quality image instead of the thumbnail. Frames stay authoritative
// for the installed table (bit-exact recall on any machine); the source
// bytes rebuild the in-session image for display and for later explicit
// re-analysis (mode switch / new scan). Layout inside the APVTS state tree:
//
//   <LENS mode="0|1" target="0|1">
//     <IMAGE osc="0" frames="<base64 float32 LE, 64*2048>"
//            thumb="<base64 PNG>" data="<base64 source PNG/JPEG bytes>"
//            seed="<hex64>" source="cat.png"/>
//     <IMAGE osc="1" .../>
//   </LENS>
//
// mode: 0 = Scan, 1 = Spectral. target: 0 = Osc A, 1 = Osc B. `source` is a
// display name only (informational). Frames are raw little-endian float32
// (x64 Windows only, SPEC section 2) encoded with standard Base64. States
// saved before `data` existed simply have no such property — loaders fall
// back to the thumbnail (display) and no in-session source.
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
                 const juce::MemoryBlock& sourceBytes,
                 juce::uint64 seed, const juce::String& sourceName);

// Drops the osc's IMAGE node (frames + thumbnail + source bytes + name).
void removeImage (juce::ValueTree& state, int osc);

// True + fills `out` (kFrameFloats floats) if osc has stored frames.
bool loadImageFrames (const juce::ValueTree& state, int osc, std::vector<float>& out);
juce::Image loadThumbnail (const juce::ValueTree& state, int osc);

// True + fills `out` with the original source-encoded image bytes (PNG/JPEG
// as loaded). False for pre-`data` states.
bool loadSourceBytes (const juce::ValueTree& state, int osc, juce::MemoryBlock& out);
juce::String sourceName (const juce::ValueTree& state, int osc);
bool hasImage (const juce::ValueTree& state, int osc);

// Session-level image across preset switches (design decision, final): a
// preset switch changes synth params only. For each osc that has an image
// in `currentState`, replace whatever image `incomingState` carries for
// that osc with the current one; oscs without a session image keep the
// incoming preset's image (the Lens factory presets still work on a clean
// session). If any image carried over, the current mode/target carry over
// with it. Returns true if at least one image was preserved.
bool preserveSessionImages (const juce::ValueTree& currentState,
                            juce::ValueTree& incomingState);
} // namespace lumen::lensstate
