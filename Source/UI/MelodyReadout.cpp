#include "UI/MelodyReadout.h"

#include "Melody/MelodyController.h"
#include "State/Parameters.h"
#include "UI/Theme.h"

using namespace lumen;

namespace
{
    void styleChip (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, theme::well);
        b.setColour (juce::TextButton::buttonOnColourId, theme::neonYellow);
        b.setColour (juce::TextButton::textColourOffId, theme::textSecondary);
        b.setColour (juce::TextButton::textColourOnId, theme::well);
    }

    // Small padlock glyph: a rounded shackle arc over a body rectangle (same
    // shape MelodySidePanel draws over REGENERATE when the master seed is
    // locked — local copy, promote alongside styleChip in the Phase 5 cleanup).
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
// SeedLabel
// ---------------------------------------------------------------------------

void MelodyReadout::SeedLabel::editorShown (juce::TextEditor* ed)
{
    juce::Label::editorShown (ed);
    ed->setInputRestrictions (8, "0123456789abcdefABCDEF");
}

void MelodyReadout::SeedLabel::textWasEdited()
{
    const auto text = getText().trim();
    if (text.isNotEmpty() && onSeedApplied != nullptr)
        onSeedApplied (static_cast<juce::uint32> (text.getHexValue64()));
    if (onEditFinished != nullptr)
        onEditFinished();
}

// ---------------------------------------------------------------------------
// LockToggle
// ---------------------------------------------------------------------------

void MelodyReadout::LockToggle::paint (juce::Graphics& g)
{
    // No enabled/dim state of its own: this toggle only exists while the
    // whole readout is visible, which (Phase 9) already requires an image to
    // be loaded — so it's never shown in a state where it'd need to dim.
    const bool locked = shared.processor.melodyController().locked();
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (locked ? theme::neonYellow.withAlpha (0.22f) : theme::well.withAlpha (0.75f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (locked ? theme::neonYellow : theme::hairline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    drawLockGlyph (g, bounds.reduced (bounds.getWidth() * 0.28f),
                   locked ? theme::neonYellow : theme::textSecondary);
}

void MelodyReadout::LockToggle::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    auto& m = shared.processor.melodyController();
    m.setLocked (! m.locked());
}

// ---------------------------------------------------------------------------
// MelodyReadout
// ---------------------------------------------------------------------------

MelodyReadout::MelodyReadout (const UiShared& sharedContext)
    : shared (sharedContext), lockToggle (sharedContext)
{
    seedLabel.setEditable (true, false, true); // single click; focus-loss without Return discards
    seedLabel.setJustificationType (juce::Justification::centredLeft);
    seedLabel.setFont (theme::font (11.0f));
    seedLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    seedLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    seedLabel.setMouseCursor (juce::MouseCursor::IBeamCursor);
    seedLabel.setTooltip ("Click to type an 8-digit hex seed");
    seedLabel.onSeedApplied = [this] (juce::uint32 v)
    {
        shared.processor.melodyController().setSeed (v);
    };
    seedLabel.onEditFinished = [this] { refreshSeedText(); };
    addAndMakeVisible (seedLabel);

    lockToggle.setTooltip ("Lock the master seed: REGENERATE reproduces the same melody");
    addAndMakeVisible (lockToggle);

    styleChip (transposeDown);
    styleChip (transposeUp);
    transposeDown.setTooltip ("Shift playback and export down a semitone (never regenerates)");
    transposeUp.setTooltip ("Shift playback and export up a semitone (never regenerates)");
    transposeDown.onClick = [this] { nudgeParam (params::melodyTranspose, -1, -12, 12); };
    transposeUp.onClick   = [this] { nudgeParam (params::melodyTranspose, +1, -12, 12); };
    addAndMakeVisible (transposeDown);
    addAndMakeVisible (transposeUp);

    styleChip (octaveDown);
    styleChip (octaveUp);
    octaveDown.setTooltip ("Shift playback and export down an octave (never regenerates)");
    octaveUp.setTooltip ("Shift playback and export up an octave (never regenerates)");
    octaveDown.onClick = [this] { nudgeParam (params::melodyOctave, -1, -2, 2); };
    octaveUp.onClick   = [this] { nudgeParam (params::melodyOctave, +1, -2, 2); };
    addAndMakeVisible (octaveDown);
    addAndMakeVisible (octaveUp);

    shared.registerAttachment (params::melodyTranspose);
    shared.registerAttachment (params::melodyOctave);

    setVisible (false); // hidden until animate() sees hasMelody()
}

MelodyReadout::~MelodyReadout() = default;

void MelodyReadout::nudgeParam (const char* paramId, int delta, int lo, int hi)
{
    if (auto* p = shared.apvts().getParameter (paramId))
    {
        const int current = juce::roundToInt (p->convertFrom0to1 (p->getValue()));
        const int next = juce::jlimit (lo, hi, current + delta);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (next)));
        p->endChangeGesture();
    }
}

int MelodyReadout::intParam (const char* paramId) const
{
    if (auto* v = shared.apvts().getRawParameterValue (paramId))
        return juce::roundToInt (v->load());
    return 0;
}

