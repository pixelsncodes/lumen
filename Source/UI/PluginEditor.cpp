#include "UI/PluginEditor.h"

#include "State/ModState.h"
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
    constexpr int kRowHeight  = 26;
} // namespace

LumenAudioProcessorEditor::MatrixRow::MatrixRow (juce::ValueTree slotTree,
                                                 const juce::StringArray& destNames)
    : slot (slotTree)
{
    const auto& sources = lumen::modstate::sourceNames();
    for (int i = 0; i < sources.size(); ++i)
        source.addItem (sources[i], i + 1);

    dest.addItem ("- none -", 1);
    for (int i = 0; i < destNames.size(); ++i)
        dest.addItem (destNames[i], i + 2);

    depth.setSliderStyle (juce::Slider::LinearHorizontal);
    depth.setRange (-1.0, 1.0, 0.01);
    depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
    depth.setDoubleClickReturnValue (true, 0.0);

    // Initial values from the tree
    const int sourceIndex = lumen::modstate::sourceFromToken (slot["source"].toString());
    source.setSelectedId (sourceIndex >= 0 ? sourceIndex + 1 : 4 /* LFO 1 */, juce::dontSendNotification);
    const int destIndex = lumen::modstate::destFromToken (slot["dest"].toString());
    dest.setSelectedId (destIndex >= 0 ? destIndex + 2 : 1, juce::dontSendNotification);
    depth.setValue (static_cast<double> (slot["depth"]), juce::dontSendNotification);
    enabled.setToggleState (static_cast<bool> (slot["enabled"]), juce::dontSendNotification);

    source.onChange = [this] { slot.setProperty ("source", lumen::modstate::sourceTokens()[source.getSelectedId() - 1], nullptr); };
    dest.onChange = [this]
    {
        const int id = dest.getSelectedId();
        slot.setProperty ("dest", id <= 1 ? juce::String() : lumen::modstate::destTokens()[id - 2], nullptr);
    };
    depth.onValueChange = [this] { slot.setProperty ("depth", depth.getValue(), nullptr); };
    enabled.onClick = [this] { slot.setProperty ("enabled", enabled.getToggleState(), nullptr); };

    addAndMakeVisible (source);
    addAndMakeVisible (dest);
    addAndMakeVisible (depth);
    addAndMakeVisible (enabled);
}

void LumenAudioProcessorEditor::MatrixRow::resized()
{
    auto r = getLocalBounds().reduced (2);
    enabled.setBounds (r.removeFromLeft (24));
    source.setBounds (r.removeFromLeft (130).reduced (2, 1));
    dest.setBounds (r.removeFromLeft (190).reduced (2, 1));
    depth.setBounds (r.reduced (2, 0));
}

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

    // Destination display names from the actual parameters.
    juce::StringArray destNames;
    for (const auto& token : lumen::modstate::destTokens())
    {
        auto* parameter = processor.apvts.getParameter (token);
        destNames.add (parameter != nullptr ? parameter->getName (64) : token);
    }

    auto matrix = processor.apvts.state.getChildWithName ("MODMATRIX");
    for (int i = 0; i < matrix.getNumChildren(); ++i)
    {
        auto* row = matrixRows.add (new MatrixRow (matrix.getChild (i), destNames));
        matrixContent.addAndMakeVisible (row);
    }
    matrixViewport.setViewedComponent (&matrixContent, false);
    matrixViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (matrixViewport);

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
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 88, 16);
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

    auto header = getLocalBounds().removeFromTop (juce::roundToInt (56.0f * scale));
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
    g.drawText ("wavetable synthesizer - phase 4 effects", headerText, juce::Justification::centredRight);

    g.setFont (juce::Font (juce::FontOptions ((float) juce::roundToInt (12.0f * scale))));
    const int labelX = juce::roundToInt (24.0f * scale);
    const int labelW = juce::roundToInt (300.0f * scale);
    const int labelH = juce::roundToInt (14.0f * scale);
    g.drawText ("MACROS + KNOB PAGE 1", labelX, juce::roundToInt (64.0f * scale), labelW, labelH,
                juce::Justification::centredLeft);
    g.drawText ("MOD MATRIX (temporary editor - drag-drop arrives in phase 5)",
                labelX, juce::roundToInt (400.0f * scale), juce::roundToInt (500.0f * scale), labelH,
                juce::Justification::centredLeft);
}

void LumenAudioProcessorEditor::layoutKnobRow (juce::Rectangle<int> row, Knob* knobs, int count, float scale)
{
    const int cellWidth = row.getWidth() / count;

    for (int i = 0; i < count; ++i)
    {
        auto cell = row.removeFromLeft (cellWidth).reduced (juce::roundToInt (12.0f * scale), 0);
        knobs[i].label.setBounds (cell.removeFromTop (juce::roundToInt (16.0f * scale)));
        knobs[i].slider.setBounds (cell);
    }
}

void LumenAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseWidth;

    auto area = getLocalBounds();
    area.removeFromTop (juce::roundToInt (82.0f * scale)); // header + section label
    area.reduce (juce::roundToInt (24.0f * scale), 0);

    auto macroRow = area.removeFromTop (juce::roundToInt (150.0f * scale));
    layoutKnobRow (macroRow, macroKnobs.data(), (int) macroKnobs.size(), scale);

    auto utilityRow = area.removeFromTop (juce::roundToInt (150.0f * scale));
    layoutKnobRow (utilityRow, utilityKnobs.data(), (int) utilityKnobs.size(), scale);

    area.removeFromTop (juce::roundToInt (36.0f * scale)); // matrix label
    auto matrixArea = area.reduced (0, juce::roundToInt (4.0f * scale));
    matrixViewport.setBounds (matrixArea);

    const int rowH = kRowHeight;
    matrixContent.setSize (matrixViewport.getWidth() - matrixViewport.getScrollBarThickness(),
                           rowH * matrixRows.size());
    for (int i = 0; i < matrixRows.size(); ++i)
        matrixRows[i]->setBounds (0, i * rowH, matrixContent.getWidth(), rowH);
}
