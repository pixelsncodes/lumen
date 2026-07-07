#pragma once

#include "Melody/MelodySequence.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace lumen
{
class SynthEngine;

// Audio-thread sequencer for a generated melody. Owns a Sequence and, when
// playing, advances an internal beat clock each block at the host tempo,
// feeding SynthEngine::noteOn/noteOff so the melody plays through Lumen's own
// voices (no host MIDI output involved). The melody has its own transport: it
// starts from the top on play() and stops (or ends) independently of the host
// transport — only the *tempo* is borrowed from the host.
//
// Threading:
//   - process() runs on the audio thread and allocates nothing.
//   - setSequence()/play()/stop()/collectGarbage() run on the message thread.
//   - Sequence ownership is handed over lock-free (a pointer exchange plus a
//     single-slot retire list reclaimed on the message thread), mirroring the
//     Wavetable retire pattern used by LensController.
// The generated melody is monophonic (each note starts where the previous one
// ends), so only a couple of notes are ever sounding at once; kMaxSounding is a
// safe fixed bound that keeps the note-off bookkeeping allocation-free.
class MelodyPlayer
{
public:
    explicit MelodyPlayer (SynthEngine& engineToUse);
    ~MelodyPlayer();

    // --- message thread ---------------------------------------------------
    void setSequence (std::unique_ptr<melody::Sequence> seq);
    void play() noexcept  { requestPlay.store (true,  std::memory_order_release); }
    void stop() noexcept  { requestStop.store (true,  std::memory_order_release); }
    bool isPlaying() const noexcept { return playing.load (std::memory_order_acquire); }
    bool hasSequence() const noexcept { return hasSeq.load (std::memory_order_acquire); }

    // Delete any sequence retired by the audio thread. Safe to call anytime on
    // the message thread; also called implicitly by setSequence().
    void collectGarbage();

    // Live playback state for the UI (message-thread read of audio-thread
    // atomics). col/row = the cell of the most recently triggered note (-1 when
    // idle); triggerSeq bumps on every note-on so the UI can time the glow.
    struct LiveState
    {
        bool         playing = false;
        int          col = -1;
        int          row = -1;
        std::uint32_t triggerSeq = 0;
    };
    LiveState liveState() const noexcept;

    // Last tempo the audio thread saw (for tempo-matched MIDI export); 120
    // until the first processed block.
    double lastBpm() const noexcept { return pubBpm.load (std::memory_order_relaxed); }

    // --- audio thread -----------------------------------------------------
    void process (double bpm, double sampleRate, int numSamples);

private:
    void startInternal() noexcept;
    void stopInternal();          // audio thread: silence melody notes, reset
    void publish() noexcept;

    SynthEngine& engine;

    // Lock-free hand-off (message -> audio) and retire (audio -> message).
    std::atomic<melody::Sequence*> incoming { nullptr };
    std::atomic<melody::Sequence*> retired  { nullptr };
    std::unique_ptr<melody::Sequence> active; // audio-thread owned once adopted

    std::atomic<bool> requestPlay { false };
    std::atomic<bool> requestStop { false };
    std::atomic<bool> playing { false };
    std::atomic<bool> hasSeq  { false };

    double  positionBeats = 0.0;
    std::size_t nextStep  = 0;

    static constexpr int kMaxSounding = 16;
    struct Sounding { int note = -1; double offBeat = 0.0; };
    Sounding sounding[kMaxSounding];
    int numSounding = 0;

    std::atomic<int>          pubCol { -1 };
    std::atomic<int>          pubRow { -1 };
    std::atomic<std::uint32_t> pubTrigger { 0 };
    std::atomic<double>       pubBpm { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyPlayer)
};
} // namespace lumen
