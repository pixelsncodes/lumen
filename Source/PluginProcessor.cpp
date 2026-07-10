#include "PluginProcessor.h"

#include "Lens/LensController.h"
#include "Melody/MelodyController.h"
#include "Melody/MelodyPlayer.h"
#include "State/EngineBindings.h"
#include "State/MidiLearn.h"
#include "State/ModState.h"
#include "State/PresetManager.h"
#include "UI/PluginEditor.h"

#include <algorithm>
#include <utility>

LumenAudioProcessor::LumenAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", lumen::params::createParameterLayout())
{
    tapBuffer.assign (kTapCapacity, 0.0f);

    apvts.state.setProperty ("stateVersion", lumen::params::kStateVersion, nullptr);

    bindingValues.reserve (lumen::bindings::all().size());
    for (const auto& binding : lumen::bindings::all())
    {
        auto* value = apvts.getRawParameterValue (binding.id);
        jassert (value != nullptr); // binding id must exist in the layout
        bindingValues.push_back (value);
    }

    initializeModState();
    lens = std::make_unique<lumen::LensController> (apvts, engine);
    melodyPlayerObj = std::make_unique<lumen::MelodyPlayer> (engine);
    melodyLoopPlaybackValue = apvts.getRawParameterValue (lumen::params::melodyLoopPlayback);
    melodyTransposeValue    = apvts.getRawParameterValue (lumen::params::melodyTranspose);
    melody = std::make_unique<lumen::MelodyController> (apvts, *lens, *melodyPlayerObj);
    midiLearnController = std::make_unique<lumen::MidiLearnController> (apvts);

    // Default patch = Slow Aurora (user-approved deviation from SPEC section
    // 16's Neon Tide — DECISIONS.md). Hosts overwrite this via
    // setStateInformation right after construction when restoring a session.
    presets = std::make_unique<lumen::PresetManager> (*this);
    presets->loadFactory ("Slow Aurora");
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
    setLatencySamples (engine.latencySamples()); // limiter lookahead (~1.5 ms)
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
    {
        engine.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        pushMidiDisplayEvent (message.getNoteNumber(), message.getFloatVelocity(), true);
    }
    else if (message.isNoteOff())
    {
        engine.noteOff (message.getNoteNumber());
        pushMidiDisplayEvent (message.getNoteNumber(), 0.0f, false);
    }
    else if (message.isController())
    {
        if (message.getControllerNumber() == 1)
            engine.setModWheel (static_cast<float> (message.getControllerValue()) / 127.0f);
        midiLearnController->handleController (message.getControllerNumber(),
                                               message.getControllerValue());
    }
    else if (message.isChannelPressure())
        engine.setAftertouch (static_cast<float> (message.getChannelPressureValue()) / 127.0f);
    else if (message.isPitchWheel())
        engine.setPitchBend ((static_cast<float> (message.getPitchWheelValue()) - 8192.0f) / 8192.0f);
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        engine.reset();
        pushMidiDisplayEvent (-1, 0.0f, false);
    }
}

void LumenAudioProcessor::pushMidiDisplayEvent (int note, float velocity, bool on) noexcept
{
    const auto scope = midiDisplayFifo.write (1);
    if (scope.blockSize1 > 0)
        midiDisplayEvents[scope.startIndex1] = { note, velocity, on };
}

int LumenAudioProcessor::readMidiDisplayEvents (MidiDisplayEvent* dest, int maxEvents) noexcept
{
    int total = 0;
    const auto scope = midiDisplayFifo.read (juce::jmin (maxEvents, midiDisplayFifo.getNumReady()));
    for (const auto [start, size] : { std::pair { scope.startIndex1, scope.blockSize1 },
                                      std::pair { scope.startIndex2, scope.blockSize2 } })
    {
        if (size > 0)
            std::copy_n (midiDisplayEvents + start, size, dest + total);
        total += size;
    }
    return total;
}

int LumenAudioProcessor::readAudioTap (float* dest, int maxSamples) noexcept
{
    int total = 0;
    const auto scope = tapFifo.read (maxSamples);
    for (const auto [start, size] : { std::pair { scope.startIndex1, scope.blockSize1 },
                                      std::pair { scope.startIndex2, scope.blockSize2 } })
    {
        if (size > 0)
            std::copy_n (tapBuffer.data() + start, size, dest + total);
        total += size;
    }
    return total;
}

