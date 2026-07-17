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

#include <cmath>
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

    // --- parameter helpers (read the APVTS without the controller instance) ---
    int choiceOf (const juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* v = apvts.getRawParameterValue (id))
            return static_cast<int> (v->load());
        return 0;
    }
    float floatOf (const juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* v = apvts.getRawParameterValue (id))
            return v->load();
        return 0.0f;
    }
    bool boolOf (const juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        return floatOf (apvts, id) > 0.5f;
    }

    // Maps the panel's parameters onto Lumena's MelodyOptions. Image Influence
    // drives brightnessBias; Complexity drives the ornament amount; Energy and
    // Repetition drive the new macro axes.
    lumena::melody::MelodyOptions optionsFromParams (const juce::AudioProcessorValueTreeState& apvts)
    {
        using namespace lumena::melody;
        MelodyOptions o;

        const int lengths[3] = { 8, 16, 32 };
        o.length = lengths[juce::jlimit (0, 2, choiceOf (apvts, params::melodyLength))];

        switch (choiceOf (apvts, params::melodyMode))
        {
            case 1:  o.mode = GenerationMode::Chords;    break;
            case 2:  o.mode = GenerationMode::Arpeggio;  break;
            default: o.mode = GenerationMode::Melody;    break;
        }

        o.phraseMode = choiceOf (apvts, params::melodyPhrase) == 1
                           ? PhraseMode::Freeform : PhraseMode::Phrased;

        switch (choiceOf (apvts, params::melodyArpPattern))
        {
            case 0:  o.arpPattern = ArpPattern::Up;       break;
            case 1:  o.arpPattern = ArpPattern::Down;     break;
            case 3:  o.arpPattern = ArpPattern::Converge; break;
            case 4:  o.arpPattern = ArpPattern::Random;   break;
            default: o.arpPattern = ArpPattern::UpDown;   break;
        }
        o.arpRate = 0.5;

        const int loopBars[5] = { 0, 1, 2, 4, 8 };
        o.loopBars = loopBars[juce::jlimit (0, 4, choiceOf (apvts, params::melodyLoopLength))];
        o.beatsPerBar = 4.0;

        o.energy            = floatOf (apvts, params::melodyEnergy);
        o.arpeggioAmount    = floatOf (apvts, params::melodyComplexity);      // Complexity
        o.brightnessBias    = floatOf (apvts, params::melodyImageInfluence);  // Image Influence
        o.repetition        = floatOf (apvts, params::melodyRepetition);
        o.imageRhythmAmount = floatOf (apvts, params::melodyDensity);         // Density (Phase 3)

        o.chordSize = 3;   // triads
        o.chordRate = 2.0; // half-note chords
        return o;
    }

    lumena::melody::RegenLocks locksFromParams (const juce::AudioProcessorValueTreeState& apvts)
    {
        lumena::melody::RegenLocks locks;
        locks.rhythm  = boolOf (apvts, params::melodyLockRhythm);
        locks.pitch   = boolOf (apvts, params::melodyLockPitch);
        locks.harmony = boolOf (apvts, params::melodyLockHarmony);
        return locks;
    }

    // Rebuild a Lumena melody from a stored plugin sequence (their fields are in
    // 1:1 correspondence; degree isn't retained but recombine/mutate don't need
    // it for pitch preservation).
    lumena::melody::Melody sequenceToMelody (const melody::Sequence& seq)
    {
        lumena::melody::Melody m;
        m.notes.reserve (seq.steps.size());
        for (const melody::Step& s : seq.steps)
        {
            lumena::midi::Note n;
            n.noteNumber  = s.note;
            n.velocity    = clampVelocity127 (static_cast<int> (std::lround (s.velocity * 127.0f)));
            n.startBeats  = s.startBeats;
            n.lengthBeats = s.lengthBeats;
            m.notes.push_back (n);
            m.degrees.push_back (0);
            m.cells.push_back (lumena::melody::GridCell { s.col, s.row });
        }
        return m;
    }

    // Convert a Lumena melody into a plugin sequence for playback/export/overlay.
    melody::Sequence melodyToSequence (const lumena::melody::Melody& mel,
                                       int cols, int rows, const juce::String& keyName)
    {
        melody::Sequence seq;
        seq.gridCols = cols;
        seq.gridRows = rows;
        seq.keyName  = keyName;
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
        return seq;
    }
} // namespace

