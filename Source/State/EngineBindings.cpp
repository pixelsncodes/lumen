#include "State/EngineBindings.h"

#include "State/Parameters.h"

namespace lumen::bindings
{
namespace
{
    int toInt (float v) noexcept   { return static_cast<int> (v + (v >= 0.0f ? 0.5f : -0.5f)); }
    bool toBool (float v) noexcept { return v >= 0.5f; }
} // namespace

const std::vector<Binding>& all()
{
    static const std::vector<Binding> table = {
        { params::filterCutoff,    [] (EngineParams& p, float v) { p.filterCutoffHz = v; } },
        { params::filterRes,       [] (EngineParams& p, float v) { p.filterRes = v; } },
        { params::filterMode,      [] (EngineParams& p, float v) { p.filterMode = toInt (v); } },
        { params::filterDrive,     [] (EngineParams& p, float v) { p.filterDriveDb = v; } },
        { params::filterKeytrack,  [] (EngineParams& p, float v) { p.filterKeytrack = v; } },
        { params::filterEnvAmount, [] (EngineParams& p, float v) { p.filterEnvAmount = v; } },

        { params::oscAEnabled,   [] (EngineParams& p, float v) { p.oscA.enabled = toBool (v); } },
        { params::oscATable,     [] (EngineParams& p, float v) { p.oscA.table = toInt (v); } },
        { params::oscAMorph,     [] (EngineParams& p, float v) { p.oscA.morph = v; } },
        { params::oscALevel,     [] (EngineParams& p, float v) { p.oscA.level = v; } },
        { params::oscAPan,       [] (EngineParams& p, float v) { p.oscA.pan = v; } },
        { params::oscASemi,      [] (EngineParams& p, float v) { p.oscA.semitones = toInt (v); } },
        { params::oscAFine,      [] (EngineParams& p, float v) { p.oscA.fineCents = v; } },
        { params::oscAUnison,    [] (EngineParams& p, float v) { p.oscA.unison = toInt (v); } },
        { params::oscADetune,    [] (EngineParams& p, float v) { p.oscA.detuneCents = v; } },
        { params::oscAWidth,     [] (EngineParams& p, float v) { p.oscA.width = v; } },
        { params::oscABlend,     [] (EngineParams& p, float v) { p.oscA.blend = v; } },
        { params::oscAPhaseRand, [] (EngineParams& p, float v) { p.oscA.phaseRandom = toBool (v); } },

        { params::oscBEnabled,   [] (EngineParams& p, float v) { p.oscB.enabled = toBool (v); } },
        { params::oscBTable,     [] (EngineParams& p, float v) { p.oscB.table = toInt (v); } },
        { params::oscBMorph,     [] (EngineParams& p, float v) { p.oscB.morph = v; } },
        { params::oscBLevel,     [] (EngineParams& p, float v) { p.oscB.level = v; } },
        { params::oscBPan,       [] (EngineParams& p, float v) { p.oscB.pan = v; } },
        { params::oscBSemi,      [] (EngineParams& p, float v) { p.oscB.semitones = toInt (v); } },
        { params::oscBFine,      [] (EngineParams& p, float v) { p.oscB.fineCents = v; } },
        { params::oscBUnison,    [] (EngineParams& p, float v) { p.oscB.unison = toInt (v); } },
        { params::oscBDetune,    [] (EngineParams& p, float v) { p.oscB.detuneCents = v; } },
        { params::oscBWidth,     [] (EngineParams& p, float v) { p.oscB.width = v; } },
        { params::oscBBlend,     [] (EngineParams& p, float v) { p.oscB.blend = v; } },
        { params::oscBPhaseRand, [] (EngineParams& p, float v) { p.oscB.phaseRandom = toBool (v); } },

        { params::subWave,   [] (EngineParams& p, float v) { p.subWave = toInt (v); } },
        { params::subOctave, [] (EngineParams& p, float v) { p.subOctave = toInt (v) + 1; } }, // choice 0/1 -> -1/-2 oct
        { params::subLevel,  [] (EngineParams& p, float v) { p.subLevel = v; } },
        { params::noiseType, [] (EngineParams& p, float v) { p.noiseType = toInt (v); } },
        { params::noiseLevel,[] (EngineParams& p, float v) { p.noiseDb = v; } },

        { params::env1Attack,  [] (EngineParams& p, float v) { p.env1.attackSeconds = v; } },
        { params::env1Decay,   [] (EngineParams& p, float v) { p.env1.decaySeconds = v; } },
        { params::env1Sustain, [] (EngineParams& p, float v) { p.env1.sustain = v; } },
        { params::env1Release, [] (EngineParams& p, float v) { p.env1.releaseSeconds = v; } },
        { params::env1Curve,   [] (EngineParams& p, float v) { p.env1.curve = v; } },
        { params::env2Attack,  [] (EngineParams& p, float v) { p.env2.attackSeconds = v; } },
        { params::env2Decay,   [] (EngineParams& p, float v) { p.env2.decaySeconds = v; } },
        { params::env2Sustain, [] (EngineParams& p, float v) { p.env2.sustain = v; } },
        { params::env2Release, [] (EngineParams& p, float v) { p.env2.releaseSeconds = v; } },
        { params::env2Curve,   [] (EngineParams& p, float v) { p.env2.curve = v; } },
        { params::env3Attack,  [] (EngineParams& p, float v) { p.env3.attackSeconds = v; } },
        { params::env3Decay,   [] (EngineParams& p, float v) { p.env3.decaySeconds = v; } },
        { params::env3Sustain, [] (EngineParams& p, float v) { p.env3.sustain = v; } },
        { params::env3Release, [] (EngineParams& p, float v) { p.env3.releaseSeconds = v; } },
        { params::env3Curve,   [] (EngineParams& p, float v) { p.env3.curve = v; } },
    };
    return table;
}

bool set (EngineParams& engineParams, const juce::String& id, float value)
{
    for (const auto& binding : all())
    {
        if (id == binding.id)
        {
            binding.apply (engineParams, value);
            return true;
        }
    }
    return false;
}
} // namespace lumen::bindings
