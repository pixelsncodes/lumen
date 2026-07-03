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

    // Image drops (and only drops — never preset/state loads, so the Lens
    // factory presets' own morph motion is never doubled) also route the
    // per-note morph journey through the image.
    applyMorphJourney (osc);
    return true;
}

void LensController::applyMorphJourney (int osc)
{
    // Env 3 becomes the journey ramp: a slow glide to the far end of the
    // image, held there for the rest of the note (constants in LensEngine.h,
    // logged in DECISIONS.md). Decay/release are left to the loaded patch.
    setParamNatural (params::env3Attack, lens::kJourneyAttackSeconds);
    setParamNatural (params::env3Sustain, 1.0f);
    setParamNatural (params::env3Curve, 0.0f);

    // Matrix route env3 -> target morph: reuse an existing slot for that
    // pair, else the first disabled slot (same policy as the chroma patch).
    const juce::String morphToken = (osc == 1 ? "oscB" : "oscA") + juce::String ("Morph");
    if (auto matrix = apvts.state.getChildWithName ("MODMATRIX"); matrix.isValid())
    {
        juce::ValueTree slot;
        for (const auto& candidate : matrix)
        {
            if (candidate["source"].toString() == "env3"
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
            slot.setProperty ("source", "env3", nullptr);
            slot.setProperty ("dest", morphToken, nullptr);
            slot.setProperty ("depth", static_cast<double> (lens::kJourneyDepth), nullptr);
            slot.setProperty ("enabled", true, nullptr);
        }
    }

    // Motion scales the travel speed: a Motion -> env3Attack map pinned
    // neutral at Motion's current value, so the drop itself changes nothing.
    auto macros = apvts.state.getChildWithName ("MACROS");
    auto motion = macros.getChild (1);
    if (! motion.isValid())
        return;
    juce::ValueTree map;
    for (const auto& candidate : motion)
        if (candidate["dest"].toString() == "env3Attack")
        {
            map = candidate;
            break;
        }
    if (! map.isValid())
    {
        if (motion.getNumChildren() >= mod::kMaxMacroMaps)
            return; // Motion is fully mapped by the patch; speed stays fixed
        map = juce::ValueTree ("MAP");
        motion.appendChild (map, nullptr);
    }
    float motionValue = 0.5f;
    if (auto* value = apvts.getRawParameterValue (params::macro2))
        motionValue = value->load();
    const float atRest = lens::kJourneyMotionSpan * motionValue;
    map.setProperty ("dest", "env3Attack", nullptr);
    map.setProperty ("min", static_cast<double> (-atRest), nullptr);
    map.setProperty ("max", static_cast<double> (lens::kJourneyMotionSpan - atRest), nullptr);
    map.setProperty ("curve", 1.0, nullptr);
}

void LensController::removeMorphJourney (int osc)
{
    const juce::String morphToken = (osc == 1 ? "oscB" : "oscA") + juce::String ("Morph");
    if (auto matrix = apvts.state.getChildWithName ("MODMATRIX"); matrix.isValid())
        for (auto slot : matrix)
            if (slot["source"].toString() == "env3" && slot["dest"].toString() == morphToken)
            {
                slot.setProperty ("enabled", false, nullptr);
                slot.setProperty ("dest", juce::String(), nullptr);
                slot.setProperty ("depth", 0.0, nullptr);
            }

    // Drop the Motion speed map only if no other osc still journeys.
    if (auto matrix = apvts.state.getChildWithName ("MODMATRIX"); matrix.isValid())
        for (const auto& slot : matrix)
            if (static_cast<bool> (slot["enabled"]) && slot["source"].toString() == "env3"
                && slot["dest"].toString().endsWith ("Morph"))
                return;
    auto motion = apvts.state.getChildWithName ("MACROS").getChild (1);
    for (int i = motion.getNumChildren(); --i >= 0;)
        if (motion.getChild (i)["dest"].toString() == "env3Attack")
            motion.removeChild (i, nullptr);
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

void LensController::removeImage (int osc)
{
    const int index = oscIndex (osc);
    session[index] = {};
    clearTable (index);
    display[index] = {};
    names[index] = {};
    lensstate::removeImage (apvts.state, index);

    // Revert to the init wavetable only if the osc is still on its Image
    // slot (leave a user's explicit factory-table choice alone).
    const juce::String tableId = index == 1 ? "oscBTable" : "oscATable";
    if (auto* value = apvts.getRawParameterValue (tableId);
        value != nullptr && juce::roundToInt (value->load()) == 4)
        setParamNatural (tableId, 0.0f); // TableChoice::basic — the Init table

    // Without an image the journey would sweep the factory table instead;
    // take its route (and the Motion speed map, if unshared) back out.
    removeMorphJourney (index);
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

    // Macro knob positions from the image statistics (SPEC 13.5 extension):
    // Tone <- Vm, Motion <- sigV, Space <- smoothness, Texture <- edges.
    const char* macroIds[4] = { params::macro1, params::macro2,
                                params::macro3, params::macro4 };
    for (int m = 0; m < 4; ++m)
        setParamNatural (macroIds[m], t.macros[m]);

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
