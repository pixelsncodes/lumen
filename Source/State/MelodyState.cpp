#include "State/MelodyState.h"

namespace lumen::melodystate
{
namespace
{
const juce::Identifier kMelody   { "MELODY" };
const juce::Identifier kSeq      { "SEQ" };
const juce::Identifier kSeed     { "seed" };
const juce::Identifier kLocked   { "locked" };
const juce::Identifier kCols     { "cols" };
const juce::Identifier kRows     { "rows" };
const juce::Identifier kBeats    { "beats" };
const juce::Identifier kKey      { "key" };
const juce::Identifier kSteps    { "steps" };
const juce::Identifier kMood     { "mood" };
const juce::Identifier kForm     { "form" };
} // namespace

juce::ValueTree ensureTree (juce::ValueTree& state)
{
    auto tree = state.getChildWithName (kMelody);
    if (! tree.isValid())
    {
        tree = juce::ValueTree (kMelody);
        state.appendChild (tree, nullptr);
    }
    return tree;
}

juce::ValueTree getTree (const juce::ValueTree& state)
{
    return state.getChildWithName (kMelody);
}

juce::uint64 seed (const juce::ValueTree& state)
{
    const auto tree = getTree (state);
    if (! tree.isValid())
        return 0;
    return static_cast<juce::uint64> (
        tree.getProperty (kSeed).toString().getHexValue64());
}

bool locked (const juce::ValueTree& state)
{
    const auto tree = getTree (state);
    return tree.isValid() && static_cast<bool> (tree.getProperty (kLocked, false));
}

void setSeed (juce::ValueTree& state, juce::uint64 seedValue)
{
    auto tree = ensureTree (state);
    tree.setProperty (kSeed, juce::String::toHexString (
                                 static_cast<juce::int64> (seedValue)), nullptr);
}

void setLocked (juce::ValueTree& state, bool lockedValue)
{
    auto tree = ensureTree (state);
    tree.setProperty (kLocked, lockedValue, nullptr);
}

void setSummary (juce::ValueTree& state, const juce::String& mood,
                 const juce::String& form)
{
    auto tree = ensureTree (state);
    tree.setProperty (kMood, mood, nullptr);
    tree.setProperty (kForm, form, nullptr);
}

juce::String summaryMood (const juce::ValueTree& state)
{
    const auto tree = getTree (state);
    return tree.isValid() ? tree.getProperty (kMood).toString() : juce::String();
}

juce::String summaryForm (const juce::ValueTree& state)
{
    const auto tree = getTree (state);
    return tree.isValid() ? tree.getProperty (kForm).toString() : juce::String();
}

void storeSequence (juce::ValueTree& state, const melody::Sequence& seq)
{
    auto tree = ensureTree (state);
    tree.removeChild (tree.getChildWithName (kSeq), nullptr);

    juce::ValueTree node (kSeq);
    node.setProperty (kCols, seq.gridCols, nullptr);
    node.setProperty (kRows, seq.gridRows, nullptr);
    node.setProperty (kBeats, seq.totalBeats, nullptr);
    node.setProperty (kKey, seq.keyName, nullptr);

    // Compact CSV: one "note,vel,start,len,col,row" record per step, ';'
    // separated. At <=32 notes this stays tiny and human-inspectable.
    //
    // velocity/startBeats/lengthBeats must round-trip exactly (a restored
    // melody has to export byte-identical MIDI to the one that was saved —
    // MelodyState.h's whole point). juce::String's operator<< for a bare
    // double/float goes through String(number) with numberOfDecimalPlaces=0,
    // which skips setting fixed-point precision and falls back to the
    // default C++ iostream precision of 6 *significant digits* — enough to
    // silently truncate a generated beat position like 12.333333333333334 to
    // "12.3333" on every save. Passing an explicit decimal-place count forces
    // fixed-point formatting with real precision: 9 places safely round-trips
    // a float (~7 significant digits) in the 0..1 velocity range, and 15
    // safely round-trips a double (~15-17 significant digits) at the beat-
    // position magnitudes this sequence ever reaches (a handful of bars).
    juce::String csv;
    csv.preallocateBytes (seq.steps.size() * 40);
    for (const auto& s : seq.steps)
    {
        csv << s.note << ',' << juce::String (s.velocity, 9) << ','
            << juce::String (s.startBeats, 15) << ','
            << juce::String (s.lengthBeats, 15) << ','
            << s.col << ',' << s.row << ';';
    }
    node.setProperty (kSteps, csv, nullptr);
    tree.appendChild (node, nullptr);
}

bool hasSequence (const juce::ValueTree& state)
{
    const auto tree = getTree (state);
    return tree.isValid() && tree.getChildWithName (kSeq).isValid();
}

bool loadSequence (const juce::ValueTree& state, melody::Sequence& out)
{
    const auto tree = getTree (state);
    if (! tree.isValid())
        return false;
    const auto node = tree.getChildWithName (kSeq);
    if (! node.isValid())
        return false;

    out = melody::Sequence {};
    out.gridCols   = static_cast<int> (node.getProperty (kCols, 8));
    out.gridRows   = static_cast<int> (node.getProperty (kRows, 8));
    out.totalBeats = static_cast<double> (node.getProperty (kBeats, 0.0));
    out.keyName    = node.getProperty (kKey).toString();

    const juce::String csv = node.getProperty (kSteps).toString();
    juce::StringArray records;
    records.addTokens (csv, ";", "");
    for (const auto& record : records)
    {
        if (record.trim().isEmpty())
            continue;
        juce::StringArray f;
        f.addTokens (record, ",", "");
        if (f.size() < 6)
            continue;
        melody::Step s;
        s.note        = f[0].getIntValue();
        s.velocity    = f[1].getFloatValue();
        s.startBeats  = f[2].getDoubleValue();
        s.lengthBeats = f[3].getDoubleValue();
        s.col         = f[4].getIntValue();
        s.row         = f[5].getIntValue();
        out.steps.push_back (s);
    }
    return true;
}
} // namespace lumen::melodystate
