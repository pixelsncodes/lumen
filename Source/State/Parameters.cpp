#include "State/Parameters.h"

namespace lumen::params
{
namespace
{
    using FloatParam = juce::AudioParameterFloat;
    using Attributes = juce::AudioParameterFloatAttributes;

    juce::NormalisableRange<float> logHzRange()
    {
        juce::NormalisableRange<float> range (20.0f, 20000.0f);
        range.setSkewForCentre (632.5f); // sqrt(20 * 20000): perceptually log sweep
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

    juce::String unitToText (float v, int)
    {
        return juce::String (v, 2);
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::NormalisableRange<float> unitRange (0.0f, 1.0f);
    const auto unitAttr = Attributes().withStringFromValueFunction (unitToText);

    // --- Frozen first 8 (SPEC section 12) --------------------------------
    layout.add (std::make_unique<FloatParam> (juce::ParameterID (macro1, 1), "Tone",    unitRange, 0.5f, unitAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID (macro2, 1), "Motion",  unitRange, 0.5f, unitAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID (macro3, 1), "Space",   unitRange, 0.3f, unitAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID (macro4, 1), "Texture", unitRange, 0.2f, unitAttr));

    layout.add (std::make_unique<FloatParam> (juce::ParameterID (filterCutoff, 1), "Filter Cutoff",
        logHzRange(), 20000.0f,
        Attributes().withLabel ("Hz").withStringFromValueFunction (hzToText)));

    layout.add (std::make_unique<FloatParam> (juce::ParameterID (filterRes, 1), "Filter Resonance", unitRange, 0.12f, unitAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID (delayMix, 1), "Delay Mix",   unitRange, 0.0f,  unitAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID (reverbMix, 1), "Reverb Mix", unitRange, 0.12f, unitAttr));

    // --- Append-only from here -------------------------------------------
    layout.add (std::make_unique<FloatParam> (juce::ParameterID (masterGain, 1), "Master Gain",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.01f), 0.0f,
        Attributes().withLabel ("dB").withStringFromValueFunction (dbToText)));

    return layout;
}
} // namespace lumen::params
