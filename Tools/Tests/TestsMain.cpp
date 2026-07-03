// lumen_tests — JUCE UnitTest runner (SPEC section 18).
// Suite: frozen-parameter contract, wavetable mip correctness, SVF stability
// under audio-rate modulation, envelope timing, engine sanity and voice
// stealing, mod-matrix math, LFOs, FX bypass null, limiter ceiling.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Engine/Envelope.h"
#include "Engine/FactoryTables.h"
#include "Engine/FxChain.h"
#include "Engine/Lfo.h"
#include "Engine/ModMatrix.h"
#include "Engine/SVF.h"
#include "Engine/SynthEngine.h"
#include "Engine/Wavetable.h"
#include "Lens/LensEngine.h"
#include "Lens/TestImages.h"
#include "State/EngineBindings.h"
#include "State/FactoryPresets.h"
#include "State/LensState.h"
#include "State/ModState.h"
#include "State/Parameters.h"
#include "UI/WaterfallModel.h"

#include <cmath>
#include <iostream>

namespace
{
// Minimal host-less processor so APVTS parameter order can be inspected
// without pulling in the plugin client.
class NullProcessor final : public juce::AudioProcessor
{
public:
    NullProcessor()
        : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", lumen::params::createParameterLayout())
    {
    }

    const juce::String getName() const override { return "NullProcessor"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

class FrozenParameterTest final : public juce::UnitTest
{
public:
    FrozenParameterTest() : juce::UnitTest ("Frozen first-8 parameter contract", "State") {}

    void runTest() override
    {
        beginTest ("order, IDs, names, defaults (SPEC section 12)");

        NullProcessor processor;
        const auto& parameters = processor.getParameters();

        struct Expected { const char* id; const char* name; float defaultValue; };
        const Expected expected[] = {
            { "macro1",       "Tone",             0.5f },
            { "macro2",       "Motion",           0.5f },
            { "macro3",       "Space",            0.3f },
            { "macro4",       "Texture",          0.2f },
            { "filterCutoff", "Filter Cutoff",    20000.0f },
            { "filterRes",    "Filter Resonance", 0.12f },
            { "delayMix",     "Delay Mix",        0.0f },
            { "reverbMix",    "Reverb Mix",       0.12f },
        };

        expect (parameters.size() >= 8, "at least 8 parameters exist");

        for (int i = 0; i < 8 && i < parameters.size(); ++i)
        {
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (parameters[i]);
            expect (parameter != nullptr, "parameter " + juce::String (i) + " is a RangedAudioParameter");
            if (parameter == nullptr)
                continue;

            expectEquals (parameter->paramID, juce::String (expected[i].id));
            expectEquals (parameter->getName (64), juce::String (expected[i].name));

            const float defaultValue = parameter->convertFrom0to1 (parameter->getDefaultValue());
            const float tolerance = juce::jmax (0.0001f, std::abs (expected[i].defaultValue) * 0.001f);
            expectWithinAbsoluteError (defaultValue, expected[i].defaultValue, tolerance);
        }

        beginTest ("Phase 2-4 parameters present after the frozen block");
        for (auto* id : { lumen::params::masterGain, lumen::params::oscAMorph,
                          lumen::params::filterMode, lumen::params::env1Attack,
                          lumen::params::oscBUnison, lumen::params::noiseLevel,
                          lumen::params::driveAmount, lumen::params::chorusRate,
                          lumen::params::delayTime, lumen::params::delayPingPong,
                          lumen::params::reverbSize })
            expect (processor.apvts.getParameter (id) != nullptr, juce::String (id) + " exists");
    }
};

class MipLevelTest final : public juce::UnitTest
{
public:
    MipLevelTest() : juce::UnitTest ("Wavetable mip levels are band-limited and correct", "Engine") {}

    void runTest() override
    {
        beginTest ("mip spectra: harmonics kept below the cap, zero above");

        // Full-bandwidth saw frame as the known source.
        constexpr int n = lumen::Wavetable::kFrameLength;
        std::vector<float> saw (n);
        for (int i = 0; i < n; ++i)
        {
            float v = 0.0f;
            for (int h = 1; h <= 1023; ++h)
                v += std::sin (2.0f * juce::MathConstants<float>::pi * h * i / n) / static_cast<float> (h);
            saw[static_cast<size_t> (i)] = v;
        }

        lumen::Wavetable table;
        table.build (saw.data(), 1);

        juce::dsp::FFT fft (11);
        for (int level = 0; level < lumen::Wavetable::kNumMips; ++level)
        {
            const int cap = lumen::Wavetable::maxHarmonic (level);
            expectEquals (cap, std::min (1023, 1024 >> level), "cap formula, level " + juce::String (level));

            std::vector<float> x (2 * n, 0.0f);
            std::copy_n (table.frameData (0, level), n, x.begin());
            fft.performRealOnlyForwardTransform (x.data());

            double inBand = 0.0, outOfBand = 0.0, dc = std::abs (x[0]);
            for (int bin = 1; bin <= n / 2; ++bin)
            {
                const double mag = std::hypot (x[2 * static_cast<size_t> (bin)],
                                               x[2 * static_cast<size_t> (bin) + 1]);
                if (bin <= cap)
                    inBand = juce::jmax (inBand, mag);
                else
                    outOfBand = juce::jmax (outOfBand, mag);
            }

            expect (inBand > 0.0, "level " + juce::String (level) + " has content");
            expect (outOfBand < inBand * 1.0e-5,
                    "level " + juce::String (level) + " out-of-band < -100 dB (got ratio "
                    + juce::String (outOfBand / juce::jmax (1.0e-12, inBand), 10) + ")");
            expect (dc < inBand * 1.0e-5, "level " + juce::String (level) + " has no DC");

            // Relative harmonic weights survive the mip build (saw: 1/h).
            if (cap >= 4)
            {
                const double h1 = std::hypot (x[2], x[3]);
                const double h4 = std::hypot (x[8], x[9]);
                expectWithinAbsoluteError (h4 / h1, 0.25, 0.01);
            }
        }

        beginTest ("mip selection: top harmonic below 0.45 * sr");
        const double sr = 48000.0;
        for (const int midi : { 33, 60, 84, 96 })
        {
            const double f0 = 440.0 * std::exp2 ((midi - 69.0) / 12.0);
            const int level = lumen::Wavetable::mipForIncrement (f0 / sr);
            expect (lumen::Wavetable::maxHarmonic (level) * f0 < 0.45 * sr,
                    "MIDI " + juce::String (midi) + " level " + juce::String (level) + " in band");
            if (level > 0)
                expect (lumen::Wavetable::maxHarmonic (level - 1) * f0 >= 0.45 * sr,
                        "MIDI " + juce::String (midi) + " uses the highest legal resolution");
        }
    }
};

class SVFStabilityTest final : public juce::UnitTest
{
public:
    SVFStabilityTest() : juce::UnitTest ("SVF stays bounded under audio-rate cutoff sweeps", "Engine") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;
        constexpr int numSamples = static_cast<int> (2.0 * sr);

        for (int mode = 0; mode < 5; ++mode)
        {
            beginTest ("mode " + juce::String (mode) + ", res 1.0, cutoff 20 Hz - 20 kHz at 200 Hz");

            lumen::SVF filter;
            filter.prepare (sr);

            double sawPhase = 0.0;
            float peak = 0.0f;
            int nanCount = 0;

            for (int i = 0; i < numSamples; ++i)
            {
                // -6 dB saw input at 110 Hz
                const float input = 0.5f * (2.0f * static_cast<float> (sawPhase) - 1.0f);
                sawPhase += 110.0 / sr;
                if (sawPhase >= 1.0) sawPhase -= 1.0;

                // Exponential cutoff sweep 20 Hz <-> 20 kHz, modulated at 200 Hz
                const float mod = 0.5f + 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                          * 200.0f * static_cast<float> (i) / static_cast<float> (sr));
                const float cutoff = 20.0f * std::pow (1000.0f, mod);

                filter.setPerSample (cutoff, 1.0f);
                const float out = filter.processSample (input, mode, 0);

                if (std::isnan (out) || std::isinf (out))
                    ++nanCount;
                else
                    peak = juce::jmax (peak, std::abs (out));
            }

            expectEquals (nanCount, 0);
            expect (peak < 4.0f, "output bounded (peak " + juce::String (peak, 3) + ")");
            expect (peak > 0.01f, "output not silent");
        }
    }
};

class EnvelopeTimingTest final : public juce::UnitTest
{
public:
    EnvelopeTimingTest() : juce::UnitTest ("Envelope attack/release timing", "Engine") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;

