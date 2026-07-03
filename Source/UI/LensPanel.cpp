#include "UI/LensPanel.h"

#include "Engine/ModDestinations.h"
#include "Lens/LensController.h"
#include "State/ModState.h"
#include "UI/Theme.h"

using namespace lumen;

namespace
{
    bool destHasModulation (const mod::Config* config, int dest)
    {
        if (config == nullptr || dest < 0)
            return false;
        for (const auto& slot : config->slots)
            if (slot.enabled && slot.dest == dest)
                return true;
        for (const auto& maps : config->macroMaps)
            for (const auto& map : maps)
                if (map.dest == dest)
                    return true;
        return false;
    }

    juce::Colour oscAccent (int osc)
    {
        return osc == 1 ? theme::accentB : theme::accentA;
    }
} // namespace

// ---------------------------------------------------------------------------
// LensImageView
// ---------------------------------------------------------------------------

LensImageView::LensImageView (const UiShared& sharedContext, std::function<int()> oscProvider)
    : shared (sharedContext), oscOf (std::move (oscProvider))
{
    setInterceptsMouseClicks (false, false);
}

float LensImageView::liveMorph (int osc) const
{
    // Base knob value normally; the live modulated value once the morph is a
    // modulation target (same rule as WavetableStackView). Morph is 0..1
    // linear, so the normalized tap value IS the morph.
    const int dest = static_cast<int> (osc == 1 ? mod::Dest::oscBMorph : mod::Dest::oscAMorph);
    if (destHasModulation (shared.processor.currentModConfig(), dest))
        return shared.tap.destNorm[dest].load (std::memory_order_relaxed);

    auto* value = shared.apvts().getRawParameterValue (osc == 1 ? "oscBMorph" : "oscAMorph");
    return value != nullptr ? value->load() : 0.0f;
}

