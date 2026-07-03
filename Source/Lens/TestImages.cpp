#include "Lens/TestImages.h"

#include "Lens/LensEngine.h"

#include <cmath>

namespace lumen::lens::testimages
{
namespace
{
    juce::Image blank (int w, int h, juce::Colour fill)
    {
        juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        juce::Graphics g (img);
        g.fillAll (fill);
        return img;
    }
} // namespace

juce::Image gradient()
{
    constexpr int w = 512, h = 64;
    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    const juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < h; ++y)
    {
        const double cycles = y + 1; // harmonic y+1 once scanned
        for (int x = 0; x < w; ++x)
        {
            const double s = 0.5 + 0.45 * std::sin (2.0 * juce::MathConstants<double>::pi
                                                    * cycles * x / w);
            const auto v = static_cast<juce::uint8> (std::lround (s * 255.0));
            data.setPixelColour (x, y, juce::Colour (v, v, v));
        }
    }
    return img;
}

juce::Image stripes()
{
    constexpr int w = 512, h = 512;
    auto img = blank (w, h, juce::Colours::black);
    const juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

    const double logMax = std::log (static_cast<double> (kMaxHarmonics));
    for (const int harmonic : kStripeHarmonics)
    {
        // Same y(h) mapping as spectral analysis (LensEngine.cpp). 3 px
        // thick so the exact harmonic's 3x3 patch is fully covered and the
        // peak is a strict maximum.
        const double yNorm = 1.0 - std::log (static_cast<double> (harmonic)) / logMax;
        const int y = static_cast<int> (std::lround (yNorm * (h - 1)));
        for (int dy = -1; dy <= 1; ++dy)
        {
            const int yy = juce::jlimit (0, h - 1, y + dy);
            for (int x = 0; x < w; ++x)
                data.setPixelColour (x, yy, juce::Colours::white);
        }
    }
    return img;
}

juce::Image checker()
{
    constexpr int w = 512, h = 512, cell = 32;
    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    const juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const bool white = ((x / cell) + (y / cell)) % 2 == 0;
            data.setPixelColour (x, y, white ? juce::Colours::white : juce::Colours::black);
        }
    return img;
}

juce::Image warm()
{
    constexpr int w = 512, h = 384;
    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    const juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

    // Cream-to-rose vertical gradient with a soft "sun" disc: everything
    // varies smoothly, so E stays near zero and Vm stays high.
    const float sunX = w * 0.35f, sunY = h * 0.30f, sunR = w * 0.45f;
    for (int y = 0; y < h; ++y)
    {
        const float ty = static_cast<float> (y) / (h - 1);
        for (int x = 0; x < w; ++x)
        {
            const float dx = (x - sunX) / sunR, dy = (y - sunY) / sunR;
            const float glow = std::exp (-(dx * dx + dy * dy)); // 0..1, smooth
            float r = 0.98f - 0.13f * ty + 0.02f * glow;
            float g = 0.86f - 0.28f * ty + 0.10f * glow;
            float b = 0.72f - 0.34f * ty + 0.12f * glow;
            data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (
                juce::jlimit (0.0f, 1.0f, r), juce::jlimit (0.0f, 1.0f, g),
                juce::jlimit (0.0f, 1.0f, b), 1.0f));
        }
    }
    return img;
}

juce::Image busy()
{
    constexpr int w = 512, h = 384, cell = 24;
    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    const juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

    // Saturated complementary cells picked by a fixed hash, overlaid with
    // hard diagonal stripes: strong edges everywhere, max luma spread.
    const juce::Colour palette[] = {
        juce::Colour (0xffff2222), juce::Colour (0xff00ccff), juce::Colour (0xffffee00),
        juce::Colour (0xff2222dd), juce::Colour (0xff111111), juce::Colour (0xfff5f5f5),
        juce::Colour (0xffff00aa), juce::Colour (0xff22cc44),
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const unsigned cellHash = static_cast<unsigned> (x / cell) * 31u
                                    + static_cast<unsigned> (y / cell) * 17u;
            auto colour = palette[cellHash % 8];
            if (((x + y) / 8) % 4 == 0) // hard diagonal stripes
                colour = colour.contrasting();
            data.setPixelColour (x, y, colour);
        }
    return img;
}

juce::Image byName (const juce::String& name)
{
    if (name == "gradient") return gradient();
    if (name == "stripes")  return stripes();
    if (name == "checker")  return checker();
    if (name == "warm")     return warm();
    if (name == "busy")     return busy();
    return {};
}
} // namespace lumen::lens::testimages