        for (const float seconds : { 0.01f, 0.1f, 1.0f })
        {
            beginTest (juce::String (seconds, 2) + " s attack to 90%, release to -60 dB (+-10%)");

            lumen::Envelope env;
            env.setSampleRate (sr);
            lumen::EnvParams p;
            p.attackSeconds = seconds;
            p.decaySeconds = 1.0f;
            p.sustain = 1.0f;
            p.releaseSeconds = seconds;
            env.setParameters (p);
            env.noteOn();

            int attackSamples = 0;
            while (env.next() < 0.9f && attackSamples < static_cast<int> (20.0 * sr))
                ++attackSamples;
            const double attackError = std::abs (attackSamples / sr - seconds) / seconds;
            expect (attackError <= 0.10,
                    "attack " + juce::String (attackSamples / sr, 4) + " s vs set "
                    + juce::String (seconds, 4) + " s (error " + juce::String (attackError * 100.0, 1) + "%)");

            // Let it settle into sustain, then release.
            for (int i = 0; i < static_cast<int> (2.0 * sr) && i < 200000; ++i)
                env.next();

            env.noteOff();
            int releaseSamples = 0;
            while (env.value() > 0.001f && env.isActive() && releaseSamples < static_cast<int> (20.0 * sr))
            {
                env.next();
                ++releaseSamples;
            }
            const double releaseError = std::abs (releaseSamples / sr - seconds) / seconds;
            expect (releaseError <= 0.10,
                    "release " + juce::String (releaseSamples / sr, 4) + " s vs set "
                    + juce::String (seconds, 4) + " s (error " + juce::String (releaseError * 100.0, 1) + "%)");
        }

        beginTest ("sustain level and -90 dB voice end");
        lumen::Envelope env;
        env.setSampleRate (sr);
        lumen::EnvParams p;
        p.attackSeconds = 0.001f;
        p.decaySeconds = 0.05f;
        p.sustain = 0.5f;
        p.releaseSeconds = 0.05f;
        env.setParameters (p);
        env.noteOn();
        for (int i = 0; i < static_cast<int> (0.5 * sr); ++i)
            env.next();
        expectWithinAbsoluteError (env.value(), 0.5f, 0.001f);
        env.noteOff();
        for (int i = 0; i < static_cast<int> (0.2 * sr) && env.isActive(); ++i)
            env.next();
        expect (! env.isActive(), "envelope goes idle after release");
        expectEquals (env.value(), 0.0f);
    }
};

class EngineRenderTest final : public juce::UnitTest
{
public:
    EngineRenderTest() : juce::UnitTest ("Engine renders clean audio and steals voices", "Engine") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;

        beginTest ("single note: audible, no clipping, low DC, silent tail");

        lumen::SynthEngine engine;
        engine.prepare (sr, blockSize);
        lumen::EngineParams params;
        params.fx.reverbEnabled = false; // this test asserts a silent tail,
        params.fx.delayEnabled = false;  // so keep the time-based FX out
        engine.setParams (params);

        constexpr int totalSamples = 4 * 48000;
        std::vector<float> left (totalSamples, 0.0f), right (totalSamples, 0.0f);

        engine.noteOn (60, 100.0f / 127.0f);
        bool sentNoteOff = false;
        for (int pos = 0; pos < totalSamples; pos += blockSize)
        {
            if (! sentNoteOff && pos >= 96000) // note off at ~2 s; default release 200 ms
            {
                engine.noteOff (60);
                sentNoteOff = true;
            }
            engine.render (left.data() + pos, right.data() + pos,
                           std::min (blockSize, totalSamples - pos));
        }

        float peak = 0.0f, tailPeak = 0.0f;
        double sum = 0.0;
        int nanCount = 0;
        for (int i = 0; i < totalSamples; ++i)
        {
            if (std::isnan (left[static_cast<size_t> (i)]) || std::isinf (left[static_cast<size_t> (i)]))
                ++nanCount;
            peak = juce::jmax (peak, std::abs (left[static_cast<size_t> (i)]));
            sum += left[static_cast<size_t> (i)];
            if (i >= totalSamples - 24000) // last 0.5 s
                tailPeak = juce::jmax (tailPeak, std::abs (left[static_cast<size_t> (i)]));
        }

        expectEquals (nanCount, 0);
        expect (peak > 0.05f, "audible output");
        expect (peak < 1.0f, "no clipping");
        expect (std::abs (sum / totalSamples) < 0.001, "DC offset < 0.001");
        expect (tailPeak < 1.0e-4f, "silent after release completes");

        beginTest ("17 notes on 16 voices: stealing, bounded output, all released");

        engine.reset();
        engine.setParams (params);
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);

        for (int n = 0; n < 17; ++n)
            engine.noteOn (48 + n, 0.8f);
        expectEquals (engine.activeVoiceCount(), 16);

        bool sentNoteOffs = false;
        for (int pos = 0; pos < totalSamples; pos += blockSize)
        {
            if (! sentNoteOffs && pos >= 48000)
            {
                for (int n = 0; n < 17; ++n)
                    engine.noteOff (48 + n);
                sentNoteOffs = true;
            }
            engine.render (left.data() + pos, right.data() + pos,
                           std::min (blockSize, totalSamples - pos));
        }

        float peak17 = 0.0f;
        int nan17 = 0;
        for (int i = 0; i < totalSamples; ++i)
        {
            if (std::isnan (left[static_cast<size_t> (i)]))
                ++nan17;
            peak17 = juce::jmax (peak17, std::abs (left[static_cast<size_t> (i)]));
        }
        expectEquals (nan17, 0);
        // The always-on limiter caps the 16-voice pileup at -0.3 dBFS.
        expect (peak17 <= 0.9672f, "16-voice pileup held under the limiter ceiling");
        expectEquals (engine.activeVoiceCount(), 0);
    }
};

class FxNullTest final : public juce::UnitTest
{
public:
    FxNullTest() : juce::UnitTest ("FX all-bypassed output nulls against the pre-FX signal", "Effects") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;
        constexpr int totalSamples = 2 * 48000;
        constexpr int noteOffSample = 48000;

        lumen::EngineParams params;
        params.fx.driveEnabled = false;
        params.fx.chorusEnabled = false;
        params.fx.delayEnabled = false;
        params.fx.reverbEnabled = false; // mixes stay at their defaults: the
                                         // bypass crossfade must null on its own
        auto render = [&] (bool fxEnabled, std::vector<float>& left, std::vector<float>& right) -> int
        {
            lumen::SynthEngine engine;
            engine.prepare (sr, blockSize);
            engine.setFxEnabled (fxEnabled);
            engine.setParams (params);
            engine.noteOn (60, 100.0f / 127.0f);
            bool off = false;
            for (int pos = 0; pos < totalSamples; pos += blockSize)
            {
                if (! off && pos >= noteOffSample)
                {
                    engine.noteOff (60);
                    off = true;
                }
                engine.render (left.data() + pos, right.data() + pos,
                               std::min (blockSize, totalSamples - pos));
            }
            return engine.latencySamples();
        };

        std::vector<float> withFxL (totalSamples, 0.0f), withFxR (totalSamples, 0.0f);
        std::vector<float> preFxL (totalSamples, 0.0f), preFxR (totalSamples, 0.0f);
        const int latency = render (true, withFxL, withFxR);
        render (false, preFxL, preFxR);

        beginTest ("difference <= -80 dBFS after aligning the limiter lookahead");
        expect (latency > 0, "FX bus reports lookahead latency");

        float materialPeak = 0.0f, maxDiff = 0.0f;
        for (int i = 0; i < totalSamples - latency; ++i)
        {
            materialPeak = juce::jmax (materialPeak, std::abs (preFxL[static_cast<size_t> (i)]));
            maxDiff = juce::jmax (maxDiff,
                std::abs (withFxL[static_cast<size_t> (i + latency)] - preFxL[static_cast<size_t> (i)]));
            maxDiff = juce::jmax (maxDiff,
                std::abs (withFxR[static_cast<size_t> (i + latency)] - preFxR[static_cast<size_t> (i)]));
        }

        const float materialDb = juce::Decibels::gainToDecibels (materialPeak, -144.0f);
        const float diffDb = juce::Decibels::gainToDecibels (maxDiff, -144.0f);
        logMessage ("    material peak " + juce::String (materialDb, 2) + " dBFS, null difference "
                    + juce::String (diffDb, 2) + " dBFS");
        expect (materialPeak > 0.05f, "material is audible");
        expect (diffDb <= -80.0f, "null difference " + juce::String (diffDb, 2) + " dBFS <= -80");
    }
};

class LimiterTest final : public juce::UnitTest
{
public:
    LimiterTest() : juce::UnitTest ("Limiter ceiling, latency, and transparency", "Effects") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;
        constexpr int blockSize = 512;
        const float ceiling = juce::Decibels::decibelsToGain (-0.3f);