MelodyController::MelodyController (juce::AudioProcessorValueTreeState& apvtsToUse,
                                    LensController& lensToUse, MelodyPlayer& playerToUse)
    : apvts (apvtsToUse), lens (lensToUse), player (playerToUse)
{
    seedValue = makeSeed(); // a starting seed; nothing is generated until asked
}

juce::uint64 MelodyController::makeSeed()
{
    // One 32-bit draw, widened. renderFresh() below seeds a std::mt19937 from
    // a single unsigned value (its constructor's actual entropy ceiling is 32
    // bits no matter how wide the type we hand it is), so a second draw here
    // would only ever be discarded — see MelodyController.h's makeSeed() note.
    return static_cast<juce::uint64> (static_cast<juce::uint32> (rng.nextInt()));
}

namespace
{
    // Generation-summary text (Phase 5). Surfaces the key detector's own
    // classification — the scale family it CHOSE plus the hue/sat/lum inputs
    // that chose it (KeySelector::chooseScaleType's documented axes) — without
    // re-running any of it. randomKey marks a Random-mode draw, where the
    // "image" was a synthetic solid colour.
    juce::String moodTextFor (const lumena::scales::KeyDetection& d, bool randomKey)
    {
        using lumena::scales::ScaleType;
        const char* bucket = "";
        switch (d.type)
        {
            case ScaleType::MajorPentatonic:
            case ScaleType::MinorPentatonic: bucket = "washed-out";   break;
            case ScaleType::BluesMinor:
            case ScaleType::HarmonicMinor:   bucket = "vivid & dark"; break;
            case ScaleType::Phrygian:        bucket = "darkest";      break;
            case ScaleType::Aeolian:         bucket = "dark";         break;
            case ScaleType::Dorian:          bucket = "dusky";        break;
            case ScaleType::Mixolydian:      bucket = "warm";         break;
            case ScaleType::Ionian:          bucket = "bright";       break;
            case ScaleType::Lydian:          bucket = "brightest";    break;
        }
        juce::String s (bucket);
        s << (d.major ? " (major)" : " (minor)")
          << " \xe2\x80\x94 hue " << juce::roundToInt (d.hue) << "\xc2\xb0"
          << ", sat " << juce::String (d.saturation, 2)
          << ", lum " << juce::String (d.value, 2);
        if (randomKey)
            s << " (random draw)";
        return s;
    }

    // Phrase-form letters by generatePhrased's construction convention:
    // phrase 0 = the motif (A), the last = the closing cadence phrase (C),
    // interior odd = A-family variations (A', A'', ...), interior even = the
    // contrasting B phrases. Non-phrased shapes name themselves.
    juce::String formTextFor (int modeChoice, int phraseChoice,
                              const std::vector<std::size_t>& phraseStarts)
    {
        if (modeChoice == 1) return "chord progression";
        if (modeChoice == 2) return "arpeggio";
        if (phraseChoice == 1) return "freeform walk";

        const std::size_t n = phraseStarts.size();
        if (n < 2)
            return "phrased";
        juce::String s;
        int variation = 0;
        for (std::size_t p = 0; p < n; ++p)
        {
            if (p > 0) s << " ";
            if (p == 0)              s << "A";
            else if (p + 1 == n)     s << "C";
            else if (p % 2 == 1)     s << "A" << juce::String::repeatedString ("\xe2\x80\xb2", ++variation);
            else                     s << "B";
        }
        return s;
    }

