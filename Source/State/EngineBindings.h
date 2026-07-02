#pragma once

#include "Engine/EngineParams.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace lumen::bindings
{
// One row per engine-facing APVTS parameter: the id and how its raw
// (denormalized) value lands in EngineParams. Used by the processor (per
// block from atomics) and by lumen_render's --set flags, so both paths are
// guaranteed to agree.
struct Binding
{
    const char* id;
    void (*apply) (EngineParams&, float);
};

const std::vector<Binding>& all();

// Applies a value by id; false if the id is unknown / not engine-facing.
bool set (EngineParams& params, const juce::String& id, float value);
} // namespace lumen::bindings
