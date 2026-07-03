#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

class LumenAudioProcessor;

namespace lumen
{
// Message-thread preset browser/loader (SPEC section 16). The list is the
// factory bank (compiled in, always available) followed by user presets
// scanned from Documents/Lumen/Presets/<Category>/<Name>.lumen. Loading
// goes through LumenAudioProcessor::loadPresetState (the exact
// setStateInformation path), so preset recall and DAW recall are the same
// code. The current preset name/category live in the state tree
// ("presetName"/"presetCategory" properties) and therefore survive host
// save/restore.
class PresetManager
{
public:
    explicit PresetManager (LumenAudioProcessor& processorToUse);

    struct Entry
    {
        juce::String name, category;
        int factoryIndex = -1; // >= 0: index into presets::bank()
        juce::File file;       // user preset file when factoryIndex < 0
        bool isFactory() const noexcept { return factoryIndex >= 0; }
    };

    static juce::File presetsDirectory(); // Documents/Lumen/Presets

    void refresh(); // rescan user presets on disk
    const std::vector<Entry>& entries() const noexcept { return list; }

    juce::String currentName() const;    // "Init" when the state has no name
    int currentIndex() const;            // -1 when the name isn't in the list

    bool loadIndex (int index);
    bool loadFactory (const juce::String& name);
    void step (int delta);               // < / > stepping, wraps around

    // Saves the live state (parameters + matrix/macros + Lens data) as a
    // user .lumen file at <Presets>/User/<Name>.lumen; overwrites same-named
    // files. User presets always live in the "User" category (name only —
    // there is no category or author prompt).
    bool saveUserPreset (const juce::String& name);

private:
    LumenAudioProcessor& processor;
    std::vector<Entry> list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
} // namespace lumen
