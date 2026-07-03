#include "UI/Controls.h"

#include "State/MidiLearn.h"
#include "State/ModState.h"
#include "UI/LumenLookAndFeel.h"
#include "UI/Theme.h"

using namespace lumen;

namespace
{
    constexpr int kLabelHeight = 13;

    bool isBipolarSource (int source)
    {
        using S = mod::Source;
        const auto s = static_cast<S> (source);
        return s == S::lfo1 || s == S::lfo2 || s == S::lfo3
            || s == S::pitchBend || s == S::keytrack || s == S::randomPerNote;
    }

    // CallOutBox content for editing one matrix slot's depth in place.
    class DepthEditor final : public juce::Component
    {
    public:
        DepthEditor (juce::ValueTree slotTree, juce::Colour accent)
            : slot (slotTree)
        {
            depth.setSliderStyle (juce::Slider::LinearHorizontal);
            depth.setRange (-1.0, 1.0, 0.01);
            depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
            depth.setColour (juce::Slider::trackColourId, accent.withAlpha (0.6f));
            depth.setColour (juce::Slider::thumbColourId, accent);
            depth.setColour (juce::Slider::backgroundColourId, theme::hairline);
            depth.setDoubleClickReturnValue (true, 0.0);
            depth.setValue (static_cast<double> (slot["depth"]), juce::dontSendNotification);
            depth.onValueChange = [this] { slot.setProperty ("depth", depth.getValue(), nullptr); };
            addAndMakeVisible (depth);
            setSize (210, 40);
        }

        void resized() override { depth.setBounds (getLocalBounds().reduced (8, 6)); }

    private:
        juce::ValueTree slot;
        juce::Slider depth;
    };
} // namespace

// ---------------------------------------------------------------------------
// KnobSlider
// ---------------------------------------------------------------------------

KnobSlider::KnobSlider()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (LumenLookAndFeel::rotaryStart, LumenLookAndFeel::rotaryEnd, true);
    setMouseDragSensitivity (220);
    setScrollWheelEnabled (true);
    setPopupDisplayEnabled (true, false, nullptr); // value bubble while dragging
    // Shift while dragging switches to velocity mode = fine adjustment.
    setVelocityBasedMode (false);
    setVelocityModeParameters (0.6, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
}

void KnobSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onContextMenu != nullptr)
            onContextMenu();
        return;
    }
    juce::Slider::mouseDown (e);
}

// ---------------------------------------------------------------------------
// ModKnob
// ---------------------------------------------------------------------------

ModKnob::ModKnob (const UiShared& sharedContext, const juce::String& paramID,
                  const juce::String& text, juce::Colour accentColour)
    : shared (sharedContext), paramId (paramID), labelText (text), accent (accentColour)
{
    destIndex = modstate::destFromToken (paramId);

    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::thumbColourId, accent);
    slider.onContextMenu = [this] { showContextMenu(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        shared.apvts(), paramId, slider);
    shared.registerAttachment (paramId);

    if (auto* parameter = shared.apvts().getParameter (paramId))
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

    shared.knobs.push_back (this);
}

ModKnob::~ModKnob()
{
    auto& registry = shared.knobs;
    registry.erase (std::remove (registry.begin(), registry.end(), this), registry.end());
}

void ModKnob::resized()
{
    auto area = getLocalBounds();
    if (labelText.isNotEmpty())
        area.removeFromBottom (kLabelHeight);

    const int size = juce::jmin (area.getWidth(), area.getHeight());
    slider.setBounds (juce::Rectangle<int> (size, size).withCentre (area.getCentre()).reduced (4));
}

void ModKnob::paint (juce::Graphics& g)
{
    if (labelText.isEmpty())
        return;

    g.setColour (theme::textSecondary);
    g.setFont (theme::font (11.0f));
    g.drawText (labelText, getLocalBounds().removeFromBottom (kLabelHeight),
                juce::Justification::centred);
}

