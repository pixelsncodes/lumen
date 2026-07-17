#include "UI/Cards.h"

#include "Lens/LensController.h"
#include "Melody/MelodyController.h"
#include "State/PresetManager.h"
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
    : CardPanel ("SUB / NOISE / VOICE", theme::accentA),
      subWave (shared, "subWave"),
      subOctave (shared, "subOctave"),
      noiseType (shared, "noiseType"),
      voiceMode (shared, "voiceMode"),
      subLevel (shared, "subLevel", "Level", theme::accentA),
      noiseLevel (shared, "noiseLevel", "Level", theme::accentA),
      glideTime (shared, "glideTime", "Glide", theme::accentA)
{
    for (auto* child : std::initializer_list<juce::Component*> {
             &subWave, &subOctave, &noiseType, &voiceMode, &subLevel, &noiseLevel, &glideTime })
        addAndMakeVisible (child);
}

void SubNoiseCard::resized()
{
    subWave.setBounds (10, 42, 86, 18);
    subOctave.setBounds (102, 42, 70, 18);
    subLevel.setBounds (186, 20, 62, 60);

    noiseType.setBounds (10, 104, 86, 18);
    noiseLevel.setBounds (186, 82, 62, 60);

    voiceMode.setBounds (10, 166, 86, 18);
    glideTime.setBounds (186, 144, 62, 60);
}

