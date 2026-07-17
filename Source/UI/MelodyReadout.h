#pragma once

#include "UI/Controls.h"

// The GENERATED info readout shown below the Lens image: KEY / MOOD / FORM /
// SEED rows, then TRANSPOSE and OCTAVE -/+ steppers. Visible only while an
// image is loaded in Lens AND a melody exists (hasImageSource() &&
// hasMelody(), Phase 9) — it's provenance for the current image, so it hides
// the moment that image is gone even if the melody it describes is still
// alive and playable (PLAY/LOOP/export stay independently gated on
// hasMelody() alone, Phases 6-7, and are unaffected by this).
// Ported from the old MelodyPanel popup's summary block (Source/UI/MelodyPanel.cpp,
// now dead code) with the same styling; new here is the SEED row's click-to-edit
// hex value and its master-lock padlock (MelodyController::setSeed()/locked()/
// setLocked() — the master seed lock, distinct from the side panel's per-domain
// RHYTHM/PITCH/HARMONY locks).
class MelodyReadout final : public juce::Component
{
public:
    explicit MelodyReadout (const UiShared& sharedContext);
    ~MelodyReadout() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    // ~60 Hz, polled unconditionally regardless of Play/Deep view
    // (PlayView::animateReadout(), Phase 8). Hides/shows with
    // hasImageSource() && hasMelody() (Phase 9 — supersedes Phase 8's
    // hasMelody()-only rule): the readout is provenance for the *current*
    // Lens image, so it hides the instant that image is gone, even if the
    // melody it described is still alive and playable. Repaints only on
    // change.
    void animate();

private:
    // Seed hex value: single-click to edit (1-8 hex digits enforced by input
    // restriction), commits via setSeed() on Return/losing focus with changes;
    // Label's lossOfFocusDiscardsChanges reverts cleanly if the user clicks
    // away without pressing Return. After any commit or revert the display is
    // resynced to the controller's actual formatted seed.
    class SeedLabel final : public juce::Label
    {
    public:
        std::function<void (juce::uint32)> onSeedApplied;
        std::function<void()> onEditFinished;

        void editorShown (juce::TextEditor* ed) override;
        void textWasEdited() override;
    };

    // Small padlock toggle bound to the master seed lock.
    class LockToggle final : public juce::Component,
                             public juce::SettableTooltipClient
    {
    public:
        explicit LockToggle (const UiShared& s) : shared (s) {}
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        UiShared shared;
    };

    void refreshSeedText();
    void nudgeParam (const char* paramId, int delta, int lo, int hi);
    int  intParam (const char* paramId) const;

    UiShared shared;

    SeedLabel seedLabel;
    LockToggle lockToggle;

    juce::TextButton transposeDown { "-" }, transposeUp { "+" };
    juce::TextButton octaveDown { "-" }, octaveUp { "+" };
    juce::Rectangle<int> transposeLabelArea, octaveLabelArea;

    // Rows painted directly (label + value pairs), matching the old popup's
    // readout: cheap enough not to need attachments or child components.
    juce::Rectangle<int> titleArea, keyRow, moodRow, formRow, seedCaptionArea;

    // hasImageSource() && hasMelody() (Phase 9) — the whole block hides
    // without either. Since visibility itself now requires an image, the
    // seed edit box and lock toggle no longer need their own separate
    // enabled/dim state (Phase 6's per-control image gate is now redundant
    // and was removed): they can never be shown without an image present.
    bool activeCache = false;
    bool lockedCache = false;
    juce::String summaryCache;
    int transposeCache = 0, octaveCache = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyReadout)
};
