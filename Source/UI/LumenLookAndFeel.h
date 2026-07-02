#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// SPEC section 14 look: flat, OP-1 Field-inspired but original. Dark panels,
// hairline strokes, per-module accent colors, Inter embedded via BinaryData.
class LumenLookAndFeel final : public juce::LookAndFeel_V4
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