void SubNoiseCard::paint (juce::Graphics& g)
{
    CardPanel::paint (g);
    g.setColour (theme::textMuted);
    g.setFont (theme::medium (11.0f));
    g.drawText ("SUB", 10, 26, 100, 12, juce::Justification::centredLeft);
    g.drawText ("NOISE", 10, 88, 100, 12, juce::Justification::centredLeft);
    g.drawText ("VOICE", 10, 150, 100, 12, juce::Justification::centredLeft);
    g.setColour (theme::hairline);
    g.drawHorizontalLine (84, 8.0f, (float) getWidth() - 8.0f);
    g.drawHorizontalLine (146, 8.0f, (float) getWidth() - 8.0f);
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
    tabs.setTooltip ("Show envelope 1, 2 or 3 (1 = volume, 2 = filter, 3 = free)");
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
    tabs.setTooltip ("Show LFO 1, 2 or 3");
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
// HeaderIconButton
// ---------------------------------------------------------------------------

HeaderIconButton::HeaderIconButton (Glyph glyphToDraw, const juce::String& tip)
    : juce::Button (tip), glyph (glyphToDraw)
{
    setTooltip (tip);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void HeaderIconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float s = juce::jmin (bounds.getWidth(), bounds.getHeight());

    juce::Colour col = highlighted ? theme::textPrimary : theme::textSecondary;
    if (glyph == Glyph::close && highlighted)
        col = theme::meterHot; // close-hover = red (#FF3B30)
    if (down)
        col = col.brighter (0.2f);
    g.setColour (col);

    if (glyph == Glyph::gear)
    {
        const int teeth = 8, n = teeth * 4;
        const float rO = s * 0.34f, rI = s * 0.20f;
        juce::Path cog;
        for (int k = 0; k <= n; ++k)
        {
            const float ang = juce::MathConstants<float>::twoPi * (float) k / (float) n;
            const float rr = (k % 4 == 0 || k % 4 == 1) ? rO : rI;
            const auto p = centre.getPointOnCircumference (rr, ang);
            if (k == 0) cog.startNewSubPath (p); else cog.lineTo (p);
        }
        cog.closeSubPath();
        g.strokePath (cog, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        g.drawEllipse (juce::Rectangle<float> (s * 0.18f, s * 0.18f).withCentre (centre), 1.4f);
    }
    else if (glyph == Glyph::minimize)
    {
        const float y = centre.y + s * 0.14f;
        g.drawLine (centre.x - s * 0.22f, y, centre.x + s * 0.22f, y, 1.4f);
    }
    else // close
    {
        const float d = s * 0.20f;
        g.drawLine (centre.x - d, centre.y - d, centre.x + d, centre.y + d, 1.4f);
        g.drawLine (centre.x + d, centre.y - d, centre.x - d, centre.y + d, 1.4f);
    }
}

// ---------------------------------------------------------------------------
// HeaderBar
// ---------------------------------------------------------------------------

HeaderBar::HeaderBar (const UiShared& shared, std::function<void (int)> onViewChange,
                      bool standaloneChrome, std::function<void()> onOpenSettings)
    : processor (shared.processor),
      standalone (standaloneChrome),
      openSettings (std::move (onOpenSettings)),
      viewTabs ({ "PLAY", "DEEP" }, std::move (onViewChange)),
      master (shared, "masterGain", "", theme::neonYellow), // unlabelled: header space is tight
      meter (shared)
{
    logo = juce::ImageCache::getFromMemory (BinaryData::lumenicon512_png,
                                            BinaryData::lumenicon512_pngSize);

    addAndMakeVisible (viewTabs);
    addAndMakeVisible (master);
    addAndMakeVisible (meter);

    presetPrev.onClick = [this] { processor.presetManager().step (-1); };
    presetNext.onClick = [this] { processor.presetManager().step (1); };
    presetPrev.setTooltip ("Previous preset");
    presetNext.setTooltip ("Next preset");
    viewTabs.setTooltip ("Play = perform view, Deep = full patch editor");
    for (auto* button : { &presetPrev, &presetNext })
        addAndMakeVisible (button);

    presetName.setButtonText (processor.presetManager().currentName());
    presetName.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    presetName.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    presetName.setColour (juce::TextButton::textColourOffId, theme::textPrimary);
    presetName.setTooltip ("Preset browser (click to open)");
    presetName.onClick = [this] { showBrowserMenu(); };
    addAndMakeVisible (presetName);

    gear.onClick = [this] { showGearMenu(); };
    addAndMakeVisible (gear);

    // Window controls live in the header only in the standalone (borderless)
    // build; the plugin host owns its frame.
    minimizeButton.onClick = [this]
    {
        if (auto* peer = getTopLevelComponent()->getPeer())
            peer->setMinimised (true);
    };
    closeButton.onClick = [this]
    {
        if (auto* peer = getTopLevelComponent()->getPeer())
            peer->handleUserClosingWindow(); // routes to the window's save+quit
    };
    addChildComponent (minimizeButton);
    addChildComponent (closeButton);
    minimizeButton.setVisible (standalone);
    closeButton.setVisible (standalone);
}

void HeaderBar::resized()
{
    presetPrev.setBounds (356, 14, 20, 20);
    presetName.setBounds (380, 12, 232, 24);
    presetNext.setBounds (616, 14, 20, 20);
    viewTabs.setBounds (648, 13, 118, 22);
    gear.setBounds (774, 12, 24, 24);
    master.setBounds (810, 5, 38, 38); // knob centre = y 24 = header centre, like the meter
    meter.setBounds (864, 14, 104, 20);
    minimizeButton.setBounds (978, 14, 22, 20);
    closeButton.setBounds (1006, 14, 22, 20);
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::well);
    g.setColour (theme::hairlineLight);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    // App icon, then the wordmark. The logo sits in the same left slot the
    // wordmark used; the text shifts right by the icon's width so nothing else
    // in the header moves.
    const int logoSize = 28;
    const int logoX = 22;
    if (logo.isValid())
        g.drawImageWithin (logo, logoX, (getHeight() - logoSize) / 2, logoSize, logoSize,
                           juce::RectanglePlacement::centred);

    g.setColour (theme::textPrimary);
    g.setFont (theme::semiBold (22.0f));
    theme::drawTrackedText (g, "LUMEN", { logoX + logoSize + 10, 0, 220, getHeight() },
                            juce::Justification::centredLeft, 0.34f);

    // Preset strip well; the name button sits transparently on top.
    const auto strip = juce::Rectangle<float> (350.0f, 12.0f, 290.0f, 24.0f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (strip, 5.0f);
    g.setColour (theme::hairline);
    g.drawRoundedRectangle (strip, 5.0f, 1.0f);
}

void HeaderBar::mouseDown (const juce::MouseEvent& e)
{
    // Standalone: the header is the title bar — drag an empty region to move
    // the borderless window (child buttons/knobs consume their own events).
    if (standalone)
        windowDragger.startDraggingComponent (getTopLevelComponent(), e);
}

void HeaderBar::mouseDrag (const juce::MouseEvent& e)
{
    if (standalone)
        windowDragger.dragComponent (getTopLevelComponent(), e, nullptr);
}

void HeaderBar::showGearMenu()
{
    // Standalone: the gear opens the audio/MIDI device settings dialog.
    if (standalone)
    {
        if (openSettings != nullptr)
            openSettings();
        return;
    }

    // Plugin: a minimal branded menu (the host owns the audio devices).
    juce::PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    // Branded "about" line: the app icon beside the name, then the version.
    juce::PopupMenu::Item brand ("Lumen");
    brand.setImage (juce::Drawable::createFromImageData (BinaryData::lumenicon512_png,
                                                         BinaryData::lumenicon512_pngSize));
    brand.setEnabled (false);
    menu.addItem (brand);
    menu.addItem (1, "Version " + juce::String (JucePlugin_VersionString), false, false);
    menu.addSeparator();
    menu.addItem (2, "MIDI Learn: right-click any knob", false, false);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (gear)
                            .withMinimumWidth (190));
}

void HeaderBar::showBrowserMenu()
{
    auto& manager = processor.presetManager();
    manager.refresh();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    const int current = manager.currentIndex();
    juce::String lastCategory;
    for (int i = 0; i < static_cast<int> (manager.entries().size()); ++i)
    {
        const auto& entry = manager.entries()[static_cast<size_t> (i)];
        if (entry.category != lastCategory)
        {
            menu.addSectionHeader (entry.category);
            lastCategory = entry.category;
        }
        menu.addItem (i + 2, entry.name, true, i == current);
    }
    menu.addSeparator();
    menu.addItem (1, "Save Preset...");

    // Three columns so the whole bank is visible at once; category headers,
    // the current-patch tick and the Save Preset... divider ride along.
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (presetName)
                            .withMinimumNumColumns (3)
                            .withMaximumNumColumns (3),
                        [this] (int result)
                        {
                            if (result == 1)
                                showSaveDialog();
                            else if (result >= 2)
                                processor.presetManager().loadIndex (result - 2);
                        });
}

