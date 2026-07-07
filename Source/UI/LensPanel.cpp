#include "UI/LensPanel.h"

#include "Engine/ModDestinations.h"
#include "Lens/LensController.h"
#include "Melody/MelodyController.h"
#include "Melody/MelodyPlayer.h"
#include "State/ModState.h"
#include "UI/MelodyPanel.h" // melodygrid::draw
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

} // namespace

// ---------------------------------------------------------------------------
// LensImageView
// ---------------------------------------------------------------------------

LensImageView::IconButton::IconButton (Glyph glyphToDraw, juce::String tip)
    : glyph (glyphToDraw)
{
    setTooltip (std::move (tip));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LensImageView::IconButton::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && onClick != nullptr)
        onClick();
}

void LensImageView::IconButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::well.withAlpha (hovered ? 0.95f : 0.75f));
    g.fillEllipse (bounds);
    g.setColour (hovered ? theme::textPrimary : theme::textSecondary);
    g.drawEllipse (bounds.reduced (0.5f), 1.0f);

    const auto inner = bounds.reduced (bounds.getWidth() * 0.30f);
    if (glyph == Glyph::remove)
    {
        g.drawLine ({ inner.getTopLeft(), inner.getBottomRight() }, 1.4f);
        g.drawLine ({ inner.getTopRight(), inner.getBottomLeft() }, 1.4f);
    }
    else
    {
        // Replace: circular arrow — an open arc plus a small arrowhead.
        juce::Path arc;
        const auto centre = inner.getCentre();
        const float r = inner.getWidth() * 0.5f;
        arc.addCentredArc (centre.x, centre.y, r, r, 0.0f, 0.6f,
                           juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath (arc, juce::PathStrokeType (1.4f));

        const auto tip = centre.getPointOnCircumference (r, 0.6f);
        juce::Path head;
        head.addTriangle (tip.x - 2.4f, tip.y - 1.2f, tip.x + 2.4f, tip.y - 1.2f,
                          tip.x, tip.y + 2.6f);
        g.fillPath (head);
    }
}

LensImageView::LensImageView (const UiShared& sharedContext, std::function<int()> oscProvider)
    : shared (sharedContext), oscOf (std::move (oscProvider))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    removeButton.onClick = [this]
    {
        shared.processor.lensController().removeImage (oscOf());
    };
    replaceButton.onClick = [this] { openChooser(); };
    addChildComponent (removeButton);
    addChildComponent (replaceButton);
}

bool LensImageView::hasImage() const
{
    return shared.processor.lensController().displayImage (oscOf()).isValid();
}

void LensImageView::openChooser()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Choose an image for LENS", juce::File(), "*.png;*.jpg;*.jpeg;*.gif");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file != juce::File())
                                  shared.processor.lensController().loadImageFile (file);
                          });
}

void LensImageView::mouseUp (const juce::MouseEvent& e)
{
    // Empty well or thumbnail click = open the chooser (load / replace).
    // The icon buttons are children, so they never reach here.
    if (getLocalBounds().contains (e.getPosition()) && ! e.mods.isPopupMenu())
        openChooser();
}

void LensImageView::resized()
{
    removeButton.setBounds (getWidth() - 22, 5, 17, 17);
    replaceButton.setBounds (getWidth() - 42, 5, 17, 17);
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
        // Empty state: dashed drop zone with a "+" — click opens the file
        // chooser, dropping an image anywhere on the window still works.
        const auto zone = bounds.reduced (7.0f);
        juce::Path outline;
        outline.addRoundedRectangle (zone, theme::wellRadius);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (theme::textMuted);
        g.fillPath (dashed);

        const auto centre = zone.getCentre();
        const float arm = 9.0f;
        g.setColour (theme::textSecondary);
        g.drawLine (centre.x - arm, centre.y - 7.0f, centre.x + arm, centre.y - 7.0f, 1.6f);
        g.drawLine (centre.x, centre.y - 7.0f - arm, centre.x, centre.y - 7.0f + arm, 1.6f);

        g.setColour (theme::textMuted);
        g.setFont (theme::font (12.0f));
        g.drawText ("click or drop an image to build a tone",
                    getLocalBounds().withTrimmedTop (24),
                    juce::Justification::centred);
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

    // Scanline synced to the live morph (SPEC 13): a white beam the full
    // image width (Scan, travelling down) or height (Spectral, travelling
    // across), slightly thick with a soft fade trailing behind the travel
    // direction so it reads as a scan beam.
    const float morph = juce::jlimit (0.0f, 1.0f, liveMorph (osc));
    const bool scanMode = lens.mode() == 0;
    constexpr float beam = 2.4f, trail = 16.0f;
    const auto white = juce::Colours::white;

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (dest.getSmallestIntegerContainer());

    if (scanMode)
    {
        const float y = dest.getY() + morph * (dest.getHeight() - 1.0f);
        g.setGradientFill ({ white.withAlpha (0.0f), dest.getX(), y - trail,
                             white.withAlpha (0.35f), dest.getX(), y, false });
        g.fillRect (juce::Rectangle<float> (dest.getX(), y - trail, dest.getWidth(), trail));
        g.setColour (white.withAlpha (0.95f));
        g.fillRect (juce::Rectangle<float> (dest.getX(), y - beam * 0.5f, dest.getWidth(), beam));
    }
    else
    {
        const float x = dest.getX() + morph * (dest.getWidth() - 1.0f);
        g.setGradientFill ({ white.withAlpha (0.0f), x - trail, dest.getY(),
                             white.withAlpha (0.35f), x, dest.getY(), false });
        g.fillRect (juce::Rectangle<float> (x - trail, dest.getY(), trail, dest.getHeight()));
        g.setColour (white.withAlpha (0.95f));
        g.fillRect (juce::Rectangle<float> (x - beam * 0.5f, dest.getY(), beam, dest.getHeight()));
    }

    // Melody sampling grid: while the Melody panel is active, overlay the grid
    // (and the sampled path / sounding-cell glow) on the Lens image so the
    // melody visibly traces its way across the picture.
    auto& melody = shared.processor.melodyController();
    if (melody.isPanelActive())
    {
        const auto live = shared.processor.melodyPlayer().liveState();
        melodygrid::DrawInfo info;
        info.imageArea = dest;
        info.cols = melody.hasMelody() ? melody.gridCols() : MelodyController::kGridResolution;
        info.rows = melody.hasMelody() ? melody.gridRows() : MelodyController::kGridResolution;
        info.seq = melody.hasMelody() ? &melody.sequence() : nullptr;
        info.liveCol = melodyGlowCol;
        info.liveRow = melodyGlowRow;
        info.glow = melodyGlow;
        info.playing = live.playing;
        info.accent = theme::neonYellow;
        melodygrid::draw (g, info);
    }
}

