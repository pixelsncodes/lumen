#include "UI/LumenLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h> // MidiKeyboardComponent colour ids

#include "UI/Theme.h"

using namespace lumen;

LumenLookAndFeel::LumenLookAndFeel()
{
    setDefaultSansSerifTypeface (theme::regularTypeface());

    setColour (juce::ResizableWindow::backgroundColourId, theme::well);
    setColour (juce::Label::textColourId, theme::textSecondary);

    setColour (juce::ComboBox::backgroundColourId, theme::well);
    setColour (juce::ComboBox::textColourId, theme::textPrimary);
    setColour (juce::ComboBox::outlineColourId, theme::hairline);
    setColour (juce::ComboBox::arrowColourId, theme::textMuted);

    setColour (juce::PopupMenu::backgroundColourId, theme::panel);
    setColour (juce::PopupMenu::textColourId, theme::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::hairlineLight);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::textPrimary);
    setColour (juce::PopupMenu::headerTextColourId, theme::textMuted);

    setColour (juce::TextButton::buttonColourId, theme::well);
    setColour (juce::TextButton::buttonOnColourId, theme::hairlineLight);
    setColour (juce::TextButton::textColourOffId, theme::textSecondary);
    setColour (juce::TextButton::textColourOnId, theme::textPrimary);

    setColour (juce::Slider::textBoxTextColourId, theme::textPrimary);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour (juce::TooltipWindow::backgroundColourId, theme::panel);
    setColour (juce::TooltipWindow::textColourId, theme::textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, theme::hairline);

    setColour (juce::BubbleComponent::backgroundColourId, theme::panel);
    setColour (juce::BubbleComponent::outlineColourId, theme::hairlineLight);

    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffc6c6c6));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff3a3a3a));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, theme::hairline);
    // Pressed/hovered keys light neon yellow from the one kAccent source —
    // the intentional exception to the pink rule (recolor pass, DECISIONS.md).
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, theme::neonYellow.withAlpha (0.25f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, theme::neonYellow.withAlpha (0.8f));
    setColour (juce::MidiKeyboardComponent::textLabelColourId, theme::textMuted);
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);

    setColour (juce::ScrollBar::thumbColourId, theme::hairlineLight);
}

juce::Font LumenLookAndFeel::getLabelFont (juce::Label& label)
{
    return theme::font (juce::jmin (13.0f, (float) label.getHeight()));
}

juce::Font LumenLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return theme::font (juce::jmin (12.0f, (float) box.getHeight() * 0.72f));
}

juce::Font LumenLookAndFeel::getPopupMenuFont()
{
    return theme::font (13.0f);
}

juce::Font LumenLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return theme::medium (juce::jmin (12.0f, (float) buttonHeight * 0.65f));
}

juce::Font LumenLookAndFeel::getSliderPopupFont (juce::Slider&)
{
    return theme::font (12.0f);
}

int LumenLookAndFeel::getSliderPopupPlacement (juce::Slider&)
{
    return juce::BubbleComponent::above;
}

void LumenLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = size * 0.5f - 2.0f;
    const float stroke = juce::jmax (1.6f, size * 0.055f);
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const bool enabled = slider.isEnabled();

    // Track ring
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (theme::hairlineLight);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc: bipolar-style sliders fill from centre, others from the start.
    const bool fromCentre = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float arcFrom = fromCentre
        ? rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)
        : rotaryStartAngle;
    juce::Path arc;
    if (std::abs (angle - arcFrom) > 0.01f)
    {
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                           juce::jmin (arcFrom, angle), juce::jmax (arcFrom, angle), true);
        g.setColour (enabled ? accent : accent.withAlpha (0.35f));
        g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // Body + needle
    const float bodyRadius = radius - stroke * 1.6f;
    g.setColour (theme::panel.brighter (0.06f));
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (theme::hairline);
    g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

    const auto needleEnd = centre.getPointOnCircumference (bodyRadius - 1.5f, angle);
    const auto needleStart = centre.getPointOnCircumference (bodyRadius * 0.35f, angle);
    g.setColour (enabled ? theme::textPrimary : theme::textMuted);
    g.drawLine ({ needleStart, needleEnd }, juce::jmax (1.5f, stroke * 0.8f));
}

void LumenLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (theme::well);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? theme::hairlineLight : theme::hairline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 11.0f;
    const float ay = (float) height * 0.5f - 1.5f;
    arrow.addTriangle (ax - 3.5f, ay, ax + 3.5f, ay, ax, ay + 4.0f);
    g.setColour (theme::textMuted);
    g.fillPath (arrow);
}

void LumenLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 20, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LumenLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (theme::panel);
    g.setColour (theme::hairlineLight);
    g.drawRect (0, 0, width, height);
}

void LumenLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool)
{
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const bool on = button.getToggleState();
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);

    if (button.getComponentID() == "power")
    {
        // Small power dot: accent ring, filled when enabled.
        const float d = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 2.0f;
        const auto circle = juce::Rectangle<float> (d, d).withCentre (bounds.getCentre());
        g.setColour (on ? accent : theme::hairlineLight);
        g.drawEllipse (circle, 1.5f);
        if (on)
        {
            g.setColour (accent);
            g.fillEllipse (circle.reduced (d * 0.28f));
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour (theme::textMuted);
            g.fillEllipse (circle.reduced (d * 0.34f));
        }
        return;
    }

    // Text chip (RND / SYNC / PP / ...): pill with the label inside.
    g.setColour (on ? accent.withAlpha (0.22f) : theme::well);
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    g.setColour (on ? accent : theme::hairline);
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);
    g.setColour (on ? theme::textPrimary : (shouldDrawButtonAsHighlighted ? theme::textSecondary : theme::textMuted));
    g.setFont (theme::medium (juce::jmin (11.0f, bounds.getHeight() * 0.62f)));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

void LumenLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour&,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    juce::Colour fill = on ? theme::hairlineLight : theme::well;
    if (shouldDrawButtonAsDown)
        fill = fill.brighter (0.1f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.05f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (on ? theme::textMuted : theme::hairline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

void LumenLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&,
                                   const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (theme::hairlineLight);
    g.drawRoundedRectangle (body, 4.0f, 1.0f);
}

void LumenLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool)
{
    g.setColour (isMouseOver ? theme::textMuted : theme::hairlineLight);
    for (int i = 1; i <= 3; ++i)
    {
        const float off = (float) i * 4.0f;
        g.drawLine ((float) w - off, (float) h - 1.0f, (float) w - 1.0f, (float) h - off, 1.2f);
    }
}

// ---------------------------------------------------------------------------
// LumenMenuLookAndFeel — branded popup menus (preset browser + gear menu)
// ---------------------------------------------------------------------------

LumenMenuLookAndFeel::LumenMenuLookAndFeel()
{
    // A non-opaque background colour makes PopupMenu's desktop window
    // per-pixel transparent (MenuWindow calls setOpaque(colour.isOpaque())),
    // so the corners outside the rounded panel show whatever is behind the
    // menu instead of an opaque white fill.
    setColour (juce::PopupMenu::backgroundColourId, juce::Colours::transparentBlack);
}

void LumenMenuLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (theme::menuPanel);
    g.fillRoundedRectangle (bounds, theme::menuRadius);
    g.setColour (theme::menuBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme::menuRadius, 1.0f);
}

int LumenMenuLookAndFeel::getPopupMenuBorderSize()
{
    return 8; // keeps items clear of the rounded corners
}

void LumenMenuLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                      int standardMenuItemHeight,
                                                      int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 60;
        idealHeight = 11;
        return;
    }

    auto font = getPopupMenuFont();
    idealHeight = standardMenuItemHeight > 0 ? juce::jmax (standardMenuItemHeight, 26)
                                             : juce::roundToInt (font.getHeight() * 1.9f);
    idealWidth = juce::GlyphArrangement::getStringWidthInt (font, text) + idealHeight + 40;
}

void LumenMenuLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& text,
                                              const juce::String& shortcutKeyText,
                                              const juce::Drawable*, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        auto r = area.reduced (10, 0);
        g.setColour (theme::menuBorder);
        g.fillRect (r.withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.reduced (4, 1);

    if (isHighlighted && isActive)
    {
        g.setColour (theme::neonYellow.withAlpha (0.10f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
    }

    auto contentColour = isTicked ? theme::neonYellow
                                  : (textColour != nullptr ? *textColour : theme::menuItem);
    if (! isActive)
        contentColour = contentColour.withMultipliedAlpha (0.4f);

    r.reduce (6, 0);
    auto tickArea = r.removeFromLeft (18);
    if (isTicked)
    {
        juce::Path tick;
        const auto t = tickArea.toFloat().reduced (tickArea.getWidth() * 0.28f,
                                                   tickArea.getHeight() * 0.34f);
        tick.startNewSubPath (t.getX(), t.getCentreY() + t.getHeight() * 0.05f);
        tick.lineTo (t.getCentreX() - t.getWidth() * 0.12f, t.getBottom());
        tick.lineTo (t.getRight(), t.getY());
        g.setColour (theme::neonYellow);
        g.strokePath (tick, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    if (hasSubMenu)
    {
        const float arrowH = 0.55f * getPopupMenuFont().getAscent();
        const float x = (float) r.removeFromRight ((int) arrowH).getX();
        const float halfH = (float) r.getCentreY();
        juce::Path arrow;
        arrow.startNewSubPath (x, halfH - arrowH * 0.5f);
        arrow.lineTo (x + arrowH * 0.55f, halfH);
        arrow.lineTo (x, halfH + arrowH * 0.5f);
        g.setColour (contentColour);
        g.strokePath (arrow, juce::PathStrokeType (1.6f));
    }

    g.setColour (contentColour);
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, r, juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (theme::menuHeader);
        g.setFont (theme::font (11.0f));
        g.drawText (shortcutKeyText, r, juce::Justification::centredRight, true);
    }
}

void LumenMenuLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g,
                                                       const juce::Rectangle<int>& area,
                                                       const juce::String& sectionName)
{
    g.setColour (theme::menuHeader);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              10.0f, juce::Font::plain)));
    theme::drawTrackedText (g, sectionName.toUpperCase(),
                            area.reduced (12, 0).withTrimmedTop (4),
                            juce::Justification::centredLeft, 0.22f);
}
