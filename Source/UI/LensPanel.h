#pragma once

#include "UI/Controls.h"

// Lens UI (SPEC sections 13/14): the image with an animated scanline synced
// to the live morph position of the target oscillator, the Scan/Spectral
// toggle, "Set patch from colors", and the A/B oscillator target selector.
// Images are dropped anywhere on the editor window (PluginEditor forwards
// to LensController); these components only display and configure.

// The image + scanline, plus the load/replace/remove affordances. Empty =
// dashed drop zone with a "+" (click opens a native file chooser; dropping
// anywhere on the window still works). Loaded = "x" removes the image,
// the replace icon or a click on the thumbnail opens the chooser.
// `oscProvider` picks which oscillator's image and live morph to show
// (the panel follows the Lens target).
class LensImageView final : public juce::Component
{
public:
    LensImageView (const UiShared& sharedContext, std::function<int()> oscProvider);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void animate(); // ~60 Hz; repaints only when something moved

    void mouseUp (const juce::MouseEvent& e) override;

private:
    // Tiny vector icon button ("x" / replace arrows) for the loaded state.
    class IconButton final : public juce::Component,
                             public juce::SettableTooltipClient
    {
    public:
        enum class Glyph { remove, replace };
        IconButton (Glyph glyphToDraw, juce::String tip);

        std::function<void()> onClick;

        void paint (juce::Graphics& g) override;
        void mouseEnter (const juce::MouseEvent&) override { hovered = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hovered = false; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        Glyph glyph;
        bool hovered = false;
    };

    float liveMorph (int osc) const;
    bool hasImage() const;
    void openChooser();

    UiShared shared;
    std::function<int()> oscOf;
    IconButton removeButton { IconButton::Glyph::remove, "Remove image" };
    IconButton replaceButton { IconButton::Glyph::replace, "Replace image" };
    std::unique_ptr<juce::FileChooser> chooser;
    float lastMorph = -1.0f;
    int lastVersion = -1;
    int lastMode = -1;
    int lastOsc = -1;
};

// Image view + the three Lens controls. `compact` = the Deep-view card
// interior (image left, controls right); otherwise the Play-view panel
// (image on top, controls below, with its own frame + title).
class LensPanel final : public juce::Component
{
public:
    LensPanel (const UiShared& sharedContext, bool compactLayout);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void animate();

private:
    UiShared shared;
    bool compact;
    LensImageView image;
    TabsBar modeTabs, targetTabs;
    juce::TextButton colorsChip { "COLORS" };
};