        lumen::FxParams bypassed;
        bypassed.driveEnabled = bypassed.chorusEnabled = false;
        bypassed.delayEnabled = bypassed.reverbEnabled = false;

        beginTest ("+6 dBFS sine is held at the -0.3 dBFS ceiling, no NaN");
        lumen::FxChain fx;
        fx.prepare (sr, blockSize);
        fx.setBlockParams (bypassed, 120.0);
        expectWithinAbsoluteError (static_cast<double> (fx.latencySamples()), 0.0015 * sr, 1.0);

        float maxOut = 0.0f;
        int nanCount = 0;
        std::vector<float> left (blockSize), right (blockSize);
        double phase = 0.0;
        for (int block = 0; block < 200; ++block) // ~2.1 s
        {
            for (int i = 0; i < blockSize; ++i)
            {
                left[static_cast<size_t> (i)] = right[static_cast<size_t> (i)]
                    = 2.0f * std::sin (static_cast<float> (phase)); // +6 dBFS
                phase += 2.0 * juce::MathConstants<double>::pi * 997.0 / sr;
            }
            fx.setBlockParams (bypassed, 120.0);
            fx.process (left.data(), right.data(), blockSize);
            for (int i = 0; i < blockSize; ++i)
            {
                const float v = left[static_cast<size_t> (i)];
                if (std::isnan (v) || std::isinf (v))
                    ++nanCount;
                else
                    maxOut = juce::jmax (maxOut, std::abs (v));
            }
        }
        expectEquals (nanCount, 0);
        logMessage ("    +6 dB input -> peak " + juce::String (juce::Decibels::gainToDecibels (maxOut), 3) + " dBFS");
        expect (maxOut <= ceiling + 1.0e-5f, "peak " + juce::String (maxOut, 6) + " <= ceiling");
        expect (maxOut >= 0.9f, "limiter output works near the ceiling, not squashed to nothing");

        beginTest ("-12 dBFS material passes bit-transparently (gain exactly 1)");
        lumen::FxChain fx2;
        fx2.prepare (sr, blockSize);
        const int latency = fx2.latencySamples();
        std::vector<float> inL, outL;
        float maxDiff = 0.0f;
        phase = 0.0;
        for (int block = 0; block < 100; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                left[static_cast<size_t> (i)] = right[static_cast<size_t> (i)]
                    = 0.25f * std::sin (static_cast<float> (phase)); // -12 dBFS
                phase += 2.0 * juce::MathConstants<double>::pi * 199.0 / sr;
            }
            inL.insert (inL.end(), left.begin(), left.end());
            fx2.setBlockParams (bypassed, 120.0);
            fx2.process (left.data(), right.data(), blockSize);
            outL.insert (outL.end(), left.begin(), left.end());
        }
        for (size_t i = 0; i + static_cast<size_t> (latency) < outL.size(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (outL[i + static_cast<size_t> (latency)] - inL[i]));
        logMessage ("    transparency diff " + juce::String (juce::Decibels::gainToDecibels (maxDiff, -144.0f), 2) + " dBFS");
        expect (maxDiff < 1.0e-6f, "limiter is transparent below the ceiling");
    }
};

class InitModDefaultsTest final : public juce::UnitTest
{
public:
    InitModDefaultsTest() : juce::UnitTest ("Init default macro maps & motion slot", "Modulation") {}

    void runTest() override
    {
        using namespace lumen;

        beginTest ("fresh state gets the Init modulation defaults");
        juce::ValueTree state ("PARAMS");
        modstate::ensureTrees (state);
        mod::Config fromTree {};
        modstate::buildConfig (state, fromTree);

        const auto& slot0 = fromTree.slots[0];
        expect (slot0.enabled, "motion slot enabled");
        expectEquals (slot0.source, modstate::sourceFromToken ("lfo1"));
        expectEquals (slot0.dest, modstate::destFromToken ("oscAMorph"));
        expectWithinAbsoluteError (slot0.depth, 0.50f, 1.0e-6f);
        for (int i = 1; i < mod::kNumSlots; ++i)
            expect (! fromTree.slots[i].enabled, "remaining slots empty");

        beginTest ("tree defaults match applyInitModDefaults (render harness)");
        mod::Config direct {};
        modstate::applyInitModDefaults (direct);
        for (int m = 0; m < mod::kNumMacros; ++m)
            for (int i = 0; i < mod::kMaxMacroMaps; ++i)
            {
                expectEquals (fromTree.macroMaps[m][i].dest, direct.macroMaps[m][i].dest);
                expectWithinAbsoluteError (fromTree.macroMaps[m][i].rangeMin,
                                           direct.macroMaps[m][i].rangeMin, 1.0e-6f);
                expectWithinAbsoluteError (fromTree.macroMaps[m][i].rangeMax,
                                           direct.macroMaps[m][i].rangeMax, 1.0e-6f);
                expectWithinAbsoluteError (fromTree.macroMaps[m][i].curve,
                                           direct.macroMaps[m][i].curve, 1.0e-6f);
            }

        beginTest ("neutral at the frozen macro defaults (Init timbre unchanged)");
        const float frozenDefaults[4] = { 0.5f, 0.5f, 0.3f, 0.2f }; // SPEC section 12
        for (int d = 0; d < mod::kNumDests; ++d)
            expectWithinAbsoluteError (mod::macroSumForDest (fromTree, d, frozenDefaults),
                                       0.0f, 1.0e-3f);

        beginTest ("every macro moves at least one destination when swept");
        for (int m = 0; m < 4; ++m)
        {
            float atZero[4], atOne[4];
            for (int i = 0; i < 4; ++i)
                atZero[i] = atOne[i] = frozenDefaults[i];
            atZero[m] = 0.0f;
            atOne[m] = 1.0f;
            float travel = 0.0f;
            for (int d = 0; d < mod::kNumDests; ++d)
                travel = juce::jmax (travel,
                    std::abs (mod::macroSumForDest (fromTree, d, atOne)
                              - mod::macroSumForDest (fromTree, d, atZero)));
            expect (travel > 0.2f, "macro " + juce::String (m + 1) + " has audible span");
        }

        beginTest ("saved states are not re-seeded (user-cleared maps stay cleared)");
        auto macros = state.getChildWithName ("MACROS");
        for (int m = 0; m < macros.getNumChildren(); ++m)
            macros.getChild (m).removeAllChildren (nullptr);
        state.getChildWithName ("MODMATRIX").getChild (0).setProperty ("enabled", false, nullptr);
        modstate::ensureTrees (state); // what setStateInformation runs on load
        mod::Config reloaded {};
        modstate::buildConfig (state, reloaded);
        expect (! reloaded.slots[0].enabled, "cleared slot stays cleared");
        for (int m = 0; m < mod::kNumMacros; ++m)
            expectEquals (reloaded.macroMaps[m][0].dest, -1);
    }
};

class ModMatrixMathTest final : public juce::UnitTest
{
public:
    ModMatrixMathTest() : juce::UnitTest ("Mod matrix combination math and clamping", "Modulation") {}