ModKnob::ModSpan ModKnob::computeModSpan() const
{
    ModSpan span;
    if (destIndex < 0)
        return span;

    if (const auto* config = shared.processor.currentModConfig())
    {
        for (const auto& slot : config->slots)
        {
            if (! slot.enabled || slot.dest != destIndex)
                continue;
            span.any = true;
            if (isBipolarSource (slot.source))
            {
                span.minOffset -= std::abs (slot.depth);
                span.maxOffset += std::abs (slot.depth);
            }
            else
            {
                span.minOffset += juce::jmin (0.0f, slot.depth);
                span.maxOffset += juce::jmax (0.0f, slot.depth);
            }
        }

        for (const auto& maps : config->macroMaps)
            for (const auto& map : maps)
                if (map.dest == destIndex)
                {
                    span.any = true;
                    span.minOffset += juce::jmin (map.rangeMin, map.rangeMax);
                    span.maxOffset += juce::jmax (map.rangeMin, map.rangeMax);
                }
    }
    return span;
}

void ModKnob::paintOverChildren (juce::Graphics& g)
{
    if (dragOver)
    {
        g.setColour (accent.withAlpha (0.9f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 1.5f);
    }

    if (destIndex < 0)
        return;

    const auto span = computeModSpan();
    if (! span.any)
        return;

    const auto knobBounds = slider.getBounds().toFloat().expanded (3.0f);
    const auto centre = knobBounds.getCentre();
    const float radius = juce::jmin (knobBounds.getWidth(), knobBounds.getHeight()) * 0.5f - 1.0f;

    const float baseNorm = static_cast<float> (slider.valueToProportionOfLength (slider.getValue()));
    auto normToAngle = [] (float n)
    {
        return LumenLookAndFeel::rotaryStart
             + juce::jlimit (0.0f, 1.0f, n) * (LumenLookAndFeel::rotaryEnd - LumenLookAndFeel::rotaryStart);
    };

    // Dim range arc = reachable modulation span around the base value.
    const float a0 = normToAngle (baseNorm + span.minOffset);
    const float a1 = normToAngle (baseNorm + span.maxOffset);
    if (a1 - a0 > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, a0, a1, true);
        g.setColour (accent.withAlpha (0.35f));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // Moving indicator = live modulated value from the engine tap.
    const float liveNorm = shared.tap.destNorm[destIndex].load (std::memory_order_relaxed);
    const auto dot = centre.getPointOnCircumference (radius, normToAngle (liveNorm));
    g.setColour (accent);
    g.fillEllipse (dot.x - 2.4f, dot.y - 2.4f, 4.8f, 4.8f);
}

void ModKnob::animate()
{
    if (destIndex < 0)
        return;

    const bool hasMod = computeModSpan().any;
    const float live = shared.tap.destNorm[destIndex].load (std::memory_order_relaxed);

    if (hasMod != lastHadMod || (hasMod && std::abs (live - lastLiveNorm) > 0.003f))
    {
        lastHadMod = hasMod;
        lastLiveNorm = live;
        repaint();
    }
}

bool ModKnob::isInterestedInDragSource (const SourceDetails& details)
{
    return destIndex >= 0 && details.description.toString().startsWith (kDragPrefix);
}

void ModKnob::itemDragEnter (const SourceDetails&)
{
    dragOver = true;
    repaint();
}

void ModKnob::itemDragExit (const SourceDetails&)
{
    dragOver = false;
    repaint();
}

void ModKnob::itemDropped (const SourceDetails& details)
{
    dragOver = false;
    addModulationSlot (details.description.toString().fromFirstOccurrenceOf (kDragPrefix, false, false));
    repaint();
}

void ModKnob::addModulationSlot (const juce::String& sourceToken)
{
    if (modstate::sourceFromToken (sourceToken) < 0)
        return;

    auto matrix = shared.matrixTree();
    int slotIndex = -1;
    for (int pass = 0; pass < 2 && slotIndex < 0; ++pass)
        for (int i = 0; i < matrix.getNumChildren() && slotIndex < 0; ++i)
        {
            const auto slot = matrix.getChild (i);
            const bool free = ! static_cast<bool> (slot["enabled"])
                              && (pass == 1 || slot["dest"].toString().isEmpty());
            if (free)
                slotIndex = i;
        }

    if (slotIndex < 0)
        return; // matrix full (24 active slots)

    auto slot = matrix.getChild (slotIndex);
    slot.setProperty ("source", sourceToken, nullptr);
    slot.setProperty ("dest", paramId, nullptr);
    slot.setProperty ("depth", 0.25, nullptr);
    slot.setProperty ("enabled", true, nullptr);
}

void ModKnob::resetToDefault()
{
    if (auto* parameter = shared.apvts().getParameter (paramId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getDefaultValue());
        parameter->endChangeGesture();
    }
}

void ModKnob::showContextMenu()
{
    const auto& sourceNames = modstate::sourceNames();
    const auto& sourceTokens = modstate::sourceTokens();
    auto matrix = shared.matrixTree();

    juce::String displayName = labelText;
    if (auto* parameter = shared.apvts().getParameter (paramId))
        displayName = parameter->getName (64);

    juce::PopupMenu menu;
    menu.addSectionHeader (displayName);
    menu.addItem (1, "Reset to default");
    menu.addSeparator();

    bool anySlots = false;
    for (int i = 0; i < matrix.getNumChildren(); ++i)
    {
        const auto slot = matrix.getChild (i);
        if (! static_cast<bool> (slot["enabled"]) || slot["dest"].toString() != paramId)
            continue;

        anySlots = true;
        const int sourceIndex = modstate::sourceFromToken (slot["source"].toString());
        const auto name = sourceIndex >= 0 ? sourceNames[sourceIndex] : slot["source"].toString();
        const float depth = static_cast<float> (static_cast<double> (slot["depth"]));

        juce::PopupMenu sub;
        sub.addItem (1000 + i, "Edit depth...");
        sub.addItem (2000 + i, "Remove");
        menu.addSubMenu (name + "   " + juce::String (depth >= 0.0f ? "+" : "")
                             + juce::String (depth, 2), sub);
    }
    if (anySlots)
        menu.addSeparator();

    juce::PopupMenu addMenu;
    for (int s = 0; s < sourceNames.size(); ++s)
        addMenu.addItem (3000 + s, sourceNames[s]);
    menu.addSubMenu ("Add modulation", addMenu);

    menu.addSeparator();
    auto& learn = shared.processor.midiLearn();
    const int boundCc = learn.boundCcFor (paramId);
    const bool armedHere = learn.isArmedFor (paramId);
    menu.addItem (4, armedHere ? "MIDI Learn: move a control..." : "MIDI Learn",
                  true, armedHere);
    menu.addItem (5, boundCc >= 0 ? "Clear MIDI (CC " + juce::String (boundCc) + ")"
                                  : "Clear MIDI",
                  boundCc >= 0);

    juce::Component::SafePointer<ModKnob> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
        [safeThis, sourceTokens] (int result)
        {
            if (safeThis == nullptr || result == 0)
                return;
            auto& self = *safeThis;
            auto tree = self.shared.matrixTree();

            if (result == 1)
            {
                self.resetToDefault();
            }
            else if (result == 4)
            {
                auto& midiLearn = self.shared.processor.midiLearn();
                if (midiLearn.isArmedFor (self.paramId))
                    midiLearn.cancelLearn(); // second click un-arms
                else
                    midiLearn.armLearn (self.paramId);
            }
            else if (result == 5)
            {
                self.shared.processor.midiLearn().clearBinding (self.paramId);
            }
            else if (result >= 3000)
            {
                self.addModulationSlot (sourceTokens[result - 3000]);
            }
            else if (result >= 2000)
            {
                auto slot = tree.getChild (result - 2000);
                slot.setProperty ("enabled", false, nullptr);
                slot.setProperty ("dest", juce::String(), nullptr);
                slot.setProperty ("depth", 0.0, nullptr);
            }
            else if (result >= 1000)
            {
                auto slot = tree.getChild (result - 1000);
                auto content = std::make_unique<DepthEditor> (slot, self.accent);
                juce::CallOutBox::launchAsynchronously (
                    std::move (content),
                    self.getTopLevelComponent()->getLocalArea (&self, self.getLocalBounds()),
                    self.getTopLevelComponent());
            }
            self.repaint();
        });
}

