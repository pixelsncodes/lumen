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

juce::Image byName (const juce::String& name)
{
    if (name == "gradient") return gradient();
    if (name == "stripes")  return stripes();
    if (name == "checker")  return checker();
    return {};
}
} // namespace lumen::lens::testimages
