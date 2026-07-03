#include "UI/Cards.h"

#include "Lens/LensController.h"
#include "UI/Theme.h"

using namespace lumen;

// ---------------------------------------------------------------------------
// CardPanel
// ---------------------------------------------------------------------------

CardPanel::CardPanel (juce::String titleText, juce::Colour accentColour)
    : title (std::move (titleText)), accent (accentColour)
{
}

void CardPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, theme::cornerRadius);
    g.setColour (theme::hairline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme::cornerRadius, 1.0f);

    if (title.isNotEmpty())
    {
        g.setColour (accent);
        g.fillRoundedRectangle (10.0f, 8.0f, 3.0f, 12.0f, 1.5f);
        g.setColour (theme::textPrimary);
        g.setFont (theme::semiBold (13.0f));
        g.drawText (title, 19, 5, 200, 16, juce::Justification::centredLeft);
    }
}

// ---------------------------------------------------------------------------
// OscCard
// ---------------------------------------------------------------------------

OscCard::OscCard (const UiShared& shared, const juce::String& prefix,
                  const juce::String& name, juce::Colour accentColour)
    : CardPanel (name, accentColour),
      power (shared, prefix + "Enabled", "", accentColour, true),
      table (shared, prefix + "Table"),
      phaseRandom (shared, prefix + "PhaseRand", "RND", accentColour),
      stack (shared, prefix + "Table", prefix + "Morph", accentColour)
{
    addAndMakeVisible (power);
    addAndMakeVisible (table);
    addAndMakeVisible (phaseRandom);
    addAndMakeVisible (stack);

    const std::pair<const char*, const char*> knobDefs[] = {
        { "Morph", "Morph" }, { "Level", "Level" }, { "Pan", "Pan" },   { "Semi", "Semi" },
        { "Fine", "Fine" },   { "Unison", "Uni" },  { "Detune", "Det" },{ "Width", "Width" },
        { "Blend", "Blend" },
    };
    for (const auto& [suffix, label] : knobDefs)
        addAndMakeVisible (knobs.add (new ModKnob (shared, prefix + suffix, label, accentColour)));
}

void OscCard::resized()
{
    power.setBounds (getWidth() - 20, 5, 16, 16);
    table.setBounds (getWidth() - 200, 4, 128, 18);
    phaseRandom.setBounds (getWidth() - 250, 5, 44, 16);
    stack.setBounds (8, 26, 150, 170);

    const int columns = 4, cellW = 51, cellH = 57;
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (164 + (i % columns) * cellW, 26 + (i / columns) * cellH, cellW, cellH);
}

void OscCard::animate()
{
    stack.animate();
}

// ---------------------------------------------------------------------------
// SubNoiseCard
// ---------------------------------------------------------------------------

SubNoiseCard::SubNoiseCard (const UiShared& shared)
    : CardPanel ("SUB / NOISE", theme::accentA),
      subWave (shared, "subWave"),
      subOctave (shared, "subOctave"),
      noiseType (shared, "noiseType"),
      subLevel (shared, "subLevel", "Level", theme::accentA),
      noiseLevel (shared, "noiseLevel", "Level", theme::accentA)
{
    for (auto* child : std::initializer_list<juce::Component*> {
             &subWave, &subOctave, &noiseType, &subLevel, &noiseLevel })
        addAndMakeVisible (child);
}

void SubNoiseCard::resized()
{
    subWave.setBounds (10, 52, 86, 18);
    subOctave.setBounds (102, 52, 70, 18);
    subLevel.setBounds (182, 32, 66, 70);

    noiseType.setBounds (10, 148, 86, 18);
    noiseLevel.setBounds (182, 122, 66, 70);
}

void SubNoiseCard::paint (juce::Graphics& g)
{
    CardPanel::paint (g);
    g.setColour (theme::textMuted);
    g.setFont (theme::medium (11.0f));
    g.drawText ("SUB", 10, 32, 100, 12, juce::Justification::centredLeft);
    g.drawText ("NOISE", 10, 128, 100, 12, juce::Justification::centredLeft);
    g.setColour (theme::hairline);
    g.drawHorizontalLine (112, 8.0f, (float) getWidth() - 8.0f);
}

