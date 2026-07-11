#include "State/LensState.h"

namespace lumen::lensstate
{
namespace
{
    const juce::Identifier kLens ("LENS");
    const juce::Identifier kImage ("IMAGE");
    const juce::Identifier kMode ("mode");
    const juce::Identifier kTarget ("target");
    const juce::Identifier kOsc ("osc");
    const juce::Identifier kFrames ("frames");
    const juce::Identifier kThumb ("thumb");
    const juce::Identifier kData ("data");
    const juce::Identifier kSeed ("seed");
    const juce::Identifier kSource ("source");

    juce::ValueTree imageNode (const juce::ValueTree& lens, int osc)
    {
        return lens.getChildWithProperty (kOsc, osc);
    }
} // namespace

juce::ValueTree ensureTree (juce::ValueTree& state)
{
    auto lens = state.getChildWithName (kLens);
    if (! lens.isValid())
    {
        lens = juce::ValueTree (kLens);
        lens.setProperty (kMode, 0, nullptr);
        lens.setProperty (kTarget, 0, nullptr);
        state.appendChild (lens, nullptr);
    }
    return lens;
}

juce::ValueTree getTree (const juce::ValueTree& state)
{
    return state.getChildWithName (kLens);
}

int mode (const juce::ValueTree& state)
{
    return juce::jlimit (0, 1, static_cast<int> (getTree (state).getProperty (kMode, 0)));
}

int target (const juce::ValueTree& state)
{
    return juce::jlimit (0, 1, static_cast<int> (getTree (state).getProperty (kTarget, 0)));
}

void setMode (juce::ValueTree& state, int newMode)
{
    ensureTree (state).setProperty (kMode, juce::jlimit (0, 1, newMode), nullptr);
}

void setTarget (juce::ValueTree& state, int osc)
{
    ensureTree (state).setProperty (kTarget, juce::jlimit (0, 1, osc), nullptr);
}

void storeImage (juce::ValueTree& state, int osc,
                 const std::vector<float>& frames,
                 const juce::MemoryBlock& thumbPng,
                 const juce::MemoryBlock& sourceBytes,
                 juce::uint64 seed, const juce::String& sourceName)
{
    if (static_cast<int> (frames.size()) != kFrameFloats)
        return;

    auto lens = ensureTree (state);
    auto node = imageNode (lens, osc);
    if (! node.isValid())
    {
        node = juce::ValueTree (kImage);
        node.setProperty (kOsc, osc, nullptr);
        lens.appendChild (node, nullptr);
    }

    node.setProperty (kFrames,
                      juce::Base64::toBase64 (frames.data(), frames.size() * sizeof (float)),
                      nullptr);
    node.setProperty (kThumb,
                      juce::Base64::toBase64 (thumbPng.getData(), thumbPng.getSize()),
                      nullptr);
    if (sourceBytes.getSize() > 0)
        node.setProperty (kData,
                          juce::Base64::toBase64 (sourceBytes.getData(), sourceBytes.getSize()),
                          nullptr);
    else
        node.removeProperty (kData, nullptr);
    node.setProperty (kSeed, juce::String::toHexString (static_cast<juce::int64> (seed)), nullptr);
    node.setProperty (kSource, sourceName, nullptr);
}

void removeImage (juce::ValueTree& state, int osc)
{
    auto lens = getTree (state);
    if (! lens.isValid())
        return;
    if (auto node = imageNode (lens, osc); node.isValid())
        lens.removeChild (node, nullptr);
}

bool loadImageFrames (const juce::ValueTree& state, int osc, std::vector<float>& out)
{
    const auto node = imageNode (getTree (state), osc);
    if (! node.isValid())
        return false;

    const auto encoded = node.getProperty (kFrames).toString();
    if (encoded.isEmpty())
        return false;

    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, encoded)
        || decoded.getDataSize() != kFrameFloats * sizeof (float))
        return false;

    out.resize (kFrameFloats);
    std::memcpy (out.data(), decoded.getData(), decoded.getDataSize());
    return true;
}

juce::Image loadThumbnail (const juce::ValueTree& state, int osc)
{
    const auto node = imageNode (getTree (state), osc);
    if (! node.isValid())
        return {};

    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, node.getProperty (kThumb).toString()))
        return {};

    return juce::ImageFileFormat::loadFrom (decoded.getData(), decoded.getDataSize());
}

bool loadSourceBytes (const juce::ValueTree& state, int osc, juce::MemoryBlock& out)
{
    const auto node = imageNode (getTree (state), osc);
    if (! node.isValid())
        return false;

    const auto encoded = node.getProperty (kData).toString();
    if (encoded.isEmpty())
        return false;

    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, encoded) || decoded.getDataSize() == 0)
        return false;

    out.replaceAll (decoded.getData(), decoded.getDataSize());
    return true;
}

juce::String sourceName (const juce::ValueTree& state, int osc)
{
    return imageNode (getTree (state), osc).getProperty (kSource).toString();
}

bool hasImage (const juce::ValueTree& state, int osc)
{
    const auto node = imageNode (getTree (state), osc);
    return node.isValid() && node.getProperty (kFrames).toString().isNotEmpty();
}

bool preserveSessionImages (const juce::ValueTree& currentState,
                            juce::ValueTree& incomingState)
{
    const auto currentLens = getTree (currentState);
    if (! currentLens.isValid())
        return false;

    bool preservedAny = false;
    for (int osc = 0; osc < 2; ++osc)
    {
        const auto currentNode = imageNode (currentLens, osc);
        if (! currentNode.isValid()
            || currentNode.getProperty (kFrames).toString().isEmpty())
            continue; // no session image on this osc: the incoming preset's
                      // image (if any) stays

        auto incomingLens = ensureTree (incomingState);
        if (auto stale = imageNode (incomingLens, osc); stale.isValid())
            incomingLens.removeChild (stale, nullptr);
        incomingLens.appendChild (currentNode.createCopy(), nullptr);
        preservedAny = true;
    }

    // The images carry their workflow settings with them; without a session
    // image the incoming preset keeps its own mode/target (the Lens factory
    // presets stay pristine on a clean session).
    if (preservedAny)
    {
        setMode (incomingState, mode (currentState));
        setTarget (incomingState, target (currentState));
    }
    return preservedAny;
}
} // namespace lumen::lensstate
