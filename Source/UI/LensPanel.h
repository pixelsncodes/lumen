#pragma once

#include "UI/Controls.h"
#include "UI/Theme.h"

#include <cstdint>

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

    // Melody sampling-grid overlay, drawn over the Lens image while the Melody
    // panel is active; the sounding-cell glow fades over ~150 ms.
    bool lastMelodyActive = false;
    std::uint32_t melodyLastTrigger = 0;
    float melodyGlow = 0.0f;
    int melodyGlowCol = -1, melodyGlowRow = -1;
};

// A small square icon toggle used for the COLORS and MELODY chips, so they stop
// eating the horizontal room the SCAN/SPECTRAL tabs need. Draws a vector glyph
// (a colour-swatch palette or an eighth note) in a rounded well; tints with an
// accent colour when toggled on.
class LensIconToggle final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    enum class Glyph { colors, melody, playStop };
    LensIconToggle (Glyph glyphToDraw, juce::Colour onColour, juce::String tip);

    std::function<void()> onClick;

    bool getToggleState() const noexcept { return on; }
    void setToggleState (bool shouldBeOn) { if (on != shouldBeOn) { on = shouldBeOn; repaint(); } }

    // Greyed, non-clickable state (used by the play chip before a melody exists).
    bool chipEnabled() const noexcept { return enabledFlag; }
    void setChipEnabled (bool e) { if (enabledFlag != e) { enabledFlag = e; repaint(); } }

    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
    void mouseUp    (const juce::MouseEvent& e) override;

private:
    Glyph glyph;
    juce::Colour accent;
    bool on = false;
    bool hovered = false;
    bool enabledFlag = true;
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
    // Colors are now always applied (no toggle); this chip drives melody playback:
    // play/stop, greyed until a melody has been generated and the panel is closed.
    LensIconToggle playChip { LensIconToggle::Glyph::playStop, lumen::theme::neonYellow,
                              "Play / stop the generated melody" };
    LensIconToggle melodyChip { LensIconToggle::Glyph::melody, lumen::theme::neonYellow,
                               "MELODY: turn this image into a playable melody" };
};
