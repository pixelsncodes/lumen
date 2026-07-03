#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/SynthEngine.h"
#include "Lens/LensEngine.h"

#include <memory>
#include <vector>

namespace lumen
{
// Message-thread owner of the Lens image tables and settings (plugin side).
// Responsibilities:
//   - decode a dropped image, run the Lens analysis (LensEngine), build the
//     mip-mapped Wavetable and swap it into the SynthEngine atomically;
//   - retire old tables safely (freed only once the engine's render counter
//     has advanced past the swap — CLAUDE.md rule 1);
//   - persist the generated frames + 64x64 PNG thumbnail in the APVTS state
//     tree (LensState) — never a path to the original file (SPEC 13);
//   - apply the SPEC 13.5 chroma patch when "Set patch from colors" is on;
//   - rebuild tables from state after setStateInformation.
//
// Every method must be called on the message thread.
class LensController
{
public:
    LensController (juce::AudioProcessorValueTreeState& apvtsToUse, SynthEngine& engineToUse);
    ~LensController();

    // Decode + analyze + install into the current target osc. False if the
    // file can't be decoded as an image.
    bool loadImageFile (const juce::File& file);
    bool loadImage (const juce::Image& image, const juce::String& sourceName);

    // Remove the osc's Lens image: clear the table/thumbnail (engine +
    // state) and revert the osc to the init wavetable if it was playing the
    // Image slot. Every other patch parameter is left as it is.
    void removeImage (int osc);

    // Settings (persisted in the LENS state tree). Changing the mode
    // re-analyzes the in-session source image if one is still in memory;
    // after a state reload only the generated table exists (SPEC 13 stores
    // no source data), so the new mode applies to the next drop.
    void setMode (int newMode);
    void setChroma (bool on);
    void setTarget (int osc);
    int mode() const;
    bool chroma() const;
    int target() const;

    // Rebuild engine tables from the stored state (ctor / setStateInformation).
    void applyStateToEngine();

    // UI access: the image shown in the Lens panel for an osc — the session
    // source while it exists, else the persisted thumbnail. Invalid Image if
    // the osc has no Lens table.
    juce::Image displayImage (int osc) const { return display[oscIndex (osc)]; }
    bool hasImage (int osc) const { return current[oscIndex (osc)] != nullptr; }
    juce::String imageName (int osc) const { return names[oscIndex (osc)]; }

    // The installed table (message-thread read for the 3D stack view).
    const Wavetable* currentTable (int osc) const { return current[oscIndex (osc)].get(); }

    // Bumped on every install/clear — visualizers repaint when it changes.
    int tableVersion() const noexcept { return version; }

private:
    static int oscIndex (int osc) noexcept { return osc == 1 ? 1 : 0; }

    void analyzeAndInstall (int osc, bool storeState, bool allowChroma);
    void applyMorphJourney (int osc);
    void removeMorphJourney (int osc);
    void installTable (int osc, const std::vector<float>& frames);
    void clearTable (int osc);
    void retire (std::unique_ptr<Wavetable> table);
    void pruneRetired();
    void applyChromaPatch (const lens::ChromaStats& stats, int osc);
    void setParamNatural (const juce::String& paramId, float naturalValue);

    juce::AudioProcessorValueTreeState& apvts;
    SynthEngine& engine;

    lens::Analysis session[2];              // in-session sources, not persisted
    juce::Image display[2];
    juce::String names[2];
    std::unique_ptr<Wavetable> current[2];
    int version = 0;

    struct Retired
    {
        std::unique_ptr<Wavetable> table;
        uint64_t renderCount = 0;
    };
    std::vector<Retired> retired;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LensController)
};
} // namespace lumen