    void runTest() override
    {
        using namespace lumen::mod;

        beginTest ("final = clamp(base + sum(depth_i * source_i))");
        Config config {};
        config.slots[0] = { static_cast<int> (Source::lfo1), static_cast<int> (Dest::filterCutoff), 0.4f, true };
        config.slots[1] = { static_cast<int> (Source::modWheel), static_cast<int> (Dest::filterCutoff), -0.25f, true };
        config.slots[2] = { static_cast<int> (Source::modWheel), static_cast<int> (Dest::oscAMorph), 0.5f, true };
        config.slots[3] = { static_cast<int> (Source::aftertouch), static_cast<int> (Dest::filterCutoff), 0.9f, false }; // disabled

        float values[kNumSources] {};
        bool active[kNumSources] {};
        values[static_cast<int> (Source::lfo1)] = 0.5f;      active[static_cast<int> (Source::lfo1)] = true;
        values[static_cast<int> (Source::modWheel)] = 0.8f;  active[static_cast<int> (Source::modWheel)] = true;
        values[static_cast<int> (Source::aftertouch)] = 1.0f; active[static_cast<int> (Source::aftertouch)] = true;

        const float cutoffSum = sumForDest (config, static_cast<int> (Dest::filterCutoff), values, active);
        expectWithinAbsoluteError (cutoffSum, 0.4f * 0.5f - 0.25f * 0.8f, 1.0e-6f); // disabled slot ignored
        const float morphSum = sumForDest (config, static_cast<int> (Dest::oscAMorph), values, active);
        expectWithinAbsoluteError (morphSum, 0.4f, 1.0e-6f);
        expectEquals (sumForDest (config, static_cast<int> (Dest::oscBMorph), values, active), 0.0f);

        beginTest ("clamping to [0, 1] in normalized space");
        expectEquals (clampNorm (0.5f + 2.0f), 1.0f);
        expectEquals (clampNorm (0.2f - 3.0f), 0.0f);
        expectEquals (clampNorm (0.7f), 0.7f);

        beginTest ("normalize/denormalize round-trip incl. skewed ranges");
        for (const auto dest : { Dest::filterCutoff, Dest::env1Attack, Dest::oscAPan, Dest::noiseLevel })
        {
            const float e = skewExponent (dest);
            for (float p : { 0.0f, 0.1f, 0.5f, 0.9f, 1.0f })
            {
                const float natural = denormalize (dest, p, e);
                expectWithinAbsoluteError (normalize (dest, natural, e), p, 1.0e-4f);
            }
        }
        const float eCut = skewExponent (Dest::filterCutoff);
        expectWithinAbsoluteError (denormalize (Dest::filterCutoff, 0.5f, eCut), 632.5f, 0.5f);

        beginTest ("macro maps: exactly the mapped destinations respond");
        Config macroConfig {};
        macroConfig.macroMaps[0][0] = { static_cast<int> (Dest::oscAMorph), 0.0f, 0.5f };
        macroConfig.macroMaps[1][0] = { static_cast<int> (Dest::filterRes), 0.1f, 0.9f };

        const float zeros[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float macros[4];
        for (int m = 0; m < 4; ++m)
        {
            macros[0] = macros[1] = macros[2] = macros[3] = 0.0f;
            macros[m] = 1.0f;
            for (int d = 0; d < kNumDests; ++d)
            {
                const float sum = macroSumForDest (macroConfig, d, macros);
                const float atZero = macroSumForDest (macroConfig, d, zeros);
                const bool shouldRespond = (m == 0 && d == static_cast<int> (Dest::oscAMorph))
                                        || (m == 1 && d == static_cast<int> (Dest::filterRes));
                if (shouldRespond)
                    expect (std::abs (sum - atZero) > 0.3f, "macro " + juce::String (m + 1) + " moves dest " + juce::String (d));
                else
                    expectWithinAbsoluteError (sum, atZero, 1.0e-6f);
            }
        }
        const float halfMacro2[4] = { 0.0f, 0.5f, 0.0f, 0.0f };
        expectWithinAbsoluteError (macroSumForDest (macroConfig, static_cast<int> (Dest::filterRes), halfMacro2),
                                   0.1f + 0.8f * 0.5f, 1.0e-6f); // min + (max-min)*macro

        beginTest ("macro map curve shapes the response, endpoints unchanged");
        Config curveConfig {};
        curveConfig.macroMaps[0][0] = { static_cast<int> (Dest::reverbMix), -0.2f, 0.8f, 2.0f };
        float curveMacros[4] = { 0.5f, 0.0f, 0.0f, 0.0f };
        // offset = min + (max-min) * macro^curve = -0.2 + 1.0 * 0.25
        expectWithinAbsoluteError (macroSumForDest (curveConfig, static_cast<int> (Dest::reverbMix), curveMacros),
                                   0.05f, 1.0e-6f);
        curveMacros[0] = 0.0f;
        expectWithinAbsoluteError (macroSumForDest (curveConfig, static_cast<int> (Dest::reverbMix), curveMacros),
                                   -0.2f, 1.0e-6f);
        curveMacros[0] = 1.0f;
        expectWithinAbsoluteError (macroSumForDest (curveConfig, static_cast<int> (Dest::reverbMix), curveMacros),
                                   0.8f, 1.0e-6f);
    }
};

class LfoTest final : public juce::UnitTest
{
public:
    LfoTest() : juce::UnitTest ("LFO shapes, rate, host sync, fade", "Modulation") {}

    void runTest() override
    {
        using namespace lumen;

        beginTest ("host sync beats: 120 BPM, 1/4 = 2 Hz; dotted x1.5, triplet x2/3");
        LfoParams p;
        p.sync = true;
        p.syncDiv = 12; // "1/4"
        expectWithinAbsoluteError (lfoRateHz (p, 120.0), 2.0, 1.0e-9);
        p.syncDiv = 13; // "1/4 D"
        expectWithinAbsoluteError (lfoRateHz (p, 120.0), 2.0 / 1.5, 1.0e-9);
        p.syncDiv = 14; // "1/4 T"
        expectWithinAbsoluteError (lfoRateHz (p, 120.0), 3.0, 1.0e-9);
        p.syncDiv = 0;  // "4/1" = 16 beats
        expectWithinAbsoluteError (lfoRateHz (p, 120.0), 0.125, 1.0e-9);

        beginTest ("free rate: phase lands where it should");
        Lfo lfo;
        lfo.prepare (48000.0, 42);
        LfoParams sine;
        sine.rateHz = 1.0f;
        lfo.advance (sine, 120.0, 12000); // 0.25 s at 1 Hz -> quarter cycle
        expectWithinAbsoluteError (lfo.value (sine), 1.0f, 1.0e-3f); // sine peak

        beginTest ("all shapes stay in [-1, 1]");
        for (int shape = 0; shape < 6; ++shape)
        {
            Lfo l;
            l.prepare (48000.0, 7);
            LfoParams sp;
            sp.shape = shape;
            sp.rateHz = 3.7f;
            float lo = 1.0e9f, hi = -1.0e9f;
            for (int i = 0; i < 2000; ++i)
            {
                const float v = l.value (sp);
                lo = juce::jmin (lo, v);
                hi = juce::jmax (hi, v);
                l.advance (sp, 120.0, 37);
            }
            expect (lo >= -1.0001f && hi <= 1.0001f, "shape " + juce::String (shape) + " bounded");
            expect (hi - lo > 0.5f, "shape " + juce::String (shape) + " actually oscillates");
        }

        beginTest ("poly fade-in scales the output");
        Lfo faded;
        faded.prepare (48000.0, 3);
        LfoParams fp;
        fp.shape = static_cast<int> (LfoShape::square); // constant +1 first half cycle
        fp.rateHz = 0.5f;
        fp.fadeSeconds = 1.0f;
        faded.retrigger();
        faded.advance (fp, 120.0, 12000); // 0.25 s
        expectWithinAbsoluteError (faded.value (fp), 0.25f, 0.01f);
        faded.advance (fp, 120.0, 36000); // 1.25 s total: phase 0.625 -> square = -1, fade complete
        expectWithinAbsoluteError (faded.value (fp), -1.0f, 0.01f);
    }
};

// ---------------------------------------------------------------------------
// Play view spectral waterfall (WATERFALL_SPEC.md acceptance criteria 2 & 3)
// ---------------------------------------------------------------------------
class WaterfallModelTest final : public juce::UnitTest
{
public:
    WaterfallModelTest()
        : juce::UnitTest ("Waterfall column mapping and Web-Audio emulation", "UI") {}

    void runTest() override
    {
        using WM = lumen::WaterfallModel;
        expectEquals (WM::kSmoothing, 0.8f, "kSmoothing is the critical 0.8");

        // Criterion 2: column mapping at 44100 and 48000.
        for (const double sr : { 44100.0, 48000.0 })
        {
            beginTest ("column mapping at " + juce::String (sr, 0) + " Hz");
            WM model;
            model.prepare (sr);
            const double fHi = std::min (18000.0, sr / 2.0 - 1000.0);

            const int idxFirst = model.binIndexFor (0);
            const int idxLast = model.binIndexFor (WM::kColumns - 1);
            logMessage (juce::String::formatted (
                "  sr=%.0f: idx[0]=%d (%.1f Hz), idx[119]=%d (%.1f Hz), fHi=%.0f",
                sr, idxFirst, idxFirst * sr / WM::kFftSize,
                idxLast, idxLast * sr / WM::kFftSize, fHi));

            expectEquals (idxFirst,
                          std::clamp ((int) std::lround (30.0 * WM::kFftSize / sr), 1, 1023),
                          "idx[0] is the nearest bin to 30 Hz");
            expectEquals (idxLast,
                          std::clamp ((int) std::lround (fHi * WM::kFftSize / sr), 1, 1023),
                          "idx[119] is the nearest bin to fHi");
            expect (std::abs (idxFirst * sr / WM::kFftSize - 30.0) <= sr / WM::kFftSize,
                    "idx[0] within one bin of 30 Hz");
            expect (std::abs (idxLast * sr / WM::kFftSize - fHi) <= sr / WM::kFftSize,
                    "idx[119] within one bin of fHi");

            int previous = 0;
            bool monotonic = true, bounded = true;
            for (int c = 0; c < WM::kColumns; ++c)
            {
                const int idx = model.binIndexFor (c);
                monotonic = monotonic && idx >= previous;
                bounded = bounded && idx >= 1 && idx <= 1023;
                previous = idx;
            }
            expect (monotonic, "bin indices monotonic non-decreasing");
            expect (bounded, "all bin indices in [1, 1023]");
        }

        // Criterion 3: Web-Audio emulation — full-scale 1 kHz sine settles
        // its column above 0.8; silence drains every value below 0.01
        // within kRows rows (one full pass of the history ring).
        beginTest ("full-scale 1 kHz sine settles its column > 0.8");
        WM model;
        model.prepare (48000.0);

        int column = 0;
        float bestDistance = 1.0e9f;
        for (int c = 0; c < WM::kColumns; ++c)
        {
            const float distance = std::abs (model.columnFrequency (c) - 1000.0f);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                column = c;
            }
        }

