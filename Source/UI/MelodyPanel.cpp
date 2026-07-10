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
      modeTabs ({ "MELODY", "CHORDS", "ARP" },
                [this] (int i) { setChoiceParam (params::melodyMode, i); }),
      keyModeTabs ({ "FROM IMAGE", "RANDOM" },
                   [this] (int i) { setChoiceParam (params::melodyKeyMode, i); }),
      lengthTabs ({ "8", "16", "32" },
                  [this] (int i) { setChoiceParam (params::melodyLength, i); }),
      phraseTabs ({ "PHRASED", "FREEFORM" },
                  [this] (int i) { setChoiceParam (params::melodyPhrase, i); }),
      arpPatternTabs ({ "UP", "DOWN", "UP/DN", "CONV", "RAND" },
                      [this] (int i) { setChoiceParam (params::melodyArpPattern, i); }),
      loopTabs ({ "OFF", "1", "2", "4", "8" },
                [this] (int i) { setChoiceParam (params::melodyLoopLength, i); }),
      dragMidi (sharedContext)
{
    addAndMakeVisible (grid);

    closeButton.setColour (juce::TextButton::buttonColourId, theme::panel);
    closeButton.setColour (juce::TextButton::textColourOffId, theme::textSecondary);
    closeButton.onClick = [this] { shared.processor.melodyController().setPanelActive (false); };
    addAndMakeVisible (closeButton);

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

    // Loop toggle: the player wraps at the sequence end instead of stopping.
    loopToggle = std::make_unique<ParamToggle> (shared, params::melodyLoopPlayback,
                                                "LOOP", theme::neonYellow);
    loopToggle->button.setTooltip ("Repeat playback from the top when the melody ends");
    addAndMakeVisible (*loopToggle);

    // Transpose stepper: +/- semitone chips driving the int param directly
    // (registered for --check-params like the tab strips). Applied by the
    // player and the MIDI export only — never regenerates, never re-seeds.
    auto nudgeTranspose = [this] (int delta)
    {
        if (auto* p = shared.apvts().getParameter (params::melodyTranspose))
        {
            const int current = juce::roundToInt (
                p->convertFrom0to1 (p->getValue()));
            const int next = juce::jlimit (-12, 12, current + delta);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (next)));
            p->endChangeGesture();
        }
    };
    styleChip (transposeDown, theme::neonYellow);
    styleChip (transposeUp, theme::neonYellow);
    transposeDown.setTooltip ("Shift playback and export down a semitone (never regenerates)");
    transposeUp.setTooltip ("Shift playback and export up a semitone (never regenerates)");
    transposeDown.onClick = [nudgeTranspose] { nudgeTranspose (-1); };
    transposeUp.onClick   = [nudgeTranspose] { nudgeTranspose (+1); };
    shared.registerAttachment (params::melodyTranspose);
    addAndMakeVisible (transposeDown);
    addAndMakeVisible (transposeUp);

    // Tab strips drive their choice params directly (no JUCE attachment), so
    // register them for the --check-params UI-coverage audit.
    shared.registerAttachment (params::melodyMode);
    shared.registerAttachment (params::melodyKeyMode);
    shared.registerAttachment (params::melodyLength);
    shared.registerAttachment (params::melodyPhrase);
    shared.registerAttachment (params::melodyArpPattern);
    shared.registerAttachment (params::melodyLoopLength);

    modeTabs.setTooltip ("Melody: a single line. Chords: a block-chord progression. Arp: a chord-tone arpeggio");
    keyModeTabs.setTooltip ("Derive the key from the image, or pick one at random");
    lengthTabs.setTooltip ("LENGTH: number of notes (chords: number of chords) generated");
    phraseTabs.setTooltip ("Phrased: motif/variation/cadence structure. Freeform: one continuous walk");
    arpPatternTabs.setTooltip ("Arpeggiator direction: up, down, up/down, converge (outside-in), or random");
    loopTabs.setTooltip ("Loop length in bars (Off = one-shot). Loops are bar-aligned so playback repeats seamlessly");
    addAndMakeVisible (modeTabs);
    addAndMakeVisible (keyModeTabs);
    addAndMakeVisible (lengthTabs);
    addAndMakeVisible (phraseTabs);
    addAndMakeVisible (arpPatternTabs);
    addAndMakeVisible (loopTabs);

    // Four macro knobs.
    energyKnob     = std::make_unique<ModKnob> (shared, params::melodyEnergy,         "ENERGY",   theme::neonYellow);
    complexityKnob = std::make_unique<ModKnob> (shared, params::melodyComplexity,     "COMPLEX",  theme::neonYellow);
    imageKnob      = std::make_unique<ModKnob> (shared, params::melodyImageInfluence, "IMAGE",    theme::neonYellow);
    repetitionKnob = std::make_unique<ModKnob> (shared, params::melodyRepetition,     "REPEAT",   theme::neonYellow);
    addAndMakeVisible (*energyKnob);
    addAndMakeVisible (*complexityKnob);
    addAndMakeVisible (*imageKnob);
    addAndMakeVisible (*repetitionKnob);

    // Regeneration: fresh material, or a small mutation of the current one; the
    // two locks constrain what either is allowed to change.
    styleChip (regenerateButton, theme::neonYellow);
    regenerateButton.setColour (juce::TextButton::textColourOffId, theme::textPrimary);
    regenerateButton.setTooltip ("Generate a fresh melody (keeps any locked dimension)");
    regenerateButton.onClick = [this] { shared.processor.melodyController().regenerate(); };
    addAndMakeVisible (regenerateButton);

    styleChip (mutateButton, theme::accentMod);
    mutateButton.setTooltip ("Nudge the current melody into a variation (keeps any locked dimension)");
    mutateButton.onClick = [this] { shared.processor.melodyController().mutate(); };
    addAndMakeVisible (mutateButton);

    lockRhythm = std::make_unique<ParamToggle> (shared, params::melodyLockRhythm, "LOCK RHYTHM", theme::neonYellow);
    lockPitch  = std::make_unique<ParamToggle> (shared, params::melodyLockPitch,  "LOCK PITCH",  theme::neonYellow);
    lockRhythm->button.setTooltip ("Keep the timing; Regenerate/Mutate change only pitch");
    lockPitch->button.setTooltip ("Keep the pitches; Regenerate/Mutate change only rhythm");
    addAndMakeVisible (*lockRhythm);
    addAndMakeVisible (*lockPitch);

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
    sectionLabels.clear();

    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (24); // title strip

    closeButton.setBounds (getWidth() - 30, 8, 20, 20);

    // Left: the image + grid visualization (square-ish), with the generation
    // summary readout and the transpose stepper beneath it.
    auto left = area.removeFromLeft (280);
    grid.setBounds (left.removeFromTop (280));
    left.removeFromTop (10);
    summaryArea = left.removeFromTop (80);
    left.removeFromTop (8);
    auto transposeRow = left.removeFromTop (22);
    transposeDown.setBounds (transposeRow.removeFromLeft (30));
    transposeUp.setBounds (transposeRow.removeFromRight (30));
    transposeLabelArea = transposeRow.reduced (4, 0);

    area.removeFromLeft (16);
    auto col = area; // right control column

    // Helpers: a plain row (control + gap) and a captioned section (small
    // uppercase label + control beneath it).
    auto row = [&col] (int h, int gap = 6)
    {
        auto r = col.removeFromTop (h);
        col.removeFromTop (gap);
        return r;
    };
    auto section = [&] (const juce::String& caption, int h, int gap = 8)
    {
        auto cap = col.removeFromTop (12);
        sectionLabels.emplace_back (caption, cap);
        col.removeFromTop (2);
        return row (h, gap);
    };

    // Transport (no caption): PLAY with the LOOP chip beside it.
    auto transportRow = row (26, 10);
    loopToggle->setBounds (transportRow.removeFromRight (64));
    transportRow.removeFromRight (8);
    playButton.setBounds (transportRow);

    modeTabs.setBounds    (section ("MODE",   18));
    keyModeTabs.setBounds (section ("KEY",    18));
    lengthTabs.setBounds  (section ("LENGTH", 18));

    // SHAPE: phrase toggle (Melody) and arp direction (Arp) share one row;
    // animate() shows whichever the current mode uses.
    auto shapeRow = section ("SHAPE", 18);
    phraseTabs.setBounds (shapeRow);
    arpPatternTabs.setBounds (shapeRow);

    // FEEL: four macro knobs across the column.
    auto knobs = section ("FEEL", 54);
    const int kw = knobs.getWidth() / 4;
    energyKnob->setBounds     (knobs.removeFromLeft (kw).reduced (3, 0));
    complexityKnob->setBounds (knobs.removeFromLeft (kw).reduced (3, 0));
    imageKnob->setBounds      (knobs.removeFromLeft (kw).reduced (3, 0));
    repetitionKnob->setBounds (knobs.reduced (3, 0));

    loopTabs.setBounds (section ("LOOP LENGTH", 18));

    // SEED: the two locks, then the two regeneration actions.
    auto lockRow = section ("SEED", 18, 6);
    lockRhythm->setBounds (lockRow.removeFromLeft (lockRow.getWidth() / 2 - 4));
    lockPitch->setBounds  (lockRow.removeFromRight (lockRow.getWidth()));
    auto actionRow = row (26, 8);
    regenerateButton.setBounds (actionRow.removeFromLeft (actionRow.getWidth() / 2 - 4));
    mutateButton.setBounds     (actionRow.removeFromRight (actionRow.getWidth()));

    // EXPORT: drag handle + save.
    auto exportRow = section ("EXPORT", 26, 0);
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

    // Generation summary under the image: what the engine detected and chose.
    {
        auto& m = shared.processor.melodyController();
        const juce::String dash = juce::String::fromUTF8 ("\xe2\x80\x94");
        const auto value = [&dash] (const juce::String& v) { return v.isNotEmpty() ? v : dash; };

        auto block = summaryArea;
        g.setColour (theme::textSecondary.withAlpha (0.85f));
        g.setFont (theme::semiBold (9.5f));
        g.drawText ("GENERATED", block.removeFromTop (12), juce::Justification::centredLeft);
        block.removeFromTop (2);

        const std::pair<const char*, juce::String> rows[] = {
            { "KEY",  value (m.detectedKey()) },
            { "MOOD", value (m.moodText()) },
            { "FORM", value (m.formText()) },
            { "SEED", m.hasMelody()
                          ? juce::String::toHexString (static_cast<juce::int64> (m.seed()))
                          : dash },
        };
        for (const auto& [label, text] : rows)
        {
            auto line = block.removeFromTop (16);
            g.setColour (theme::textMuted);
            g.setFont (theme::semiBold (9.5f));
            g.drawText (label, line.removeFromLeft (38), juce::Justification::centredLeft);
            g.setColour (theme::textSecondary);
            g.setFont (theme::font (11.0f));
            g.drawText (text, line, juce::Justification::centredLeft);
        }
    }

    // Transpose readout between the +/- chips.
    {
        const int t = choiceParam (params::melodyTranspose);
        g.setColour (theme::textMuted);
        g.setFont (theme::semiBold (9.5f));
        g.drawText ("TRANSPOSE", transposeLabelArea, juce::Justification::centredLeft);
        g.setColour (theme::textSecondary);
        g.setFont (theme::medium (11.0f));
        g.drawText ((t > 0 ? "+" : "") + juce::String (t) + " st",
                    transposeLabelArea, juce::Justification::centredRight);
    }

    // Section captions above each control group.
    g.setFont (theme::semiBold (9.5f));
    g.setColour (theme::textSecondary.withAlpha (0.85f));
    for (const auto& [caption, rect] : sectionLabels)
        g.drawText (caption, rect, juce::Justification::centredLeft);
}

