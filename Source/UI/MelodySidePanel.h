#pragma once

#include "UI/Controls.h"

#include <memory>
#include <utility>
#include <vector>

// The Melody side panel: a window extension carrying the Lumena generator
// controls (transport, mode/key/length/shape, the FEEL macros, loop length,
// the regeneration locks + actions, and MIDI export). It docks in the strip
// immediately to the right of the base 1040x660 canvas (logical x = kBaseWidth)
// at full content height; the editor widens by kPanelWidth to reveal it when
// the Lens "MELODY" toggle is active — the same polled-bool mechanism the old
// popup used. The base UI never moves (no image/grid here; the Lens image keeps
// drawing the sampling-grid overlay in its own panel).
//
// The GENERATED readout (KEY/MOOD/FORM/SEED) and the TRANSPOSE/OCTAVE steppers
// are intentionally NOT here — those move under the Lens image in a later phase
// and still live in the old MelodyPanel until then.
class MelodySidePanel final : public juce::Component
{
public:
    explicit MelodySidePanel (const UiShared& sharedContext);
    ~MelodySidePanel() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void animate();

    // Logical width of the panel strip; the editor widens by this amount when
    // the panel opens and places it just past the base canvas at full content
    // height, then scales the whole content as one unit like every other view.
    static constexpr int kPanelWidth = 316;

private:
    // A drag handle that exports the melody to a temp .mid and starts an
    // external file drag so it can be dropped straight into a DAW timeline.
    // Ported from MelodyPanel::MidiDragSource (same behaviour).
    class MidiDragSource final : public juce::Component
    {
    public:
        explicit MidiDragSource (const UiShared& s) : shared (s)
        {
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }
        void paint (juce::Graphics& g) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    private:
        UiShared shared;
        bool dragging = false;
    };

    void setChoiceParam (const char* paramId, int index);
    int  choiceParam (const char* paramId) const;
    void refreshTransportLabel();

    UiShared shared;

    juce::TextButton closeButton { "x" };

    juce::TextButton playButton { "PLAY" };
    std::unique_ptr<ParamToggle> loopToggle; // wraps playback at the loop end

    // Mode selector + grouped controls (same params/attachments as the popup).
    TabsBar modeTabs;          // MELODY / CHORDS / ARP
    TabsBar keyModeTabs;       // FROM IMAGE / RANDOM
    TabsBar lengthTabs;        // 8 / 16 / 32  (LENGTH)
    TabsBar phraseTabs;        // PHRASED / FREEFORM (Melody mode SHAPE)
    TabsBar arpPatternTabs;    // UP / DOWN / UP-DN / CONV / RAND (Arp mode SHAPE)
    TabsBar loopTabs;          // OFF / 1 / 2 / 4 / 8  (LOOP LENGTH)

    // Five musical macro knobs.
    std::unique_ptr<ModKnob> energyKnob, complexityKnob, imageKnob, repetitionKnob,
                             densityKnob;

    // Regeneration.
    juce::TextButton regenerateButton { "REGENERATE" };
    juce::TextButton mutateButton { "MUTATE" };
    std::unique_ptr<ParamToggle> lockRhythm, lockPitch, lockHarmony;

    // Export.
    juce::TextButton saveButton { "SAVE .MID" };
    MidiDragSource dragMidi;

    // Section captions, computed in resized() and drawn in paint().
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sectionLabels;

    // REGENERATE's master-seed-lock indicator: when the master seed is locked,
    // a small padlock glyph is drawn over the button (paintOverChildren) and
    // the fill dimmed, because regenerating with the seed locked and params
    // unchanged intentionally reproduces the same sequence.
    juce::Rectangle<int> regenerateBounds;
    bool lockedCache = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodySidePanel)
};