void HeaderBar::showSaveDialog()
{
    auto& manager = processor.presetManager();
    auto* window = new juce::AlertWindow ("Save Preset",
                                          "Stores the patch in Documents/Lumen/Presets.",
                                          juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", manager.currentName(), "Name");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, window] (int result)
        {
            if (result == 1)
                processor.presetManager().saveUserPreset (
                    window->getTextEditorContents ("name"));
        }), true);
}

void HeaderBar::animate()
{
    meter.animate();

    const auto name = processor.presetManager().currentName();
    if (name != shownName)
    {
        shownName = name;
        presetName.setButtonText (name);
    }
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
      notes (keyboardState),
      waterfall (sharedContext, history),
      lensPanel (sharedContext, false),
      readout (sharedContext),
      keyboard (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible (waterfall);
    addAndMakeVisible (lensPanel);
    addChildComponent (readout); // hidden until animate() sees hasMelody()

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
    waterfall.setBounds (16, 12, 740, 288);
    layoutLensColumn();

    for (int m = 0; m < 4; ++m)
        macroKnobs[m]->setBounds (300 + m * 110, 316, 110, 148);

    keyboard.setBounds (16, 484, 1008, 116);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 15.0f); // 15 white keys
}

void PlayView::layoutLensColumn()
{
    // Normal dock: same rect the Lens panel has always used, readout directly
    // beneath it (clear of the macro knob row, which ends at x=740). While the
    // melody side panel is open, that rect sits entirely underneath it (side
    // panel right-docks at kPanelWidth from the right edge, 316 wide) — so
    // both the Lens image and the GENERATED readout beneath it would be
    // hidden. The readout's row is shared with the macro knobs (x 300-740),
    // so sliding the column only as far as clearing the side panel would land
    // it on top of them; instead slide all the way to the left edge, clear of
    // both the knobs and the side panel. It's allowed to overlap the
    // waterfall/spectrogram display either way.
    constexpr int kLensW = 260, kLensH = 288, kLensY = 12;
    constexpr int kLensNormalX = 764, kLensOpenX = 16;
    constexpr int kReadoutY = 304, kReadoutBottom = 476; // clears the keyboard (y 484) by 8px

    const bool sidePanelOpen = shared.processor.melodyController().isPanelActive();
    const int lensX = sidePanelOpen ? kLensOpenX : kLensNormalX;

    lensPanel.setBounds (lensX, kLensY, kLensW, kLensH);
    readout.setBounds (lensX, kReadoutY, kLensW, kReadoutBottom - kReadoutY);
}

