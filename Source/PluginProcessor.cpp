#include "PluginProcessor.h"

#include "State/EngineBindings.h"
#include "State/ModState.h"
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

    initializeModState();
}

LumenAudioProcessor::~LumenAudioProcessor()
{
    apvts.state.removeListener (this);
}

void LumenAudioProcessor::initializeModState()
{
    lumen::modstate::ensureTrees (apvts.state);
    publishModConfig();
    apvts.state.addListener (this);
}

void LumenAudioProcessor::publishModConfig()
{
    auto* entry = &modConfigPool[nextPoolEntry];
    nextPoolEntry = (nextPoolEntry + 1) % 8;
    lumen::modstate::buildConfig (apvts.state, *entry);
    publishedModConfig.store (entry, std::memory_order_release);
}

void LumenAudioProcessor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    const auto parent = tree.getParent();
    if (tree.hasType ("SLOT") || tree.hasType ("MAP")
        || parent.hasType ("MODMATRIX") || parent.hasType ("MACRO"))
        publishModConfig();
}

void LumenAudioProcessor::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent.hasType ("MODMATRIX") || parent.hasType ("MACROS") || parent.hasType ("MACRO"))
        publishModConfig();
}

void LumenAudioProcessor::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent.hasType ("MODMATRIX") || parent.hasType ("MACROS") || parent.hasType ("MACRO"))
        publishModConfig();
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

void LumenAudioProcessor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff())
        engine.noteOff (message.getNoteNumber());
    else if (message.isController() && message.getControllerNumber() == 1)
        engine.setModWheel (static_cast<float> (message.getControllerValue()) / 127.0f);
    else if (message.isChannelPressure())
        engine.setAftertouch (static_cast<float> (message.getChannelPressureValue()) / 127.0f);
    else if (message.isPitchWheel())
        engine.setPitchBend ((static_cast<float> (message.getPitchWheelValue()) - 8192.0f) / 8192.0f);
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        engine.reset();
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

    if (auto* config = publishedModConfig.load (std::memory_order_acquire))
        params.mod = *config;

    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            if (const auto bpm = position->getBpm())
                params.bpm = *bpm > 1.0 ? *bpm : 120.0;

    engine.setParams (params);

    // Sample-accurate note events: render up to each event, then apply it.
    int segmentStart = 0;
    for (const auto metadata : midiMessages)
    {
        const int position = juce::jlimit (0, buffer.getNumSamples(), metadata.samplePosition);
        renderSegment (buffer, segmentStart, position - segmentStart);
        segmentStart = position;
        handleMidiMessage (metadata.getMessage());
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
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.state.removeListener (this);
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            initializeModState(); // re-ensure trees, republish, re-listen
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LumenAudioProcessor();
}
