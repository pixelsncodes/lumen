#include "UI/PluginEditor.h"

#include "State/Parameters.h"

namespace
{
    namespace colours
    {
        const juce::Colour panel        (0xff1c1c1f);
        const juce::Colour well         (0xff0f0f12);
        const juce::Colour hairline     (0xff2a2a2e);
        const juce::Colour textPrimary  (0xffe8e6e3);
        const juce::Colour textSecondary(0xff9a9aa2);
        const juce::Colour textMuted    (0xff6f6f76);
    }

    constexpr int kBaseWidth  = 1040;
    constexpr int kBaseHeight = 660;
} // namespace

LumenAudioProcessorEditor::LumenAudioProcessorEditor (LumenAudioProcessor& processorToUse)
    : juce::AudioProcessorEditor (&processorToUse),
      processor (processorToUse)
{
    const juce::Colour accentA      (0xffff7a45); // orange — Osc A / Macro 1
    const juce::Colour accentB      (0xff4aa8ff); // blue   — Osc B / Macro 2
    const juce::Colour accentFilter (0xff3ddc84); // green  — filter + FX / Macro 3
    const juce::Colour accentMod    (0xffc9c9cf); // warm gray — modulation / Macro 4

    setUpKnob (macroKnobs[0], lumen::params::macro1, "Tone",    accentA);
    setUpKnob (macroKnobs[1], lumen::params::macro2, "Motion",  accentB);
    setUpKnob (macroKnobs[2], lumen::params::macro3, "Space",   accentFilter);
    setUpKnob (macroKnobs[3], lumen::params::macro4, "Texture", accentMod);

    setUpKnob (utilityKnobs[0], lumen::params::filterCutoff, "Cutoff",     accentFilter);
    setUpKnob (utilityKnobs[1], lumen::params::filterRes,    "Resonance",  accentFilter);
    setUpKnob (utilityKnobs[2], lumen::params::delayMix,     "Delay Mix",  accentFilter);
    setUpKnob (utilityKnobs[3], lumen::params::reverbMix,    "Reverb Mix", accentFilter);
    setUpKnob (utilityKnobs[4], lumen::params::masterGain,   "Master",     accentMod);

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kBaseWidth * 0.7), juce::roundToInt (kBaseHeight * 0.7),
                     kBaseWidth * 2, kBaseHeight * 2);
    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);

    setSize (kBaseWidth, kBaseHeight);
}

void LumenAudioProcessorEditor::setUpKnob (Knob& knob, const char* paramID, const juce::String& text, juce::Colour accent)
{
    auto& s = knob.slider;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 88, 18);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, colours::hairline);
    s.setColour (juce::Slider::thumbColourId, accent);
    s.setColour (juce::Slider::textBoxTextColourId, colours::textPrimary);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    knob.label.setText (text, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, colours::textSecondary);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramID, s);
}

void LumenAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float scale = (float) getWidth() / (float) kBaseWidth;

    g.fillAll (colours::panel);

    auto header = getLocalBounds().removeFromTop (juce::roundToInt (64.0f * scale));
    g.setColour (colours::well);
    g.fillRect (header);
    g.setColour (colours::hairline);
    g.fillRect (0, header.getBottom(), getWidth(), juce::jmax (1, juce::roundToInt (scale)));

    auto headerText = header.reduced (juce::roundToInt (24.0f * scale), 0);
    g.setColour (colours::textPrimary);
    g.setFont (juce::Font (juce::FontOptions ((float) juce::roundToInt (22.0f * scale), juce::Font::bold)));
    g.drawText ("LUMEN", headerText, juce::Justification::centredLeft);

    g.setColour (colours::textMuted);
    g.setFont (juce::Font (juce::FontOptions ((float) juce::roundToInt (13.0f * scale))));
    g.drawText ("wavetable synthesizer - phase 1 skeleton", headerText, juce::Justification::centredRight);

    g.setColour (colours::textMuted);
    g.setFont (juce::Font (juce::FontOptions ((float) juce::roundToInt (12.0f * scale))));
    auto sectionY = juce::roundToInt (86.0f * scale);
    g.drawText ("MACROS", juce::roundToInt (24.0f * scale), sectionY, juce::roundToInt (200.0f * scale),
                juce::roundToInt (16.0f * scale), juce::Justification::centredLeft);
}

void LumenAudioProcessorEditor::layoutKnobRow (juce::Rectangle<int> row, Knob* knobs, int count, float scale)
{
    const int cellWidth = row.getWidth() / count;

    for (int i = 0; i < count; ++i)
    {
        auto cell = row.removeFromLeft (cellWidth).reduced (juce::roundToInt (12.0f * scale), 0);
        knobs[i].label.setBounds (cell.removeFromTop (juce::roundToInt (20.0f * scale)));
        knobs[i].slider.setBounds (cell);
    }
}

void LumenAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseWidth;

    auto area = getLocalBounds();
    area.removeFromTop (juce::roundToInt (104.0f * scale));   // header + section label
    area.reduce (juce::roundToInt (24.0f * scale), juce::roundToInt (8.0f * scale));

    auto macroRow = area.removeFromTop (juce::roundToInt (280.0f * scale));
    layoutKnobRow (macroRow, macroKnobs.data(), (int) macroKnobs.size(), scale);

    area.removeFromTop (juce::roundToInt (32.0f * scale));
    auto utilityRow = area.removeFromTop (juce::roundToInt (200.0f * scale));
    layoutKnobRow (utilityRow, utilityKnobs.data(), (int) utilityKnobs.size(), scale);
}