void MelodyPanel::animate()
{
    grid.animate();
    refreshTransportLabel();

    // Repaint the summary block when generation changes what it says.
    {
        auto& m = shared.processor.melodyController();
        juce::String composed;
        composed << m.detectedKey() << '|' << m.moodText() << '|' << m.formText()
                 << '|' << juce::String::toHexString (static_cast<juce::int64> (m.seed()))
                 << '|' << (m.hasMelody() ? 1 : 0);
        if (composed != summaryCache)
        {
            summaryCache = composed;
            repaint (summaryArea);
        }
    }

    // Repaint the transpose readout when the param moves (UI or automation).
    {
        const int t = choiceParam (params::melodyTranspose);
        if (t != transposeCache)
        {
            transposeCache = t;
            repaint (transposeLabelArea);
        }
    }

    if (modeTabs.active() != choiceParam (params::melodyMode))
        modeTabs.setActive (choiceParam (params::melodyMode), false);
    if (keyModeTabs.active() != choiceParam (params::melodyKeyMode))
        keyModeTabs.setActive (choiceParam (params::melodyKeyMode), false);
    if (lengthTabs.active() != choiceParam (params::melodyLength))
        lengthTabs.setActive (choiceParam (params::melodyLength), false);
    if (phraseTabs.active() != choiceParam (params::melodyPhrase))
        phraseTabs.setActive (choiceParam (params::melodyPhrase), false);
    if (arpPatternTabs.active() != choiceParam (params::melodyArpPattern))
        arpPatternTabs.setActive (choiceParam (params::melodyArpPattern), false);
    if (loopTabs.active() != choiceParam (params::melodyLoopLength))
        loopTabs.setActive (choiceParam (params::melodyLoopLength), false);

    // SHAPE row: the phrase toggle belongs to Melody mode, the arp direction to
    // Arp mode; Chords uses neither. Show whichever applies (they share a rect).
    const int mode = choiceParam (params::melodyMode); // 0 Melody, 1 Chords, 2 Arp
    const bool showPhrase = (mode == 0);
    const bool showArp    = (mode == 2);
    if (phraseTabs.isVisible() != showPhrase)
        phraseTabs.setVisible (showPhrase);
    if (arpPatternTabs.isVisible() != showArp)
        arpPatternTabs.setVisible (showArp);
}
