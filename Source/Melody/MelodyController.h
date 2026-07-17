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
    void regenerate();         // fresh melody at a seed: new if unlocked, reused if locked
    void mutate();             // small variation of the current melody (honours locks)
    void setLocked (bool shouldLock);
    // Pin an explicit seed and regenerate with it. Takes 32 bits: the engine's
    // std::mt19937 is single-value-seeded, so that's the actual entropy
    // ceiling regardless of the type seed()/setSeed() are declared with — see
    // makeSeed()'s comment.
    void setSeed (juce::uint32 newSeed);
    void play();               // play the current melody through the engine
    void stop();

    // --- queries ----------------------------------------------------------
    bool locked() const noexcept { return lockedFlag; }
    // Widened to 64 bits only to match the persisted hex format (MelodyState);
    // the value itself never exceeds 32 bits (see makeSeed()).
    juce::uint64 seed() const noexcept { return seedValue; }
    bool isPlaying() const;
    bool hasMelody() const noexcept { return ! currentSeq.steps.empty(); }
    juce::String detectedKey() const { return currentSeq.keyName; }
    // Generation summary (Phase 5): what the engine detected and chose, built
    // from the KeyDetection/phrase provenance at generate() time (never
    // recomputed) and persisted with the sequence. Empty before the first
    // generation. mood = classification bucket + its hue/sat/lum inputs;
    // form = phrase letters ("A A\xe2\x80\xb2 B A\xe2\x80\xb3 C") or the mode's shape name.
    juce::String moodText() const { return moodValue; }
    juce::String formText() const { return formValue; }
    int gridCols() const noexcept { return currentSeq.gridCols; }
    int gridRows() const noexcept { return currentSeq.gridRows; }
    const melody::Sequence& sequence() const noexcept { return currentSeq; }
    // The current melody's chord progression (root degrees) — what "Lock Harmony"
    // holds fixed across a regeneration. Empty before the first generation.
    const std::vector<int>& progression() const noexcept { return currentProgression; }

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
    // A single 32-bit draw widened to 64 bits. A second draw would add no real
    // entropy: renderFresh() seeds a std::mt19937 (32-bit single-value seed),
    // so anything beyond the low 32 bits is discarded downstream regardless.
    juce::uint64 makeSeed();
    void installSequence (const melody::Sequence& seq); // copy, persist, hand to player

    juce::AudioProcessorValueTreeState& apvts;
    LensController& lens;
    MelodyPlayer& player;
    juce::Random rng;

    juce::uint64 seedValue = 0;
    bool lockedFlag  = false;
    bool panelActive = false;
    juce::String moodValue, formValue; // generation summary (see moodText())
    melody::Sequence currentSeq; // message-thread copy for the UI + export
    // The current melody's chord progression, kept so "Lock Harmony" can carry it
    // into a regeneration. In-memory for the session (not persisted across reload
    // yet — see SESSION_NOTES Phase 4b). Empty until the first generation.
    std::vector<int> currentProgression;
    // The current melody's phrase boundaries (note indices), kept so the lock
    // splice can be phrase-aware — the stored Sequence doesn't carry them. Note
    // indices survive the Sequence round-trip. In-memory for the session.
    std::vector<std::size_t> currentPhraseStarts;
};
} // namespace lumen
