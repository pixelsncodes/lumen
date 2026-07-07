#include "UI/MelodyPanel.h"

#include "UI/Theme.h"

MelodyPanel::MelodyPanel (const UiShared& sharedContext) : shared (sharedContext) {}

void MelodyPanel::resized() {}

void MelodyPanel::paint (juce::Graphics& g)
{
    g.fillAll (lumen::theme::panel);
}

void MelodyPanel::animate() {}
