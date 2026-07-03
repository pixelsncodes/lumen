#pragma once

#include <juce_graphics/juce_graphics.h>

// Procedurally generated, fully deterministic test images (PHASES Phase 6:
// vertical gradient, stripes, checker). Shared by lumen_render --gen-image
// and the unit tests, so file-level and in-memory verification use the
// exact same pixels.

namespace lumen::lens::testimages
{
// 512x64 "vertical gradient" of spatial frequency: row r holds exactly
// (r + 1) sine cycles, so Scan-mode frame i is (about) harmonic i + 1 and
// the spectral centroid rises monotonically down the morph range. (A plain
// luminance gradient has constant rows, which DC removal turns into the
// silence guard — see DECISIONS.md.)
juce::Image gradient();

// 512x512 black with 3-px white horizontal stripes at the exact log-map
// positions of these harmonics — Spectral mode must put its peaks there.
inline constexpr int kStripeHarmonics[] = { 2, 4, 8, 16, 32, 64 };
inline constexpr int kNumStripes = 6;
juce::Image stripes();

// 512x512 checkerboard, 32-px cells (chroma: zero saturation, high edges).
juce::Image checker();

// By name: "gradient" | "stripes" | "checker"; invalid Image if unknown.
juce::Image byName (const juce::String& name);
} // namespace lumen::lens::testimages
