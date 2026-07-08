#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Melody/MelodySequence.h"

#include <cstdint>
#include <vector>

namespace lumen
{
class LensController;
class MelodyPlayer;

// Message-thread owner of the melody feature. Bridges Lumen's loaded Lens image
// into the Lumena library, runs the image -> key -> Markov -> notes pipeline,
// and hands the resulting Sequence to the audio-thread MelodyPlayer. Also owns
// the two non-parameter bits of melody state (RNG seed + lock), the persisted
// note sequence, and MIDI export.
//
// Only this class's .cpp includes the Lumena headers, so nothing else in the
// plugin (or the UI) depends on the library's types.
//
// The musical controls (key mode, length, brightness bias, phrase mode,
// ornaments) are ordinary APVTS parameters read at generate() time; seed and
// lock live in the MELODY state sub-tree.
class MelodyController
{
public:
    MelodyController (juce::AudioProcessorValueTreeState& apvtsToUse,
                      LensController& lensToUse, MelodyPlayer& playerToUse);

    // Grid resolution (per axis) used for both generation and the UI overlay,
    // so the drawn grid always matches what Lumena sampled.
    static constexpr int kGridResolution = 8;

    // --- actions (message thread) ----------------------------------------
    void generate();           // sample the current Lens image into a melody
    void reroll();             // new random seed, then generate (no-op if locked)
    void regenerate();         // fresh melody at a new seed, honouring the locks
    void mutate();             // small variation of the current melody (honours locks)
    void setLocked (bool shouldLock);
    void play();               // play the current melody through the engine
    void stop();

    // --- queries ----------------------------------------------------------
    bool locked() const noexcept { return lockedFlag; }
    juce::uint64 seed() const noexcept { return seedValue; }
    bool isPlaying() const;
    bool hasMelody() const noexcept { return ! currentSeq.steps.empty(); }
    juce::String detectedKey() const { return currentSeq.keyName; }
    int gridCols() const noexcept { return currentSeq.gridCols; }
    int gridRows() const noexcept { return currentSeq.gridRows; }
    const melody::Sequence& sequence() const noexcept { return currentSeq; }

    // Panel visibility: toggled from the Lens UI, polled by the editor to show
    // /hide the overlay and by the Lens image view to draw the grid.
    bool isPanelActive() const noexcept { return panelActive; }
    void setPanelActive (bool active) noexcept { panelActive = active; }
    void togglePanel() noexcept { panelActive = ! panelActive; }

    // --- export -----------------------------------------------------------
    std::vector<unsigned char> toMidiBytes() const;   // tempo-matched SMF bytes
    juce::File writeTempMidiFile() const;             // for drag-and-drop; {} on fail
    bool saveMidiFile (const juce::File& dest) const;

    // Rebuild the in-memory melody + player state from a just-loaded APVTS
    // state tree (called after setStateInformation / preset load).
    void applyState();

private:
    juce::uint64 makeSeed();
    void installSequence (const melody::Sequence& seq); // copy, persist, hand to player

    juce::AudioProcessorValueTreeState& apvts;
    LensController& lens;
    MelodyPlayer& player;
    juce::Random rng;

    juce::uint64 seedValue = 0;
    bool lockedFlag  = false;
    bool panelActive = false;
    melody::Sequence currentSeq; // message-thread copy for the UI + export
};
} // namespace lumen
