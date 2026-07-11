#pragma once

#include <juce_opengl/juce_opengl.h>

#include "UI/Cards.h"
#include "UI/LumenLookAndFeel.h"
#include "UI/MelodyPanel.h"

#include <atomic>
#include <cstdint>
#include <functional>

// Phase 5 editor: Play + Deep views (SPEC 14), OpenGL-accelerated with an
// identical software paint path (all drawing goes through paint(), so
// --screenshot snapshots match what OpenGL renders), 60 Hz UI clock that
// drains the processor's audio tap and animates the visible visualizers,
// drag-and-drop modulation, resizing 70-200% via a whole-content scale
// transform, and a frame-time debug HUD (toggle: H key).
class LumenAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        public juce::DragAndDropContainer,
                                        public juce::FileDragAndDropTarget,
                                        private juce::Timer,
                                        private juce::MidiKeyboardState::Listener
{
public:
    explicit LumenAudioProcessorEditor (LumenAudioProcessor& processorToUse);
    ~LumenAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Drop an image ANYWHERE on the window (SPEC 13): forwards to the
    // processor's LensController.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    // 0 = play, 1 = deep. Persisted as a state property ("uiView").
    void setView (int index);

    // --- verification hooks (SPEC 18 harness) ---------------------------
    struct FrameStats
    {
        int frames = 0;
        double averageMs = 0.0;
        double maxMs = 0.0;
    };
    void setHudEnabled (bool enabled);
    FrameStats getFrameStats() const;
    juce::StringArray missingParameterIds() const; // --check-params

    // Open the branded header menus for a --screenshot run (SPEC 18).
    void showPresetMenu();
    void showGearMenu();

    // Display a control's tooltip for a --screenshot run (tooltips pass):
    // finds the knob attached to paramId and pins its tip next to it.
    bool showTooltipFor (const juce::String& paramId);

    // Screenshot/headless runs skip the OpenGL context (software path only —
    // identical output by construction). Set before creating the editor.
    static bool disableOpenGL;

    // Header chrome: -1 = auto (standalone app -> window controls, plugin ->
    // none), 0 = force plugin chrome, 1 = force standalone chrome. The
    // --screenshot harness forces a value to capture either header. Set before
    // creating the editor.
    static int chromeOverride;

    // Set by the standalone app so the header gear can open the audio/MIDI
    // settings dialog without the shared UI depending on standalone headers.
    static std::function<void()> standaloneSettingsHook;

private:
    void timerCallback() override;
    void handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int, int note, float) override;

    LumenAudioProcessor& processor;
    LumenLookAndFeel lumenLnf;

    juce::MidiKeyboardState keyboardState;
    AudioHistory history;
    std::set<juce::String> attachedIds;
    std::vector<ModKnob*> knobRegistry;
    UiShared shared;

    juce::Component content; // fixed 1040x660, scaled by transform on resize
    std::unique_ptr<HeaderBar> header;
    std::unique_ptr<DeepView> deepView;
    std::unique_ptr<PlayView> playView;
    std::unique_ptr<MelodyPanel> melodyPanel; // floating overlay, on top of the views
    juce::TooltipWindow tooltipWindow { &content }; // parented: tips render inside the editor

    juce::OpenGLContext glContext;
    bool glAttached = false;

    int tick = 0;
    bool applyingExternalMidi = false; // guards keyboardState listener re-entry
    // Notes the internal melody/chord/arp player currently has lit on the
    // keyboard (channel 2), so each timer tick only diffs against the player's
    // published sounding set. Kept separate from live host/hardware MIDI
    // (channel 1) so neither source clears the other's keys.
    std::uint32_t melodyLitMask[4] = { 0, 0, 0, 0 };

    // Lens drop feedback (overlay while dragging, brief message after).
    bool fileDragOver = false;
    int dropMessageFrames = 0;
    juce::String dropMessage;

    // Frame-time instrumentation (written on the paint thread, read from the
    // message thread by --stress; HUD text is drawn on the paint thread).
    bool hudEnabled = false;
    double paintStartMs = 0.0;
    std::atomic<juce::uint32> frameCount { 0 };
    std::atomic<juce::uint64> frameSumUs { 0 };
    std::atomic<juce::uint32> frameMaxUs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenAudioProcessorEditor)
};
