#include "State/PresetManager.h"

#include "PluginProcessor.h"
#include "State/FactoryPresets.h"

#include <algorithm>

namespace lumen
{
namespace
{
    constexpr const char* kExtension = ".lumen";
    constexpr const char* kUserCategory = "User";
} // namespace

PresetManager::PresetManager (LumenAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    refresh();
}

juce::File PresetManager::presetsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Lumen").getChildFile ("Presets");
}

void PresetManager::refresh()
{
    list.clear();

    for (int i = 0; i < static_cast<int> (presets::bank().size()); ++i)
    {
        const auto& preset = presets::bank()[static_cast<size_t> (i)];
        list.push_back ({ preset.name, preset.category, i, {} });
    }

    // User presets: every .lumen file under <Presets>/, any depth. Factory
    // names win a collision (the factory bank is the reference the manual
    // points at). Display-time bucketing: whatever folder or stored category
    // a file has, it shows under the single "User" section pinned after the
    // factory bank — existing saved presets migrate with no state change.
    std::vector<Entry> user;
    for (const auto& file : presetsDirectory().findChildFiles (
             juce::File::findFiles, true, juce::String ("*") + kExtension))
    {
        const auto name = file.getFileNameWithoutExtension();
        if (presets::find (name) != nullptr)
            continue;
        user.push_back ({ name, kUserCategory, -1, file });
    }
    std::sort (user.begin(), user.end(), [] (const Entry& a, const Entry& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });
    list.insert (list.end(), user.begin(), user.end());
}

juce::String PresetManager::currentName() const
{
    return processor.apvts.state.getProperty ("presetName", "Init").toString();
}

int PresetManager::currentIndex() const
{
    const auto name = currentName();
    for (int i = 0; i < static_cast<int> (list.size()); ++i)
        if (list[static_cast<size_t> (i)].name.equalsIgnoreCase (name))
            return i;
    return -1;
}

bool PresetManager::loadIndex (int index)
{
    if (index < 0 || index >= static_cast<int> (list.size()))
        return false;

    const auto& entry = list[static_cast<size_t> (index)];
    if (entry.isFactory())
    {
        const auto& preset = presets::bank()[static_cast<size_t> (entry.factoryIndex)];
        processor.loadPresetState (presets::buildState (preset, processor.apvts),
                                   /*keepSessionImage=*/true);
        return true;
    }

    const auto xml = juce::parseXML (entry.file);
    if (xml == nullptr || ! xml->hasTagName (processor.apvts.state.getType()))
        return false;

    auto state = juce::ValueTree::fromXml (*xml);
    // Older/hand-edited files: name/category follow the file location.
    if (! state.hasProperty ("presetName"))
        state.setProperty ("presetName", entry.name, nullptr);
    if (! state.hasProperty ("presetCategory"))
        state.setProperty ("presetCategory", entry.category, nullptr);
    processor.loadPresetState (state, /*keepSessionImage=*/true);
    return true;
}

bool PresetManager::loadFactory (const juce::String& name)
{
    if (const auto* preset = presets::find (name))
    {
        processor.loadPresetState (presets::buildState (*preset, processor.apvts),
                                   /*keepSessionImage=*/true);
        return true;
    }
    return false;
}

void PresetManager::step (int delta)
{
    const int count = static_cast<int> (list.size());
    if (count == 0)
        return;
    const int current = currentIndex();
    const int next = current < 0 ? (delta >= 0 ? 0 : count - 1)
                                 : ((current + delta) % count + count) % count;
    loadIndex (next);
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    const auto legalName = juce::File::createLegalFileName (name.trim());
    if (legalName.isEmpty())
        return false;

    processor.apvts.state.setProperty ("presetName", legalName, nullptr);
    processor.apvts.state.setProperty ("presetCategory", juce::String (kUserCategory), nullptr);
    // No author prompt for user saves; drop any factory author riding along.
    processor.apvts.state.removeProperty ("presetAuthor", nullptr);

    const auto file = presetsDirectory().getChildFile (kUserCategory)
                          .getChildFile (legalName + kExtension);
    if (! file.getParentDirectory().createDirectory())
        return false;

    const auto xml = processor.apvts.copyState().createXml();
    if (xml == nullptr || ! xml->writeTo (file))
        return false;

    refresh();
    return true;
}
} // namespace lumen
