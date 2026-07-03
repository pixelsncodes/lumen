#include "Lens/LensController.h"

#include "State/LensState.h"
#include "State/Parameters.h"

namespace lumen
{
LensController::LensController (juce::AudioProcessorValueTreeState& apvtsToUse,
                                SynthEngine& engineToUse)
    : apvts (apvtsToUse), engine (engineToUse)
{
    lensstate::ensureTree (apvts.state);
    applyStateToEngine();
}

LensController::~LensController()
{
    // The processor tears down after audio has stopped; drop the pointers so
    // the engine never dangles into freed tables.
    engine.setImageTable (0, nullptr);
    engine.setImageTable (1, nullptr);
}

bool LensController::loadImageFile (const juce::File& file)
{
    return loadImage (juce::ImageFileFormat::loadFrom (file), file.getFileName());
}

bool LensController::loadImage (const juce::Image& image, const juce::String& sourceName)
{
    auto analysis = lens::analyzeImage (image);
    if (! analysis.valid)
        return false;

    const int osc = target();
    session[osc] = std::move (analysis);
    display[osc] = session[osc].working;
    names[osc] = sourceName;
    analyzeAndInstall (osc, true, true);
    return true;
}

void LensController::analyzeAndInstall (int osc, bool storeState, bool allowChroma)
{
    const auto& analysis = session[osc];
    if (! analysis.valid)
        return;

    const auto frames = lens::buildFrames (analysis,
                                           static_cast<lens::Mode> (mode()));
    installTable (osc, frames);

    if (storeState)
    {
        const auto thumbPng = lens::encodePng (lens::makeThumbnail (analysis));
        lensstate::storeImage (apvts.state, osc, frames, thumbPng,
                               analysis.seed, names[osc]);
    }

    // The result loads into the osc's Image slot and takes over (SPEC 13.6).
    const juce::String prefix = osc == 1 ? "oscB" : "oscA";
    setParamNatural (prefix + "Table", 4.0f); // TableChoice::image
    setParamNatural (prefix + "Enabled", 1.0f);

    if (allowChroma && chroma())
        applyChromaPatch (lens::chromaStats (analysis), osc);
}

void LensController::installTable (int osc, const std::vector<float>& frames)
{
    if (static_cast<int> (frames.size()) != lens::kNumFrames * lens::kFrameLength)
        return;

    auto table = std::make_unique<Wavetable>();
    table->build (frames.data(), lens::kNumFrames, lens::kHarmonicCap, lens::kPeakTarget);

    retire (std::move (current[osc]));
    current[osc] = std::move (table);
    engine.setImageTable (osc, current[osc].get());
    ++version;
    pruneRetired();
}

void LensController::clearTable (int osc)
{
    if (current[osc] == nullptr)
        return;
    engine.setImageTable (osc, nullptr);
    retire (std::move (current[osc]));
    display[osc] = {};
    names[osc] = {};
    ++version;
    pruneRetired();
}

void LensController::retire (std::unique_ptr<Wavetable> table)
{
    if (table != nullptr)
        retired.push_back ({ std::move (table), engine.renderCallCount() });
}

void LensController::pruneRetired()
{
    // A table retired at render count N can still be read by a render call
    // that was in flight during the swap; once the counter reaches N + 2
    // that call has certainly finished. If the host never renders, entries
    // simply wait here (bounded by the number of image loads this session).
    const auto now = engine.renderCallCount();
    std::erase_if (retired, [now] (const Retired& r) { return now >= r.renderCount + 2; });
}

void LensController::setMode (int newMode)
{
    if (newMode == mode())
        return;
    lensstate::setMode (apvts.state, newMode);

    // Re-analyze the target osc if its source image is still in memory this
    // session. The chroma patch is mode-independent, so it is not re-applied.
    const int osc = target();
    if (session[osc].valid)
        analyzeAndInstall (osc, true, false);
}

void LensController::setChroma (bool on)
{
    if (on == chroma())
        return;
    lensstate::setChroma (apvts.state, on);

    // Turning it on with an image already loaded applies the patch now.
    const int osc = target();
    if (on && session[osc].valid)
        applyChromaPatch (lens::chromaStats (session[osc]), osc);
}

void LensController::setTarget (int osc)
{
    lensstate::setTarget (apvts.state, osc);
}

int LensController::mode() const     { return lensstate::mode (apvts.state); }
bool LensController::chroma() const  { return lensstate::chroma (apvts.state); }
int LensController::target() const   { return lensstate::target (apvts.state); }

void LensController::applyStateToEngine()
{
    lensstate::ensureTree (apvts.state);

    std::vector<float> frames;
    for (int osc = 0; osc < 2; ++osc)
    {
        session[osc] = {}; // the loaded state has no source image, only data
        if (lensstate::loadImageFrames (apvts.state, osc, frames))
        {
            installTable (osc, frames);
            display[osc] = lensstate::loadThumbnail (apvts.state, osc);
            names[osc] = lensstate::sourceName (apvts.state, osc);
        }
        else
        {
            clearTable (osc);
        }
    }
}

void LensController::setParamNatural (const juce::String& paramId, float naturalValue)
{
    if (auto* param = apvts.getParameter (paramId))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (naturalValue));
        param->endChangeGesture();
    }
}

void LensController::applyChromaPatch (const lens::ChromaStats& stats, int osc)
{
    const auto t = lens::patchTargetsFor (stats);
    const juce::String prefix = osc == 1 ? "oscB" : "oscA";

    setParamNatural ("filterMode", static_cast<float> (t.filterMode));
    setParamNatural (params::filterCutoff, t.cutoffHz);
    setParamNatural (params::filterRes, t.res);
    setParamNatural (prefix + "Detune", t.detuneCents);
    setParamNatural (prefix + "Unison", static_cast<float> (t.unison));
    setParamNatural (params::env1Attack, t.attackSeconds);
    setParamNatural (params::env1Release, t.releaseSeconds);
    setParamNatural (params::driveAmount, t.driveDb);
    setParamNatural (params::driveEnabled, t.driveDb > 0.05f ? 1.0f : 0.0f);
    setParamNatural (params::noiseLevel, t.noiseDb);
    setParamNatural (params::reverbMix, t.reverbMix);
    setParamNatural (params::macro4, t.macro4);

    // LFO 1 -> target osc morph: sine, poly, free-running (SPEC 13.5).
    setParamNatural (params::lfo1Shape, 0.0f);
    setParamNatural (params::lfo1Sync, 0.0f);
    setParamNatural (params::lfo1Mode, 0.0f);
    setParamNatural (params::lfo1Rate, t.lfoRateHz);

    // Route through the matrix: reuse an existing lfo1 -> morph slot, else
    // the first disabled slot. Property edits republish the config through
    // the processor's ValueTree listener.
    auto matrix = apvts.state.getChildWithName ("MODMATRIX");
    if (! matrix.isValid())
        return;

    const juce::String morphToken = prefix + "Morph";
    juce::ValueTree slot;
    for (const auto& candidate : matrix)
    {
        if (candidate["source"].toString() == "lfo1"
            && candidate["dest"].toString() == morphToken)
        {
            slot = candidate;
            break;
        }
        if (! slot.isValid() && ! static_cast<bool> (candidate["enabled"]))
            slot = candidate;
    }
    if (slot.isValid())
    {
        slot.setProperty ("source", "lfo1", nullptr);
        slot.setProperty ("dest", morphToken, nullptr);
        slot.setProperty ("depth", static_cast<double> (t.lfoDepth), nullptr);
        slot.setProperty ("enabled", true, nullptr);
    }
}
} // namespace lumen