void LumenAudioProcessor::uiNoteOn (int midiNote, float velocity) noexcept
{
    const auto scope = keyFifo.write (1);
    if (scope.blockSize1 > 0)
        keyEvents[scope.startIndex1] = { midiNote, velocity, true };
}

void LumenAudioProcessor::uiNoteOff (int midiNote) noexcept
{
    const auto scope = keyFifo.write (1);
    if (scope.blockSize1 > 0)
        keyEvents[scope.startIndex1] = { midiNote, 0.0f, false };
}

void LumenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto blockStartTicks = juce::Time::getHighResolutionTicks();

    buffer.clear();

    // On-screen keyboard events (lock-free FIFO from the editor).
    {
        const auto scope = keyFifo.read (keyFifo.getNumReady());
        for (const auto [start, size] : { std::pair { scope.startIndex1, scope.blockSize1 },
                                          std::pair { scope.startIndex2, scope.blockSize2 } })
            for (int i = 0; i < size; ++i)
            {
                const auto& e = keyEvents[start + i];
                if (e.on)
                    engine.noteOn (e.note, e.velocity);
                else
                    engine.noteOff (e.note);
            }
    }

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

    // Advance the melody sequencer: it triggers its own notes into the engine
    // at the host tempo (block-accurate), independent of the host transport.
    if (melodyLoopPlaybackValue != nullptr)
        melodyPlayerObj->setLooping (melodyLoopPlaybackValue->load() > 0.5f);
    if (melodyTransposeValue != nullptr)
        melodyPlayerObj->setTranspose (juce::roundToInt (melodyTransposeValue->load()));
    melodyPlayerObj->process (params.bpm, getSampleRate(), buffer.getNumSamples());

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

    // --- UI taps (all lock-free, no allocation) -------------------------
    const int numSamples = buffer.getNumSamples();
    {
        // Post-limiter mono mix for the scope/spectrum; drop what won't fit.
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (1);
        const auto scope = tapFifo.write (numSamples);
        int written = 0;
        for (const auto [start, size] : { std::pair { scope.startIndex1, scope.blockSize1 },
                                          std::pair { scope.startIndex2, scope.blockSize2 } })
        {
            for (int i = 0; i < size; ++i)
                tapBuffer[static_cast<size_t> (start + i)] = 0.5f * (l[written + i] + r[written + i]);
            written += size;
        }
    }

    const auto& levels = engine.meterLevels();
    meterAtomics.peakL.store (levels.peakL, std::memory_order_relaxed);
    meterAtomics.peakR.store (levels.peakR, std::memory_order_relaxed);
    meterAtomics.rmsL.store (levels.rmsL, std::memory_order_relaxed);
    meterAtomics.rmsR.store (levels.rmsR, std::memory_order_relaxed);

    // Real-time budget check (HUD / --stress dropout logging). A high-res
    // tick read is a userspace counter read — RT-safe (DECISIONS.md).
    const double elapsedSeconds = juce::Time::highResolutionTicksToSeconds (
        juce::Time::getHighResolutionTicks() - blockStartTicks);
    const double budgetSeconds = numSamples / getSampleRate();
    if (budgetSeconds > 0.0)
    {
        audioLoad.store (static_cast<float> (elapsedSeconds / budgetSeconds),
                         std::memory_order_relaxed);
        if (elapsedSeconds > budgetSeconds)
            dropouts.fetch_add (1, std::memory_order_relaxed);
    }
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
            loadPresetState (juce::ValueTree::fromXml (*xml));
}

void LumenAudioProcessor::loadPresetState (juce::ValueTree newState)
{
    if (! newState.hasType (apvts.state.getType()))
        return;
    apvts.state.removeListener (this);
    apvts.replaceState (std::move (newState));
    initializeModState(); // re-ensure trees, republish, re-listen
    if (lens != nullptr)
        lens->applyStateToEngine(); // rebuild Lens tables from the state
    if (melody != nullptr)
        melody->applyState(); // restore seed/lock + the exact saved melody
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LumenAudioProcessor();
}