void LensImageView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::well);
    g.fillRoundedRectangle (bounds, theme::wellRadius);
    g.setColour (theme::hairline);
    g.drawRoundedRectangle (bounds, theme::wellRadius, 1.0f);

    auto& lens = shared.processor.lensController();
    const int osc = oscOf();
    const auto img = lens.displayImage (osc);

    if (! img.isValid())
    {
        g.setColour (theme::textMuted);
        g.setFont (theme::font (12.0f));
        g.drawText ("drop an image to build a tone",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    // Fit the image, aspect preserved, centred in the well.
    const auto inner = bounds.reduced (5.0f);
    const float scale = juce::jmin (inner.getWidth() / static_cast<float> (img.getWidth()),
                                    inner.getHeight() / static_cast<float> (img.getHeight()));
    const float w = img.getWidth() * scale, h = img.getHeight() * scale;
    const juce::Rectangle<float> dest (inner.getCentreX() - w * 0.5f,
                                       inner.getCentreY() - h * 0.5f, w, h);
    g.drawImage (img, dest);
    g.setColour (theme::hairlineLight);
    g.drawRect (dest, 1.0f);

    // Scanline synced to the live morph (SPEC 13): Scan mode travels down
    // the image, Spectral mode travels across the columns. Coloured with
    // the target oscillator's accent.
    const float morph = juce::jlimit (0.0f, 1.0f, liveMorph (osc));
    const auto accent = oscAccent (osc);
    const bool scanMode = lens.mode() == 0;

    if (scanMode)
    {
        const float y = dest.getY() + morph * (dest.getHeight() - 1.0f);
        g.setColour (accent.withAlpha (0.30f));
        g.fillRect (juce::Rectangle<float> (dest.getX(), y - 1.5f, dest.getWidth(), 4.0f));
        g.setColour (accent);
        g.fillRect (juce::Rectangle<float> (dest.getX(), y, dest.getWidth(), 1.4f));
    }
    else
    {
        const float x = dest.getX() + morph * (dest.getWidth() - 1.0f);
        g.setColour (accent.withAlpha (0.30f));
        g.fillRect (juce::Rectangle<float> (x - 1.5f, dest.getY(), 4.0f, dest.getHeight()));
        g.setColour (accent);
        g.fillRect (juce::Rectangle<float> (x, dest.getY(), 1.4f, dest.getHeight()));
    }
}

void LensImageView::animate()
{
    auto& lens = shared.processor.lensController();
    const int osc = oscOf();
    const float morph = liveMorph (osc);
    const int version = lens.tableVersion();
    const int mode = lens.mode();

    if (std::abs (morph - lastMorph) > 0.002f || version != lastVersion
        || mode != lastMode || osc != lastOsc)
    {
        lastMorph = morph;
        lastVersion = version;
        lastMode = mode;
        lastOsc = osc;
        repaint();
    }
}

// ---------------------------------------------------------------------------
// LensPanel
// ---------------------------------------------------------------------------

LensPanel::LensPanel (const UiShared& sharedContext, bool compactLayout)
    : shared (sharedContext),
      compact (compactLayout),
      image (sharedContext, [this] { return shared.processor.lensController().target(); }),
      modeTabs ({ "SCAN", compactLayout ? "SPEC" : "SPECTRAL" }, // compact card is 96 px wide
                [this] (int index) { shared.processor.lensController().setMode (index); }),
      targetTabs ({ "A", "B" },
                  [this] (int index) { shared.processor.lensController().setTarget (index); })
{
    addAndMakeVisible (image);
    addAndMakeVisible (modeTabs);
    addAndMakeVisible (targetTabs);

    colorsChip.setClickingTogglesState (false);
    colorsChip.setColour (juce::TextButton::buttonOnColourId, theme::accentMod);
    colorsChip.setTooltip ("Set patch from colors");
    colorsChip.onClick = [this]
    {
        auto& lens = shared.processor.lensController();
        lens.setChroma (! lens.chroma());
    };
    addAndMakeVisible (colorsChip);
}

void LensPanel::resized()
{
    auto area = getLocalBounds();

    if (compact)
    {
        // Deep card interior: image left, control column right.
        image.setBounds (area.removeFromLeft (area.getWidth() - 96));
        auto column = area.withTrimmedLeft (6);
        modeTabs.setBounds (column.removeFromTop (17));
        column.removeFromTop (5);
        targetTabs.setBounds (column.removeFromTop (17));
        column.removeFromTop (5);
        colorsChip.setBounds (column.removeFromTop (17));
    }
    else
    {
        // Play-view panel: title strip, image, one control row.
        area.reduce (10, 8);
        area.removeFromTop (18); // title
        auto controls = area.removeFromBottom (20);
        image.setBounds (area.withTrimmedBottom (6));
        targetTabs.setBounds (controls.removeFromRight (58));
        controls.removeFromRight (6);
        colorsChip.setBounds (controls.removeFromRight (62));
        controls.removeFromRight (6);
        modeTabs.setBounds (controls);
    }
}

void LensPanel::paint (juce::Graphics& g)
{
    if (compact)
        return;

    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, theme::cornerRadius);
    g.setColour (theme::hairline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme::cornerRadius, 1.0f);

    g.setColour (theme::accentMod);
    g.fillRoundedRectangle (10.0f, 9.0f, 3.0f, 12.0f, 1.5f);
    g.setColour (theme::textPrimary);
    g.setFont (theme::semiBold (13.0f));
    g.drawText ("LENS", 19, 6, 120, 16, juce::Justification::centredLeft);

    const auto name = shared.processor.lensController().imageName (
        shared.processor.lensController().target());
    if (name.isNotEmpty())
    {
        g.setColour (theme::textMuted);
        g.setFont (theme::font (11.0f));
        g.drawText (name, getLocalBounds().withTrimmedRight (12).withHeight (28),
                    juce::Justification::centredRight);
    }
}

void LensPanel::animate()
{
    image.animate();

    // Poll the settings so external changes (state load, the other panel)
    // stay reflected; these calls are cheap ValueTree property reads.
    auto& lens = shared.processor.lensController();
    if (modeTabs.active() != lens.mode())
        modeTabs.setActive (lens.mode(), false);
    if (targetTabs.active() != lens.target())
        targetTabs.setActive (lens.target(), false);
    if (colorsChip.getToggleState() != lens.chroma())
    {
        colorsChip.setToggleState (lens.chroma(), juce::dontSendNotification);
        repaint();
    }
}
