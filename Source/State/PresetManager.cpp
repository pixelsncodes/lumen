#include "State/PresetManager.h"

#include "PluginProcessor.h"
#include "State/FactoryPresets.h"

#include <algorithm>

namespace lumen
{
namespace
{
    constexpr const char* kExtension = ".lumen";

    int categoryRank (const juce::String& category)
    {
        const int index = presets::categories().indexOf (category);
        return index >= 0 ? index : presets::categories().size(); // user categories last
    }
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

    // User presets: <Presets>/<Category>/<Name>.lumen. Factory names win a
    // collision (the factory bank is the reference the manual points at).
    std::vector<Entry> user;
    for (const auto& file : presetsDirectory().findChildFiles (
             juce::File::findFiles, true, juce::String ("*") + kExtension))
    {
        const auto name = file.getFileNameWithoutExtension();
        if (presets::find (name) != nullptr)
            continue;
        const auto parent = file.getParentDirectory();
        const auto category = parent == presetsDirectory() ? juce::String ("User")
                                                           : parent.getFileName();
        user.push_back ({ name, category, -1, file });
    }
    std::sort (user.begin(), user.end(), [] (const Entry& a, const Entry& b)
    {
        if (const int rank = categoryRank (a.category) - categoryRank (b.category); rank != 0)
            return rank < 0;
        if (a.category != b.category)
            return a.category.compareIgnoreCase (b.category) < 0;
        return a.name.compareIgnoreCase (b.name) < 0;
    });
    list.insert (list.end(), user.begin(), user.end());
}

juce::String PresetManager::currentName() const
{
    return processor.apvts.state.getProperty ("presetName", "Init").toString();
}

juce::String PresetManager::currentCategory() const
{
    return processor.apvts.state.getProperty ("presetCategory", "").toString();
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
        processor.loadPresetState (presets::buildState (preset, processor.apvts));
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
    processor.loadPresetState (state);
    return true;
}

bool PresetManager::loadFactory (const juce::String& name)
{
    if (const auto* preset = presets::find (name))
    {
        processor.loadPresetState (presets::buildState (*preset, processor.apvts));
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

bool PresetManager::saveUserPreset (const juce::String& name, const juce::String& category,
                                    const juce::String& author)
{
    const auto legalName = juce::File::createLegalFileName (name.trim());
    if (legalName.isEmpty())
        return false;
    const auto legalCategory = juce::File::createLegalFileName (
        category.trim().isEmpty() ? juce::String ("User") : category.trim());

    processor.apvts.state.setProperty ("presetName", legalName, nullptr);
    processor.apvts.state.setProperty ("presetCategory", legalCategory, nullptr);
    processor.apvts.state.setProperty ("presetAuthor", author.trim(), nullptr);

    const auto file = presetsDirectory().getChildFile (legalCategory)
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
