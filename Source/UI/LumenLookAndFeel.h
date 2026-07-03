#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// SPEC section 14 look: flat, OP-1 Field-inspired but original. Dark panels,
// hairline strokes, per-module accent colors, Inter embedded via BinaryData.
class LumenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LumenLookAndFeel();

    juce::Font getLabelFont (juce::Label& label) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    int getSliderPopupPlacement (juce::Slider&) override;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawBubble (juce::Graphics& g, juce::BubbleComponent& bubble,
                     const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;

    void drawCornerResizer (juce::Graphics& g, int w, int h,
                            bool isMouseOver, bool isMouseDragging) override;

    // Rotary sweep used everywhere: -135 deg .. +135 deg around 12 o'clock.
    static constexpr float rotaryStart = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float rotaryEnd   = juce::MathConstants<float>::pi * 2.75f;
};

// Branded PopupMenu look, reused by the preset browser and the gear menu:
// rounded #151518 panel with a #2a2a2f border, uppercase mono section
// headers, #d8d5cc items, a neon-yellow hover row + ticked current item, and
// hairline separators. Inherits every other control style from LumenLookAndFeel
// so a menu shown with it set still matches the rest of the plugin.
class LumenMenuLookAndFeel final : public LumenLookAndFeel
{
public:
    LumenMenuLookAndFeel();

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;
    int getPopupMenuBorderSize() override;

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override;
};