void MelodyReadout::refreshSeedText()
{
    auto& m = shared.processor.melodyController();
    const auto hex = juce::String::toHexString (static_cast<juce::uint32> (m.seed()))
                          .paddedLeft ('0', 8).toLowerCase();
    seedLabel.setText (hex, juce::dontSendNotification);
}

void MelodyReadout::resized()
{
    auto area = getLocalBounds().reduced (8, 6);

    titleArea = area.removeFromTop (13);
    area.removeFromTop (3);

    auto rowH = 16;
    keyRow  = area.removeFromTop (rowH); area.removeFromTop (1);
    moodRow = area.removeFromTop (rowH); area.removeFromTop (1);
    formRow = area.removeFromTop (rowH); area.removeFromTop (1);

    auto seedRow = area.removeFromTop (18);
    seedCaptionArea = seedRow.removeFromLeft (38);
    lockToggle.setBounds (seedRow.removeFromRight (18));
    seedRow.removeFromRight (6);
    seedLabel.setBounds (seedRow);

    area.removeFromTop (8);

    auto transposeRow = area.removeFromTop (20);
    transposeDown.setBounds (transposeRow.removeFromLeft (26));
    transposeUp.setBounds (transposeRow.removeFromRight (26));
    transposeLabelArea = transposeRow.reduced (4, 0);

    area.removeFromTop (5);

    auto octaveRow = area.removeFromTop (20);
    octaveDown.setBounds (octaveRow.removeFromLeft (26));
    octaveUp.setBounds (octaveRow.removeFromRight (26));
    octaveLabelArea = octaveRow.reduced (4, 0);
}

void MelodyReadout::paint (juce::Graphics& g)
{
    auto& m = shared.processor.melodyController();
    const juce::String dash = juce::String::fromUTF8 ("\xe2\x80\x94");
    const auto value = [&dash] (const juce::String& v) { return v.isNotEmpty() ? v : dash; };

    g.setColour (theme::textSecondary.withAlpha (0.85f));
    g.setFont (theme::semiBold (9.5f));
    g.drawText ("GENERATED", titleArea, juce::Justification::centredLeft);

    auto drawRow = [&g] (juce::Rectangle<int> row, const char* label, const juce::String& text)
    {
        auto line = row;
        g.setColour (theme::textMuted);
        g.setFont (theme::semiBold (9.5f));
        g.drawText (label, line.removeFromLeft (38), juce::Justification::centredLeft);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (11.0f));
        g.drawText (text, line, juce::Justification::centredLeft);
    };
    drawRow (keyRow,  "KEY",  value (m.detectedKey()));
    drawRow (moodRow, "MOOD", value (m.moodText()));
    drawRow (formRow, "FORM", value (m.formText()));

    g.setColour (theme::textMuted);
    g.setFont (theme::semiBold (9.5f));
    g.drawText ("SEED", seedCaptionArea, juce::Justification::centredLeft);

    // Transpose / octave readouts, drawn between their own +/- chips (label
    // left-justified, value right-justified, same rect — matches the old
    // popup's readout exactly).
    const int t = intParam (params::melodyTranspose);
    g.setColour (theme::textMuted);
    g.setFont (theme::semiBold (9.5f));
    g.drawText ("TRANSPOSE", transposeLabelArea, juce::Justification::centredLeft);
    g.setColour (theme::textSecondary);
    g.setFont (theme::medium (11.0f));
    g.drawText ((t > 0 ? "+" : "") + juce::String (t) + " st",
                transposeLabelArea, juce::Justification::centredRight);

    const int o = intParam (params::melodyOctave);
    g.setColour (theme::textMuted);
    g.setFont (theme::semiBold (9.5f));
    g.drawText ("OCTAVE", octaveLabelArea, juce::Justification::centredLeft);
    g.setColour (theme::textSecondary);
    g.setFont (theme::medium (11.0f));
    g.drawText ((o > 0 ? "+" : "") + juce::String (o) + " oct",
                octaveLabelArea, juce::Justification::centredRight);
}

void MelodyReadout::animate()
{
    auto& m = shared.processor.melodyController();
    // Phase 9: requires the image too, not just the melody — the readout is
    // provenance for the *current* Lens image, so it hides the instant that
    // image is gone even if the melody it describes is still playable/
    // exportable (that stays gated on hasMelody() alone in MelodySidePanel,
    // Phases 6-7, untouched by this).
    const bool active = m.hasImageSource() && m.hasMelody();
    if (active != activeCache)
    {
        activeCache = active;
        setVisible (active);
        if (active)
            refreshSeedText();
    }
    if (! active)
        return;

    juce::String composed;
    composed << m.detectedKey() << '|' << m.moodText() << '|' << m.formText() << '|' << m.seed();
    if (composed != summaryCache)
    {
        summaryCache = composed;
        if (! seedLabel.isBeingEdited())
            refreshSeedText();
        repaint();
    }

    const bool locked = m.locked();
    if (locked != lockedCache)
    {
        lockedCache = locked;
        lockToggle.repaint();
    }

    const int t = intParam (params::melodyTranspose);
    if (t != transposeCache)
    {
        transposeCache = t;
        repaint (transposeLabelArea);
    }

    const int o = intParam (params::melodyOctave);
    if (o != octaveCache)
    {
        octaveCache = o;
        repaint (octaveLabelArea);
    }
}