void LensImageView::animate()
{
    auto& lens = shared.processor.lensController();
    const int osc = oscOf();
    const float morph = liveMorph (osc);
    const int version = lens.tableVersion();
    const int mode = lens.mode();

    // Melody overlay glow: track the currently-sounding cell and fade it out.
    auto& melody = shared.processor.melodyController();
    const bool melodyActive = melody.isPanelActive();
    bool melodyRepaint = melodyActive != lastMelodyActive;
    lastMelodyActive = melodyActive;
    if (melodyActive)
    {
        const auto live = shared.processor.melodyPlayer().liveState();
        if (live.triggerSeq != melodyLastTrigger)
        {
            melodyLastTrigger = live.triggerSeq;
            melodyGlow = 1.0f;
            melodyGlowCol = live.col;
            melodyGlowRow = live.row;
            melodyRepaint = true;
        }
        else if (melodyGlow > 0.0f)
        {
            melodyGlow = juce::jmax (0.0f, melodyGlow - 0.11f);
            melodyRepaint = true;
        }
        if (! live.playing && melodyGlow > 0.0f)
        {
            melodyGlow = 0.0f;
            melodyRepaint = true;
        }
    }

    if (std::abs (morph - lastMorph) > 0.002f || version != lastVersion
        || mode != lastMode || osc != lastOsc || melodyRepaint)
    {
        lastMorph = morph;
        lastVersion = version;
        lastMode = mode;
        lastOsc = osc;

        const bool loaded = hasImage();
        removeButton.setVisible (loaded);
        replaceButton.setVisible (loaded);
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
    modeTabs.setTooltip ("Scan plays image rows as waveforms; Spectral reads it as a spectrogram");
    targetTabs.setTooltip ("Which oscillator receives the image wavetable");
    addAndMakeVisible (modeTabs);
    addAndMakeVisible (targetTabs);

    colorsChip.setClickingTogglesState (false);
    colorsChip.setColour (juce::TextButton::buttonOnColourId, theme::accentMod);
    colorsChip.setTooltip ("COLORS: an image drop also sets the patch from the image's colors");
    colorsChip.onClick = [this]
    {
        auto& lens = shared.processor.lensController();
        lens.setChroma (! lens.chroma());
    };
    addAndMakeVisible (colorsChip);

    melodyChip.setClickingTogglesState (false);
    melodyChip.setColour (juce::TextButton::buttonOnColourId, theme::neonYellow);
    melodyChip.setTooltip ("Open the MELODY panel: turn this image into a playable melody");
    melodyChip.onClick = [this] { shared.processor.melodyController().togglePanel(); };
    addAndMakeVisible (melodyChip);
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
        column.removeFromTop (5);
        melodyChip.setBounds (column.removeFromTop (17));
    }
    else
    {
        // Play-view panel: title strip, image, one control row. The mode
        // tabs get the width A/B can spare so SPECTRAL sets naturally.
        area.reduce (10, 8);
        area.removeFromTop (18); // title
        auto controls = area.removeFromBottom (20);
        image.setBounds (area.withTrimmedBottom (6));
        targetTabs.setBounds (controls.removeFromRight (44));
        controls.removeFromRight (6);
        melodyChip.setBounds (controls.removeFromRight (58));
        controls.removeFromRight (6);
        colorsChip.setBounds (controls.removeFromRight (58));
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

    const bool melodyActive = shared.processor.melodyController().isPanelActive();
    if (melodyChip.getToggleState() != melodyActive)
        melodyChip.setToggleState (melodyActive, juce::dontSendNotification);
}
