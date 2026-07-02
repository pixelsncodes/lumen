// lumen_tests — JUCE UnitTest runner (SPEC section 18).
// Phase 2 suite: frozen-parameter contract, wavetable mip correctness, SVF
// stability under audio-rate modulation, envelope timing, engine sanity and
// voice stealing.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Engine/Envelope.h"
#include "Engine/FactoryTables.h"
#include "Engine/SVF.h"
#include "Engine/SynthEngine.h"
#include "Engine/Wavetable.h"
#include "State/Parameters.h"

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

        beginTest ("Phase 2 parameters present after the frozen block");
        for (auto* id : { lumen::params::masterGain, lumen::params::oscAMorph,
                          lumen::params::filterMode, lumen::params::env1Attack,
                          lumen::params::oscBUnison, lumen::params::noiseLevel })
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
        // Blow-up guard, not a loudness spec: 16 unity-ish voices may sum
        // loud pre-limiter (the limiter arrives in Phase 4).
        expect (peak17 < 8.0f, "16-voice pileup stays bounded pre-limiter");
        expectEquals (engine.activeVoiceCount(), 0);
    }
};

FrozenParameterTest frozenParameterTest;
MipLevelTest mipLevelTest;
SVFStabilityTest svfStabilityTest;
EnvelopeTimingTest envelopeTimingTest;
EngineRenderTest engineRenderTest;
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
