#include "Melody/MelodyPlayer.h"

#include "Engine/SynthEngine.h"

#include <cmath>

namespace lumen
{
MelodyPlayer::MelodyPlayer (SynthEngine& engineToUse) : engine (engineToUse) {}

MelodyPlayer::~MelodyPlayer()
{
    // All threads have stopped by the time the processor is destroyed; reclaim
    // every sequence the hand-off/retire slots may still hold.
    delete incoming.exchange (nullptr);
    delete retired.exchange (nullptr);
}

void MelodyPlayer::setSequence (std::unique_ptr<melody::Sequence> seq)
{
    collectGarbage();
    // Publish the new sequence; if a previous one was queued but never adopted
    // by the audio thread, it is ours to delete (the audio thread only ever
    // takes the latest pointer or nullptr).
    melody::Sequence* raw = seq.release();
    delete incoming.exchange (raw, std::memory_order_acq_rel);
    hasSeq.store (raw != nullptr, std::memory_order_release);
}

void MelodyPlayer::collectGarbage()
{
    delete retired.exchange (nullptr, std::memory_order_acq_rel);
}

MelodyPlayer::LiveState MelodyPlayer::liveState() const noexcept
{
    LiveState s;
    s.playing    = playing.load (std::memory_order_acquire);
    s.col        = pubCol.load (std::memory_order_relaxed);
    s.row        = pubRow.load (std::memory_order_relaxed);
    s.triggerSeq = pubTrigger.load (std::memory_order_relaxed);
    return s;
}

void MelodyPlayer::startInternal() noexcept
{
    positionBeats = 0.0;
    nextStep = 0;
    numSounding = 0;
    playing.store (true, std::memory_order_release);
}

void MelodyPlayer::stopInternal()
{
    for (int i = 0; i < numSounding; ++i)
        engine.noteOff (sounding[i].note);
    numSounding = 0;
    playing.store (false, std::memory_order_release);
    pubCol.store (-1, std::memory_order_relaxed);
    pubRow.store (-1, std::memory_order_relaxed);
}

void MelodyPlayer::publish() noexcept
{
    // playing is already an atomic kept current by start/stop; nothing else to
    // do here beyond what the trigger loop already stored.
}

void MelodyPlayer::process (double bpm, double sampleRate, int numSamples)
{
    // Adopt a newly published sequence, retiring the old one for the message
    // thread to delete (never freed on the audio thread).
    if (auto* inc = incoming.exchange (nullptr, std::memory_order_acquire))
    {
        // Stop anything still sounding from the previous melody.
        for (int i = 0; i < numSounding; ++i)
            engine.noteOff (sounding[i].note);
        numSounding = 0;

        melody::Sequence* old = active.release();
        active.reset (inc);
        // Single retire slot; the message thread drains it before publishing
        // another sequence, so it is empty here in practice.
        delete retired.exchange (old, std::memory_order_acq_rel);

        // A fresh sequence restarts the transport cleanly if we were playing.
        positionBeats = 0.0;
        nextStep = 0;
    }

    const double safeBpm = bpm > 1.0 ? bpm : 120.0;
    pubBpm.store (safeBpm, std::memory_order_relaxed);

    if (requestStop.exchange (false, std::memory_order_acquire))
        stopInternal();
    if (requestPlay.exchange (false, std::memory_order_acquire))
        startInternal();

    if (! playing.load (std::memory_order_acquire) || active == nullptr
        || numSamples <= 0 || sampleRate <= 0.0)
        return;

    const double blockBeats = (safeBpm / 60.0) * (numSamples / sampleRate);
    const double blockEnd = positionBeats + blockBeats;

    // Release notes whose tail ends within this block.
    for (int i = 0; i < numSounding;)
    {
        if (sounding[i].offBeat <= blockEnd)
        {
            engine.noteOff (sounding[i].note);
            sounding[i] = sounding[--numSounding];
        }
        else
        {
            ++i;
        }
    }

    // Trigger notes whose onset falls in [positionBeats, blockEnd).
    const auto& steps = active->steps;
    while (nextStep < steps.size() && steps[nextStep].startBeats < blockEnd)
    {
        const melody::Step& s = steps[nextStep];
        engine.noteOn (s.note, s.velocity);
        if (numSounding < kMaxSounding)
            sounding[numSounding++] = { s.note, s.startBeats + s.lengthBeats };
        pubCol.store (s.col, std::memory_order_relaxed);
        pubRow.store (s.row, std::memory_order_relaxed);
        pubTrigger.fetch_add (1, std::memory_order_relaxed);
        ++nextStep;
    }

    positionBeats = blockEnd;

    // End of the melody. Looping: wrap the transport back to beat 0 the moment
    // the block crosses totalBeats — release whatever still sounds (generated
    // sequences end on the loop boundary, so this is their natural note-off),
    // keep the overshoot so the loop length stays exact over many repeats, and
    // trigger any step whose onset falls inside the overshoot so the loop's
    // first note is not pushed a block late. One-shot: stop as before.
    if (nextStep >= steps.size() && positionBeats >= active->totalBeats)
    {
        if (looping.load (std::memory_order_relaxed) && active->totalBeats > 0.0)
        {
            for (int i = 0; i < numSounding; ++i)
                engine.noteOff (sounding[i].note);
            numSounding = 0;

            positionBeats -= active->totalBeats;
            if (positionBeats >= active->totalBeats) // pathological short loop
                positionBeats = std::fmod (positionBeats, active->totalBeats);
            nextStep = 0;

            while (nextStep < steps.size()
                   && steps[nextStep].startBeats < positionBeats)
            {
                const melody::Step& s = steps[nextStep];
                engine.noteOn (s.note, s.velocity);
                if (numSounding < kMaxSounding)
                    sounding[numSounding++] = { s.note, s.startBeats + s.lengthBeats };
                pubCol.store (s.col, std::memory_order_relaxed);
                pubRow.store (s.row, std::memory_order_relaxed);
                pubTrigger.fetch_add (1, std::memory_order_relaxed);
                ++nextStep;
            }
        }
        else if (numSounding == 0)
        {
            stopInternal();
        }
    }
}
} // namespace lumen
