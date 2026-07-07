#include "UI/MelodyPanel.h"

#include "Lens/LensController.h"
#include "Melody/MelodyController.h"
#include "Melody/MelodyPlayer.h"
#include "State/Parameters.h"
#include "UI/Theme.h"

using namespace lumen;

// ---------------------------------------------------------------------------
// Shared grid-overlay drawing
// ---------------------------------------------------------------------------

namespace melodygrid
{
namespace
{
    juce::Rectangle<float> cellRect (const DrawInfo& info, int col, int row)
    {
        const float cw = info.imageArea.getWidth() / juce::jmax (1, info.cols);
        const float ch = info.imageArea.getHeight() / juce::jmax (1, info.rows);
        return { info.imageArea.getX() + col * cw, info.imageArea.getY() + row * ch, cw, ch };
    }
} // namespace

void draw (juce::Graphics& g, const DrawInfo& info)
{
    const auto area = info.imageArea;
    if (area.isEmpty() || info.cols <= 0 || info.rows <= 0)
        return;

    // Thin, semi-transparent accent grid lines.
    g.setColour (info.accent.withAlpha (0.22f));
    for (int c = 1; c < info.cols; ++c)
    {
        const float x = area.getX() + area.getWidth() * c / info.cols;
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.0f);
    }
    for (int r = 1; r < info.rows; ++r)
    {
        const float y = area.getY() + area.getHeight() * r / info.rows;
        g.drawLine (area.getX(), y, area.getRight(), y, 1.0f);
    }

    // Stopped: trace the sampled path faintly (dots joined in visiting order).
    if (! info.playing && info.seq != nullptr && ! info.seq->steps.empty())
    {
        juce::Path path;
        for (std::size_t i = 0; i < info.seq->steps.size(); ++i)
        {
            const auto& s = info.seq->steps[i];
            const auto centre = cellRect (info, s.col, s.row).getCentre();
            if (i == 0) path.startNewSubPath (centre);
            else        path.lineTo (centre);
        }
        g.setColour (info.accent.withAlpha (0.30f));
        g.strokePath (path, juce::PathStrokeType (1.2f));

        g.setColour (info.accent.withAlpha (0.55f));
        for (const auto& s : info.seq->steps)
        {
            const auto centre = cellRect (info, s.col, s.row).getCentre();
            g.fillEllipse (centre.x - 1.6f, centre.y - 1.6f, 3.2f, 3.2f);
        }
    }

    // Playing: glow the currently sounding cell, fading over ~150 ms.
    if (info.glow > 0.001f && info.liveCol >= 0 && info.liveRow >= 0)
    {
        const auto cell = cellRect (info, info.liveCol, info.liveRow);
        g.setColour (info.accent.withAlpha (0.10f + 0.45f * info.glow));
        g.fillRect (cell);
        g.setColour (info.accent.withAlpha (0.35f + 0.55f * info.glow));
        g.drawRect (cell, 1.4f);
    }
}
} // namespace melodygrid

namespace
{
    // Fit an image into an area, aspect preserved and centred — the same fit
    // LensImageView uses, so the grid overlay lines up with the picture.
    juce::Rectangle<float> fitImage (const juce::Image& img, juce::Rectangle<float> area)
    {
        if (! img.isValid() || img.getWidth() <= 0 || img.getHeight() <= 0)
            return {};
        const float scale = juce::jmin (area.getWidth() / (float) img.getWidth(),
                                        area.getHeight() / (float) img.getHeight());
        const float w = img.getWidth() * scale, h = img.getHeight() * scale;
        return { area.getCentreX() - w * 0.5f, area.getCentreY() - h * 0.5f, w, h };
    }

    void styleChip (juce::TextButton& b, juce::Colour on)
    {
        b.setColour (juce::TextButton::buttonColourId, theme::well);
        b.setColour (juce::TextButton::buttonOnColourId, on);
        b.setColour (juce::TextButton::textColourOffId, theme::textSecondary);
        b.setColour (juce::TextButton::textColourOnId, theme::well);
    }
} // namespace

// ---------------------------------------------------------------------------
// GridView
// ---------------------------------------------------------------------------

void MelodyPanel::GridView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::well);
    g.fillRoundedRectangle (bounds, theme::wellRadius);

    auto& lens = shared.processor.lensController();
    const auto img = lens.displayImage (lens.target());
    const auto inner = bounds.reduced (5.0f);

    if (! img.isValid())
    {
        g.setColour (theme::textMuted);
        g.setFont (theme::font (12.0f));
        g.drawText ("load an image in LENS to generate a melody", inner,
                    juce::Justification::centred);
        return;
    }

    const auto dest = fitImage (img, inner);
    g.drawImage (img, dest);
    g.setColour (theme::hairlineLight);
    g.drawRect (dest, 1.0f);

    auto& melody = shared.processor.melodyController();
    const auto live = shared.processor.melodyPlayer().liveState();

    melodygrid::DrawInfo info;
    info.imageArea = dest;
    info.cols = melody.hasMelody() ? melody.gridCols() : MelodyController::kGridResolution;
    info.rows = melody.hasMelody() ? melody.gridRows() : MelodyController::kGridResolution;
    info.seq = melody.hasMelody() ? &melody.sequence() : nullptr;
    info.liveCol = glowCol;
    info.liveRow = glowRow;
    info.glow = glow;
    info.playing = live.playing;
    info.accent = theme::neonYellow;

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (dest.getSmallestIntegerContainer());
    melodygrid::draw (g, info);
}

