#pragma once

#include "Engine/EngineParams.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <vector>

namespace lumen
{
// Master effects bus (SPEC section 10), fixed order:
//   Drive -> Chorus -> Delay -> Reverb -> Limiter -> master gain -> meter.
//
// Every effect is bypassable via a 20 ms smoothed dry/wet crossfade whose
// dry leg is bit-exact (out = in + mix * (wet - in)), so the all-bypassed
// chain nulls against the pre-FX signal apart from the limiter's constant
// lookahead delay (latencySamples()). Effects keep writing their delay
// lines while bypassed so re-enabling has history and never clicks.
// The limiter is always on and guarantees |out| <= ceiling by construction
// (sliding-window minimum + boxcar over exactly the lookahead window).
//
// Continuous parameters live in `smooth`: SynthEngine's modulation pass
// lands base + global-matrix values on them each block (same mechanism as
// the voice parameters); setBlockParams() handles the discrete switches
// and overrides the delay-time target when host sync is active.
class FxChain
{
public:
    using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    struct Smoothers
    {
        Smoothed driveDb, driveTone,
                 chorusRateHz, chorusDepth, chorusMix,
                 delayTimeMs, delayFeedback, delayDampHz, delayMix,
                 reverbSize, reverbDamp, reverbWidth, reverbMix,
                 masterGainDb;
    };

    struct Levels
    {
        float peakL = 0.0f, peakR = 0.0f;
        float rmsL = 0.0f, rmsR = 0.0f;
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset(); // clear all delay lines / filter state (hard stop)

    // Once per block, after the engine's modulation pass: discrete switches,
    // bypass crossfade targets, tempo-synced delay time.
    void setBlockParams (const FxParams& params, double bpm);

    void process (float* left, float* right, int numSamples);

    int latencySamples() const noexcept { return lookahead; }
    const Levels& levels() const noexcept { return meter; }

    // Measured tanh loudness compensation at a -12 dBFS sine: table[d] is the
    // output gain for d dB of pre-tanh drive. Shared with the filter drive.
    static void measureTanhCompensation (float* table, int numEntries);

    Smoothers smooth;

private:
    static float readHermite (const std::vector<float>& buffer, int writePos, float delaySamples) noexcept;

    double sampleRate = 48000.0;
    bool primed = false;

    // Discrete per-block state (from setBlockParams)
    bool delaySync = false;
    bool delayPingPong = false;
    Smoothed driveEnable, chorusEnable, delayEnable, reverbEnable;

    float driveComp[25] {};
    float tiltStateL = 0.0f, tiltStateR = 0.0f; // one-pole split at 800 Hz
    float tiltCoeff = 0.0f;

    // Chorus: one modulated line per channel, quadrature LFOs (SPEC 10)
    std::vector<float> chorusBufL, chorusBufR;
    int chorusPos = 0;
    double chorusPhase = 0.0;

    // Delay
    std::vector<float> delayBufL, delayBufR;
    int delayPos = 0;
    float dampStateL = 0.0f, dampStateR = 0.0f;

    juce::dsp::Reverb reverb;
    std::vector<float> wetL, wetR; // reverb scratch (sized in prepare)

    // Lookahead limiter (always on)
    int lookahead = 0;                    // ~1.5 ms
    float ceiling = 0.966051f;            // -0.3 dBFS
    float releaseAlpha = 0.0f;            // ~80 ms one-pole
    std::vector<float> limDelayL, limDelayR, limTargets, limEnvHist;
    int limPos = 0, limHistPos = 0;
    double limBoxSum = 0.0;
    float limReleaseEnv = 1.0f;

    Levels meter;
};
} // namespace lumen
