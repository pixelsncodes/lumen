#pragma once

#include "Engine/EngineParams.h"
#include "Engine/Envelope.h"
#include "Engine/Lfo.h"
#include "Engine/SVF.h"
#include "Engine/Wavetable.h"

#include <cstdint>

namespace lumen
{
// Per-sample parameter buffers filled by SynthEngine (20 ms smoothed),
// shared by all voices in a block.
struct BlockBuffers
{
    const float* morphA;
    const float* levelA;
    const float* morphB;
    const float* levelB;
    const float* subLevel;
    const float* noiseLin;
    const float* cutoffHz;
    const float* res;
    const float* driveDb;
    const float* envAmount;
    const float* driveComp; // 25-entry gain-compensation table, indexed by drive dB
};

// Block-rate values a voice needs to configure its oscillators: smoothed
// params are given as start/end pairs and lerped per sample inside the voice
// (pitch and pan ramps), discrete params as plain values.
struct OscBlockGlobals
{
    const Wavetable* table = nullptr;
    bool  enabled = false;
    int   unison = 1;
    int   semitones = 0;
    float fineStart = 0.0f,   fineEnd = 0.0f;    // cents
    float detuneStart = 0.0f, detuneEnd = 0.0f;  // cents, +- spread
    float widthStart = 0.0f,  widthEnd = 0.0f;
    float blendStart = 0.5f,  blendEnd = 0.5f;
    float panStart = 0.0f,    panEnd = 0.0f;
};

struct VoiceBlockGlobals
{
    OscBlockGlobals oscA, oscB;
    int   subWave = 0;
    int   subOctave = 1;
    int   noiseType = 0;
    int   filterMode = 1;
    float keytrack = 0.0f;
    EnvParams env1, env2, env3;
    double bendStart = 1.0, bendEnd = 1.0; // pitch-bend frequency ratio ramp
};

class Voice
{
public:
    static constexpr int kMaxUnison = 8;

    void prepare (double sampleRate, uint32_t noiseSeed);
    // glideFromSemis < 0 = start at pitch (no glide); otherwise the pitch
    // slews from glideFromSemis to midiNote over glideSeconds (SPEC 11).
    void startNote (int midiNote, float velocity, uint64_t& rngState,
                    bool phaseRandomA, bool phaseRandomB,
                    float glideFromSemis = -1.0f, float glideSeconds = 0.0f);
    // Legato pitch change: retargets the glide only — no envelope/LFO
    // retrigger, no phase reset, velocity kept from the first note.
    void retune (int midiNote, float glideSeconds);
    void noteOff();
    void kill();

    bool isActive() const noexcept    { return active; }
    bool isReleasing() const noexcept { return active && env1.isReleasing(); }
    float envLevel() const noexcept   { return env1.value(); }
    int currentNote() const noexcept  { return note; }
    float currentPitchSemis() const noexcept { return static_cast<float> (pitchSemis); }
    uint64_t age() const noexcept     { return noteOnOrder; }
    void setAge (uint64_t order) noexcept { noteOnOrder = order; }

    // Configure per-block ramps, then add numSamples into outL/outR.
    void startBlock (const VoiceBlockGlobals& globals, int numSamples);
    void render (float* outL, float* outR, int numSamples, const BlockBuffers& buffers);

    // --- Modulation sources (read at block start, SPEC section 9) -------
    float envValue (int index) const noexcept
    {
        return index == 0 ? env1.value() : (index == 1 ? env2.value() : env3.value());
    }
    float polyLfoValue (int index, const LfoParams& p) const noexcept { return polyLfo[index].value (p); }
    double polyLfoPhase (int index) const noexcept { return polyLfo[index].currentPhase(); }
    void advancePolyLfos (const LfoParams* p, double bpm, int numSamples) noexcept
    {
        for (int k = 0; k < 3; ++k)
            polyLfo[k].advance (p[k], bpm, numSamples);
    }
    float velocityNorm() const noexcept { return velocityGain; }
    float keytrackNorm() const noexcept // note 60 = 0, bipolar
    {
        const float v = (static_cast<float> (note) - 60.0f) / 64.0f;
        return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
    }
    float randomNorm() const noexcept { return randomValue; }

private:
    struct UnisonLane
    {
        double phase = 0.0;
        double inc = 0.0, incStep = 0.0;
        float gainL = 0.0f, gainLStep = 0.0f;
        float gainR = 0.0f, gainRStep = 0.0f;
        int mip = 0;
    };

    struct OscState
    {
        UnisonLane lanes[kMaxUnison];
        const Wavetable* table = nullptr;
        int unison = 1;
        bool enabled = false;
    };

    void configureOsc (OscState& osc, const OscBlockGlobals& g, int numSamples,
                       float hzStart, float hzEnd,
                       double bendStart, double bendEnd);
    float renderOscSample (OscState& osc, float morph, float& outR) noexcept;

    double sampleRate = 48000.0;
    bool active = false;
    int note = 60;
    float velocityGain = 0.0f;
    float keytrackFactor = 1.0f;
    uint64_t noteOnOrder = 0;

    // Pitch glide (SPEC 11): pitchSemis slews linearly in semitone space
    // toward pitchTargetSemis; rate 0 = snapped. Advanced once per block in
    // startBlock; the per-sample ramp rides the existing increment lerp.
    double pitchSemis = 60.0;
    double pitchTargetSemis = 60.0;
    double glideSemisPerSample = 0.0;

    OscState oscA, oscB;
    int filterMode = 1;

    // Sub oscillator (tracks Osc A pitch pre-detune, SPEC section 5)
    double subPhase = 0.0;
    double subInc = 0.0, subIncStep = 0.0;
    int subWave = 0;

    // Noise (free-running, deterministic per-voice seed)
    uint32_t noiseState = 1;
    int noiseType = 0;
    float pinkB0 = 0.0f, pinkB1 = 0.0f, pinkB2 = 0.0f;

    Envelope env1, env2, env3;
    SVF filter;

    Lfo polyLfo[3];
    float randomValue = 0.0f; // bipolar random-per-note, drawn at startNote
};
} // namespace lumen