// ---------------------------------------------------------------------------
// FilterCard
// ---------------------------------------------------------------------------

FilterCard::FilterCard (const UiShared& shared)
    : CardPanel ("FILTER", theme::accentFilter),
      mode (shared, "filterMode"),
      spectrum (shared, *shared.history)
{
    addAndMakeVisible (mode);
    addAndMakeVisible (spectrum);

    const std::pair<const char*, const char*> knobDefs[] = {
        { "filterCutoff", "Cutoff" }, { "filterRes", "Res" },   { "filterDrive", "Drive" },
        { "filterKeytrack", "Key" },  { "filterEnvAmount", "Env 2" },
    };
    for (const auto& [id, label] : knobDefs)
        addAndMakeVisible (knobs.add (new ModKnob (shared, id, label, theme::accentFilter)));
}

void FilterCard::resized()
{
    mode.setBounds (getWidth() - 128, 4, 120, 18);
    spectrum.setBounds (8, 26, 330, 146);

    const int columns = 3, cellW = 54, cellH = 73;
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (346 + (i % columns) * cellW, 26 + (i / columns) * cellH, cellW, cellH);
}

void FilterCard::animate (bool fftTick)
{
    spectrum.animate (fftTick);
}

// ---------------------------------------------------------------------------
// FxRack
// ---------------------------------------------------------------------------

FxRack::FxRack (const UiShared& shared)
    : CardPanel ("EFFECTS", theme::accentFilter)
{
    const auto fxAccent = theme::accentFilter;

    auto addKnob = [&] (Strip& strip, const char* id, const char* label)
    {
        strip.controls.add (new ModKnob (shared, id, label, fxAccent));
        strip.widths.push_back (46);
    };
    auto addChip = [&] (Strip& strip, const char* id, const char* text, int width)
    {
        strip.controls.add (new ParamToggle (shared, id, text, fxAccent));
        strip.widths.push_back (width);
    };
    auto addCombo = [&] (Strip& strip, const char* id, int width)
    {
        strip.controls.add (new ChoiceCombo (shared, id));
        strip.widths.push_back (width);
    };

    strips[0].name = "DRIVE";
    strips[0].power = std::make_unique<ParamToggle> (shared, "driveEnabled", "", accent, true);
    addKnob (strips[0], "driveAmount", "Amt");
    addKnob (strips[0], "driveTone", "Tone");

    strips[1].name = "CHORUS";
    strips[1].power = std::make_unique<ParamToggle> (shared, "chorusEnabled", "", accent, true);
    addKnob (strips[1], "chorusRate", "Rate");
    addKnob (strips[1], "chorusDepth", "Depth");
    addKnob (strips[1], "chorusMix", "Mix");

    strips[2].name = "DELAY";
    strips[2].power = std::make_unique<ParamToggle> (shared, "delayEnabled", "", accent, true);
    addChip (strips[2], "delaySync", "SYNC", 42);
    addCombo (strips[2], "delayDiv", 56);
    addKnob (strips[2], "delayTime", "Time");
    addKnob (strips[2], "delayFeedback", "FB");
    addKnob (strips[2], "delayDamp", "Damp");
    addChip (strips[2], "delayPingPong", "PP", 34);
    addKnob (strips[2], "delayMix", "Mix");

    strips[3].name = "REVERB";
    strips[3].power = std::make_unique<ParamToggle> (shared, "reverbEnabled", "", accent, true);
    addKnob (strips[3], "reverbSize", "Size");
    addKnob (strips[3], "reverbDamp", "Damp");
    addKnob (strips[3], "reverbWidth", "Width");
    addKnob (strips[3], "reverbMix", "Mix");

    for (auto& strip : strips)
    {
        addAndMakeVisible (*strip.power);
        for (auto* control : strip.controls)
            addAndMakeVisible (control);
    }
}

