#include "Melody/MelodyController.h"

#include "Lens/LensController.h"
#include "Melody/MelodyPlayer.h"
#include "State/MelodyState.h"
#include "State/Parameters.h"

// Lumena library (external/lumena, linked as Lumena::Lumena). This is the only
// translation unit in the plugin that touches the library's headers.
#include "image/BrightnessGrid.h"
#include "image/Image.h"
#include "melody/MelodyGenerator.h"
#include "midi/MidiFileWriter.h"
#include "midi/MidiSequence.h"
#include "scales/KeySelector.h"
#include "scales/Scale.h"

#include <random>

namespace lumen
{
namespace
{
    // Bridge Lumen's decoded image (JUCE stores it ARGB) into a Lumena image
    // (tightly-packed RGBA8, top-left origin, per image::Image's contract).
    // getPixelColour hides the underlying byte order, so this is correct for
    // ARGB, RGB, or single-channel sources alike.
    lumena::image::Image toLumenaImage (const juce::Image& source)
    {
        if (! source.isValid())
            return {};

        const int w = source.getWidth();
        const int h = source.getHeight();
        std::vector<std::uint8_t> rgba (static_cast<std::size_t> (w) * h * 4);

        const juce::Image::BitmapData data (source, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const juce::Colour c = data.getPixelColour (x, y);
                const std::size_t i = (static_cast<std::size_t> (y) * w + x) * 4;
                rgba[i + 0] = c.getRed();
                rgba[i + 1] = c.getGreen();
                rgba[i + 2] = c.getBlue();
                rgba[i + 3] = c.getAlpha();
            }
        }
        return lumena::image::Image (w, h, std::move (rgba));
    }

    // A solid 2x2 RGBA image of one HSV colour — used to drive Lumena's
    // circle-of-fifths key detection for the Random key mode without shipping a
    // scales JSON file (the detector maps average hue+saturation to a key).
    lumena::image::Image solidColourImage (float hue01, float sat)
    {
        const juce::Colour c = juce::Colour::fromHSV (hue01, sat, 0.9f, 1.0f);
        std::vector<std::uint8_t> rgba (2u * 2u * 4u);
        for (int p = 0; p < 4; ++p)
        {
            rgba[p * 4 + 0] = c.getRed();
            rgba[p * 4 + 1] = c.getGreen();
            rgba[p * 4 + 2] = c.getBlue();
            rgba[p * 4 + 3] = 255;
        }
        return lumena::image::Image (2, 2, std::move (rgba));
    }

    int clampVelocity127 (int v) { return juce::jlimit (1, 127, v); }
} // namespace

MelodyController::MelodyController (juce::AudioProcessorValueTreeState& apvtsToUse,
                                    LensController& lensToUse, MelodyPlayer& playerToUse)
    : apvts (apvtsToUse), lens (lensToUse), player (playerToUse)
{
    seedValue = makeSeed(); // a starting seed; nothing is generated until asked
}

int MelodyController::choiceIndex (const char* paramId) const
{
    if (auto* p = apvts.getRawParameterValue (paramId))
        return static_cast<int> (p->load());
    return 0;
}

float MelodyController::floatParam (const char* paramId) const
{
    if (auto* p = apvts.getRawParameterValue (paramId))
        return p->load();
    return 0.0f;
}

juce::uint64 MelodyController::makeSeed()
{
    // Two 32-bit draws make a full 64-bit seed; the value only needs to be
    // fresh and reproducible once stored, not cryptographic.
    const auto hi = static_cast<juce::uint64> (static_cast<juce::uint32> (rng.nextInt()));
    const auto lo = static_cast<juce::uint64> (static_cast<juce::uint32> (rng.nextInt()));
    return (hi << 32) ^ lo;
}

