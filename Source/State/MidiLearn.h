#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace lumen
{
// MIDI Learn (SPEC section 17): right-click a knob -> Learn -> the next
// incoming CC binds to that parameter. Bindings are GLOBAL — persisted at
// %APPDATA%/Lumen/midi_map.xml, shared by every instance and session, never
// stored in preset/DAW state (that is what makes the Launchkey knobs feel
// like hardware: mapped once, mapped everywhere).
//
// Threading: the CC table is 128 atomic parameter pointers. The audio thread
// only reads/exchanges atomics and calls the host parameter setters (the
// standard MIDI-learn path — no allocation, no locks). Arming, clearing and
// the map file live on the message thread; poll() (editor timer) finalizes a
// learn the audio thread captured and persists it.
class MidiLearnController
{
public:
    explicit MidiLearnController (juce::AudioProcessorValueTreeState& apvts);
    MidiLearnController (juce::AudioProcessorValueTreeState& apvts, const juce::File& mapFile);

    static juce::File defaultMapFile(); // %APPDATA%/Lumen/midi_map.xml

    // --- Message thread -------------------------------------------------
    void armLearn (const juce::String& paramId);
    void cancelLearn() noexcept { armed.store (nullptr, std::memory_order_release); }
    bool isArmed() const noexcept { return armed.load (std::memory_order_acquire) != nullptr; }
    bool isArmedFor (const juce::String& paramId) const;

    int boundCcFor (const juce::String& paramId) const; // -1 = unbound
    void clearBinding (const juce::String& paramId);    // persists

    // Editor timer tick: if the audio thread completed a learn, enforce
    // one-CC-per-parameter and write the map file. Returns true then (the
    // caller may refresh menus/indicators).
    bool poll();

    // --- Audio thread ---------------------------------------------------
    // Every incoming CC (any number, any channel) routes through here.
    void handleController (int ccNumber, int ccValue) noexcept;

private:
    void loadFromFile();
    void saveToFile() const;

    juce::AudioProcessorValueTreeState& state;
    juce::File file;
    std::atomic<juce::RangedAudioParameter*> ccParams[128] {};
    std::atomic<juce::RangedAudioParameter*> armed { nullptr };
    std::atomic<int> lastLearnedCc { -1 };

    JUCE_DECLARE_NON_COPYABLE (MidiLearnController)
};
} // namespace lumen
