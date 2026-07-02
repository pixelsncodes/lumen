// lumen_tests — JUCE UnitTest runner (SPEC section 18).
// Phase 1 scope: frozen-parameter contract + test-voice sanity. The DSP test
// suite (mip levels, SVF stability, envelope timing, ...) grows from Phase 2.

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/TestVoice.h"
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

        beginTest ("master gain present after the frozen block");
        auto* gain = processor.apvts.getParameter (lumen::params::masterGain);
        expect (gain != nullptr, "masterGain exists");
    }
};

class TestVoiceRenderTest final : public juce::UnitTest
{
public:
    TestVoiceRenderTest() : juce::UnitTest ("Test voice renders a clean saw", "Engine") {}

    void runTest() override
    {
        beginTest ("1 s render: level, DC, NaNs, release tail");

        constexpr double sampleRate = 48000.0;
        constexpr int totalSamples = 48000;

        juce::Synthesiser synth;
        for (int i = 0; i < 16; ++i)
            synth.addVoice (new lumen::TestVoice());
        synth.addSound (new lumen::TestSound());
        synth.setCurrentPlaybackSampleRate (sampleRate);

        juce::AudioBuffer<float> buffer (2, totalSamples);
        buffer.clear();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 24000); // release ends ~0.7 s

        constexpr int blockSize = 512;
        for (int position = 0; position < totalSamples; position += blockSize)
            synth.renderNextBlock (buffer, midi, position, juce::jmin (blockSize, totalSamples - position));

        int nanCount = 0;
        float peak = 0.0f;
        double sum = 0.0;

        for (int channel = 0; channel < 2; ++channel)
        {
            const float* data = buffer.getReadPointer (channel);
            for (int i = 0; i < totalSamples; ++i)
            {
                if (std::isnan (data[i]) || std::isinf (data[i]))
                    ++nanCount;
                peak = juce::jmax (peak, std::abs (data[i]));
                sum += data[i];
            }
        }

        expectEquals (nanCount, 0);
        expect (peak > 0.05f, "audible output");
        expect (peak < 1.0f, "no clipping");
        expect (std::abs (sum / (2.0 * totalSamples)) < 0.01, "DC offset small");

        const float tailPeak = buffer.getMagnitude (totalSamples - 1000, 1000);
        expect (tailPeak < 1.0e-4f, "silent after release completes");
    }
};

FrozenParameterTest frozenParameterTest;
TestVoiceRenderTest testVoiceRenderTest;
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::cout << (failures == 0 ? "ALL TESTS PASSED\n"
                                : "TEST FAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
