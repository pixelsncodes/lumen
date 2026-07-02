#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

#include <set>
#include <vector>

class ModKnob;
class AudioHistory;

// Shared context handed to every UI widget: the processor, the live-value
// tap, the editor-owned audio history (drained from the processor's tap
// FIFO), and the registries the editor uses for parameter-coverage checks
// (--check-params) and mod-arc animation ticks.
struct UiShared
{
    LumenAudioProcessor& processor;
    lumen::UiTap& tap;
    const AudioHistory* history;
    std::set<juce::String>& attachedIds;
    std::vector<ModKnob*>& knobs;

    juce::AudioProcessorValueTreeState& apvts() const noexcept { return processor.apvts; }
    juce::ValueTree matrixTree() const { return processor.apvts.state.getChildWithName ("MODMATRIX"); }
    void registerAttachment (const juce::String& paramID) const { attachedIds.insert (paramID); }
};

// Rotary slider implementing the SPEC 14 control contract pieces that live
// at the Slider level: vertical drag, Shift = fine (velocity mode swell),
// mouse-wheel steps, double-click reset, value bubble while dragging, and a
// right-click hook (the owning ModKnob shows the modulation menu).
class KnobSlider final : public juce::Slider
{
public:
    KnobSlider();

    std::function<void()> onContextMenu;

    void mouseDown (const juce::MouseEvent& e) override;
};

// A labelled, modulatable rotary knob: APVTS attachment, animated mod arc
// (static arc = base value, moving indicator = live modulated value, dim
// range arc = reachable span), drag-and-drop target for mod-source chips,
// right-click menu (modulation list / add / edit depth / remove; MIDI Learn
// placeholders until Phase 7).
class ModKnob final : public juce::Component,
                      public juce::DragAndDropTarget
{
public:
    ModKnob (const UiShared& sharedContext, const juce::String& paramID,
             const juce::String& labelText, juce::Colour accentColour);
    ~ModKnob() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;

    // Called ~60 Hz by the editor; repaints only when the live value moved.
    void animate();

    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails& details) override;

    const juce::String& parameterID() const noexcept { return paramId; }

    static constexpr const char* kDragPrefix = "modsrc:";

private:
    struct ModSpan
    {
        float minOffset = 0.0f, maxOffset = 0.0f;
        bool any = false;
    };
    ModSpan computeModSpan() const;
    void showContextMenu();
    void addModulationSlot (const juce::String& sourceToken);
    void resetToDefault();

    UiShared shared;
    juce::String paramId;
    juce::String labelText;
    juce::Colour accent;
    int destIndex = -1; // lumen::mod::Dest, -1 = not modulatable

    KnobSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    bool dragOver = false;
    bool lastHadMod = false;
    float lastLiveNorm = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModKnob)
};

// ComboBox bound to a choice parameter.
class ChoiceCombo final : public juce::Component
{
public:
    ChoiceCombo (const UiShared& sharedContext, const juce::String& paramID);

    void resized() override { box.setBounds (getLocalBounds()); }

    juce::ComboBox box;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Toggle bound to a bool parameter. Style "power" = round enable dot,
// anything else = text chip pill.
class ParamToggle final : public juce::Component
{
public:
    ParamToggle (const UiShared& sharedContext, const juce::String& paramID,
                 const juce::String& text, juce::Colour accentColour,
                 bool powerStyle = false);

    void resized() override { button.setBounds (getLocalBounds()); }

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Small segmented tab strip ("1 2 3", "PLAY DEEP").
class TabsBar final : public juce::Component
{
public:
    TabsBar (const juce::StringArray& labels, std::function<void (int)> onChangeCallback);

    void resized() override;
    void setActive (int index, bool notify);
    int active() const noexcept { return activeIndex; }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::function<void (int)> onChange;
    int activeIndex = 0;
};

// Draggable modulation-source grab handle (SPEC 9 UX). Glows with the live
// source value from the engine tap.
class ModSourceChip final : public juce::Component
{
public:
    ModSourceChip (const UiShared& sharedContext, int sourceIndex,
                   const juce::String& text, juce::Colour accentColour);

    void paint (juce::Graphics& g) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hovered = false; repaint(); }

private:
    UiShared shared;
    int source;
    juce::String text;
    juce::Colour accent;
    bool hovered = false;
    bool dragging = false;
};
