#include "State/LensState.h"

namespace lumen::lensstate
{
namespace
{
    const juce::Identifier kLens ("LENS");
    const juce::Identifier kImage ("IMAGE");
    const juce::Identifier kMode ("mode");
    const juce::Identifier kChroma ("chroma");
    const juce::Identifier kTarget ("target");
    const juce::Identifier kOsc ("osc");
    const juce::Identifier kFrames ("frames");
    const juce::Identifier kThumb ("thumb");
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
        lens.setProperty (kChroma, 0, nullptr);
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

bool chroma (const juce::ValueTree& state)
{
    return static_cast<int> (getTree (state).getProperty (kChroma, 0)) != 0;
}

int target (const juce::ValueTree& state)
{
    return juce::jlimit (0, 1, static_cast<int> (getTree (state).getProperty (kTarget, 0)));
}

void setMode (juce::ValueTree& state, int newMode)
{
    ensureTree (state).setProperty (kMode, juce::jlimit (0, 1, newMode), nullptr);
}

void setChroma (juce::ValueTree& state, bool on)
{
    ensureTree (state).setProperty (kChroma, on ? 1 : 0, nullptr);
}

void setTarget (juce::ValueTree& state, int osc)
{
    ensureTree (state).setProperty (kTarget, juce::jlimit (0, 1, osc), nullptr);
}

void storeImage (juce::ValueTree& state, int osc,
                 const std::vector<float>& frames,
                 const juce::MemoryBlock& thumbPng,
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

juce::String sourceName (const juce::ValueTree& state, int osc)
{
    return imageNode (getTree (state), osc).getProperty (kSource).toString();
}

bool hasImage (const juce::ValueTree& state, int osc)
{
    const auto node = imageNode (getTree (state), osc);
    return node.isValid() && node.getProperty (kFrames).toString().isNotEmpty();
}
} // namespace lumen::lensstate
