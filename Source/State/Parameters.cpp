#include "State/Parameters.h"

#include "Engine/EngineParams.h"

namespace lumen::params
{
namespace
{
    using FloatParam  = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using BoolParam   = juce::AudioParameterBool;
    using IntParam    = juce::AudioParameterInt;
    using Attributes  = juce::AudioParameterFloatAttributes;
    using Layout      = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::NormalisableRange<float> logHzRange()
    {
        juce::NormalisableRange<float> range (20.0f, 20000.0f);
        range.setSkewForCentre (632.5f); // sqrt(20 * 20000): perceptually log sweep
        return range;
    }

    juce::NormalisableRange<float> logTimeRange (float minSeconds, float maxSeconds)
    {
        juce::NormalisableRange<float> range (minSeconds, maxSeconds);
        range.setSkewForCentre (std::sqrt (minSeconds * maxSeconds));
        return range;
    }

    juce::String hzToText (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    }

    juce::String dbToText (float v, int)
    {
        return v <= -59.95f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB";
    }

    juce::String unitToText (float v, int)  { return juce::String (v, 2); }
    juce::String centsToText (float v, int) { return juce::String (v, 1) + " ct"; }

    juce::String secondsToText (float v, int)
    {
        return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                        : juce::String (v, 2) + " s";
    }

    juce::String percentToText (float v, int)
    {
        return juce::String (juce::roundToInt (v * 100.0f)) + " %";
    }

    juce::ParameterID pid (const char* id) { return juce::ParameterID (id, 1); }

    void addEnvelope (Layout& layout, const char* attackId, const char* decayId,
                      const char* sustainId, const char* releaseId, const char* curveId,
                      const juce::String& name, const lumen::EnvParams& defaults)
    {
        const auto secAttr = Attributes().withLabel ("s").withStringFromValueFunction (secondsToText);
        const auto unitAttr = Attributes().withStringFromValueFunction (unitToText);

        layout.add (std::make_unique<FloatParam> (pid (attackId), name + " Attack",
            logTimeRange (0.001f, 10.0f), defaults.attackSeconds, secAttr));
        layout.add (std::make_unique<FloatParam> (pid (decayId), name + " Decay",
            logTimeRange (0.001f, 10.0f), defaults.decaySeconds, secAttr));
        layout.add (std::make_unique<FloatParam> (pid (sustainId), name + " Sustain",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.sustain, unitAttr));
        layout.add (std::make_unique<FloatParam> (pid (releaseId), name + " Release",
            logTimeRange (0.005f, 15.0f), defaults.releaseSeconds, secAttr));
        layout.add (std::make_unique<FloatParam> (pid (curveId), name + " Curve",
            juce::NormalisableRange<float> (-1.0f, 1.0f), defaults.curve, unitAttr));
    }