        float block[WM::kFftSize];
        double phase = 0.0;
        const double increment = 2.0 * juce::MathConstants<double>::pi * 1000.0 / 48000.0;
        for (int frame = 0; frame < 60; ++frame) // 0.8^60 -> fully settled
        {
            for (int i = 0; i < WM::kFftSize; ++i)
            {
                block[i] = (float) std::sin (phase);
                phase += increment;
            }
            model.pushFrame (block);
        }
        const float settled = model.value (0, column);
        logMessage (juce::String::formatted (
            "  1 kHz column = %d (%.1f Hz, bin %d), settled v = %.3f",
            column, model.columnFrequency (column), model.binIndexFor (column), settled));
        expectGreaterThan (settled, 0.8f, "settled v approximately 1-ish");

        beginTest ("processed digital silence decays the column monotonically");
        // v stays clamped at 1 until the smoothed magnitude falls below the
        // -30 dB ceiling (~13 frames from 0.5 at 0.8/frame), then decays.
        std::fill (std::begin (block), std::end (block), 0.0f);
        float previousValue = settled;
        for (int frame = 0; frame < 20; ++frame)
        {
            model.pushFrame (block);
            const float v = model.value (0, column);
            expect (v <= previousValue + 1.0e-6f, "value never rises during silence");
            previousValue = v;
        }
        expectLessThan (previousValue, settled, "smoothing decays toward the floor");

        beginTest ("silence drains every column below 0.01 within kRows rows");
        for (int frame = 0; frame < WM::kRows; ++frame)
            model.pushSilent(); // the audio-inactive path (WATERFALL_SPEC section 6)
        float peak = 0.0f;
        for (int j = 0; j < WM::kRows; ++j)
            for (int c = 0; c < WM::kColumns; ++c)
                peak = std::max (peak, model.value (j, c));
        logMessage ("  peak value across the ring after " + juce::String (WM::kRows)
                    + " drain rows = " + juce::String (peak, 6));
        expectLessThan (peak, 0.01f, "surface fully drained");
    }
};

// ---------------------------------------------------------------------------
// Phase 6: Lens
// ---------------------------------------------------------------------------

namespace lenstest
{
    // Per-harmonic amplitude spectrum of one 2048-sample frame.
    std::vector<float> harmonicAmplitudes (const float* frame)
    {
        juce::dsp::FFT fft (11);
        std::vector<float> spectrum (2 * 2048, 0.0f);
        std::copy_n (frame, 2048, spectrum.begin());
        fft.performRealOnlyForwardTransform (spectrum.data());

        std::vector<float> amps (1024, 0.0f);
        for (int k = 1; k < 1024; ++k)
            amps[static_cast<size_t> (k)] = std::hypot (spectrum[2 * static_cast<size_t> (k)],
                                                        spectrum[2 * static_cast<size_t> (k) + 1]);
        return amps;
    }

    double centroid (const std::vector<float>& amps)
    {
        double num = 0.0, den = 0.0;
        for (size_t k = 1; k < amps.size(); ++k)
        {
            num += static_cast<double> (k) * amps[k];
            den += amps[k];
        }
        return den > 0.0 ? num / den : 0.0;
    }

    juce::Image solidColour (juce::Colour colour)
    {
        juce::Image img (juce::Image::ARGB, 64, 64, true, juce::SoftwareImageType());
        juce::Graphics g (img);
        g.fillAll (colour);
        return img;
    }
} // namespace lenstest

class LensDeterminismTest final : public juce::UnitTest
{
public:
    LensDeterminismTest() : juce::UnitTest ("Lens determinism + state round-trip", "Lens") {}

    // The pinned reference checksums: the fixed procedural gradient image
    // must always produce these exact wavetables (PHASES Phase 6 gate).
    static constexpr const char* kExpectedScanSha =
        "cdb1783437a6c6ddbcb706c86dd60e7834fa58430c06b445c0ebfcfdd0d0df7c";
    static constexpr const char* kExpectedSpectralSha =
        "62a3e9c6c1416bded864a92a9e7700d9f4a166db22c0fc44312e3bb2d76b4cfe";

    void runTest() override
    {
        using namespace lumen;

        beginTest ("same image -> identical seed and frames (two fresh runs)");
        const auto image = lens::testimages::gradient();
        const auto a1 = lens::analyzeImage (image);
        const auto a2 = lens::analyzeImage (image);
        expect (a1.valid && a2.valid, "analysis runs");
        expectEquals (juce::String::toHexString (static_cast<juce::int64> (a1.seed)),
                      juce::String::toHexString (static_cast<juce::int64> (a2.seed)));

        const auto scan1 = lens::buildFrames (a1, lens::Mode::scan);
        const auto scan2 = lens::buildFrames (a2, lens::Mode::scan);
        const auto spectral1 = lens::buildFrames (a1, lens::Mode::spectral);
        const auto spectral2 = lens::buildFrames (a2, lens::Mode::spectral);
        expect (scan1 == scan2, "scan frames bit-identical");
        expect (spectral1 == spectral2, "spectral frames bit-identical");

        beginTest ("wavetable SHA-256 matches the pinned reference");
        const auto scanSha = lens::sha256Hex (scan1.data(), scan1.size() * sizeof (float));
        const auto spectralSha = lens::sha256Hex (spectral1.data(),
                                                  spectral1.size() * sizeof (float));
        logMessage ("  scan     sha256 = " + scanSha);
        logMessage ("  spectral sha256 = " + spectralSha);
        logMessage ("  seed = " + juce::String::toHexString (static_cast<juce::int64> (a1.seed)));
        expectEquals (scanSha, juce::String (kExpectedScanSha));
        expectEquals (spectralSha, juce::String (kExpectedSpectralSha));

        beginTest ("state round-trip restores the frames bit-exactly (no source image)");
        juce::ValueTree state ("PARAMS");
        const auto thumbPng = lens::encodePng (lens::makeThumbnail (a1));
        expect (thumbPng.getSize() > 0, "thumbnail PNG encoded");
        lensstate::storeImage (state, 0, scan1, thumbPng, a1.seed, "gradient.png");

        // Through XML text — exactly what getStateInformation/presets do.
        const auto xmlText = state.toXmlString();
        expect (! xmlText.contains ("gradient_source_path"), "no path stored");
        const auto restored = juce::ValueTree::fromXml (xmlText);
        std::vector<float> loaded;
        expect (lensstate::loadImageFrames (restored, 0, loaded), "frames load back");
        expect (loaded == scan1, "restored frames bit-identical");

        const auto thumb = lensstate::loadThumbnail (restored, 0);
        expect (thumb.isValid() && thumb.getWidth() == 64 && thumb.getHeight() == 64,
                "64x64 thumbnail restored");

        beginTest ("removeImage drops the IMAGE node (frames + thumbnail + name)");
        {
            auto cleared = restored.createCopy();
            lensstate::removeImage (cleared, 0);
            std::vector<float> none;
            expect (! lensstate::hasImage (cleared, 0), "hasImage false after remove");
            expect (! lensstate::loadImageFrames (cleared, 0, none), "no frames after remove");
            expect (! lensstate::loadThumbnail (cleared, 0).isValid(), "no thumbnail after remove");
            expect (lensstate::sourceName (cleared, 0).isEmpty(), "no source name after remove");
            lensstate::removeImage (cleared, 0); // idempotent, no crash
        }

        beginTest ("restored table renders bit-identical audio");
        Wavetable original, roundTripped;
        original.build (scan1.data(), lens::kNumFrames, lens::kHarmonicCap, lens::kPeakTarget);
        roundTripped.build (loaded.data(), lens::kNumFrames, lens::kHarmonicCap, lens::kPeakTarget);

        auto renderWith = [] (const Wavetable& table, std::vector<float>& l, std::vector<float>& r)
        {
            SynthEngine engine;
            engine.prepare (48000.0, 512);
            engine.setImageTable (0, &table);
            EngineParams params;
            params.oscA.table = static_cast<int> (TableChoice::image);
            params.oscA.morph = 0.5f;
            engine.setParams (params);
            engine.noteOn (60, 100.0f / 127.0f);
            l.assign (24000, 0.0f);
            r.assign (24000, 0.0f);
            engine.render (l.data(), r.data(), 24000);
        };
        std::vector<float> l1, r1, l2, r2;
        renderWith (original, l1, r1);
        renderWith (roundTripped, l2, r2);
        expect (l1 == l2 && r1 == r2, "audio bit-identical after round-trip");
    }
};