void FxRack::resized()
{
    const int stripH = 38;
    for (int s = 0; s < 4; ++s)
    {
        const int y = 24 + s * (stripH + 1);
        strips[s].power->setBounds (8, y + (stripH - 14) / 2, 14, 14);

        int x = 88;
        for (int c = 0; c < strips[s].controls.size(); ++c)
        {
            auto* control = strips[s].controls[c];
            const int w = strips[s].widths[(size_t) c];
            if (dynamic_cast<ModKnob*> (control) != nullptr)
                control->setBounds (x, y, w, stripH);
            else if (dynamic_cast<ChoiceCombo*> (control) != nullptr)
                control->setBounds (x, y + (stripH - 17) / 2, w, 17);
            else
                control->setBounds (x, y + (stripH - 15) / 2, w, 15);
            x += w + 5;
        }
    }
}

void FxRack::paint (juce::Graphics& g)
{
    CardPanel::paint (g);
    g.setFont (theme::medium (11.0f));
    const int stripH = 38;
    for (int s = 0; s < 4; ++s)
    {
        const int y = 24 + s * (stripH + 1);
        g.setColour (theme::textSecondary);
        g.drawText (strips[s].name, 26, y, 58, stripH, juce::Justification::centredLeft);
        if (s > 0)
        {
            g.setColour (theme::hairline.withAlpha (0.7f));
            g.drawHorizontalLine (y - 1, 8.0f, (float) getWidth() - 8.0f);
        }
    }
}

// ---------------------------------------------------------------------------
// EnvCard
// ---------------------------------------------------------------------------

EnvCard::EnvCard (const UiShared& shared)
    : CardPanel ("ENVELOPES", theme::neonYellow),
      tabs ({ "1", "2", "3" }, [this] (int index) { setActiveEnv (index); })
{
    addAndMakeVisible (tabs);

    for (int e = 0; e < 3; ++e)
    {
        addChildComponent (editors.add (new EnvelopeEditor (shared, e, theme::neonYellow)));

        auto* set = knobSets.add (new juce::OwnedArray<ModKnob>());
        const juce::String prefix = "env" + juce::String (e + 1);
        const std::pair<const char*, const char*> knobDefs[] = {
            { "Attack", "A" }, { "Decay", "D" }, { "Sustain", "S" },
            { "Release", "R" }, { "Curve", "Crv" },
        };
        for (const auto& [suffix, label] : knobDefs)
        {
            auto* knob = set->add (new ModKnob (shared, prefix + suffix, label, theme::neonYellow));
            addChildComponent (knob);
        }
    }
    setActiveEnv (0);
}

void EnvCard::resized()
{
    tabs.setBounds (getWidth() - 116, 4, 108, 17);

    for (int e = 0; e < 3; ++e)
    {
        editors[e]->setBounds (8, 24, 224, 92);
        for (int k = 0; k < knobSets[e]->size(); ++k)
            (*knobSets[e])[k]->setBounds (240 + k * 34, 24, 34, 92);
    }
}

void EnvCard::setActiveEnv (int index)
{
    active = index;
    for (int e = 0; e < 3; ++e)
    {
        editors[e]->setVisible (e == active);
        for (auto* knob : *knobSets[e])
            knob->setVisible (e == active);
    }
}

void EnvCard::animate()
{
    editors[active]->animate();
}

// ---------------------------------------------------------------------------
// LfoCard
// ---------------------------------------------------------------------------

LfoCard::LfoCard (const UiShared& shared)
    : CardPanel ("LFOS", theme::neonYellow),
      tabs ({ "1", "2", "3" }, [this] (int index) { setActiveLfo (index); })
{
    addAndMakeVisible (tabs);

    for (int k = 0; k < 3; ++k)
    {
        auto& set = sets[k];
        const juce::String prefix = "lfo" + juce::String (k + 1);

        set.view = std::make_unique<LfoView> (shared, k, theme::neonYellow);
        set.shape = std::make_unique<ChoiceCombo> (shared, prefix + "Shape");
        set.mode = std::make_unique<ChoiceCombo> (shared, prefix + "Mode");
        set.syncDiv = std::make_unique<ChoiceCombo> (shared, prefix + "SyncDiv");
        set.sync = std::make_unique<ParamToggle> (shared, prefix + "Sync", "SYNC", theme::neonYellow);
        set.knobs.add (new ModKnob (shared, prefix + "Rate", "Rate", theme::neonYellow));
        set.knobs.add (new ModKnob (shared, prefix + "Phase", "Phase", theme::neonYellow));
        set.knobs.add (new ModKnob (shared, prefix + "Fade", "Fade", theme::neonYellow));

        addChildComponent (*set.view);
        addChildComponent (*set.shape);
        addChildComponent (*set.mode);
        addChildComponent (*set.syncDiv);
        addChildComponent (*set.sync);
        for (auto* knob : set.knobs)
            addChildComponent (knob);
    }
    setActiveLfo (0);
}

