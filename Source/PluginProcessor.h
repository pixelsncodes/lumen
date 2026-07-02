#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/SynthEngine.h"
#include "State/Parameters.h"

class LumenAudioProcessor final : public juce::AudioProcessor,
                                  private juce::ValueTree::Listener
{
public:
    LumenAudioProcessor();
    ~LumenAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 15.0; } // max env release

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    void renderSegment (juce::AudioBuffer<float>& buffer, int start, int numSamples);
    void handleMidiMessage (const juce::MidiMessage& message);
    void initializeModState();
    void publishModConfig(); // message thread: parse ValueTree -> POD, publish

    // ValueTree::Listener (matrix/macro edits from the UI)
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    lumen::SynthEngine engine;

    // One atomic per engine binding, same order as lumen::bindings::all().
    std::vector<std::atomic<float>*> bindingValues;

    // Lock-free matrix publish: message thread writes the next pool entry and
    // swaps the pointer; the audio thread copies from the published entry at
    // block start. Pool depth 8 makes write-while-read effectively impossible
    // (would need 8 edits inside one block render).
    lumen::mod::Config modConfigPool[8];
    std::atomic<lumen::mod::Config*> publishedModConfig { nullptr };
    int nextPoolEntry = 0;

    juce::SmoothedValue<float> masterGainLinear;
    std::atomic<float>* masterGainDb = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenAudioProcessor)
};