// ---------------------------------------------------------------------------
// ChoiceCombo / ParamToggle / TabsBar / ModSourceChip
// ---------------------------------------------------------------------------

ChoiceCombo::ChoiceCombo (const UiShared& sharedContext, const juce::String& paramID)
{
    if (auto* parameter = sharedContext.apvts().getParameter (paramID))
    {
        const auto choices = parameter->getAllValueStrings();
        for (int i = 0; i < choices.size(); ++i)
            box.addItem (choices[i], i + 1);
    }
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        sharedContext.apvts(), paramID, box);
    sharedContext.registerAttachment (paramID);
}

ParamToggle::ParamToggle (const UiShared& sharedContext, const juce::String& paramID,
                          const juce::String& text, juce::Colour accentColour, bool powerStyle)
{
    button.setComponentID (powerStyle ? "power" : "chip");
    button.setButtonText (text);
    button.setColour (juce::TextButton::buttonOnColourId, accentColour);
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        sharedContext.apvts(), paramID, button);
    sharedContext.registerAttachment (paramID);
}

TabsBar::TabsBar (const juce::StringArray& labels, std::function<void (int)> onChangeCallback)
    : onChange (std::move (onChangeCallback))
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* button = buttons.add (new juce::TextButton (labels[i]));
        button->setClickingTogglesState (false);
        button->onClick = [this, i] { setActive (i, true); };
        addAndMakeVisible (button);
    }
    if (! buttons.isEmpty())
        buttons[0]->setToggleState (true, juce::dontSendNotification);
}