void LfoCard::resized()
{
    tabs.setBounds (getWidth() - 116, 4, 108, 17);

    for (auto& set : sets)
    {
        set.view->setBounds (8, 24, 118, 92);
        set.shape->setBounds (134, 26, 80, 17);
        set.mode->setBounds (218, 26, 52, 17);
        set.sync->setBounds (276, 27, 46, 15);
        for (int k = 0; k < set.knobs.size(); ++k)
            set.knobs[k]->setBounds (134 + k * 46, 46, 46, 70);
        set.syncDiv->setBounds (278, 68, 52, 17);
    }
}

void LfoCard::setActiveLfo (int index)
{
    active = index;
    for (int k = 0; k < 3; ++k)
    {
        auto& set = sets[k];
        const bool visible = k == active;
        set.view->setVisible (visible);
        set.shape->setVisible (visible);
        set.mode->setVisible (visible);
        set.syncDiv->setVisible (visible);
        set.sync->setVisible (visible);
        for (auto* knob : set.knobs)
            knob->setVisible (visible);
    }
}

void LfoCard::animate()
{
    sets[active].view->animate();
}

// ---------------------------------------------------------------------------
// LensCard
// ---------------------------------------------------------------------------

LensCard::LensCard (const UiShared& shared)
    : CardPanel ("LENS", theme::accentMod),
      panel (shared, true)
{
    addAndMakeVisible (panel);
}

void LensCard::resized()
{
    panel.setBounds (getLocalBounds().reduced (8).withTrimmedTop (16));
}

void LensCard::animate()
{
    panel.animate();
}

// ---------------------------------------------------------------------------
// FooterBar
// ---------------------------------------------------------------------------

FooterBar::FooterBar (const UiShared& shared, const AudioHistory& history)
    : scope (history, theme::accentMod)
{
    const char* macroIds[] = { "macro1", "macro2", "macro3", "macro4" };
    const char* macroNames[] = { "Tone", "Motion", "Space", "Texture" };
    for (int m = 0; m < 4; ++m)
        addAndMakeVisible (macroKnobs.add (
            new ModKnob (shared, macroIds[m], macroNames[m], theme::macroAccent (m))));

    struct ChipDef { int source; const char* text; juce::Colour colour; };
    const ChipDef chipDefs[] = {
        { 0, "ENV 1", theme::accentMod }, { 1, "ENV 2", theme::accentMod },
        { 2, "ENV 3", theme::accentMod }, { 3, "LFO 1", theme::accentMod },
        { 4, "LFO 2", theme::accentMod }, { 5, "LFO 3", theme::accentMod },
        { 6, "MAC 1", theme::macroAccent (0) }, { 7, "MAC 2", theme::macroAccent (1) },
        { 8, "MAC 3", theme::macroAccent (2) }, { 9, "MAC 4", theme::macroAccent (3) },
    };
    for (const auto& def : chipDefs)
        addAndMakeVisible (chips.add (new ModSourceChip (shared, def.source, def.text, def.colour)));

    addAndMakeVisible (scope);
}

void FooterBar::resized()
{
    for (int m = 0; m < 4; ++m)
        macroKnobs[m]->setBounds (8 + m * 72, 2, 72, 84);

    for (int c = 0; c < chips.size(); ++c)
        chips[c]->setBounds (312 + (c % 5) * 62, 14 + (c / 5) * 24, 56, 18);

    scope.setBounds (778, 8, 254, 74);
}