void MelodyPanel::GridView::animate()
{
    const auto live = shared.processor.melodyPlayer().liveState();
    bool needsRepaint = false;

    if (live.triggerSeq != lastTrigger)
    {
        lastTrigger = live.triggerSeq;
        glow = 1.0f;
        glowCol = live.col;
        glowRow = live.row;
        needsRepaint = true;
    }
    else if (glow > 0.0f)
    {
        glow = juce::jmax (0.0f, glow - 0.11f); // ~150 ms fade at 60 Hz
        needsRepaint = true;
    }

    if (! live.playing && glow > 0.0f)
    {
        glow = 0.0f;
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

// ---------------------------------------------------------------------------
// MidiDragSource
// ---------------------------------------------------------------------------

void MelodyPanel::MidiDragSource::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const bool enabled = shared.processor.melodyController().hasMelody();
    g.setColour (theme::well);
    g.fillRoundedRectangle (bounds, theme::wellRadius);
    g.setColour (enabled ? theme::neonYellow.withAlpha (0.6f) : theme::hairline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme::wellRadius, 1.0f);
    g.setColour (enabled ? theme::textPrimary : theme::textMuted);
    g.setFont (theme::medium (12.0f));
    g.drawText ("DRAG MIDI", getLocalBounds(), juce::Justification::centred);
}

void MelodyPanel::MidiDragSource::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        return;
    if (e.getDistanceFromDragStart() < 6)
        return;

    const juce::File file = shared.processor.melodyController().writeTempMidiFile();
    if (file == juce::File())
        return;

    dragging = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        { file.getFullPathName() }, /*canMoveFiles=*/false, this);
}

// ---------------------------------------------------------------------------
// MelodyPanel
// ---------------------------------------------------------------------------

MelodyPanel::MelodyPanel (const UiShared& sharedContext)
    : shared (sharedContext),
      grid (sharedContext),
      keyModeTabs ({ "FROM IMAGE", "RANDOM" },
                   [this] (int i) { setChoiceParam (params::melodyKeyMode, i); }),
      lengthTabs ({ "8", "16", "32" },
                  [this] (int i) { setChoiceParam (params::melodyLength, i); }),
      phraseTabs ({ "PHRASED", "FREEFORM" },
                  [this] (int i) { setChoiceParam (params::melodyPhrase, i); }),
      dragMidi (sharedContext)
{
    addAndMakeVisible (grid);

    closeButton.setColour (juce::TextButton::buttonColourId, theme::panel);
    closeButton.setColour (juce::TextButton::textColourOffId, theme::textSecondary);
    closeButton.onClick = [this] { shared.processor.melodyController().setPanelActive (false); };
    addAndMakeVisible (closeButton);

    styleChip (generateButton, theme::neonYellow);
    generateButton.setColour (juce::TextButton::textColourOffId, theme::textPrimary);
    generateButton.onClick = [this] { shared.processor.melodyController().generate(); };
    addAndMakeVisible (generateButton);

    styleChip (playButton, theme::neonYellow);
    playButton.onClick = [this]
    {
        auto& m = shared.processor.melodyController();
        if (m.isPlaying())
        {
            m.stop();
        }
        else
        {
            if (! m.hasMelody())
                m.generate();
            m.play();
        }
        refreshTransportLabel();
    };
    addAndMakeVisible (playButton);

    // These choice params are driven by the tab strips above (not via a JUCE
    // attachment), so register them for the --check-params UI-coverage audit.
    shared.registerAttachment (params::melodyKeyMode);
    shared.registerAttachment (params::melodyLength);
    shared.registerAttachment (params::melodyPhrase);

    keyModeTabs.setTooltip ("Derive the key from the image, or pick one at random");
    lengthTabs.setTooltip ("Number of notes in the generated melody");
    phraseTabs.setTooltip ("Phrased: motif/variation/cadence structure. Freeform: one continuous walk");
    addAndMakeVisible (keyModeTabs);
    addAndMakeVisible (lengthTabs);
    addAndMakeVisible (phraseTabs);

    styleChip (rerollButton, theme::accentMod);
    rerollButton.setTooltip ("Generate a new melody from a fresh random seed");
    rerollButton.onClick = [this] { shared.processor.melodyController().reroll(); };
    addAndMakeVisible (rerollButton);

    styleChip (lockButton, theme::neonYellow);
    lockButton.setTooltip ("Lock the seed so re-roll keeps the current melody");
    lockButton.onClick = [this]
    {
        auto& m = shared.processor.melodyController();
        m.setLocked (! m.locked());
    };
    addAndMakeVisible (lockButton);

    biasKnob      = std::make_unique<ModKnob> (shared, params::melodyBias, "BRIGHT", theme::neonYellow);
    ornamentsKnob = std::make_unique<ModKnob> (shared, params::melodyOrnaments, "ORNAMENT", theme::neonYellow);
    addAndMakeVisible (*biasKnob);
    addAndMakeVisible (*ornamentsKnob);

    styleChip (saveButton, theme::accentMod);
    saveButton.setColour (juce::TextButton::textColourOffId, theme::textPrimary);
    saveButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Save melody as MIDI", juce::File(), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, chooser] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file != juce::File())
                                      shared.processor.melodyController().saveMidiFile (
                                          file.withFileExtension ("mid"));
                              });
    };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (dragMidi);

    refreshTransportLabel();
}