void TabsBar::resized()
{
    auto area = getLocalBounds();
    const int count = juce::jmax (1, buttons.size());

    // Each segment gets its label's natural width (the LookAndFeel button
    // font plus the fitted-text indents) with the leftover shared evenly, so
    // a long label like SPECTRAL is never horizontally compressed while
    // equal labels ("1 2 3", "A B") still split the bar equally. Falls back
    // to the plain equal split when the bar is too narrow to set naturally.
    const auto font = theme::medium (juce::jmin (12.0f, (float) getHeight() * 0.65f));
    juce::Array<int> widths;
    int total = 0;
    for (auto* button : buttons)
    {
        const int w = juce::GlyphArrangement::getStringWidthInt (font, button->getButtonText()) + 12;
        widths.add (w);
        total += w;
    }

    if (total > area.getWidth())
    {
        const int w = area.getWidth() / count;
        for (auto* button : buttons)
            button->setBounds (area.removeFromLeft (w).reduced (1, 0));
        return;
    }

    const int share = (area.getWidth() - total) / count;
    for (int i = 0; i < buttons.size(); ++i)
    {
        const int w = i == buttons.size() - 1 ? area.getWidth() // last takes the rounding slack
                                              : widths[i] + share;
        buttons[i]->setBounds (area.removeFromLeft (w).reduced (1, 0));
    }
}

void TabsBar::setActive (int index, bool notify)
{
    activeIndex = juce::jlimit (0, buttons.size() - 1, index);
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == activeIndex, juce::dontSendNotification);
    if (notify && onChange != nullptr)
        onChange (activeIndex);
}

ModSourceChip::ModSourceChip (const UiShared& sharedContext, int sourceIndex,
                              const juce::String& chipText, juce::Colour accentColour)
    : shared (sharedContext), source (sourceIndex), text (chipText), accent (accentColour)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ModSourceChip::paint (juce::Graphics& g)
{
    const float value = juce::jlimit (0.0f, 1.0f,
        std::abs (shared.tap.sourceValue[source].load (std::memory_order_relaxed)));
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (accent.withAlpha (0.08f + 0.25f * value));
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    g.setColour (accent.withAlpha (hovered ? 1.0f : 0.45f + 0.55f * value));
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);
    g.setColour (hovered ? theme::textPrimary : theme::textSecondary);
    g.setFont (theme::medium (10.0f));
    g.drawText (text, bounds, juce::Justification::centred);
}

void ModSourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        container->startDragging (juce::String (ModKnob::kDragPrefix)
                                      + modstate::sourceTokens()[source],
                                  this, juce::ScaledImage(), false);
        dragging = false;
    }
}