void FooterBar::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRect (getLocalBounds());
    g.setColour (theme::hairlineLight);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    g.setColour (theme::textMuted);
    g.setFont (theme::font (10.0f));
    g.drawText ("drag a source onto any knob to modulate it - right-click a knob to edit",
                312, 64, 400, 12, juce::Justification::centredLeft);
}

void FooterBar::animate()
{
    scope.animate();
    for (auto* chip : chips)
        chip->repaint();
}

// ---------------------------------------------------------------------------
// HeaderBar
// ---------------------------------------------------------------------------

HeaderBar::HeaderBar (const UiShared& shared, std::function<void (int)> onViewChange)
    : viewTabs ({ "PLAY", "DEEP" }, std::move (onViewChange)),
      master (shared, "masterGain", "Main", theme::neonYellow),
      meter (shared)
{
    addAndMakeVisible (viewTabs);
    addAndMakeVisible (master);
    addAndMakeVisible (meter);

    for (auto* button : { &presetPrev, &presetNext })
    {
        button->setEnabled (false); // preset browser arrives in Phase 7
        addAndMakeVisible (button);
    }
}

void HeaderBar::resized()
{
    presetPrev.setBounds (354, 14, 22, 20);
    presetNext.setBounds (624, 14, 22, 20);
    viewTabs.setBounds (700, 13, 130, 22);
    master.setBounds (848, 1, 46, 46);
    meter.setBounds (910, 14, 120, 20);
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::well);
    g.setColour (theme::hairlineLight);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    g.setColour (theme::textPrimary);
    g.setFont (theme::semiBold (22.0f));
    g.drawText ("LUMEN", 24, 0, 160, getHeight(), juce::Justification::centredLeft);

    // Preset strip (placeholder until Phase 7).
    const auto strip = juce::Rectangle<float> (350.0f, 12.0f, 300.0f, 24.0f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (strip, 5.0f);
    g.setColour (theme::hairline);
    g.drawRoundedRectangle (strip, 5.0f, 1.0f);
    g.setColour (theme::textSecondary);
    g.setFont (theme::medium (13.0f));
    g.drawText ("Init", strip.toNearestInt(), juce::Justification::centred);
}

void HeaderBar::animate()
{
    meter.animate();
}

// ---------------------------------------------------------------------------
// DeepView
// ---------------------------------------------------------------------------

DeepView::DeepView (const UiShared& shared, const AudioHistory& history)
    : oscA (shared, "oscA", "OSC A", theme::accentA),
      oscB (shared, "oscB", "OSC B", theme::accentB),
      subNoise (shared),
      filter (shared),
      fx (shared),
      env (shared),
      lfo (shared),
      lens (shared),
      footer (shared, history)
{
    for (auto* child : std::initializer_list<juce::Component*> {
             &oscA, &oscB, &subNoise, &filter, &fx, &env, &lfo, &lens, &footer })
        addAndMakeVisible (child);
}

void DeepView::resized()
{
    oscA.setBounds (8, 4, 376, 204);
    oscB.setBounds (388, 4, 376, 204);
    subNoise.setBounds (768, 4, 264, 204);

    filter.setBounds (8, 212, 516, 180);
    fx.setBounds (528, 212, 504, 180);

    env.setBounds (8, 396, 420, 122);
    lfo.setBounds (432, 396, 340, 122);
    lens.setBounds (776, 396, 256, 122);

    footer.setBounds (0, 522, getWidth(), 90);
}

void DeepView::animate (bool fftTick)
{
    oscA.animate();
    oscB.animate();
    filter.animate (fftTick);
    env.animate();
    lfo.animate();
    lens.animate();
    footer.animate();
}

// ---------------------------------------------------------------------------
// PlayView
// ---------------------------------------------------------------------------

