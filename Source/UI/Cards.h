#pragma once

#include <juce_audio_utils/juce_audio_utils.h> // MidiKeyboardComponent

#include "UI/Visualizers.h"

// SPEC section 14 Deep/Play view building blocks. The whole UI is laid out
// at the 1040x660 base size (the editor scales the content component with a
// transform for 70-200% resizing), so all layout code uses fixed pixels.

// Flat panel with a title strip (SPEC 14: flat, 8-10 px radius, hairlines).
class CardPanel : public juce::Component
{
public:
    CardPanel (juce::String titleText, juce::Colour accentColour);

    void paint (juce::Graphics& g) override;

protected:
    juce::String title;
    juce::Colour accent;
};

// Oscillator card: power, table select, 3D stack, morph + 8 more knobs,
// phase-randomize chip. Identical for A and B (prefix + accent).
class OscCard final : public CardPanel
{
public:
    OscCard (const UiShared& shared, const juce::String& prefix,
             const juce::String& name, juce::Colour accent);

    void resized() override;
    void animate();

private:
    ParamToggle power;
    ChoiceCombo table;
    ParamToggle phaseRandom;
    WavetableStackView stack;
    juce::OwnedArray<ModKnob> knobs;
};

class SubNoiseCard final : public CardPanel
{
public:
    explicit SubNoiseCard (const UiShared& shared);

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    ChoiceCombo subWave, subOctave, noiseType;
    ModKnob subLevel, noiseLevel;
};

class FilterCard final : public CardPanel
{
public:
    explicit FilterCard (const UiShared& shared);

    void resized() override;
    void animate (bool fftTick);

private:
    ChoiceCombo mode;
    SpectrumView spectrum;
    juce::OwnedArray<ModKnob> knobs;
};

// Four fixed strips: Drive, Chorus, Delay, Reverb (SPEC 10 order).
class FxRack final : public CardPanel
{
public:
    explicit FxRack (const UiShared& shared);

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct Strip
    {
        juce::String name;
        std::unique_ptr<ParamToggle> power;
        juce::OwnedArray<juce::Component> controls; // knobs/combos/chips in order
        std::vector<int> widths;                    // layout width per control
    };
    Strip strips[4];
};

class EnvCard final : public CardPanel
{
public:
    explicit EnvCard (const UiShared& shared);

    void resized() override;
    void animate();
    void setActiveEnv (int index);

private:
    TabsBar tabs;
    juce::OwnedArray<EnvelopeEditor> editors;      // 3
    juce::OwnedArray<juce::OwnedArray<ModKnob>> knobSets; // 3 x (A D S R Curve)
    int active = 0;
};

class LfoCard final : public CardPanel
{
public:
    explicit LfoCard (const UiShared& shared);

    void resized() override;
    void animate();
    void setActiveLfo (int index);

private:
    struct LfoControls
    {
        std::unique_ptr<LfoView> view;
        std::unique_ptr<ChoiceCombo> shape, mode, syncDiv;
        std::unique_ptr<ParamToggle> sync;
        juce::OwnedArray<ModKnob> knobs; // rate, phase, fade
    };
    TabsBar tabs;
    LfoControls sets[3];
    int active = 0;
};

// Placeholder until the Lens engine lands in Phase 6 (DECISIONS.md).
class LensCard final : public CardPanel
{
public:
    LensCard();
    void paint (juce::Graphics& g) override;
};

// Deep-view footer: macros 1-4, mod-source grab handles, scope strip.
class FooterBar final : public juce::Component
{
public:
    FooterBar (const UiShared& shared, const AudioHistory& history);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void animate();

private:
    juce::OwnedArray<ModKnob> macroKnobs;
    juce::OwnedArray<ModSourceChip> chips;
    ScopeView scope;
};

// Header: logo | preset strip (Phase 7 placeholder) | Play/Deep | master + meter.
class HeaderBar final : public juce::Component
{
public:
    HeaderBar (const UiShared& shared, std::function<void (int)> onViewChange);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void animate();
    void setViewIndex (int index) { viewTabs.setActive (index, false); }

private:
    TabsBar viewTabs;
    juce::TextButton presetPrev { "<" }, presetNext { ">" };
    ModKnob master;
    MeterView meter;
};

// Deep view: the SPEC 14 card grid + footer.
class DeepView final : public juce::Component
{
public:
    DeepView (const UiShared& shared, const AudioHistory& history);

    void resized() override;
    void animate (bool fftTick);

private:
    OscCard oscA, oscB;
    SubNoiseCard subNoise;
    FilterCard filter;
    FxRack fx;
    EnvCard env;
    LfoCard lfo;
    LensCard lens;
    FooterBar footer;
};

// Play view: one large context visualizer, Lens drop zone, big macros,
// 2-octave mouse keyboard (SPEC 14).
class PlayView final : public juce::Component
{
public:
    PlayView (const UiShared& shared, const AudioHistory& history,
              juce::MidiKeyboardState& keyboardState);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void animate();

private:
    UiShared shared;
    WavetableStackView stackA, stackB;
    ScopeView scope;
    juce::OwnedArray<ModKnob> macroKnobs;
    juce::MidiKeyboardComponent keyboard;
    juce::Rectangle<int> lensZone;
};
