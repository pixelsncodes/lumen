#pragma once

#include "Engine/EngineParams.h"
#include "Engine/FxChain.h"
#include "Engine/UiTap.h"
#include "Engine/Voice.h"

#include <juce_audio_basics/juce_audio_basics.h>

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

private:
    using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    struct OscSmoothers
    {
        Smoothed morph, level, pan, fine, detune, width, blend;
    };

    void renderChunk (float* outL, float* outR, int numSamples);
    Voice* findVoiceFor (int midiNote);

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

    OscSmoothers smoothA, smoothB;
    Smoothed subLevel, noiseLin, cutoff, res, drive, envAmount, keytrack, bendSemis;

    std::vector<float> bufMorphA, bufLevelA, bufMorphB, bufLevelB,
                       bufSubLevel, bufNoiseLin, bufCutoff, bufRes,
                       bufDrive, bufEnvAmount;
    std::vector<float> scratch[10]; // per-voice overrides, one per buffered dest
    float driveComp[25] {}; // dB -> output gain compensation (measured at prepare)

    // Modulation state
    Lfo monoLfo[3];
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
