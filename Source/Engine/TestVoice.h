#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace lumen
{
// Temporary Phase 1 test voice: polyBLEP band-limited saw with a fixed
// 5 ms attack / 200 ms release envelope. Replaced by the wavetable engine
// in Phase 2. Engine code: no GUI includes, no allocation while rendering.

struct TestSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class TestVoice final : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<TestSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        level    = 0.2f * (0.3f + 0.7f * velocity);
        phase    = 0.0;
        phaseInc = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber) / getSampleRate();

        juce::ADSR::Parameters envParams;
        envParams.attack  = 0.005f;
        envParams.decay   = 0.0f;
        envParams.sustain = 1.0f;
        envParams.release = 0.2f;

        adsr.setSampleRate (getSampleRate());
        adsr.setParameters (envParams);
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! adsr.isActive())
            return;

        auto* left  = output.getWritePointer (0, startSample);
        auto* right = output.getNumChannels() > 1 ? output.getWritePointer (1, startSample) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = nextSawSample() * level * adsr.getNextSample();

            left[i] += sample;
            if (right != nullptr)
                right[i] += sample;

            if (! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

    using juce::SynthesiserVoice::renderNextBlock; // keep double-precision overload visible

private:
    float nextSawSample()
    {
        const auto t  = (float) phase;
        const auto dt = (float) phaseInc;

        float value = 2.0f * t - 1.0f;
        value -= polyBlep (t, dt);

        phase += phaseInc;
        if (phase >= 1.0)
            phase -= 1.0;

        return value;
    }

    // Smooths the saw's wrap discontinuity over +-1 sample: band-limited enough
    // for a temporary voice (the real mip-mapped wavetables arrive in Phase 2).
    static float polyBlep (float t, float dt)
    {
        if (dt <= 0.0f)
            return 0.0f;

        if (t < dt)
        {
            const float x = t / dt;
            return x + x - x * x - 1.0f;
        }

        if (t > 1.0f - dt)
        {
            const float x = (t - 1.0f) / dt;
            return x * x + x + x + 1.0f;
        }

        return 0.0f;
    }

    double phase = 0.0;
    double phaseInc = 0.0;
    float level = 0.0f;
    juce::ADSR adsr;
};
} // namespace lumen
