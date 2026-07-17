#include "UI/MelodySidePanel.h"

#include "Melody/MelodyController.h"
#include "State/Parameters.h"
#include "UI/Theme.h"

using namespace lumen;

namespace
{
    // Chip colouring helper — same look the popup used for its transport and
    // action buttons (local anon-namespace copy of MelodyPanel's styleChip;
    // promote to Controls.h in the Phase 5 cleanup when the popup goes away).
    void styleChip (juce::TextButton& b, juce::Colour on)
    {
        b.setColour (juce::TextButton::buttonColourId, theme::well);
        b.setColour (juce::TextButton::buttonOnColourId, on);
        b.setColour (juce::TextButton::textColourOffId, theme::textSecondary);
        b.setColour (juce::TextButton::textColourOnId, theme::well);
    }

    // A small padlock: a rounded shackle arc over a body rectangle. Drawn over
    // REGENERATE when the master seed is locked.
    void drawLockGlyph (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    {
        const float bodyW = area.getWidth();
        const float bodyH = area.getHeight() * 0.6f;
        const juce::Rectangle<float> body (area.getX(), area.getBottom() - bodyH, bodyW, bodyH);
        g.setColour (colour);
        g.fillRoundedRectangle (body, 1.4f);

        const float shackleR = bodyW * 0.32f;
        const float cx = area.getCentreX();
        const float cy = body.getY();
        juce::Path shackle;
        shackle.addCentredArc (cx, cy, shackleR, shackleR, 0.0f,
                               -juce::MathConstants<float>::halfPi,
                               juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.4f));
    }
} // namespace

// ---------------------------------------------------------------------------
// MidiDragSource (ported from MelodyPanel)
// ---------------------------------------------------------------------------

void MelodySidePanel::MidiDragSource::paint (juce::Graphics& g)
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

void MelodySidePanel::MidiDragSource::mouseDrag (const juce::MouseEvent& e)
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
// MelodySidePanel
// ---------------------------------------------------------------------------

MelodySidePanel::MelodySidePanel (const UiShared& sharedContext)
    : shared (sharedContext),
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
    closeButton.setColour (juce::TextButton::buttonColourId, theme::panel);
    closeButton.setColour (juce::TextButton::textColourOffId, theme::textSecondary);
    closeButton.setTooltip ("Close the melody panel");
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

    // Five macro knobs.
    energyKnob     = std::make_unique<ModKnob> (shared, params::melodyEnergy,         "ENERGY",   theme::neonYellow);
    complexityKnob = std::make_unique<ModKnob> (shared, params::melodyComplexity,     "COMPLEX",  theme::neonYellow);
    imageKnob      = std::make_unique<ModKnob> (shared, params::melodyImageInfluence, "IMAGE",    theme::neonYellow);
    repetitionKnob = std::make_unique<ModKnob> (shared, params::melodyRepetition,     "REPEAT",   theme::neonYellow);
    densityKnob    = std::make_unique<ModKnob> (shared, params::melodyDensity,        "DENSITY",  theme::neonYellow);
    addAndMakeVisible (*energyKnob);
    addAndMakeVisible (*complexityKnob);
    addAndMakeVisible (*imageKnob);
    addAndMakeVisible (*repetitionKnob);
    addAndMakeVisible (*densityKnob);

    // Regeneration: fresh material, or a small mutation of the current one; the
    // three locks constrain what either is allowed to change.
    styleChip (regenerateButton, theme::neonYellow);
    regenerateButton.setColour (juce::TextButton::textColourOffId, theme::textPrimary);
    regenerateButton.setTooltip ("Generate a fresh melody (keeps any locked dimension)");
    regenerateButton.onClick = [this] { shared.processor.melodyController().regenerate(); };
    addAndMakeVisible (regenerateButton);

    styleChip (mutateButton, theme::accentMod);
    mutateButton.setTooltip ("Nudge the current melody into a variation (keeps any locked dimension)");
    mutateButton.onClick = [this] { shared.processor.melodyController().mutate(); };
    addAndMakeVisible (mutateButton);

    lockRhythm  = std::make_unique<ParamToggle> (shared, params::melodyLockRhythm,  "RHYTHM",  theme::neonYellow);
    lockPitch   = std::make_unique<ParamToggle> (shared, params::melodyLockPitch,   "PITCH",   theme::neonYellow);
    lockHarmony = std::make_unique<ParamToggle> (shared, params::melodyLockHarmony, "HARMONY", theme::neonYellow);
    lockRhythm->button.setTooltip ("Keep the timing; Regenerate/Mutate change only pitch");
    lockPitch->button.setTooltip ("Keep the pitches; Regenerate/Mutate change only rhythm");
    lockHarmony->button.setTooltip ("Keep the chord progression while pitch/rhythm re-roll");
    addAndMakeVisible (*lockRhythm);
    addAndMakeVisible (*lockPitch);
    addAndMakeVisible (*lockHarmony);

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

MelodySidePanel::~MelodySidePanel() = default;

void MelodySidePanel::setChoiceParam (const char* paramId, int index)
{
    if (auto* p = shared.apvts().getParameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (index)));
        p->endChangeGesture();
    }
}