    void addOscillator (Layout& layout, const juce::String& prefix, const juce::String& name,
                        const lumen::OscParams& defaults)
    {
        const auto unitAttr = Attributes().withStringFromValueFunction (unitToText);
        const auto centsAttr = Attributes().withLabel ("ct").withStringFromValueFunction (centsToText);
        const juce::StringArray tables { "Basic", "PWM", "Harmonic Rise", "Formant", "Image" };

        auto id = [&prefix] (const char* suffix) { return juce::ParameterID (prefix + suffix, 1); };

        layout.add (std::make_unique<BoolParam>  (id ("Enabled"), name + " On", defaults.enabled));
        layout.add (std::make_unique<ChoiceParam> (id ("Table"), name + " Table", tables, defaults.table));
        layout.add (std::make_unique<FloatParam> (id ("Morph"), name + " Morph",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.morph, unitAttr));
        layout.add (std::make_unique<FloatParam> (id ("Level"), name + " Level",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.level, unitAttr));
        layout.add (std::make_unique<FloatParam> (id ("Pan"), name + " Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f), defaults.pan, unitAttr));
        layout.add (std::make_unique<IntParam>   (id ("Semi"), name + " Semitones", -24, 24, defaults.semitones));
        layout.add (std::make_unique<FloatParam> (id ("Fine"), name + " Fine",
            juce::NormalisableRange<float> (-100.0f, 100.0f), defaults.fineCents, centsAttr));
        layout.add (std::make_unique<IntParam>   (id ("Unison"), name + " Unison", 1, 8, defaults.unison));
        layout.add (std::make_unique<FloatParam> (id ("Detune"), name + " Detune",
            juce::NormalisableRange<float> (0.0f, 50.0f), defaults.detuneCents, centsAttr));
        layout.add (std::make_unique<FloatParam> (id ("Width"), name + " Width",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.width, unitAttr));
        layout.add (std::make_unique<FloatParam> (id ("Blend"), name + " Blend",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.blend, unitAttr));
        layout.add (std::make_unique<BoolParam>  (id ("PhaseRand"), name + " Phase Random", defaults.phaseRandom));
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Layout layout;
    const lumen::EngineParams defaults; // single source of default values

    const juce::NormalisableRange<float> unitRange (0.0f, 1.0f);
    const auto unitAttr = Attributes().withStringFromValueFunction (unitToText);
    const auto dbAttr = Attributes().withLabel ("dB").withStringFromValueFunction (dbToText);

    // --- Frozen first 8 (SPEC section 12) --------------------------------
    layout.add (std::make_unique<FloatParam> (pid (macro1), "Tone",    unitRange, 0.5f, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (macro2), "Motion",  unitRange, 0.5f, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (macro3), "Space",   unitRange, 0.3f, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (macro4), "Texture", unitRange, 0.2f, unitAttr));

    layout.add (std::make_unique<FloatParam> (pid (filterCutoff), "Filter Cutoff",
        logHzRange(), defaults.filterCutoffHz,
        Attributes().withLabel ("Hz").withStringFromValueFunction (hzToText)));

    layout.add (std::make_unique<FloatParam> (pid (filterRes), "Filter Resonance",
        unitRange, defaults.filterRes, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (delayMix), "Delay Mix",   unitRange, 0.0f,  unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (reverbMix), "Reverb Mix", unitRange, 0.12f, unitAttr));

    // --- Append-only from here -------------------------------------------
    layout.add (std::make_unique<FloatParam> (pid (masterGain), "Master Gain",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.01f), 0.0f, dbAttr));

    // Phase 2: oscillators, sub/noise, filter, envelopes
    addOscillator (layout, "oscA", "Osc A", defaults.oscA);
    addOscillator (layout, "oscB", "Osc B", defaults.oscB);

    layout.add (std::make_unique<ChoiceParam> (pid (subWave), "Sub Wave",
        juce::StringArray { "Sine", "Triangle", "Square" }, defaults.subWave));
    layout.add (std::make_unique<ChoiceParam> (pid (subOctave), "Sub Octave",
        juce::StringArray { "-1 oct", "-2 oct" }, defaults.subOctave - 1));
    layout.add (std::make_unique<FloatParam> (pid (subLevel), "Sub Level",
        unitRange, defaults.subLevel, unitAttr));

    layout.add (std::make_unique<ChoiceParam> (pid (noiseType), "Noise Type",
        juce::StringArray { "White", "Pink" }, defaults.noiseType));
    layout.add (std::make_unique<FloatParam> (pid (noiseLevel), "Noise Level",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), defaults.noiseDb, dbAttr));

    layout.add (std::make_unique<ChoiceParam> (pid (filterMode), "Filter Mode",
        juce::StringArray { "LP12", "LP24", "HP12", "BP12", "Notch" }, defaults.filterMode));
    layout.add (std::make_unique<FloatParam> (pid (filterDrive), "Filter Drive",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), defaults.filterDriveDb, dbAttr));
    layout.add (std::make_unique<FloatParam> (pid (filterKeytrack), "Filter Keytrack",
        unitRange, defaults.filterKeytrack,
        Attributes().withLabel ("%").withStringFromValueFunction (percentToText)));
    layout.add (std::make_unique<FloatParam> (pid (filterEnvAmount), "Filter Env Amount",
        juce::NormalisableRange<float> (-1.0f, 1.0f), defaults.filterEnvAmount, unitAttr));

    addEnvelope (layout, env1Attack, env1Decay, env1Sustain, env1Release, env1Curve, "Env 1", defaults.env1);
    addEnvelope (layout, env2Attack, env2Decay, env2Sustain, env2Release, env2Curve, "Env 2", defaults.env2);
    addEnvelope (layout, env3Attack, env3Decay, env3Sustain, env3Release, env3Curve, "Env 3", defaults.env3);

    return layout;
}
} // namespace lumen::params
