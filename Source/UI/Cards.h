#pragma once

#include <juce_audio_utils/juce_audio_utils.h> // MidiKeyboardComponent

#include "UI/LensPanel.h"
#include "UI/LumenLookAndFeel.h"
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

// The Lens card (SPEC 13/14): image + scanline, Scan/Spectral toggle,
// "Set patch from colors", A/B target selector.
class LensCard final : public CardPanel
{
public:
    explicit LensCard (const UiShared& shared);

    void resized() override;
    void animate();

private:
    LensPanel panel;
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

// Small header glyph button (gear / minimize / close). Close-hover goes red.
class HeaderIconButton final : public juce::Button
{
public:
    enum class Glyph { gear, minimize, close };
    HeaderIconButton (Glyph glyphToDraw, const juce::String& tip);

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    Glyph glyph;
};

// Header: wordmark | preset strip (name + browser popup + < >) | Play/Deep |
// gear | master + meter | window controls. The strip reads the live preset
// name off the state tree each animate tick, so DAW recall and factory loads
// stay in sync. In the standalone build the header IS the title bar: the gear
// opens the audio/MIDI settings, minimize/close sit past the meter, and
// dragging an empty region moves the window. In the plugin build the host owns
// the frame — no window controls, and the gear opens a minimal settings menu.
class HeaderBar final : public juce::Component
{
public:
    HeaderBar (const UiShared& shared, std::function<void (int)> onViewChange,
               bool standaloneChrome, std::function<void()> onOpenSettings);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void animate();
    void setViewIndex (int index) { viewTabs.setActive (index, false); }

    // Public for the --screenshot harness (SPEC 18): open the branded menus.
    void showBrowserMenu();
    void showGearMenu();

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void showSaveDialog();

    LumenAudioProcessor& processor;
    bool standalone;
    std::function<void()> openSettings;
    LumenMenuLookAndFeel menuLnf;

    TabsBar viewTabs;
    juce::TextButton presetPrev { "<" }, presetNext { ">" };
    juce::TextButton presetName;
    juce::String shownName;
    HeaderIconButton gear { HeaderIconButton::Glyph::gear, "Settings" };
    ModKnob master;
    MeterView meter;
    HeaderIconButton minimizeButton { HeaderIconButton::Glyph::minimize, "Minimize" };
    HeaderIconButton closeButton { HeaderIconButton::Glyph::close, "Close" };
    juce::ComponentDragger windowDragger;
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

// 2-octave mouse keyboard with flat sharp keys: JUCE's default drawBlackNote
// paints the key face with colour.brighter(), which caps how dark a sharp
// can render — this fills the exact blackNoteColourId and applies the
// down/over overlays flat (recolor pass: sharps #3A3A3A, pressed = kAccent).
class FlatKeyboard final : public juce::MidiKeyboardComponent
{
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;

    void drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override
    {
        g.setColour (noteFillColour);
        g.fillRect (area);

        if (isDown)
            g.setColour (findColour (keyDownOverlayColourId));
        else if (isOver)
            g.setColour (findColour (mouseOverKeyOverlayColourId));
        else
            return;
        g.fillRect (area);
    }
};

// Play view: the permanent waterfall scene (grid + spectral surface,
// WATERFALL_SPEC.md — idle shows the drained flat surface, no context
// switching), Lens drop zone, big macros, 2-octave mouse keyboard (SPEC 14).
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
    WaterfallView waterfall;
    LensPanel lensPanel; // the drop zone is the real Lens panel
    juce::OwnedArray<ModKnob> macroKnobs;
    FlatKeyboard keyboard;
};
