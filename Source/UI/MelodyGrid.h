#pragma once

#include <juce_graphics/juce_graphics.h>

#include "Melody/MelodySequence.h"

// Shared grid-overlay drawing used by both the Melody side panel's image view
// and the small Lens image view (so the sampling grid, the sounding-cell glow
// and the sampled-path trace look identical in both places).
namespace melodygrid
{
struct DrawInfo
{
    juce::Rectangle<float> imageArea;             // where the image is drawn
    int cols = 8, rows = 8;
    const lumen::melody::Sequence* seq = nullptr; // for the path trace (may be null)
    int liveCol = -1, liveRow = -1;               // currently sounding cell
    float glow = 0.0f;                            // 0..1 highlight intensity
    bool playing = false;
    juce::Colour accent;
};

// Draws the grid lines, the faint sampled-path trace (when stopped), and the
// sounding-cell glow. Assumes g is already clipped to imageArea if desired.
void draw (juce::Graphics& g, const DrawInfo& info);
} // namespace melodygrid
