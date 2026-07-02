#include "PluginProcessor.h"

#include "State/EngineBindings.h"
#include "UI/PluginEditor.h"

LumenAudioProcessor::LumenAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", lumen::params::createParameterLayout())
{
    apvts.state.setProperty ("stateVersion", lumen::params::kStateVersion, nullptr);

    bindingValues.reserve (lumen::bindings::all().size());
    for (const auto& binding : lumen::bindings::all())
    {
        auto* value = apvts.getRawParameterValue (binding.id);
        jassert (value != nullptr); // binding id must exist in the layout
        bindingValues.push_back (value);
    }

    masterGainDb = apvts.getRawParameterValue (lumen::params::masterGain);
}

const juce::String LumenAudioProcessor::getName() const
{
    return "Lumen";
}

void LumenAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);

    masterGainLinear.reset (sampleRate, 0.02); // ~20 ms smoothing (SPEC section 12)
    masterGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterGainDb->load(), -60.0f));
}

void LumenAudioProcessor::reset()
{
    engine.reset();
}

bool LumenAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LumenAudioProcessor::renderSegment (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    if (numSamples <= 0)
        return;
    engine.render (buffer.getWritePointer (0, start), buffer.getWritePointer (1, start), numSamples);
}

void LumenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // Snapshot APVTS -> engine once per block (no locks, plain atomics).
    lumen::EngineParams params;
    const auto& bindings = lumen::bindings::all();
    for (size_t i = 0; i < bindings.size(); ++i)
        bindings[i].apply (params, bindingValues[i]->load());
    engine.setParams (params);

    // Sample-accurate note events: render up to each event, then apply it.
    int segmentStart = 0;
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const int position = juce::jlimit (0, buffer.getNumSamples(), metadata.samplePosition);

        renderSegment (buffer, segmentStart, position - segmentStart);
        segmentStart = position;

        if (message.isNoteOn())
            engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            engine.noteOff (message.getNoteNumber());
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            engine.reset();
    }
    renderSegment (buffer, segmentStart, buffer.getNumSamples() - segmentStart);

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
