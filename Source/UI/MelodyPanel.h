#pragma once

#include "UI/Controls.h"
#include "Melody/MelodySequence.h"

#include <memory>
#include <utility>
#include <vector>

// Shared grid-overlay drawing used by both the Melody overlay's image view and
// the small Lens image view (so the sampling grid, the sounding-cell glow and
// the sampled-path trace look identical in both places).
namespace melodygrid
{
struct DrawInfo
{
    juce::Rectangle<float> imageArea;             // where the image is drawn
    int cols = 8, rows = 8;
    const lumen::melody::Sequence* seq = nullptr; // for the path trace (may be null)
    int liveCol = -1, liveRow = -1;               // currently sounding cell
    float glow = 0.0f;                            // 0..1 highlight intensity
    bool playing = false;
    juce::Colour accent;
};

// Draws the grid lines, the faint sampled-path trace (when stopped), and the
// sounding-cell glow. Assumes g is already clipped to imageArea if desired.
void draw (juce::Graphics& g, const DrawInfo& info);
} // namespace melodygrid

// The Melody overlay panel: the Lens image with the sampling-grid visualization,
// the Lumena generator controls, transport, and MIDI export. Shown as a
// floating card over the editor when the Lens "MELODY" toggle is active.
class MelodyPanel final : public juce::Component
{
public:
    explicit MelodyPanel (const UiShared& sharedContext);
    ~MelodyPanel() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void animate();

private:
    // The image + live sampling-grid visualization.
    class GridView final : public juce::Component
    {
    public:
        explicit GridView (const UiShared& s) : shared (s) {}
        void paint (juce::Graphics& g) override;
        void animate();

    private:
        UiShared shared;
        std::uint32_t lastTrigger = 0;
        float glow = 0.0f;
        int   glowCol = -1, glowRow = -1;
    };

    // A drag handle that exports the melody to a temp .mid and starts an
    // external file drag so it can be dropped straight into a DAW timeline.
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
    GridView grid;

    juce::TextButton playButton { "PLAY" };
    std::unique_ptr<ParamToggle> loopToggle; // wraps playback at the loop end

    // Mode selector + grouped controls.
    TabsBar modeTabs;          // MELODY / CHORDS / ARP
    TabsBar keyModeTabs;       // FROM IMAGE / RANDOM
    TabsBar lengthTabs;        // 8 / 16 / 32  (LENGTH)
    TabsBar phraseTabs;        // PHRASED / FREEFORM (Melody mode)
    TabsBar arpPatternTabs;    // UP / DOWN / UP-DN / CONV / RAND (Arp mode)
    TabsBar loopTabs;          // OFF / 1 / 2 / 4 / 8  (LOOP LENGTH)

    // Four musical macro knobs.
    std::unique_ptr<ModKnob> energyKnob, complexityKnob, imageKnob, repetitionKnob;

    // Regeneration.
    juce::TextButton regenerateButton { "REGENERATE" };
    juce::TextButton mutateButton { "MUTATE" };
    std::unique_ptr<ParamToggle> lockRhythm, lockPitch;

    // Export.
    juce::TextButton saveButton { "SAVE .MID" };
    MidiDragSource dragMidi;

    // Section captions, computed in resized() and drawn in paint().
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sectionLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyPanel)
};