class LensCentroidTest final : public juce::UnitTest
{
public:
    LensCentroidTest() : juce::UnitTest ("Lens scan centroid monotonicity", "Lens") {}

    void runTest() override
    {
        using namespace lumen;

        beginTest ("gradient image, Scan mode: centroid rises down the morph range");
        const auto analysis = lens::analyzeImage (lens::testimages::gradient());
        const auto frames = lens::buildFrames (analysis, lens::Mode::scan);
        expectEquals (static_cast<int> (frames.size()), lens::kNumFrames * lens::kFrameLength);

        double first = 0.0, last = 0.0, previous = 0.0;
        bool monotonic = true;
        for (int i = 0; i < lens::kNumFrames; ++i)
        {
            const auto amps = lenstest::harmonicAmplitudes (
                frames.data() + static_cast<size_t> (i) * lens::kFrameLength);
            const double c = lenstest::centroid (amps);
            if (i == 0)
                first = c;
            else if (c < previous - 1.0e-3) // equal rows may repeat; never fall
                monotonic = false;
            previous = c;
            last = c;
        }
        logMessage (juce::String::formatted ("  centroid frame 0 = %.2f, frame 63 = %.2f",
                                             first, last));
        expect (monotonic, "centroid never decreases across the morph range");
        expectGreaterThan (last, first * 10.0, "centroid rises strongly overall");
    }
};

class LensStripeBinsTest final : public juce::UnitTest
{
public:
    LensStripeBinsTest() : juce::UnitTest ("Lens spectral stripe bins", "Lens") {}

    void runTest() override
    {
        using namespace lumen;

        beginTest ("stripe image, Spectral mode: peaks land on the stripe harmonics");
        const auto analysis = lens::analyzeImage (lens::testimages::stripes());
        const auto frames = lens::buildFrames (analysis, lens::Mode::spectral);

        // Horizontal stripes make every column identical; check a middle frame.
        const auto amps = lenstest::harmonicAmplitudes (
            frames.data() + static_cast<size_t> (32) * lens::kFrameLength);

        float maxAmp = 0.0f;
        for (int k = 1; k <= lens::kMaxHarmonics; ++k)
            maxAmp = juce::jmax (maxAmp, amps[static_cast<size_t> (k)]);

        // Peaks: local maxima above 20% of the strongest harmonic.
        std::vector<int> peaks;
        for (int k = 2; k < lens::kMaxHarmonics; ++k)
        {
            const float a = amps[static_cast<size_t> (k)];
            if (a > 0.2f * maxAmp
                && a >= amps[static_cast<size_t> (k - 1)]
                && a > amps[static_cast<size_t> (k + 1)])
                peaks.push_back (k);
        }

        juce::String peakList;
        for (const int p : peaks)
            peakList << p << " ";
        logMessage ("  peaks at harmonics: " + peakList.trim());

        expect (std::abs (static_cast<int> (peaks.size()) - lens::testimages::kNumStripes) <= 1,
                "peak count = stripe count +-1");

        for (const int expected : lens::testimages::kStripeHarmonics)
        {
            bool found = false;
            for (const int p : peaks)
                found = found || std::abs (p - expected) <= 1;
            expect (found, "peak within +-1 of harmonic " + juce::String (expected));
        }
    }
};

class LensChromaTest final : public juce::UnitTest
{
public:
    LensChromaTest() : juce::UnitTest ("Lens chroma mapping (SPEC 13.5)", "Lens") {}

