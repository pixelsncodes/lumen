#pragma once

#include "UI/Controls.h"

// Lens UI (SPEC sections 13/14): the image with an animated scanline synced
// to the live morph position of the target oscillator, the Scan/Spectral
// toggle, "Set patch from colors", and the A/B oscillator target selector.
// Images are dropped anywhere on the editor window (PluginEditor forwards
// to LensController); these components only display and configure.

// The image + scanline. `oscProvider` picks which oscillator's image and
// live morph to show (the panel follows the Lens target; the Play-view
// context visualizer follows whichever osc it is showing).
class LensImageView final : public juce::Component
{
public:
    LensImageView (const UiShared& sharedContext, std::function<int()> oscProvider);

    void paint (juce::Graphics& g) override;
    void animate(); // ~60 Hz; repaints only when something moved

private:
    float liveMorph (int osc) const;

    UiShared shared;
    std::function<int()> oscOf;
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