    // Runs the full image -> key -> Lumena melody pipeline at a given seed,
    // returning the Lumena melody plus the full key detection (scale, key
    // name, and the provenance the summary readout shows) and grid size.
    // Shared by generate() and regenerate(). Returns false if no image.
    bool renderFresh (const juce::AudioProcessorValueTreeState& apvts,
                      const juce::Image& jimg, juce::uint64 seed,
                      lumena::melody::Melody& outMelody,
                      lumena::scales::KeyDetection& outDetection,
                      int& outCols, int& outRows,
                      const std::vector<int>& lockedProgression = {})
    {
        if (! jimg.isValid())
            return false;
        const lumena::image::Image img = toLumenaImage (jimg);
        if (img.empty())
            return false;

        const int gridN = MelodyController::kGridResolution;
        const lumena::image::BrightnessGrid grid (img, gridN, gridN);

        // seed is always <= 0xFFFFFFFF by construction (makeSeed()/setSeed()),
        // so this truncation is lossless; std::mt19937's own seed ceiling is
        // 32 bits regardless.
        std::mt19937 gen (static_cast<std::uint32_t> (seed));

        const lumena::scales::KeySelector selector;
        lumena::scales::KeyDetection detection;
        if (choiceOf (apvts, params::melodyKeyMode) == 1) // Random
        {
            std::uniform_real_distribution<double> hueDist (0.0, 1.0);
            std::uniform_real_distribution<double> satDist (0.0, 1.0);
            const float hue01 = static_cast<float> (hueDist (gen));
            const float sat = 0.2f + 0.7f * static_cast<float> (satDist (gen));
            detection = selector.detect (solidColourImage (hue01, sat));
        }
        else
        {
            detection = selector.detect (img);
        }

        lumena::melody::MelodyOptions opts = optionsFromParams (apvts);
        // Lock Harmony: voice over the carried progression instead of drawing a
        // fresh one (empty = normal draw, byte-identical to before 4b).
        if (! lockedProgression.empty())
            opts.progression = lockedProgression;
        outMelody    = lumena::melody::generateMelody (grid, detection.scale, opts, gen);
        outDetection = detection;
        outCols      = grid.columns();
        outRows      = grid.rows();
        return true;
    }
} // namespace

void MelodyController::generate()
{
    const juce::Image jimg = lens.displayImage (lens.target());
    lumena::melody::Melody mel;
    lumena::scales::KeyDetection detection;
    int cols = 0, rows = 0;
    if (! renderFresh (apvts, jimg, seedValue, mel, detection, cols, rows))
        return; // no image loaded: nothing to sample

    currentProgression = mel.progression;  // remember harmony for Lock Harmony
    currentPhraseStarts = mel.phraseStarts; // remember phrases for phrase-aware splice
    moodValue = moodTextFor (detection, choiceOf (apvts, params::melodyKeyMode) == 1);
    formValue = formTextFor (choiceOf (apvts, params::melodyMode),
                             choiceOf (apvts, params::melodyPhrase), mel.phraseStarts);
    installSequence (melodyToSequence (mel, cols, rows,
                                       juce::String (detection.keyName)));
}

