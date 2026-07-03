#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/SynthEngine.h"
#include "State/Parameters.h"

#include <atomic>
#include <vector>

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

    // ------------------------------------------------------------------
    // UI bridge (SPEC sections 10/15): everything below is lock-free.
    // ------------------------------------------------------------------

    // Live modulation values published by the engine each block.
    lumen::UiTap& uiTap() noexcept { return engine.uiTap(); }

    // Latest parsed matrix/macro config (stable between UI edits) — used by
    // knobs to know their modulation span without re-parsing the ValueTree.
    const lumen::mod::Config* currentModConfig() const noexcept
    {
        return publishedModConfig.load (std::memory_order_acquire);
    }

    // Post-limiter mono audio tap for the scope/spectrum. Single consumer:
    // the editor drains it on its UI timer into a local history buffer.
    // Returns the number of samples read into dest.
    int readAudioTap (float* dest, int maxSamples) noexcept;

    // On-screen keyboard -> audio thread (applied at the next block start).
    void uiNoteOn (int midiNote, float velocity) noexcept;
    void uiNoteOff (int midiNote) noexcept;

    // Incoming (host/hardware) MIDI notes -> keyboard display. The audio
    // thread publishes note on/offs here; the editor drains them on its UI
    // timer into the MidiKeyboardState. note == -1 means "all notes off".
    struct MidiDisplayEvent { int note; float velocity; bool on; };
    int readMidiDisplayEvents (MidiDisplayEvent* dest, int maxEvents) noexcept;

    // Output meter levels (peak/RMS per block, linear).
    struct MeterAtomics
    {
        std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
        std::atomic<float> rmsL { 0.0f }, rmsR { 0.0f };
    };
    const MeterAtomics& meterLevels() const noexcept { return meterAtomics; }

    // processBlock duration vs. real-time budget (frame HUD / --stress).
    // load = duration / budget of the most recent block; dropouts = blocks
    // that exceeded their budget since the last resetPerfCounters().
    float currentAudioLoad() const noexcept { return audioLoad.load (std::memory_order_relaxed); }
    int dropoutCount() const noexcept { return dropouts.load (std::memory_order_relaxed); }
    void resetPerfCounters() noexcept { dropouts.store (0); audioLoad.store (0.0f); }

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

    // Audio tap: SPSC ring, audio thread writes, editor timer reads.
    static constexpr int kTapCapacity = 16384;
    juce::AbstractFifo tapFifo { kTapCapacity };
    std::vector<float> tapBuffer;

    // On-screen keyboard events: message thread writes, audio thread reads.
    struct KeyEvent { int note; float velocity; bool on; };
    static constexpr int kKeyFifoCapacity = 128;
    juce::AbstractFifo keyFifo { kKeyFifoCapacity };
    KeyEvent keyEvents[kKeyFifoCapacity] {};

    // Incoming MIDI notes for the keyboard display: audio thread writes,
    // editor timer reads. Overflow drops events (display only).
    static constexpr int kMidiDisplayCapacity = 256;
    juce::AbstractFifo midiDisplayFifo { kMidiDisplayCapacity };
    MidiDisplayEvent midiDisplayEvents[kMidiDisplayCapacity] {};
    void pushMidiDisplayEvent (int note, float velocity, bool on) noexcept;

    MeterAtomics meterAtomics;
    std::atomic<float> audioLoad { 0.0f };
    std::atomic<int> dropouts { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenAudioProcessor)
};
