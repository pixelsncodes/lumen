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

    // Init-patch modulation defaults (SPEC pillar 1 / section 14). All macro
    // ranges are chosen to sum to a ZERO offset at the frozen macro defaults
    // (0.5 / 0.5 / 0.3 / 0.2, SPEC section 12), so the Init timbre stays
    // exactly as approved and the knobs sweep away from it:
    //   Tone     down = darker (cutoff), up = hotter (filter drive)
    //   Motion   speed of the built-in LFO1 -> Osc A morph wobble
    //   Space    reverb + delay mix, dry at 0
    //   Texture  unison detune spread + noise bed
    struct InitSlot { const char* source; const char* dest; float depth; };
    constexpr InitSlot kInitSlots[] = {
        { "lfo1", "oscAMorph", 0.25f },
    };

    struct InitMap { int macro; const char* dest; float min; float max; };
    constexpr InitMap kInitMaps[] = {
        { 0, "filterCutoff", -0.60f,  0.60f  },
        { 0, "filterDrive",  -0.25f,  0.25f  },
        { 1, "lfo1Rate",     -0.20f,  0.20f  },
        { 2, "reverbMix",    -0.195f, 0.455f },
        { 2, "delayMix",     -0.12f,  0.28f  },
        { 3, "oscADetune",   -0.10f,  0.40f  },
        { 3, "noiseLevel",   -0.10f,  0.40f  },
    };
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
        "reverbSize", "reverbDamp", "reverbWidth",
        "lfo1Rate", "lfo2Rate", "lfo3Rate"
    };
    return tokens;
}

int sourceFromToken (const juce::String& token) { return sourceTokens().indexOf (token); }
int destFromToken (const juce::String& token)   { return destTokens().indexOf (token); }

void ensureTrees (juce::ValueTree& state)
{
    auto matrix = state.getChildWithName (kMatrixType);
    const bool freshMatrix = ! matrix.isValid();
    if (freshMatrix)
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
    const bool freshMacros = ! macros.isValid();
    if (freshMacros)
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

    // Init defaults only on freshly created trees; saved states (which always
    // carry both nodes, even with every slot/map empty) load untouched.
    if (freshMatrix)
    {
        int slotIndex = 0;
        for (const auto& s : kInitSlots)
        {
            auto slot = matrix.getChild (slotIndex++);
            slot.setProperty ("source", s.source, nullptr);
            slot.setProperty ("dest", s.dest, nullptr);
            slot.setProperty ("depth", s.depth, nullptr);
            slot.setProperty ("enabled", true, nullptr);
        }
    }
    if (freshMacros)
    {
        for (const auto& m : kInitMaps)
        {
            juce::ValueTree map (kMapType);
            map.setProperty ("dest", m.dest, nullptr);
            map.setProperty ("min", m.min, nullptr);
            map.setProperty ("max", m.max, nullptr);
            macros.getChild (m.macro).appendChild (map, nullptr);
        }
    }
}

void applyInitModDefaults (mod::Config& out)
{
    int slotIndex = 0;
    for (const auto& s : kInitSlots)
    {
        auto& slot = out.slots[slotIndex++];
        slot.source = sourceFromToken (s.source);
        slot.dest = destFromToken (s.dest);
        slot.depth = s.depth;
        slot.enabled = slot.source >= 0 && slot.dest >= 0;
    }

    int mapIndex[mod::kNumMacros] {};
    for (const auto& m : kInitMaps)
    {
        const int dest = destFromToken (m.dest);
        if (dest < 0 || mapIndex[m.macro] >= mod::kMaxMacroMaps)
            continue;
        auto& map = out.macroMaps[m.macro][mapIndex[m.macro]++];
        map.dest = dest;
        map.rangeMin = m.min;
        map.rangeMax = m.max;
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