void MelodyController::installSequence (const melody::Sequence& seq)
{
    currentSeq = seq; // message-thread copy for the UI overlay + export

    // Persist: seed + the note sequence itself (see MelodyState for why the
    // notes are stored rather than regenerated) + the generation summary.
    melodystate::setSeed (apvts.state, seedValue);
    melodystate::setLocked (apvts.state, lockedFlag);
    melodystate::setSummary (apvts.state, moodValue, formValue);
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

void MelodyController::regenerate()
{
    const lumena::melody::RegenLocks locks = locksFromParams (apvts);
    // Master lock (locked()/setLocked()): reuse the pinned seed instead of
    // drawing a fresh one. The rhythm/pitch/harmony dimension locks still
    // govern the splice below either way.
    const juce::uint64 newSeed = lockedFlag ? seedValue : makeSeed();

    const juce::Image jimg = lens.displayImage (lens.target());
    lumena::melody::Melody cand;
    lumena::scales::KeyDetection detection;
    int cols = 0, rows = 0;
    // Lock Harmony: carry the current progression into the fresh candidate so its
    // pitch/rhythm re-roll under a fixed harmony (empty otherwise = fresh draw).
    const std::vector<int> carryProgression =
        locks.harmony ? currentProgression : std::vector<int> {};
    if (! renderFresh (apvts, jimg, newSeed, cand, detection, cols, rows,
                       carryProgression))
        return;
    const lumena::scales::Scale& scale = detection.scale;

    currentProgression = cand.progression;  // keep the latest for future carries
    lumena::melody::Melody out = cand;
    // If a dimension is locked, carry it over from the previous melody.
    if ((locks.rhythm || locks.pitch) && ! currentSeq.steps.empty())
    {
        lumena::melody::Melody prev = sequenceToMelody (currentSeq);
        // The stored Sequence carries no phrase boundaries, so restore the ones
        // remembered from when this melody was generated (note indices survive the
        // Sequence round-trip: same count, same order) — this is what lets the
        // splice be phrase-aware (4b-fix). Empty ones fall back to index-based.
        prev.phraseStarts = currentPhraseStarts;
        out = lumena::melody::recombineLocked (prev, cand, scale, locks,
                                               optionsFromParams (apvts));
    }

    seedValue = newSeed;
    currentPhraseStarts = out.phraseStarts;  // the new melody's phrases
    moodValue = moodTextFor (detection, choiceOf (apvts, params::melodyKeyMode) == 1);
    formValue = formTextFor (choiceOf (apvts, params::melodyMode),
                             choiceOf (apvts, params::melodyPhrase), out.phraseStarts);
    installSequence (melodyToSequence (out, cols, rows,
                                       juce::String (detection.keyName)));
}

void MelodyController::mutate()
{
    if (currentSeq.steps.empty())
    {
        generate(); // nothing to mutate yet
        return;
    }

    // Re-derive the current melody's scale by re-detecting at its seed (the
    // render is discarded; only the scale is needed for diatonic nudging).
    const juce::Image jimg = lens.displayImage (lens.target());
    lumena::melody::Melody discard;
    lumena::scales::KeyDetection detection;
    int cols = 0, rows = 0;
    if (! renderFresh (apvts, jimg, seedValue, discard, detection, cols, rows))
        return;
    const lumena::scales::Scale& scale = detection.scale;

    const lumena::melody::Melody base = sequenceToMelody (currentSeq);
    const lumena::melody::RegenLocks locks = locksFromParams (apvts);
    const lumena::melody::MelodyOptions opts = optionsFromParams (apvts);

    // Mutation is deliberately off-seed (a fresh variation), so the persisted
    // note sequence — not the seed — is what preserves it across reloads.
    std::mt19937 mrng (static_cast<std::uint32_t> (makeSeed()));
    const lumena::melody::Melody out =
        lumena::melody::mutate (base, scale, locks, 0.25, opts, mrng);

    installSequence (melodyToSequence (out, currentSeq.gridCols,
                                       currentSeq.gridRows, currentSeq.keyName));
}

void MelodyController::setLocked (bool shouldLock)
{
    lockedFlag = shouldLock;
    melodystate::setLocked (apvts.state, lockedFlag);
}

void MelodyController::setSeed (juce::uint32 newSeed)
{
    seedValue = newSeed;
    generate(); // regenerate() would draw a fresh seed when unlocked; this
                // pins the exact value the caller asked for, honouring both
                // lock states the same way (generate() never re-seeds).
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

bool MelodyController::hasImageSource() const
{
    return lens.displayImage (lens.target()).isValid();
}

std::vector<unsigned char> MelodyController::toMidiBytes() const
{
    if (currentSeq.steps.empty())
        return {};

    // Export what you hear: apply the live Transpose + Octave params the same
    // way the player does (per note, clamped). The stored sequence stays
    // untouched.
    const int transpose = juce::roundToInt (floatOf (apvts, params::melodyTranspose))
                          + 12 * juce::roundToInt (floatOf (apvts, params::melodyOctave));

    std::vector<lumena::midi::Note> notes;
    notes.reserve (currentSeq.steps.size());
    for (const auto& s : currentSeq.steps)
    {
        lumena::midi::Note n;
        n.noteNumber  = juce::jlimit (0, 127, s.note + transpose);
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
    moodValue  = melodystate::summaryMood (apvts.state);
    formValue  = melodystate::summaryForm (apvts.state);
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