int MelodySidePanel::choiceParam (const char* paramId) const
{
    if (auto* v = shared.apvts().getRawParameterValue (paramId))
        return static_cast<int> (v->load());
    return 0;
}

void MelodySidePanel::refreshTransportLabel()
{
    playButton.setButtonText (shared.processor.melodyController().isPlaying() ? "STOP" : "PLAY");
}

void MelodySidePanel::resized()
{
    sectionLabels.clear();

    // The panel is a full-height window extension (Phase 3b superseded the
    // fix-B height cap), so the ten sections breathe across the whole content
    // height with the roomy, evenly-spaced layout the mockup shows.
    auto area = getLocalBounds().reduced (16, 16);

    closeButton.setBounds (getWidth() - 30, 12, 20, 20);
    area.removeFromTop (22); // clear the close-button strip

    // Row / captioned-section helpers, mirroring the popup's layout idiom.
    auto row = [&area] (int h, int gap = 18)
    {
        auto r = area.removeFromTop (h);
        area.removeFromTop (gap);
        return r;
    };
    auto section = [&] (const juce::String& caption, int h, int gap = 18)
    {
        auto cap = area.removeFromTop (14);
        sectionLabels.emplace_back (caption, cap);
        area.removeFromTop (4);
        return row (h, gap);
    };

    // Transport (no caption): PLAY with the LOOP chip beside it.
    auto transportRow = row (30, 18);
    loopToggle->setBounds (transportRow.removeFromRight (72));
    transportRow.removeFromRight (8);
    playButton.setBounds (transportRow);

    modeTabs.setBounds    (section ("MODE",   22));
    keyModeTabs.setBounds (section ("KEY",    22));
    lengthTabs.setBounds  (section ("LENGTH", 22));

    // SHAPE: phrase toggle (Melody) and arp direction (Arp) share one row;
    // animate() shows whichever the current mode uses.
    auto shapeRow = section ("SHAPE", 22);
    phraseTabs.setBounds (shapeRow);
    arpPatternTabs.setBounds (shapeRow);

    // FEEL: five macro knobs across the column.
    auto knobs = section ("FEEL", 62);
    const int kw = knobs.getWidth() / 5;
    energyKnob->setBounds     (knobs.removeFromLeft (kw).reduced (2, 0));
    complexityKnob->setBounds (knobs.removeFromLeft (kw).reduced (2, 0));
    imageKnob->setBounds      (knobs.removeFromLeft (kw).reduced (2, 0));
    repetitionKnob->setBounds (knobs.removeFromLeft (kw).reduced (2, 0));
    densityKnob->setBounds    (knobs.reduced (2, 0));

    loopTabs.setBounds (section ("LOOP LENGTH", 22));

    // SEED: the three regeneration locks, then the two regeneration actions.
    auto lockRow = section ("SEED", 22);
    const int lw = lockRow.getWidth() / 3;
    lockRhythm->setBounds  (lockRow.removeFromLeft (lw).withTrimmedRight (4));
    lockPitch->setBounds   (lockRow.removeFromLeft (lw).withTrimmedRight (4));
    lockHarmony->setBounds (lockRow);

    auto actionRow = row (30, 18);
    regenerateButton.setBounds (actionRow.removeFromLeft (actionRow.getWidth() / 2 - 4));
    mutateButton.setBounds     (actionRow.removeFromRight (actionRow.getWidth()));
    regenerateBounds = regenerateButton.getBounds();

    // EXPORT: drag handle + save.
    auto exportRow = section ("EXPORT", 26, 0);
    dragMidi.setBounds (exportRow.removeFromLeft (exportRow.getWidth() / 2 - 4));
    saveButton.setBounds (exportRow.removeFromRight (exportRow.getWidth()));
}

void MelodySidePanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, theme::cornerRadius);
    g.setColour (theme::neonYellow.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme::cornerRadius, 1.0f);

    // Section captions above each control group.
    g.setFont (theme::semiBold (9.5f));
    g.setColour (theme::textSecondary.withAlpha (0.85f));
    for (const auto& [caption, rect] : sectionLabels)
        g.drawText (caption, rect, juce::Justification::centredLeft);
}

void MelodySidePanel::paintOverChildren (juce::Graphics& g)
{
    // Master-seed-lock indicator on REGENERATE: a dim scrim plus a small
    // padlock in the top-right corner, because regenerating with the seed
    // locked and params unchanged intentionally reproduces the same sequence.
    if (! lockedCache || regenerateBounds.isEmpty())
        return;

    const auto b = regenerateBounds.toFloat();
    g.setColour (theme::panel.withAlpha (0.35f));
    g.fillRoundedRectangle (b, theme::wellRadius);

    const juce::Rectangle<float> lockArea (b.getRight() - 15.0f, b.getY() + 5.0f, 8.0f, 9.0f);
    drawLockGlyph (g, lockArea, theme::neonYellow.withAlpha (0.85f));
}

void MelodySidePanel::animate()
{
    refreshTransportLabel();

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

    // REGENERATE lock indicator: repaint its corner when the master lock flips.
    const bool locked = shared.processor.melodyController().locked();
    if (locked != lockedCache)
    {
        lockedCache = locked;
        repaint (regenerateBounds);
    }
}
