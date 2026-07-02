#pragma once

// Plain value struct handed from the parameter layer (APVTS or the render
// tool's --set flags) to the SynthEngine once per block. Member initializers
// are THE defaults: State/Parameters.cpp builds the APVTS layout from them so
// plugin and headless harness can never disagree.
//
// Pure DSP layer: no JUCE includes here.

#include "Engine/Lfo.h"
#include "Engine/ModMatrix.h"

namespace lumen
{
enum class TableChoice   { basic = 0, pwm, harmonicRise, formant, image };
enum class FilterMode    { lp12 = 0, lp24, hp12, bp12, notch };
enum class SubWave       { sine = 0, triangle, square };
enum class NoiseType     { white = 0, pink };

struct OscParams
{
    bool  enabled       = true;
    int   table         = 0;      // TableChoice
    float morph         = 0.0f;   // 0..1 across frames
    float level         = 0.7f;   // linear 0..1
    float pan           = 0.0f;   // -1..1
    int   semitones     = 0;      // -24..24
    float fineCents     = 0.0f;   // -100..100
    int   unison        = 1;      // 1..8
    float detuneCents   = 10.0f;  // 0..50 (full spread, symmetric)
    float width         = 0.7f;   // 0..1 stereo spread of unison voices
    float blend         = 0.5f;   // 0 = center voices, 1 = detuned edges
    bool  phaseRandom   = true;   // random start phase per unison voice per note
};

struct EnvParams
{
    float attackSeconds  = 0.005f; // 0.001..10, log
    float decaySeconds   = 0.5f;   // 0.001..10, log
    float sustain        = 1.0f;   // 0..1
    float releaseSeconds = 0.2f;   // 0.005..15, log
    float curve          = 0.0f;   // -1 (exponential) .. +1 (logarithmic)
};

struct FxParams // SPEC section 10, fixed order Drive->Chorus->Delay->Reverb->Limiter
{
    bool  driveEnabled   = false;
    float driveDb        = 0.0f;    // 0..24, tanh amount
    float driveTone      = 0.0f;    // -1..1, +-6 dB tilt at 800 Hz

    bool  chorusEnabled  = false;
    float chorusRateHz   = 0.5f;    // 0.05..5
    float chorusDepth    = 0.5f;    // 0..1
    float chorusMix      = 0.5f;    // 0..1

    bool  delayEnabled   = true;
    bool  delaySync      = true;
    float delayTimeMs    = 400.0f;  // 1..2000, used when !delaySync
    int   delayDiv       = 12;      // "1/4", same table as the LFOs
    float delayFeedback  = 0.35f;   // 0..0.95
    float delayDampHz    = 8000.0f; // 1000..16000 feedback lowpass
    bool  delayPingPong  = false;
    float delayMix       = 0.0f;    // frozen parameter #7

    bool  reverbEnabled  = true;
    float reverbSize     = 0.5f;    // 0..1
    float reverbDamp     = 0.5f;    // 0..1
    float reverbWidth    = 1.0f;    // 0..1
    float reverbMix      = 0.12f;   // frozen parameter #8

    float masterGainDb   = 0.0f;    // -60 (= -inf) .. +6
};

struct EngineParams
{
    OscParams oscA {};
    OscParams oscB { .enabled = false };

    int   subWave    = 0;       // SubWave
    int   subOctave  = 1;       // 1 = -1 octave, 2 = -2 octaves
    float subLevel   = 0.0f;    // linear 0..1

    int   noiseType  = 0;       // NoiseType
    float noiseDb    = -60.0f;  // -60 (off) .. 0 dBFS-ish

    int   filterMode      = 1;      // FilterMode (default LP24)
    float filterCutoffHz  = 20000.0f;
    float filterRes       = 0.12f;  // 0..1 -> Q 0.5..12
    float filterDriveDb   = 0.0f;   // 0..24, tanh pre-filter, gain-compensated
    float filterKeytrack  = 0.0f;   // 0..1 (100% = cutoff follows note 1:1)
    float filterEnvAmount = 0.0f;   // -1..1, Env 2 -> cutoff, +-5 octaves span

    EnvParams env1 {};                          // -> voice amplitude (fixed)
    EnvParams env2 { .releaseSeconds = 0.3f };  // -> filter via filterEnvAmount
    EnvParams env3 {};                          // free (matrix)

    // --- Phase 3: modulation ------------------------------------------
    LfoParams lfo[3] {};
    float macroValues[4] { 0.5f, 0.5f, 0.3f, 0.2f }; // frozen macro1..4 defaults
    double bpm = 120.0;                              // host tempo, fallback 120
    mod::Config mod {};                              // matrix slots + macro maps

    // --- Phase 4: effects ---------------------------------------------
    FxParams fx {};
};
} // namespace lumen
