#pragma once

#include "Engine/EngineParams.h"
#include "Engine/Wavetable.h"

namespace lumen
{
// The four analytic factory tables (SPEC section 4), 64 frames each, built
// deterministically on first use (message thread / prepareToPlay — never the
// audio thread). `TableChoice::image` resolves to Basic until the Lens
// engine lands in Phase 6 (DECISIONS.md).
namespace factory
{
    constexpr int kNumFrames = 64;

    // Basic morphs sine -> triangle -> saw -> square with pure waveforms at
    // frames 0 / 21 / 42 / 63. Pure saw morph position = 42/63.
    constexpr float kBasicSawMorph = 42.0f / 63.0f;

    const Wavetable& get (TableChoice choice);
    const Wavetable& forIndex (int tableParamValue); // clamps, maps image->basic
} // namespace factory
} // namespace lumen
