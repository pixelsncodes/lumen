#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

#include <array>

// Phase 1 placeholder editor: the frozen first-8 parameters plus master gain
// as labelled knobs on the SPEC section 14 palette. The real Play/Deep views
// arrive in Phase 5.
class LumenAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit LumenAudioProcessorEditor (LumenAudioProcessor& processorToUse);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setUpKnob (Knob& knob, const char* paramID, const juce::String& text, juce::Colour accent);
    void layoutKnobRow (juce::Rectangle<int> row, Knob* knobs, int count, float scale);

    LumenAudioProcessor& processor;
    std::array<Knob, 4> macroKnobs;
    std::array<Knob, 5> utilityKnobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenAudioProcessorEditor)
};
