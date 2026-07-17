#pragma once

#include "UI/Controls.h"

// The GENERATED info readout shown below the Lens image once a melody exists:
// KEY / MOOD / FORM / SEED rows, then TRANSPOSE and OCTAVE -/+ steppers.
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
    void animate(); // ~60 Hz; hides/shows with hasMelody(), repaints only on change

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

    bool activeCache = false;   // hasMelody() — the whole block hides without it
    bool lockedCache = false;
    juce::String summaryCache;
    int transposeCache = 0, octaveCache = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyReadout)
};