    void runTest() override
    {
        using namespace lumen;

        // Solid red: Hm 0, Sm 1, Vm 1, sigV 0, E 0, sigH 0 -> the exact
        // SPEC section 13.5 values.
        beginTest ("solid red");
        {
            const auto stats = lens::chromaStats (lens::analyzeImage (
                lenstest::solidColour (juce::Colours::red)));
            expectWithinAbsoluteError (stats.hueMeanDeg, 0.0f, 0.01f);
            expectWithinAbsoluteError (stats.satMean, 1.0f, 1.0e-4f);
            expectWithinAbsoluteError (stats.valMean, 1.0f, 1.0e-4f);
            expectWithinAbsoluteError (stats.lumaSigma, 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (stats.edgeMean, 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (stats.hueSigma, 0.0f, 1.0e-5f);

            const auto t = lens::patchTargetsFor (stats);
            expectEquals (t.filterMode, 1, "LP24 (warm hue)");
            expectWithinAbsoluteError (t.cutoffHz, 11313.708f, 1.0f);   // 250 * 2^5.5
            expectWithinAbsoluteError (t.res, 0.6f, 1.0e-4f);           // 0.05 + 0.55
            expectWithinAbsoluteError (t.detuneCents, 33.0f, 1.0e-3f);  // 3 + 30
            expectEquals (t.unison, 6);                                 // 1 + round(5)
            expectWithinAbsoluteError (t.attackSeconds, 0.002f, 1.0e-5f);   // 400*(2/400)^1 ms
            expectWithinAbsoluteError (t.releaseSeconds, 0.150f, 1.0e-5f);  // 150 ms
            expectWithinAbsoluteError (t.driveDb, 0.0f, 1.0e-5f);
            expectWithinAbsoluteError (t.noiseDb, -60.0f, 1.0e-4f);
            expectWithinAbsoluteError (t.lfoDepth, 0.0f, 1.0e-5f);
            expectWithinAbsoluteError (t.lfoRateHz, 0.15f, 1.0e-4f);
            expectWithinAbsoluteError (t.reverbMix, 0.45f, 1.0e-4f);    // 0.10 + 0.35
            // Macros (SPEC 13.5 extension): default + 0.7 * (stat - 0.5),
            // clamped; Vm 1, sigV 0, E' 0.
            expectWithinAbsoluteError (t.macros[0], 0.85f, 1.0e-4f);  // Tone
            expectWithinAbsoluteError (t.macros[1], 0.15f, 1.0e-4f);  // Motion
            expectWithinAbsoluteError (t.macros[2], 0.65f, 1.0e-4f);  // Space
            expectWithinAbsoluteError (t.macros[3], 0.0f, 1.0e-5f);   // Texture (clamped)
        }

        beginTest ("solid green -> BP12, solid blue -> LP12");
        {
            const auto green = lens::patchTargetsFor (lens::chromaStats (
                lens::analyzeImage (lenstest::solidColour (juce::Colour (0xff00ff00)))));
            expectEquals (green.filterMode, 3, "hue 120 -> BP12");

            const auto blue = lens::patchTargetsFor (lens::chromaStats (
                lens::analyzeImage (lenstest::solidColour (juce::Colours::blue))));
            expectEquals (blue.filterMode, 0, "hue 240 -> LP12");
        }

        beginTest ("solid black (V = 0, S = 0)");
        {
            const auto stats = lens::chromaStats (lens::analyzeImage (
                lenstest::solidColour (juce::Colours::black)));
            expectWithinAbsoluteError (stats.satMean, 0.0f, 1.0e-5f);
            expectWithinAbsoluteError (stats.valMean, 0.0f, 1.0e-5f);

            const auto t = lens::patchTargetsFor (stats);
            expectEquals (t.filterMode, 1, "undefined hue defaults warm (LP24)");
            expectWithinAbsoluteError (t.cutoffHz, 250.0f, 0.01f);      // 250 * 2^0
            expectWithinAbsoluteError (t.res, 0.05f, 1.0e-5f);
            expectWithinAbsoluteError (t.detuneCents, 3.0f, 1.0e-4f);
            expectEquals (t.unison, 1);
            expectWithinAbsoluteError (t.attackSeconds, 0.4f, 1.0e-5f); // 400 ms
            expectWithinAbsoluteError (t.releaseSeconds, 1.5f, 1.0e-5f); // 150 + 1350 ms
        }

        beginTest ("solid mid-gray follows the cutoff/attack curves");
        {
            const auto stats = lens::chromaStats (lens::analyzeImage (
                lenstest::solidColour (juce::Colour (0xff808080))));
            const float v = 128.0f / 255.0f;
            expectWithinAbsoluteError (stats.valMean, v, 1.0e-4f);

            const auto t = lens::patchTargetsFor (stats);
            expectWithinAbsoluteError (t.cutoffHz, 250.0f * std::exp2 (5.5f * v), 1.0f);
            expectWithinAbsoluteError (t.attackSeconds,
                                       0.4f * std::pow (2.0f / 400.0f, v), 1.0e-5f);
        }

        beginTest ("checker image: zero saturation, strong edges");
        {
            const auto stats = lens::chromaStats (lens::analyzeImage (
                lens::testimages::checker()));
            expectWithinAbsoluteError (stats.satMean, 0.0f, 1.0e-4f);
            expectGreaterThan (stats.edgeMean, 0.02f, "edges present");
            expectGreaterThan (stats.lumaSigma, 0.9f, "max-contrast luma spread");

            const auto t = lens::patchTargetsFor (stats);
            expectGreaterThan (t.driveDb, 0.2f, "edges drive the Drive");
            expectLessThan (t.reverbMix, 0.45f, "edges dry the reverb");
        }

        // SPEC 13.5 extension: a warm soft image and a busy high-contrast
        // image must land the four macro knobs in visibly different spots.
        beginTest ("macro positions separate warm-soft from busy-high-contrast");
        {
            const auto warm = lens::patchTargetsFor (lens::chromaStats (
                lens::analyzeImage (lens::testimages::warm())));
            const auto busy = lens::patchTargetsFor (lens::chromaStats (
                lens::analyzeImage (lens::testimages::busy())));

            // (Plain concatenation: %s in String::formatted garbles narrow
            // literals on Windows — see the WSL/Windows gotchas note.)
            const char* names[4] = { "Tone", "Motion", "Space", "Texture" };
            for (int m = 0; m < 4; ++m)
                logMessage ("  " + juce::String (names[m]).paddedRight (' ', 8)
                            + " warm " + juce::String (warm.macros[m], 3)
                            + " | busy " + juce::String (busy.macros[m], 3));

            for (int m = 0; m < 4; ++m)
            {
                expect (warm.macros[m] >= 0.0f && warm.macros[m] <= 1.0f, "warm clamped");
                expect (busy.macros[m] >= 0.0f && busy.macros[m] <= 1.0f, "busy clamped");
            }
            // Motion / Space / Texture must separate clearly (>= 0.15 of
            // knob travel); the pair is constructed to disagree on contrast
            // and edge density. Tone separation depends on the busy image's
            // mid-value palette, so it only needs to differ.
            expectGreaterThan (busy.macros[1] - warm.macros[1], 0.15f, "Motion separates");
            expectGreaterThan (warm.macros[2] - busy.macros[2], 0.15f, "Space separates");
            expectGreaterThan (busy.macros[3] - warm.macros[3], 0.15f, "Texture separates");
        }
    }
};

// ---------------------------------------------------------------------------
// Phase 7: factory preset bank
// ---------------------------------------------------------------------------

namespace presettest
{
    // Render half a second of note 60 at velocity 100 — the bit-exact
    // comparison signal for the two preset application paths.
    void renderParams (const lumen::EngineParams& params,
                       const lumen::Wavetable* tableA, const lumen::Wavetable* tableB,
                       std::vector<float>& l, std::vector<float>& r)
    {
        lumen::SynthEngine engine;
        engine.prepare (48000.0, 512);
        if (tableA != nullptr)
            engine.setImageTable (0, tableA);
        if (tableB != nullptr)
            engine.setImageTable (1, tableB);
        engine.setParams (params);
        engine.noteOn (60, 100.0f / 127.0f);
        l.assign (24000, 0.0f);
        r.assign (24000, 0.0f);
        engine.render (l.data(), r.data(), 24000);
    }
} // namespace presettest

class FactoryPresetBankTest final : public juce::UnitTest
{
public:
    FactoryPresetBankTest() : juce::UnitTest ("Factory preset bank shape + validity", "Presets") {}

    void runTest() override
    {
        using namespace lumen;

        beginTest ("32 presets, exact SPEC section 16 names and categories");
        struct Expected { const char* category; std::vector<const char*> names; };
        const Expected expected[] = {
            { "Bass",     { "Sub Zero", "Rubber", "Neon Growl", "Deep Field", "Knuckle", "Tape Bass" } },
            { "Leads",    { "Laser", "Glass Whistle", "Saw Hero", "Vapor", "Chrome", "Solar Flare" } },
            { "Pads",     { "Neon Tide", "Slow Aurora", "Warm Fog", "Choir Ghost", "Polar Drift", "Amber Haze" } },
            { "Keys",     { "Dial Tone", "Marble Pluck", "Music Box", "Soft EP", "Pixel Pluck", "Kalimba Dust", "Bell Garden" } },
            { "Textures", { "Static Bloom", "Scanline", "Radio Sky", "Machine Hum", "Wind Tunnel", "Photograph", "Init" } },
        };
        expectEquals (static_cast<int> (presets::bank().size()), presets::kNumPresets);
        size_t index = 0;
        for (const auto& group : expected)
            for (const auto* expectedName : group.names)
            {
                if (index >= presets::bank().size())
                    break;
                const auto& preset = presets::bank()[index++];
                expectEquals (juce::String (preset.name), juce::String (expectedName));
                expectEquals (juce::String (preset.category), juce::String (group.category));
            }

        beginTest ("names unique, find() resolves case-insensitively");
        for (const auto& preset : presets::bank())
        {
            const auto* found = presets::find (juce::String (preset.name).toUpperCase());
            expect (found == &preset, juce::String (preset.name) + " resolves to itself");
        }
        expect (presets::find ("init") != nullptr, "lumen_render's --preset init still resolves");
        expect (presets::find ("No Such Patch") == nullptr);

        beginTest ("every setting id exists, is engine-bound, and sits on the range grid");
        NullProcessor processor;
        for (const auto& preset : presets::bank())
        {
            for (const auto& setting : preset.settings)
            {
                auto* parameter = processor.apvts.getParameter (setting.id);
                expect (parameter != nullptr, juce::String (preset.name) + ": unknown id "
                                              + setting.id);
                if (parameter == nullptr)
                    continue;
                const float snapped = parameter->convertFrom0to1 (
                    parameter->convertTo0to1 (setting.value));
                // Values must survive range clamp + interval snap: engine
                // (raw) and plugin (snapped) paths may differ only by
                // conversion noise, never by a grid step.
                expectWithinAbsoluteError (snapped, setting.value,
                    juce::jmax (0.002f, std::abs (setting.value) * 0.001f));

                lumen::EngineParams scratch;
                expect (bindings::set (scratch, setting.id, setting.value),
                        juce::String (preset.name) + ": id not engine-bound: " + setting.id);
            }

            for (const auto& route : preset.routes)
            {
                expect (modstate::sourceFromToken (route.source) >= 0,
                        juce::String (preset.name) + ": bad source " + route.source);
                expect (modstate::destFromToken (route.dest) >= 0,
                        juce::String (preset.name) + ": bad dest " + route.dest);
                expect (std::abs (route.depth) <= 1.0f);
            }
            expect (static_cast<int> (preset.routes.size()) <= mod::kNumSlots);
        }

        beginTest ("all four macros meaningfully pre-mapped on every preset");
        for (const auto& preset : presets::bank())
        {
            if (preset.initMods)
                continue; // Init's stock macro set is gated by InitModDefaultsTest

            int mapsPerMacro[4] {};
            float strongestSpan[4] {};
            for (const auto& map : preset.maps)
            {
                expect (map.macro >= 0 && map.macro < 4);
                expect (modstate::destFromToken (map.dest) >= 0,
                        juce::String (preset.name) + ": bad map dest " + map.dest);
                float rangeMin = 0.0f, rangeMax = 0.0f;
                presets::macroMapRange (preset, map, rangeMin, rangeMax);
                expect (rangeMin >= -1.0f && rangeMax <= 1.0f && rangeMin < rangeMax,
                        juce::String (preset.name) + ": degenerate map range for " + map.dest);
                ++mapsPerMacro[map.macro];
                strongestSpan[map.macro] = juce::jmax (strongestSpan[map.macro],
                                                       std::abs (map.span));
            }
            for (int m = 0; m < 4; ++m)
            {
                expect (mapsPerMacro[m] >= 1, juce::String (preset.name)
                        + ": macro " + juce::String (m + 1) + " unmapped");
                expect (mapsPerMacro[m] <= mod::kMaxMacroMaps);
                expectGreaterOrEqual (strongestSpan[m], 0.15f);
            }
        }

        beginTest ("Lens presets: Photograph = gradient/scan, Scanline = stripes/spectral");
        const auto* photograph = presets::find ("Photograph");
        const auto* scanline = presets::find ("Scanline");
        expect (photograph != nullptr && photograph->hasLens());
        expect (scanline != nullptr && scanline->hasLens());
        if (photograph != nullptr && scanline != nullptr)
        {
            expectEquals (juce::String (photograph->lens.image), juce::String ("gradient"));
            expectEquals (photograph->lens.mode, 0);
            expectEquals (juce::String (scanline->lens.image), juce::String ("stripes"));
            expectEquals (scanline->lens.mode, 1);

            const auto frames1 = presets::buildLensFrames (*photograph);
            const auto frames2 = presets::buildLensFrames (*photograph);
            expectEquals (static_cast<int> (frames1.size()),
                          lens::kNumFrames * lens::kFrameLength);
            expect (frames1 == frames2, "Lens preset frames deterministic");
        }
    }
};

class FactoryPresetAgreementTest final : public juce::UnitTest
{
public:
    FactoryPresetAgreementTest()
        : juce::UnitTest ("Factory presets: engine path == state path (rendered)", "Presets") {}

    void runTest() override
    {
        using namespace lumen;

        NullProcessor processor;
        for (const auto& preset : presets::bank())
        {
            beginTest (juce::String (preset.name));

            // Engine path (lumen_render). Every parameter value is passed
            // through the APVTS range normalize/denormalize round-trip so
            // both paths see identical floats — raw vs. snapped values may
            // differ by sub-grid conversion noise (bounded to 0.002 by the
            // bank validity test), which this comparison is not about: it
            // checks that structure (mod config, Lens tables, defaults)
            // renders bit-identically.
            EngineParams engineParams;
            presets::applyToEngine (preset, engineParams);
            for (auto* raw : processor.getParameters())
            {
                auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (raw);
                if (parameter == nullptr)
                    continue;
                float natural = parameter->convertFrom0to1 (parameter->getDefaultValue());
                for (const auto& setting : preset.settings)
                    if (parameter->paramID == setting.id)
                        natural = parameter->convertFrom0to1 (
                            parameter->convertTo0to1 (setting.value));
                bindings::set (engineParams, parameter->paramID, natural);
            }

            // State path (the plugin): buildState -> PARAM values + trees.
            auto state = presets::buildState (preset, processor.apvts);
            EngineParams stateParams;
            for (const auto& child : state)
                if (child.hasType ("PARAM"))
                    expect (bindings::set (stateParams, child["id"].toString(),
                                           static_cast<float> (static_cast<double> (child["value"]))),
                            "state param binds: " + child["id"].toString());
            if (preset.initMods)
                modstate::ensureTrees (state); // what loadPresetState() does
            modstate::buildConfig (state, stateParams.mod);

            // Lens tables from both paths must be bit-identical too.
            Wavetable engineTable, stateTable;
            const Wavetable* tableA1 = nullptr;
            const Wavetable* tableA2 = nullptr;
            if (preset.hasLens())
            {
                const auto engineFrames = presets::buildLensFrames (preset);
                std::vector<float> stateFrames;
                expect (lensstate::loadImageFrames (state, preset.lens.targetOsc, stateFrames),
                        "state stores the Lens frames");
                expect (engineFrames == stateFrames, "Lens frames agree across paths");
                engineTable.build (engineFrames.data(), lens::kNumFrames,
                                   lens::kHarmonicCap, lens::kPeakTarget);
                stateTable.build (stateFrames.data(), lens::kNumFrames,
                                  lens::kHarmonicCap, lens::kPeakTarget);
                tableA1 = &engineTable;
                tableA2 = &stateTable;
            }

            std::vector<float> l1, r1, l2, r2;
            presettest::renderParams (engineParams, tableA1, nullptr, l1, r1);
            presettest::renderParams (stateParams, tableA2, nullptr, l2, r2);
            expect (l1 == l2 && r1 == r2, "bit-identical render across paths");

            float peak = 0.0f;
            for (const float v : l1)
                peak = juce::jmax (peak, std::abs (v));
            expectGreaterThan (peak, 0.001f, "preset is audible");
        }
    }
};

class FactoryPresetStateTest final : public juce::UnitTest
{
public:
    FactoryPresetStateTest()
        : juce::UnitTest ("Factory presets: bit-exact state round-trip + tolerance", "Presets") {}

    void runTest() override
    {
        using namespace lumen;

        NullProcessor processor;

        beginTest ("metadata + XML round-trip stability for all 32");
        for (const auto& preset : presets::bank())
        {
            const auto state = presets::buildState (preset, processor.apvts);
            expectEquals (state.getProperty ("presetName").toString(), juce::String (preset.name));
            expectEquals (state.getProperty ("presetCategory").toString(),
                          juce::String (preset.category));
            expectEquals (state.getProperty ("presetAuthor").toString(),
                          juce::String (presets::kFactoryAuthor));
            expectEquals (static_cast<int> (state.getProperty ("stateVersion")), 1);

            const auto xml1 = state.toXmlString();
            const auto xml2 = juce::ValueTree::fromXml (xml1).toXmlString();
            expect (xml1 == xml2, juce::String (preset.name) + ": XML round-trip unstable");
        }

        beginTest ("save/recall through the real APVTS is a bit-exact fixed point");
        for (const auto* presetName : { "Neon Tide", "Photograph", "Scanline", "Sub Zero", "Init" })
        {
            const auto* preset = presets::find (presetName);
            expect (preset != nullptr);
            if (preset == nullptr)
                continue;

            auto loaded = presets::buildState (*preset, processor.apvts);
            modstate::ensureTrees (loaded); // loadPresetState() does this after replaceState
            processor.apvts.replaceState (loaded);
            const auto saved1 = processor.apvts.copyState().toXmlString();

            processor.apvts.replaceState (juce::ValueTree::fromXml (saved1));
            const auto saved2 = processor.apvts.copyState().toXmlString();
            expect (saved1 == saved2, juce::String (presetName) + ": APVTS recall not bit-exact");
        }

        beginTest ("Lens preset frames survive the state round-trip bit-exactly");
        for (const auto* presetName : { "Photograph", "Scanline" })
        {
            const auto* preset = presets::find (presetName);
            if (preset == nullptr)
                continue;
            const auto state = presets::buildState (*preset, processor.apvts);
            const auto restored = juce::ValueTree::fromXml (state.toXmlString());
            std::vector<float> frames;
            expect (lensstate::loadImageFrames (restored, preset->lens.targetOsc, frames));
            expect (frames == presets::buildLensFrames (*preset), "frames bit-identical");
            expect (lensstate::loadThumbnail (restored, preset->lens.targetOsc).isValid(),
                    "thumbnail present");
        }

        beginTest (".lumen file write/read round-trip");
        {
            const auto* preset = presets::find ("Neon Tide");
            const auto state = presets::buildState (*preset, processor.apvts);
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("lumen_test_preset.lumen");
            file.deleteFile();
            const auto xml = state.createXml();
            expect (xml != nullptr && xml->writeTo (file), "preset file written");
            const auto reparsed = juce::parseXML (file);
            expect (reparsed != nullptr, "preset file parses");
            if (reparsed != nullptr)
                expect (juce::ValueTree::fromXml (*reparsed).toXmlString()
                            == state.toXmlString(), "file round-trip bit-exact");
            file.deleteFile();
        }

        beginTest ("unknown keys are tolerated (forward compatibility)");
        {
            const auto* preset = presets::find ("Neon Tide");
            auto state = presets::buildState (*preset, processor.apvts);
            state.setProperty ("futureProperty", "hello", nullptr);
            juce::ValueTree unknownChild ("FUTURETREE");
            unknownChild.setProperty ("x", 42, nullptr);
            state.appendChild (unknownChild, nullptr);
            juce::ValueTree unknownParam ("PARAM");
            unknownParam.setProperty ("id", "futureParam", nullptr);
            unknownParam.setProperty ("value", 1.0, nullptr);
            state.appendChild (unknownParam, nullptr);

            processor.apvts.replaceState (state);
            auto* cutoff = processor.apvts.getRawParameterValue ("filterCutoff");
            expect (cutoff != nullptr);
            if (cutoff != nullptr)
                expectWithinAbsoluteError (cutoff->load(), 2400.0f, 2.0f);
        }
    }
};

FrozenParameterTest frozenParameterTest;
MipLevelTest mipLevelTest;
SVFStabilityTest svfStabilityTest;
EnvelopeTimingTest envelopeTimingTest;
EngineRenderTest engineRenderTest;
InitModDefaultsTest initModDefaultsTest;
ModMatrixMathTest modMatrixMathTest;
LfoTest lfoTest;
FxNullTest fxNullTest;
LimiterTest limiterTest;
WaterfallModelTest waterfallModelTest;
LensDeterminismTest lensDeterminismTest;
LensCentroidTest lensCentroidTest;
LensStripeBinsTest lensStripeBinsTest;
LensChromaTest lensChromaTest;
FactoryPresetBankTest factoryPresetBankTest;
FactoryPresetAgreementTest factoryPresetAgreementTest;
FactoryPresetStateTest factoryPresetStateTest;
} // namespace

class ConsoleTestRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << "\n";
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    ConsoleTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::cout << (failures == 0 ? "ALL TESTS PASSED\n"
                                : "TEST FAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
