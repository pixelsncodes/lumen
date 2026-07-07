#pragma once

#include "UI/Controls.h"

// Melody overlay panel (Lumena integration). Toggled from the Lens section; a
// self-contained tool with the image + sampling-grid visualization, the
// generator controls, transport, and MIDI export. Placeholder until the full
// UI lands.
class MelodyPanel final : public juce::Component
{
public:
    explicit MelodyPanel (const UiShared& sharedContext);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void animate();

private:
    UiShared shared;
};