PlayView::PlayView (const UiShared& sharedContext, const AudioHistory& history,
                    juce::MidiKeyboardState& keyboardState)
    : shared (sharedContext),
      stackA (sharedContext, "oscATable", "oscAMorph", theme::accentA),
      stackB (sharedContext, "oscBTable", "oscBMorph", theme::accentB),
      waterfall (sharedContext, history),
      bigLens (sharedContext, [this] { return bigLensOsc; }),
      lensPanel (sharedContext, false),
      keyboard (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible (stackA);
    addChildComponent (stackB);
    addChildComponent (waterfall);
    addChildComponent (bigLens);
    addAndMakeVisible (lensPanel);

    const char* macroIds[] = { "macro1", "macro2", "macro3", "macro4" };
    const char* macroNames[] = { "Tone", "Motion", "Space", "Texture" };
    for (int m = 0; m < 4; ++m)
        addAndMakeVisible (macroKnobs.add (
            new ModKnob (shared, macroIds[m], macroNames[m], theme::macroAccent (m))));

    keyboard.setAvailableRange (48, 72); // 2 octaves, C3..C5 (SPEC 14)
    keyboard.setScrollButtonsVisible (false);
    addAndMakeVisible (keyboard);
}

void PlayView::resized()
{
    const auto visualizer = juce::Rectangle<int> (16, 12, 740, 288);
    stackA.setBounds (visualizer);
    stackB.setBounds (visualizer);
    waterfall.setBounds (visualizer);
    bigLens.setBounds (visualizer);
    lensPanel.setBounds (764, 12, 260, 288);

    for (int m = 0; m < 4; ++m)
        macroKnobs[m]->setBounds (300 + m * 110, 316, 110, 148);

    keyboard.setBounds (16, 484, 1008, 116);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 15.0f); // 15 white keys
}

void PlayView::paint (juce::Graphics& g)
{
    g.fillAll (theme::well);
}

void PlayView::animate()
{
    // Context auto-selection (SPEC 14, DECISIONS.md): the spectral waterfall
    // whenever sound is coming out (voices active or FX tail still audible);
    // when audio stops it keeps advancing all-zero rows for 44 frames so the
    // surface drains toward the horizon (~0.73 s, WATERFALL_SPEC section 6)
    // before handing back to the idle visual: the Osc A stack, the Osc B
    // stack if only B is on, and the waterfall again when both oscillators
    // are off (the old scope fallback role).
    const auto& meter = shared.processor.meterLevels();
    const bool audioActive =
        shared.tap.activeVoices.load (std::memory_order_relaxed) > 0
        || meter.peakL.load (std::memory_order_relaxed) > 0.0005f   // ~-66 dBFS
        || meter.peakR.load (std::memory_order_relaxed) > 0.0005f;
    audioHoldFrames = audioActive ? lumen::WaterfallModel::kRows
                                  : juce::jmax (0, audioHoldFrames - 1);

    auto* enabledA = shared.apvts().getRawParameterValue ("oscAEnabled");
    auto* enabledB = shared.apvts().getRawParameterValue ("oscBEnabled");
    auto* tableA = shared.apvts().getRawParameterValue ("oscATable");
    auto* tableB = shared.apvts().getRawParameterValue ("oscBTable");
    const bool aOn = enabledA != nullptr && enabledA->load() > 0.5f;
    const bool bOn = enabledB != nullptr && enabledB->load() > 0.5f;

    // Phase 6 (SPEC 13.6/14): the image view takes over the idle visual
    // whenever the shown oscillator is playing its Lens table.
    auto& lensController = shared.processor.lensController();
    const bool imageA = aOn && tableA != nullptr
                        && juce::roundToInt (tableA->load()) == 4 && lensController.hasImage (0);
    const bool imageB = bOn && ! aOn && tableB != nullptr
                        && juce::roundToInt (tableB->load()) == 4 && lensController.hasImage (1);

    const bool showWaterfall = audioHoldFrames > 0 || (! aOn && ! bOn);
    const bool showLens = ! showWaterfall && (imageA || imageB);
    bigLensOsc = imageA ? 0 : 1;
    const bool showA = ! showWaterfall && ! showLens && aOn;
    const bool showB = ! showWaterfall && ! showLens && ! aOn && bOn;
    stackA.setVisible (showA);
    stackB.setVisible (showB);
    bigLens.setVisible (showLens);
    waterfall.setVisible (showWaterfall);

    if (showA)
        stackA.animate();
    if (showB)
        stackB.animate();
    if (showLens)
        bigLens.animate();
    if (showWaterfall)
        waterfall.animate (audioActive);

    lensPanel.animate();
}
