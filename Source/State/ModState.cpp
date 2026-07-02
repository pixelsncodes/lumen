#include "State/ModState.h"

namespace lumen::modstate
{
namespace
{
    const juce::Identifier kMatrixType ("MODMATRIX");
    const juce::Identifier kSlotType ("SLOT");
    const juce::Identifier kMacrosType ("MACROS");
    const juce::Identifier kMacroType ("MACRO");
    const juce::Identifier kMapType ("MAP");
} // namespace

const juce::StringArray& sourceTokens()
{
    static const juce::StringArray tokens {
        "env1", "env2", "env3", "lfo1", "lfo2", "lfo3",
        "macro1", "macro2", "macro3", "macro4",
        "velocity", "wheel", "aftertouch", "bend", "keytrack", "random"
    };
    return tokens;
}

const juce::StringArray& sourceNames()
{
    static const juce::StringArray names {
        "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3",
        "Macro 1", "Macro 2", "Macro 3", "Macro 4",
        "Velocity", "Mod Wheel", "Aftertouch", "Pitch Bend", "Keytrack", "Random"
    };
    return names;
}

const juce::StringArray& destTokens()
{
    // Order must match mod::Dest (these are the APVTS parameter ids).
    static const juce::StringArray tokens {
        "filterCutoff", "filterRes", "filterDrive", "filterKeytrack", "filterEnvAmount",
        "oscAMorph", "oscALevel", "oscAPan", "oscAFine", "oscADetune", "oscAWidth", "oscABlend",
        "oscBMorph", "oscBLevel", "oscBPan", "oscBFine", "oscBDetune", "oscBWidth", "oscBBlend",
        "subLevel", "noiseLevel",
        "env1Attack", "env1Decay", "env1Sustain", "env1Release", "env1Curve",
        "env2Attack", "env2Decay", "env2Sustain", "env2Release", "env2Curve",
        "env3Attack", "env3Decay", "env3Sustain", "env3Release", "env3Curve",
        "delayMix", "reverbMix", "masterGain",
        "driveAmount", "driveTone",
        "chorusRate", "chorusDepth", "chorusMix",
        "delayTime", "delayFeedback", "delayDamp",
        "reverbSize", "reverbDamp", "reverbWidth"
    };
    return tokens;
}

int sourceFromToken (const juce::String& token) { return sourceTokens().indexOf (token); }
int destFromToken (const juce::String& token)   { return destTokens().indexOf (token); }

void ensureTrees (juce::ValueTree& state)
{
    auto matrix = state.getChildWithName (kMatrixType);
    if (! matrix.isValid())
    {
        matrix = juce::ValueTree (kMatrixType);
        state.appendChild (matrix, nullptr);
    }
    while (matrix.getNumChildren() < mod::kNumSlots)
    {
        juce::ValueTree slot (kSlotType);
        slot.setProperty ("source", "lfo1", nullptr);
        slot.setProperty ("dest", "", nullptr);
        slot.setProperty ("depth", 0.0, nullptr);
        slot.setProperty ("enabled", false, nullptr);
        matrix.appendChild (slot, nullptr);
    }

    auto macros = state.getChildWithName (kMacrosType);
    if (! macros.isValid())
    {
        macros = juce::ValueTree (kMacrosType);
        state.appendChild (macros, nullptr);
    }
    while (macros.getNumChildren() < mod::kNumMacros)
    {
        juce::ValueTree macro (kMacroType);
        macro.setProperty ("index", macros.getNumChildren(), nullptr);
        macros.appendChild (macro, nullptr);
    }
}

void buildConfig (const juce::ValueTree& state, mod::Config& out)
{
    out = {};

    const auto matrix = state.getChildWithName (kMatrixType);
    for (int i = 0; i < mod::kNumSlots && i < matrix.getNumChildren(); ++i)
    {
        const auto slot = matrix.getChild (i);
        if (! slot.hasType (kSlotType))
            continue;
        auto& s = out.slots[i];
        s.source = sourceFromToken (slot["source"].toString());
        s.dest = destFromToken (slot["dest"].toString());
        s.depth = juce::jlimit (-1.0f, 1.0f, static_cast<float> (static_cast<double> (slot["depth"])));
        s.enabled = static_cast<bool> (slot["enabled"]) && s.source >= 0 && s.dest >= 0;
        if (s.source < 0)
            s.source = 0;
    }

    const auto macros = state.getChildWithName (kMacrosType);
    for (int m = 0; m < mod::kNumMacros && m < macros.getNumChildren(); ++m)
    {
        const auto macro = macros.getChild (m);
        int mapIndex = 0;
        for (int c = 0; c < macro.getNumChildren() && mapIndex < mod::kMaxMacroMaps; ++c)
        {
            const auto map = macro.getChild (c);
            if (! map.hasType (kMapType))
                continue;
            const int dest = destFromToken (map["dest"].toString());
            if (dest < 0)
                continue;
            auto& mm = out.macroMaps[m][mapIndex++];
            mm.dest = dest;
            mm.rangeMin = juce::jlimit (-1.0f, 1.0f, static_cast<float> (static_cast<double> (map["min"])));
            mm.rangeMax = juce::jlimit (-1.0f, 1.0f, static_cast<float> (static_cast<double> (map["max"])));
        }
    }
}
} // namespace lumen::modstate
