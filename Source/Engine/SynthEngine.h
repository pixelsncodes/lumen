#pragma once

#include "Engine/EngineParams.h"
#include "Engine/FxChain.h"
#include "Engine/UiTap.h"
#include "Engine/Voice.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

namespace lumen
{
// 16-voice polyphonic engine (SPEC section 11) + modulation matrix
// (SPEC sections 8-9). Pure DSP: the processor and the render harness both
// drive it via setParams + noteOn/noteOff + MIDI setters + render.
//
// Modulation model: global sources (macros, wheel, aftertouch, bend, mono
// LFOs) are combined per block in normalized space and land on the existing
// 20 ms smoothers; poly sources (envs, poly LFOs, velocity, keytrack,
// random-per-note) are combined per voice per block and ramped across the
// block (scratch-buffer overrides for the per-sample-buffered destinations,
// start/end adjustments for the block-ramped ones). render() ADDS into
// caller-zeroed buffers and allocates nothing.
class SynthEngine
{
public:
    static constexpr int kNumVoices = 16;

    void prepare (double sampleRate, int maxBlockSize);
    void setParams (const EngineParams& params); // once per block, before render
    void noteOn (int midiNote, float velocity);  // velocity 0..1
    void noteOff (int midiNote);
    void reset();                                // hard stop (all voices killed)

    // MIDI-driven global mod sources (audio thread, per block)
    void setModWheel (float v) noexcept   { modWheel = v; }
    void setAftertouch (float v) noexcept { aftertouch = v; }
    void setPitchBend (float v) noexcept  { pitchBend = v; } // -1..+1

    void render (float* outL, float* outR, int numSamples);

    // Master FX bus (Phase 4). setFxEnabled(false) taps the pre-FX voice sum
    // (harness null tests); latency is the limiter lookahead.
    void setFxEnabled (bool enabled) noexcept { fxEnabled = enabled; }
    int latencySamples() const noexcept { return fxEnabled ? fx.latencySamples() : 0; }
    const FxChain::Levels& meterLevels() const noexcept { return fx.levels(); }

    // --- Phase 6: Lens image tables --------------------------------------
    // Per-osc Image table (SPEC 4/13): the message thread swaps the pointer
    // atomically; render() resolves TableChoice::image through it (falling
    // back to Basic while empty). The CALLER owns the table and must keep a
    // retired table alive until renderCallCount() has advanced by >= 2 past
    // the swap (any render concurrent with the swap has finished by then) —
    // see LensController.
    void setImageTable (int oscIndex, const Wavetable* table) noexcept
    {
        (oscIndex == 0 ? imageTableA : imageTableB)
            .store (table, std::memory_order_release);
    }

    uint64_t renderCallCount() const noexcept
    {
        return renderCounter.load (std::memory_order_acquire);
    }

    // Block-rate live-value snapshot for the UI (mod arcs, LFO markers).
    // Written on the audio thread each chunk, read via atomics only.
    UiTap& uiTap() noexcept { return ui; }

    int activeVoiceCount() const noexcept
    {
        int n = 0;
        for (const auto& v : voices)
            n += v.isActive() ? 1 : 0;
        return n;
    }

    // Newest active voice's note / live glide pitch (tests + UI; -1 / 0 when
    // silent). Message-thread reads race harmlessly with the audio thread.
    int newestActiveNote() const noexcept
    {
        const auto* v = newestActiveVoice();
        return v != nullptr ? v->currentNote() : -1;
    }

    float newestActivePitchSemis() const noexcept
    {
        const auto* v = newestActiveVoice();
        return v != nullptr ? v->currentPitchSemis() : 0.0f;
    }

private:
    using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    struct OscSmoothers
    {
        Smoothed morph, level, pan, fine, detune, width, blend;
    };

    void renderChunk (float* outL, float* outR, int numSamples);
    Voice* findVoiceFor (int midiNote);

    // Voice modes (SPEC section 11)
    const Voice* newestActiveVoice() const noexcept;
    Voice* newestActiveVoice() noexcept
    {
        return const_cast<Voice*> (std::as_const (*this).newestActiveVoice());
    }
    void heldPush (int midiNote) noexcept;
    void heldRemove (int midiNote) noexcept;
    void retriggerMono (Voice& voice, int midiNote, float velocity);

    // Modulation helpers
    float baseNaturalFor (int dest) const noexcept;
    Smoothed* smootherFor (int dest) noexcept;
    static int bufferedIndexFor (int dest) noexcept; // -1 or 0..9 (BlockBuffers order)
    void applyGlobalModulation (int numSamples);
    void applyPolyModulation (int voiceIndex, VoiceBlockGlobals& globals,
                              BlockBuffers& buffers, int numSamples,
                              bool publishToUi);
    void publishUiTap (int newestVoice);

    double sampleRate = 48000.0;
    int maxBlock = 0;
    bool primed = false;
    uint64_t rngState = 0;
    uint64_t noteCounter = 0;

    EngineParams current {};
    Voice voices[kNumVoices];
    FxChain fx;
    bool fxEnabled = true;

    // Held-key stack in press order (maintained in every mode so a mid-play
    // mode switch behaves): last entry = most recent key = the mono note.
    int heldNotes[128] {};
    int numHeld = 0;
    float lastNoteSemis = -1.0f; // last played note, for always-glide across gaps

    // Lens image tables (Phase 6): owned by the caller, swapped atomically.
    std::atomic<const Wavetable*> imageTableA { nullptr }, imageTableB { nullptr };
    std::atomic<uint64_t> renderCounter { 0 };

    OscSmoothers smoothA, smoothB;
    Smoothed subLevel, noiseLin, cutoff, res, drive, envAmount, keytrack, bendSemis;

    std::vector<float> bufMorphA, bufLevelA, bufMorphB, bufLevelB,
                       bufSubLevel, bufNoiseLin, bufCutoff, bufRes,
                       bufDrive, bufEnvAmount;
    std::vector<float> scratch[10]; // per-voice overrides, one per buffered dest
    float driveComp[25] {}; // dB -> output gain compensation (measured at prepare)

    // Modulation state
    Lfo monoLfo[3];
    LfoParams effLfo[3] {};        // current.lfo with the modulated rate applied
    float effLfoRateHz[3] {};      // written by the dest loop, consumed next chunk
    bool  lfoRateModActive[3] {};
    float modWheel = 0.0f, aftertouch = 0.0f, pitchBend = 0.0f;
    float destExponent[mod::kNumDests] {};
    float globalNormSum[mod::kNumDests] {};
    bool  globalActive[mod::kNumDests] {};
    int   polySlotIndices[mod::kNumSlots] {};
    int   numPolySlots = 0;
    float voicePolyPrev[kNumVoices][mod::kNumDests] {};
    bool  voiceModPrimed[kNumVoices] {};
    EnvParams effEnv1 {}, effEnv2 {}, effEnv3 {}; // env params after global mod

    // UI snapshot scratch (audio thread only) + the published atomics.
    float uiGlobalNorm[mod::kNumDests] {}; // base + global matrix + macros, normalized
    float uiPolyNorm[mod::kNumDests] {};   // newest voice's poly sums this block
    UiTap ui;
};
} // namespace lumen
