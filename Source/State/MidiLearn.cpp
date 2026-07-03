#include "State/MidiLearn.h"

namespace lumen
{
MidiLearnController::MidiLearnController (juce::AudioProcessorValueTreeState& apvts)
    : MidiLearnController (apvts, defaultMapFile())
{
}

MidiLearnController::MidiLearnController (juce::AudioProcessorValueTreeState& apvts,
                                          const juce::File& mapFile)
    : state (apvts), file (mapFile)
{
    loadFromFile();
}

juce::File MidiLearnController::defaultMapFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Lumen")
        .getChildFile ("midi_map.xml");
}

void MidiLearnController::armLearn (const juce::String& paramId)
{
    armed.store (state.getParameter (paramId), std::memory_order_release);
}

bool MidiLearnController::isArmedFor (const juce::String& paramId) const
{
    auto* p = armed.load (std::memory_order_acquire);
    return p != nullptr && p->paramID == paramId;
}

int MidiLearnController::boundCcFor (const juce::String& paramId) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (auto* p = ccParams[cc].load (std::memory_order_acquire))
            if (p->paramID == paramId)
                return cc;
    return -1;
}

void MidiLearnController::clearBinding (const juce::String& paramId)
{
    bool changed = false;
    for (int cc = 0; cc < 128; ++cc)
        if (auto* p = ccParams[cc].load (std::memory_order_acquire))
            if (p->paramID == paramId)
            {
                ccParams[cc].store (nullptr, std::memory_order_release);
                changed = true;
            }
    if (changed)
        saveToFile();
}

bool MidiLearnController::poll()
{
    const int cc = lastLearnedCc.exchange (-1, std::memory_order_acq_rel);
    if (cc < 0)
        return false;

    // One CC per parameter: a re-learn moves the binding instead of fanning
    // one knob out over several hardware controls.
    auto* learned = ccParams[cc].load (std::memory_order_acquire);
    for (int i = 0; i < 128; ++i)
        if (i != cc && ccParams[i].load (std::memory_order_acquire) == learned)
            ccParams[i].store (nullptr, std::memory_order_release);

    saveToFile();
    return true;
}

void MidiLearnController::handleController (int ccNumber, int ccValue) noexcept
{
    if (ccNumber < 0 || ccNumber >= 128)
        return;

    if (auto* toLearn = armed.exchange (nullptr, std::memory_order_acq_rel))
    {
        ccParams[ccNumber].store (toLearn, std::memory_order_release);
        lastLearnedCc.store (ccNumber, std::memory_order_release);
        // fall through: the learning gesture already moves the knob
    }

    if (auto* parameter = ccParams[ccNumber].load (std::memory_order_acquire))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (static_cast<float> (ccValue) / 127.0f);
        parameter->endChangeGesture();
    }
}

void MidiLearnController::loadFromFile()
{
    const auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("MIDIMAP"))
        return;

    for (const auto* bind : xml->getChildWithTagNameIterator ("BIND"))
    {
        const int cc = bind->getIntAttribute ("cc", -1);
        if (cc >= 0 && cc < 128)
            ccParams[cc].store (state.getParameter (bind->getStringAttribute ("param")),
                                std::memory_order_release);
    }
}

void MidiLearnController::saveToFile() const
{
    juce::XmlElement xml ("MIDIMAP");
    xml.setAttribute ("version", 1);
    for (int cc = 0; cc < 128; ++cc)
        if (auto* p = ccParams[cc].load (std::memory_order_acquire))
        {
            auto* bind = xml.createNewChildElement ("BIND");
            bind->setAttribute ("cc", cc);
            bind->setAttribute ("param", p->paramID);
        }

    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}
} // namespace lumen