void PlayView::paint (juce::Graphics& g)
{
    g.fillAll (theme::well);
}

void PlayView::paintOverChildren (juce::Graphics& g)
{
    // Edge arrows over the keyboard ends: dim affordances that light neon
    // yellow while notes sound beyond the visible range on that side.
    const auto kb = keyboard.getBounds().toFloat();
    auto drawArrow = [&g, &kb] (bool leftSide, bool lit)
    {
        const float cy = kb.getCentreY();
        const float cx = leftSide ? kb.getX() + 12.0f : kb.getRight() - 12.0f;
        const float dir = leftSide ? -1.0f : 1.0f;

        if (lit)
        {
            g.setColour (theme::neonYellow.withAlpha (0.25f));
            g.fillEllipse (cx - 11.0f, cy - 11.0f, 22.0f, 22.0f); // soft halo
        }
        juce::Path arrow;
        arrow.addTriangle (cx + dir * 6.0f, cy,
                           cx - dir * 3.5f, cy - 6.5f,
                           cx - dir * 3.5f, cy + 6.5f);
        g.setColour (lit ? theme::neonYellow : theme::textMuted.withAlpha (0.4f));
        g.fillPath (arrow);
        g.setColour (theme::well.withAlpha (lit ? 0.9f : 0.5f)); // reads on a lit key too
        g.strokePath (arrow, juce::PathStrokeType (1.2f));
    };
    drawArrow (true, leftArrowLit);
    drawArrow (false, rightArrowLit);
}

void PlayView::animate()
{
    // The waterfall scene (grid + surface) is permanent (WATERFALL_SPEC
    // section 6 — no context switching): idle shows the drained flat
    // surface, ridges rise whenever sound is coming out (voices active or
    // FX tail still audible), release drains in place.
    const auto& meter = shared.processor.meterLevels();
    const bool audioActive =
        shared.tap.activeVoices.load (std::memory_order_relaxed) > 0
        || meter.peakL.load (std::memory_order_relaxed) > 0.0005f   // ~-66 dBFS
        || meter.peakR.load (std::memory_order_relaxed) > 0.0005f;

    waterfall.animate (audioActive);
    lensPanel.animate();
    readout.animate();

    // Reposition the Lens column (image + readout) when the melody side
    // panel opens/closes, so they stay clear of it (or restore on close).
    const bool sidePanelOpen = shared.processor.melodyController().isPanelActive();
    if (sidePanelOpen != sidePanelOpenCache)
    {
        sidePanelOpenCache = sidePanelOpen;
        layoutLensColumn();
    }

    // Keyboard follow: scan the sounding notes once per tick. If any sounding
    // note is visible the window must not move (a held key sliding under the
    // mouse would be worse than an off-screen note); if ALL are outside,
    // slide whole octaves toward them (C-aligned, clamped to 0..120).
    int lowestNote = 128, highestNote = -1;
    bool anyVisible = false;
    const int rangeLow = keyboard.getRangeStart(), rangeHigh = keyboard.getRangeEnd();
    for (int n = 0; n < 128; ++n)
    {
        if (! notes.isNoteOnForChannels (0xffff, n))
            continue;
        lowestNote = juce::jmin (lowestNote, n);
        highestNote = juce::jmax (highestNote, n);
        anyVisible = anyVisible || (n >= rangeLow && n <= rangeHigh);
    }

    if (highestNote >= 0 && ! anyVisible)
    {
        int shift = 0;
        if (lowestNote > rangeHigh)                     // everything above: slide up
            shift = (lowestNote - rangeHigh + 11) / 12 * 12;
        else if (highestNote < rangeLow)                // everything below: slide down
            shift = -((rangeLow - highestNote + 11) / 12 * 12);
        const int newLow = juce::jlimit (0, 96, rangeLow + shift);
        if (newLow != rangeLow)
            keyboard.setAvailableRange (newLow, newLow + 24);
    }

    const bool left = highestNote >= 0 && lowestNote < keyboard.getRangeStart();
    const bool right = highestNote >= 0 && highestNote > keyboard.getRangeEnd();
    if (left != leftArrowLit || right != rightArrowLit)
    {
        leftArrowLit = left;
        rightArrowLit = right;
        repaint (keyboard.getBounds());
    }
}