void MelodyController::generate()
{
    const int osc = lens.target();
    const juce::Image jimg = lens.displayImage (osc);
    if (! jimg.isValid())
        return; // no image loaded: nothing to sample

    const lumena::image::Image img = toLumenaImage (jimg);
    if (img.empty())
        return;

    const int gridN = kGridResolution;
    const lumena::image::BrightnessGrid grid (img, gridN, gridN);

    // One RNG drives the whole pass so a fixed seed is fully reproducible. In
    // Random key mode the key colour is drawn first, then the same RNG feeds
    // the melody walk.
    std::mt19937 gen (static_cast<std::uint32_t> (seedValue ^ (seedValue >> 32)));

    const lumena::scales::KeySelector selector;
    lumena::scales::KeyDetection detection;
    if (choiceIndex (params::melodyKeyMode) == 1) // Random
    {
        std::uniform_real_distribution<double> hueDist (0.0, 1.0);
        std::uniform_real_distribution<double> satDist (0.0, 1.0);
        const float hue01 = static_cast<float> (hueDist (gen));
        // Keep saturation clear of the grayscale floor and spanning the
        // major/minor threshold (0.5) so both modes remain reachable.
        const float sat = 0.2f + 0.7f * static_cast<float> (satDist (gen));
        detection = selector.detect (solidColourImage (hue01, sat));
    }
    else // From Image
    {
        detection = selector.detect (img);
    }

    lumena::melody::MelodyOptions opts;
    const int lengths[3] = { 8, 16, 32 };
    opts.length = lengths[juce::jlimit (0, 2, choiceIndex (params::melodyLength))];
    opts.brightnessBias = floatParam (params::melodyBias);
    opts.arpeggioAmount = floatParam (params::melodyOrnaments);
    opts.phraseMode = choiceIndex (params::melodyPhrase) == 1
                          ? lumena::melody::PhraseMode::Freeform
                          : lumena::melody::PhraseMode::Phrased;
    // rhythm (Flowing), cellPath (RandomWalk) and octaveSpan (2) keep Lumena's
    // musical defaults.

    const lumena::melody::Melody mel =
        lumena::melody::generateMelody (grid, detection.scale, opts, gen);

    melody::Sequence seq;
    seq.gridCols = grid.columns();
    seq.gridRows = grid.rows();
    seq.keyName = juce::String (detection.keyName);
    seq.steps.reserve (mel.notes.size());
    double totalBeats = 0.0;
    for (std::size_t i = 0; i < mel.notes.size(); ++i)
    {
        const auto& n = mel.notes[i];
        melody::Step s;
        s.note        = juce::jlimit (0, 127, n.noteNumber);
        s.velocity    = clampVelocity127 (n.velocity) / 127.0f;
        s.startBeats  = n.startBeats;
        s.lengthBeats = n.lengthBeats;
        if (i < mel.cells.size())
        {
            s.col = mel.cells[i].col;
            s.row = mel.cells[i].row;
        }
        totalBeats = juce::jmax (totalBeats, n.startBeats + n.lengthBeats);
        seq.steps.push_back (s);
    }
    seq.totalBeats = totalBeats;

    installSequence (seq);
}

void MelodyController::installSequence (const melody::Sequence& seq)
{
    currentSeq = seq; // message-thread copy for the UI overlay + export

    // Persist: seed + the note sequence itself (see MelodyState for why the
    // notes are stored rather than regenerated).
    melodystate::setSeed (apvts.state, seedValue);
    melodystate::setLocked (apvts.state, lockedFlag);
    melodystate::storeSequence (apvts.state, currentSeq);

    // Hand a fresh copy to the audio-thread player.
    player.setSequence (std::make_unique<melody::Sequence> (currentSeq));
}

void MelodyController::reroll()
{
    if (lockedFlag)
        return; // locked: keep the current seed
    seedValue = makeSeed();
    generate();
}

void MelodyController::setLocked (bool shouldLock)
{
    lockedFlag = shouldLock;
    melodystate::setLocked (apvts.state, lockedFlag);
}

void MelodyController::play()
{
    if (player.hasSequence())
        player.play();
}

void MelodyController::stop()
{
    player.stop();
}

bool MelodyController::isPlaying() const
{
    return player.isPlaying();
}

std::vector<unsigned char> MelodyController::toMidiBytes() const
{
    if (currentSeq.steps.empty())
        return {};

    std::vector<lumena::midi::Note> notes;
    notes.reserve (currentSeq.steps.size());
    for (const auto& s : currentSeq.steps)
    {
        lumena::midi::Note n;
        n.noteNumber  = s.note;
        n.velocity    = clampVelocity127 (juce::roundToInt (s.velocity * 127.0f));
        n.startBeats  = s.startBeats;
        n.lengthBeats = s.lengthBeats;
        notes.push_back (n);
    }

    const double bpm = player.lastBpm();
    const lumena::midi::MidiSequence sequence (notes, bpm, 480);
    return lumena::midi::MidiFileWriter::toBytes (sequence);
}

juce::File MelodyController::writeTempMidiFile() const
{
    const auto bytes = toMidiBytes();
    if (bytes.empty())
        return {};

    const auto name = "LumenMelody_"
                      + juce::String::toHexString (static_cast<juce::int64> (seedValue))
                      + ".mid";
    const juce::File dest =
        juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    if (dest.replaceWithData (bytes.data(), bytes.size()))
        return dest;
    return {};
}

bool MelodyController::saveMidiFile (const juce::File& dest) const
{
    const auto bytes = toMidiBytes();
    if (bytes.empty())
        return false;
    return dest.replaceWithData (bytes.data(), bytes.size());
}

void MelodyController::applyState()
{
    seedValue  = melodystate::seed (apvts.state);
    lockedFlag = melodystate::locked (apvts.state);
    if (seedValue == 0)
        seedValue = makeSeed();

    melody::Sequence restored;
    if (melodystate::loadSequence (apvts.state, restored))
    {
        currentSeq = restored;
        player.setSequence (std::make_unique<melody::Sequence> (currentSeq));
    }
    else
    {
        currentSeq = melody::Sequence {};
    }
}
} // namespace lumen
