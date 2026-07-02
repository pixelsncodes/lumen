#pragma once

#include "Engine/EngineParams.h"
#include "Engine/Voice.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace lumen
{
// 16-voice polyphonic engine (SPEC section 11). Pure DSP: the processor and
// the render harness both drive it via setParams + noteOn/noteOff + render.
// All continuous parameters are smoothed here (~20 ms) so every client gets
// zipper-free behavior. render() ADDS into caller-zeroed buffers and
// allocates nothing.
class SynthEngine
{
public:
    static constexpr int kNumVoices = 16;

    void prepare (double sampleRate, int maxBlockSize);
    void setParams (const EngineParams& params); // once per block, before render
    void noteOn (int midiNote, float velocity);  // velocity 0..1
    void noteOff (int midiNote);
    void reset();                                // hard stop (all voices killed)

    void render (float* outL, float* outR, int numSamples);

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
    static void setTarget (Smoothed& s, float value, bool snap);
    void pushTargets (bool snap);

    double sampleRate = 48000.0;
    int maxBlock = 0;
    bool primed = false;
    uint64_t rngState = 0;
    uint64_t noteCounter = 0;

    EngineParams current {};
    Voice voices[kNumVoices];

    OscSmoothers smoothA, smoothB;
    Smoothed subLevel, noiseLin, cutoff, res, drive, envAmount, keytrack;

    std::vector<float> bufMorphA, bufLevelA, bufMorphB, bufLevelB,
                       bufSubLevel, bufNoiseLin, bufCutoff, bufRes,
                       bufDrive, bufEnvAmount;
    float driveComp[25] {}; // dB -> output gain compensation (measured at prepare)
};
} // namespace lumen
