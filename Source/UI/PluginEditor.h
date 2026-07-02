#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

#include <array>

// Phase 3 placeholder editor: frozen first-8 + master gain knobs, plus the
// temporary mod-matrix list panel (PHASES.md Phase 3 — the drag-drop
// modulation UX arrives with the real Play/Deep views in Phase 5).
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

    // One editable matrix slot row, bound directly to its SLOT ValueTree.
    struct MatrixRow final : public juce::Component
    {
        MatrixRow (juce::ValueTree slotTree, const juce::StringArray& destNames);
        void resized() override;

        juce::ValueTree slot;
        juce::ComboBox source, dest;
        juce::Slider depth;
        juce::ToggleButton enabled;
    };

    void setUpKnob (Knob& knob, const char* paramID, const juce::String& text, juce::Colour accent);
    void layoutKnobRow (juce::Rectangle<int> row, Knob* knobs, int count, float scale);

    LumenAudioProcessor& processor;
    std::array<Knob, 4> macroKnobs;
    std::array<Knob, 5> utilityKnobs;

    juce::Viewport matrixViewport;
    juce::Component matrixContent;
    juce::OwnedArray<MatrixRow> matrixRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenAudioProcessorEditor)
};
