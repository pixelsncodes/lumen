#include "UI/PluginEditor.h"

#include "UI/Theme.h"

namespace
{
    constexpr int kBaseWidth = 1040;
    constexpr int kBaseHeight = 660;
} // namespace

bool LumenAudioProcessorEditor::disableOpenGL = false;

LumenAudioProcessorEditor::LumenAudioProcessorEditor (LumenAudioProcessor& processorToUse)
    : juce::AudioProcessorEditor (&processorToUse),
      processor (processorToUse),
      shared { processorToUse, processorToUse.uiTap(), &history, attachedIds, knobRegistry }
{
    setLookAndFeel (&lumenLnf);

    header = std::make_unique<HeaderBar> (shared, [this] (int index) { setView (index); });
    deepView = std::make_unique<DeepView> (shared, history);
    playView = std::make_unique<PlayView> (shared, history, keyboardState);

    content.addAndMakeVisible (*header);
    content.addAndMakeVisible (*playView);
    content.addChildComponent (*deepView);
    content.setBounds (0, 0, kBaseWidth, kBaseHeight);
    header->setBounds (0, 0, kBaseWidth, 48);
    playView->setBounds (0, 48, kBaseWidth, kBaseHeight - 48);
    deepView->setBounds (0, 48, kBaseWidth, kBaseHeight - 48);
    addAndMakeVisible (content);

    keyboardState.addListener (this);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kBaseWidth * 0.7), juce::roundToInt (kBaseHeight * 0.7),
                     kBaseWidth * 2, kBaseHeight * 2);
    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);
    setSize (kBaseWidth, kBaseHeight);

    // Restore the persisted view choice (default: Play — SPEC pillar 1).
    setView (processor.apvts.state.getProperty ("uiView", "play").toString() == "deep" ? 1 : 0);

    if (! disableOpenGL)
    {
        glContext.setContinuousRepainting (false);
        glContext.attachTo (*this);
        glAttached = true;
    }

    startTimerHz (60);
}

LumenAudioProcessorEditor::~LumenAudioProcessorEditor()
{
    if (glAttached)
        glContext.detach();
    stopTimer();
    keyboardState.removeListener (this);
    setLookAndFeel (nullptr);
}

void LumenAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
}

void LumenAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (hudEnabled)
        paintStartMs = juce::Time::getMillisecondCounterHiRes();
    g.fillAll (lumen::theme::well);
}

void LumenAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (! hudEnabled)
        return;

    const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - paintStartMs;
    const auto elapsedUs = static_cast<juce::uint32> (elapsedMs * 1000.0);
    frameCount.fetch_add (1, std::memory_order_relaxed);
    frameSumUs.fetch_add (elapsedUs, std::memory_order_relaxed);
    juce::uint32 previousMax = frameMaxUs.load (std::memory_order_relaxed);
    while (elapsedUs > previousMax
           && ! frameMaxUs.compare_exchange_weak (previousMax, elapsedUs, std::memory_order_relaxed))
    {
    }

    const auto stats = getFrameStats();
    const auto text = juce::String::formatted (
        "frame %.2f ms avg / %.2f ms max (%d frames)  |  audio load %.0f%%  |  dropouts %d",
        stats.averageMs, stats.maxMs, stats.frames,
        processor.currentAudioLoad() * 100.0f, processor.dropoutCount());

    const juce::Rectangle<int> box (8, 52, 470, 22);
    g.setColour (lumen::theme::well.withAlpha (0.85f));
    g.fillRoundedRectangle (box.toFloat(), 4.0f);
    g.setColour (lumen::theme::accentFilter);
    g.setFont (lumen::theme::medium (12.0f));
    g.drawText (text, box.reduced (8, 0), juce::Justification::centredLeft);
}

bool LumenAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'h' || key.getTextCharacter() == 'H')
    {
        setHudEnabled (! hudEnabled);
        return true;
    }
    return false;
}

void LumenAudioProcessorEditor::setView (int index)
{
    const bool deep = index == 1;
    deepView->setVisible (deep);
    playView->setVisible (! deep);
    header->setViewIndex (deep ? 1 : 0);
    processor.apvts.state.setProperty ("uiView", deep ? "deep" : "play", nullptr);
}

void LumenAudioProcessorEditor::setHudEnabled (bool enabled)
{
    hudEnabled = enabled;
    frameCount.store (0);
    frameSumUs.store (0);
    frameMaxUs.store (0);
    repaint();
}

LumenAudioProcessorEditor::FrameStats LumenAudioProcessorEditor::getFrameStats() const
{
    FrameStats stats;
    stats.frames = (int) frameCount.load (std::memory_order_relaxed);
    if (stats.frames > 0)
        stats.averageMs = (double) frameSumUs.load (std::memory_order_relaxed) / stats.frames / 1000.0;
    stats.maxMs = frameMaxUs.load (std::memory_order_relaxed) / 1000.0;
    return stats;
}

juce::StringArray LumenAudioProcessorEditor::missingParameterIds() const
{
    juce::StringArray missing;
    for (auto* parameter : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            if (attachedIds.find (ranged->getParameterID()) == attachedIds.end())
                missing.add (ranged->getParameterID());
    return missing;
}

void LumenAudioProcessorEditor::timerCallback()
{
    ++tick;

    // Drain the processor's lock-free audio tap into the UI-side history.
    float tapChunk[4096];
    const int numRead = processor.readAudioTap (tapChunk, 4096);
    if (numRead > 0)
        history.push (tapChunk, numRead);

    header->animate();
    if (deepView->isVisible())
        deepView->animate (tick % 2 == 0); // FFT at ~30 Hz (SPEC 15)
    else
        playView->animate();

    for (auto* knob : knobRegistry)
        if (knob->isShowing())
            knob->animate();

    // HUD mode: force a full-editor repaint each tick so the measured frame
    // time is the worst-case full redraw.
    if (hudEnabled)
        repaint();
}

void LumenAudioProcessorEditor::handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity)
{
    processor.uiNoteOn (note, velocity);
}

void LumenAudioProcessorEditor::handleNoteOff (juce::MidiKeyboardState*, int, int note, float)
{
    processor.uiNoteOff (note);
}