MelodyPanel::~MelodyPanel() = default;

void MelodyPanel::setChoiceParam (const char* paramId, int index)
{
    if (auto* p = shared.apvts().getParameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (index)));
        p->endChangeGesture();
    }
}

int MelodyPanel::choiceParam (const char* paramId) const
{
    if (auto* v = shared.apvts().getRawParameterValue (paramId))
        return static_cast<int> (v->load());
    return 0;
}

void MelodyPanel::refreshTransportLabel()
{
    playButton.setButtonText (shared.processor.melodyController().isPlaying() ? "STOP" : "PLAY");
}

void MelodyPanel::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (24); // title strip

    closeButton.setBounds (getWidth() - 30, 8, 20, 20);

    // Left: the image + grid visualization (square-ish).
    auto left = area.removeFromLeft (280);
    grid.setBounds (left.removeFromTop (280));

    area.removeFromLeft (14);
    auto col = area; // right control column

    generateButton.setBounds (col.removeFromTop (30));
    col.removeFromTop (8);
    playButton.setBounds (col.removeFromTop (26));
    col.removeFromTop (12);

    auto row = [&col] (int h, int gap = 8) { auto r = col.removeFromTop (h); col.removeFromTop (gap); return r; };

    keyModeTabs.setBounds (row (18));
    lengthTabs.setBounds (row (18));
    phraseTabs.setBounds (row (18));

    auto lockRow = row (18);
    rerollButton.setBounds (lockRow.removeFromLeft (lockRow.getWidth() / 2 - 4));
    lockButton.setBounds (lockRow.removeFromRight (lockRow.getWidth()));

    col.removeFromTop (6);
    auto knobs = col.removeFromTop (58);
    biasKnob->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).reduced (4, 0));
    ornamentsKnob->setBounds (knobs.reduced (4, 0));

    col.removeFromTop (8);
    auto exportRow = col.removeFromTop (26);
    dragMidi.setBounds (exportRow.removeFromLeft (exportRow.getWidth() / 2 - 4));
    saveButton.setBounds (exportRow.removeFromRight (exportRow.getWidth()));
}

void MelodyPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, theme::cornerRadius);
    g.setColour (theme::neonYellow.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme::cornerRadius, 1.0f);

    // Title.
    g.setColour (theme::neonYellow);
    g.fillRoundedRectangle (14.0f, 15.0f, 3.0f, 14.0f, 1.5f);
    g.setColour (theme::textPrimary);
    g.setFont (theme::semiBold (14.0f));
    g.drawText ("MELODY", 24, 11, 160, 18, juce::Justification::centredLeft);

    // Detected key readout under the image.
    const auto key = shared.processor.melodyController().detectedKey();
    g.setColour (theme::textSecondary);
    g.setFont (theme::font (12.0f));
    g.drawText ("Detected: " + (key.isNotEmpty() ? key : juce::String ("\xe2\x80\x94")),
                18, getHeight() - 26, 300, 18, juce::Justification::centredLeft);
}

void MelodyPanel::animate()
{
    grid.animate();
    refreshTransportLabel();

    if (keyModeTabs.active() != choiceParam (params::melodyKeyMode))
        keyModeTabs.setActive (choiceParam (params::melodyKeyMode), false);
    if (lengthTabs.active() != choiceParam (params::melodyLength))
        lengthTabs.setActive (choiceParam (params::melodyLength), false);
    if (phraseTabs.active() != choiceParam (params::melodyPhrase))
        phraseTabs.setActive (choiceParam (params::melodyPhrase), false);

    const bool locked = shared.processor.melodyController().locked();
    if (lockButton.getToggleState() != locked)
    {
        lockButton.setToggleState (locked, juce::dontSendNotification);
        lockButton.setColour (juce::TextButton::textColourOffId,
                              locked ? theme::neonYellow : theme::textSecondary);
        rerollButton.setAlpha (locked ? 0.5f : 1.0f);
        repaint();
    }
}
