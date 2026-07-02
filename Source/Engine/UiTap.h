#pragma once

#include "Engine/ModMatrix.h"

#include <atomic>

namespace lumen
{
// Lock-free block-rate snapshot the audio thread publishes for the UI
// (SPEC section 15: visualizers never lock or allocate against the audio
// thread). Pure stdlib — safe in the Engine layer.
//
// destNorm[d] = the final normalized value of destination d after global
// matrix + macro + (newest active voice's) poly modulation — what the
// animated mod-arc indicator on a knob shows. sourceValue[s] = the current
// value of each mod source (global sources per block; poly sources from the
// newest active voice). lfoPhase[k] = 0..1 phase of LFO k (mono LFO, or the
// newest voice's poly LFO) for the LFO view's position marker.
//
// Values are refreshed only while the host is calling render(); the UI
// treats them as "last known" (arcs freeze when audio stops — acceptable).
struct UiTap
{
    std::atomic<float> destNorm[mod::kNumDests] {};
    std::atomic<float> sourceValue[mod::kNumSources] {};
    std::atomic<float> lfoPhase[3] {};
    std::atomic<int>   activeVoices { 0 };
};
} // namespace lumen
