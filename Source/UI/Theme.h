#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "BinaryData.h"

// SPEC section 14 palette and the embedded Inter typefaces (SIL OFL, license
// embedded alongside the fonts in BinaryData and committed to the repo).
namespace lumen::theme
{
// Panels / wells / hairlines
inline const juce::Colour panel        { 0xff1c1c1f };
inline const juce::Colour well         { 0xff0f0f12 };
inline const juce::Colour hairline     { 0xff2a2a2e };
inline const juce::Colour hairlineLight{ 0xff38383e };

// Text
inline const juce::Colour textPrimary  { 0xffe8e6e3 };
inline const juce::Colour textSecondary{ 0xff9a9aa2 };
inline const juce::Colour textMuted    { 0xff6f6f76 };

// Neon accent trio (recolor pass — user-directed deviation from the SPEC
// section 14 hexes, DECISIONS.md). Each neon is exactly one named constant;
// kAccentRgb is THE yellow — the waterfall derives its entire
// WATERFALL_SPEC section 5 palette from it (Visualizers.h), so retuning any
// neon is a one-liner here.
inline constexpr juce::uint32 kAccentRgb = 0xFAFF00;
inline const juce::Colour neonYellow { 0xff000000 | kAccentRgb };
inline const juce::Colour neonPink   { 0xffff2e9f };
inline const juce::Colour neonCyan   { 0xff19e3e3 };
inline const juce::Colour meterHot   { 0xffff3b30 }; // meter clip/hot zone, >= -3 dBFS

// Accents (module colors)
inline const juce::Colour accentA      = neonPink;    // Osc A, Macro 1
inline const juce::Colour accentB      = neonCyan;    // Osc B, Macro 2
inline const juce::Colour accentFilter = neonYellow;  // filter + FX + meter, Macro 3
inline const juce::Colour accentMod    { 0xffc9c9cf }; // warm gray: mod chips/scope, Macro 4

inline juce::Colour macroAccent (int index)
{
    switch (index)
    {
        case 0:  return accentA;
        case 1:  return accentB;
        case 2:  return accentFilter;
        default: return accentMod;
    }
}

inline juce::Typeface::Ptr regularTypeface()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (
        BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    return tf;
}

inline juce::Typeface::Ptr mediumTypeface()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (
        BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
    return tf;
}

inline juce::Typeface::Ptr semiBoldTypeface()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (
        BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    return tf;
}

// SPEC 14 sizes: 12 / 13 / 15 / 22.
inline juce::Font font (float height)      { return juce::Font (juce::FontOptions (regularTypeface()).withHeight (height)); }
inline juce::Font medium (float height)    { return juce::Font (juce::FontOptions (mediumTypeface()).withHeight (height)); }
inline juce::Font semiBold (float height)  { return juce::Font (juce::FontOptions (semiBoldTypeface()).withHeight (height)); }

// Flat design, 8-10 px corner radius (SPEC 14).
inline constexpr float cornerRadius = 8.0f;
inline constexpr float wellRadius = 6.0f;

// Branded popup-menu palette (preset browser + gear menu share one look).
inline const juce::Colour menuPanel   { 0xff151518 };
inline const juce::Colour menuBorder  { 0xff2a2a2f };
inline const juce::Colour menuItem     { 0xffd8d5cc };
inline const juce::Colour menuHeader   { 0xff6f6d64 };
inline constexpr float menuRadius = 10.0f;

// Draws a single styled string with extra inter-letter tracking (em is the
// font height), e.g. the "L U M E N" wordmark and menu section headers. One
// string, not literal spaces — advances each glyph by its own width so the
// kerning stays even. Text is vertically centred in `area`.
inline void drawTrackedText (juce::Graphics& g, const juce::String& text,
                             juce::Rectangle<int> area, juce::Justification just,
                             float trackingEm)
{
    const auto f = g.getCurrentFont();
    const float tracking = trackingEm * f.getHeight();

    float total = -tracking;
    juce::Array<float> advances;
    for (auto c : text)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, juce::String::charToString (c))
                        + tracking;
        advances.add (w);
        total += w;
    }

    float x = (float) area.getX();
    if (just.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - total * 0.5f;
    else if (just.testFlags (juce::Justification::right))
        x = (float) area.getRight() - total;

    const int baseline = juce::roundToInt (
        area.getY() + (area.getHeight() + f.getAscent() - f.getDescent()) * 0.5f);

    int i = 0;
    for (auto c : text)
    {
        g.drawSingleLineText (juce::String::charToString (c), juce::roundToInt (x), baseline);
        x += advances[i++];
    }
}
} // namespace lumen::theme
