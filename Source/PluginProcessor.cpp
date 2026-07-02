#include "PluginProcessor.h"

#include "UI/PluginEditor.h"

namespace
{
    constexpr int kNumVoices = 16;
} // namespace

LumenAudioProcessor::LumenAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", lumen::params::createParameterLayout())
{
    apvts.state.setProperty ("stateVersion", lumen::params::kStateVersion, nullptr);

    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new lumen::TestVoice());

    synth.addSound (new lumen::TestSound());
    synth.setNoteStealingEnabled (true);

    masterGainDb = apvts.getRawParameterValue (lumen::params::masterGain);
}

const juce::String LumenAudioProcessor::getName() const
{
    return "Lumen";
}

void LumenAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    synth.setCurrentPlaybackSampleRate (sampleRate);

    masterGainLinear.reset (sampleRate, 0.02); // ~20 ms smoothing (SPEC section 12)
    masterGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterGainDb->load(), -60.0f));
}

bool LumenAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LumenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    masterGainLinear.setTargetValue (juce::Decibels::decibelsToGain (masterGainDb->load(), -60.0f));
    masterGainLinear.applyGain (buffer, buffer.getNumSamples());
}

juce::AudioProcessorEditor* LumenAudioProcessor::createEditor()
{
    return new LumenAudioProcessorEditor (*this);
}

void LumenAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LumenAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LumenAudioProcessor();
}
