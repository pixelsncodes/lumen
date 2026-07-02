#include "Engine/SynthEngine.h"

#include "Engine/FactoryTables.h"

#include <cmath>

namespace lumen
{
namespace
{
    constexpr double kSmoothingSeconds = 0.02; // SPEC section 12
    constexpr uint64_t kRngSeed = 0x4C756D656E325232ULL; // deterministic (no rand()/time())

    float noiseDbToLinear (float db) noexcept
    {
        return db <= -59.95f ? 0.0f : std::pow (10.0f, db / 20.0f);
    }
} // namespace

void SynthEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    maxBlock = std::max (16, maxBlockSize);
    rngState = kRngSeed;
    noteCounter = 0;
    primed = false;

    // Force the deterministic factory-table build now (never on the audio thread).
    factory::get (TableChoice::basic);
    factory::get (TableChoice::pwm);
    factory::get (TableChoice::harmonicRise);
    factory::get (TableChoice::formant);

    for (int i = 0; i < kNumVoices; ++i)
        voices[i].prepare (sr, 0x9E3779B9u ^ (static_cast<uint32_t> (i) * 2654435761u));

    for (auto* buffer : { &bufMorphA, &bufLevelA, &bufMorphB, &bufLevelB,
                          &bufSubLevel, &bufNoiseLin, &bufCutoff, &bufRes,
                          &bufDrive, &bufEnvAmount })
        buffer->assign (static_cast<size_t> (maxBlock), 0.0f);

    for (auto* s : { &smoothA.morph, &smoothA.level, &smoothA.pan, &smoothA.fine,
                     &smoothA.detune, &smoothA.width, &smoothA.blend,
                     &smoothB.morph, &smoothB.level, &smoothB.pan, &smoothB.fine,
                     &smoothB.detune, &smoothB.width, &smoothB.blend,
                     &subLevel, &noiseLin, &cutoff, &res, &drive, &envAmount, &keytrack })
        s->reset (sr, kSmoothingSeconds);

    // Measure tanh drive gain compensation at a -12 dBFS sine (DECISIONS.md):
    // comp[d] restores the RMS a 0.25-amplitude sine loses/gains through
    // tanh(10^(d/20) * x).
    constexpr int kProbe = 256;
    double rmsIn = 0.0;
    for (int i = 0; i < kProbe; ++i)
    {
        const double v = 0.25 * std::sin (2.0 * 3.14159265358979323846 * i / kProbe);
        rmsIn += v * v;
    }
    rmsIn = std::sqrt (rmsIn / kProbe);

    for (int d = 0; d <= 24; ++d)
    {
        const double g = std::pow (10.0, d / 20.0);
        double rmsOut = 0.0;
        for (int i = 0; i < kProbe; ++i)
        {
            const double v = std::tanh (g * 0.25 * std::sin (2.0 * 3.14159265358979323846 * i / kProbe));
            rmsOut += v * v;
        }
        rmsOut = std::sqrt (rmsOut / kProbe);
        driveComp[d] = static_cast<float> (rmsIn / std::max (1.0e-9, rmsOut));
    }

    reset();
}

void SynthEngine::setTarget (Smoothed& s, float value, bool snap)
{
    if (snap)
        s.setCurrentAndTargetValue (value);
    else
        s.setTargetValue (value);
}

void SynthEngine::pushTargets (bool snap)
{
    const auto& p = current;
    setTarget (smoothA.morph,  p.oscA.morph, snap);
    setTarget (smoothA.level,  p.oscA.level, snap);
    setTarget (smoothA.pan,    p.oscA.pan, snap);
    setTarget (smoothA.fine,   p.oscA.fineCents, snap);
    setTarget (smoothA.detune, p.oscA.detuneCents, snap);
    setTarget (smoothA.width,  p.oscA.width, snap);
    setTarget (smoothA.blend,  p.oscA.blend, snap);
    setTarget (smoothB.morph,  p.oscB.morph, snap);
    setTarget (smoothB.level,  p.oscB.level, snap);
    setTarget (smoothB.pan,    p.oscB.pan, snap);
    setTarget (smoothB.fine,   p.oscB.fineCents, snap);
    setTarget (smoothB.detune, p.oscB.detuneCents, snap);
    setTarget (smoothB.width,  p.oscB.width, snap);
    setTarget (smoothB.blend,  p.oscB.blend, snap);
    setTarget (subLevel,  p.subLevel, snap);
    setTarget (noiseLin,  noiseDbToLinear (p.noiseDb), snap);
    setTarget (cutoff,    p.filterCutoffHz, snap);
    setTarget (res,       p.filterRes, snap);
    setTarget (drive,     p.filterDriveDb, snap);
    setTarget (envAmount, p.filterEnvAmount, snap);
    setTarget (keytrack,  p.filterKeytrack, snap);
}

void SynthEngine::setParams (const EngineParams& params)
{
    current = params;
    pushTargets (! primed);
    primed = true;
}

Voice* SynthEngine::findVoiceFor (int)
{
    for (auto& v : voices)
        if (! v.isActive())
            return &v;

    // Steal the quietest releasing voice, otherwise the oldest (SPEC 11).
    Voice* quietest = nullptr;
    for (auto& v : voices)
        if (v.isReleasing() && (quietest == nullptr || v.envLevel() < quietest->envLevel()))
            quietest = &v;
    if (quietest != nullptr)
        return quietest;

    Voice* oldest = &voices[0];
    for (auto& v : voices)
        if (v.age() < oldest->age())
            oldest = &v;
    return oldest;
}

void SynthEngine::noteOn (int midiNote, float velocity)
{
    auto* voice = findVoiceFor (midiNote);
    voice->setAge (++noteCounter);
    voice->startNote (midiNote, velocity, rngState,
                      current.oscA.phaseRandom, current.oscB.phaseRandom);
}

void SynthEngine::noteOff (int midiNote)
{
    for (auto& v : voices)
        if (v.isActive() && v.currentNote() == midiNote && ! v.isReleasing())
            v.noteOff();
}

void SynthEngine::reset()
{
    for (auto& v : voices)
        v.kill();
}

void SynthEngine::render (float* outL, float* outR, int numSamples)
{
    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = std::min (numSamples - offset, maxBlock);
        renderChunk (outL + offset, outR + offset, chunk);
        offset += chunk;
    }
}

void SynthEngine::renderChunk (float* outL, float* outR, int numSamples)
{
    // Per-sample smoothed buffers shared by all voices.
    struct Fill { Smoothed* s; std::vector<float>* buffer; };
    const Fill fills[] = {
        { &smoothA.morph, &bufMorphA }, { &smoothA.level, &bufLevelA },
        { &smoothB.morph, &bufMorphB }, { &smoothB.level, &bufLevelB },
        { &subLevel, &bufSubLevel },    { &noiseLin, &bufNoiseLin },
        { &cutoff, &bufCutoff },        { &res, &bufRes },
        { &drive, &bufDrive },          { &envAmount, &bufEnvAmount },
    };
    for (const auto& f : fills)
    {
        float* d = f.buffer->data();
        if (f.s->isSmoothing())
            for (int i = 0; i < numSamples; ++i)
                d[i] = f.s->getNextValue();
        else
            std::fill_n (d, numSamples, f.s->getCurrentValue());
    }

    // Block-rate start/end pairs for pitch/pan ramps inside the voices.
    VoiceBlockGlobals globals;
    auto fillOsc = [numSamples] (OscBlockGlobals& g, const OscParams& p, OscSmoothers& s)
    {
        g.table = &factory::forIndex (p.table);
        g.enabled = p.enabled;
        g.unison = p.unison;
        g.semitones = p.semitones;
        g.fineStart = s.fine.getCurrentValue();     g.fineEnd = s.fine.skip (numSamples);
        g.detuneStart = s.detune.getCurrentValue(); g.detuneEnd = s.detune.skip (numSamples);
        g.widthStart = s.width.getCurrentValue();   g.widthEnd = s.width.skip (numSamples);
        g.blendStart = s.blend.getCurrentValue();   g.blendEnd = s.blend.skip (numSamples);
        g.panStart = s.pan.getCurrentValue();       g.panEnd = s.pan.skip (numSamples);
    };
    fillOsc (globals.oscA, current.oscA, smoothA);
    fillOsc (globals.oscB, current.oscB, smoothB);

    globals.subWave = current.subWave;
    globals.subOctave = current.subOctave;
    globals.noiseType = current.noiseType;
    globals.filterMode = current.filterMode;
    globals.keytrack = keytrack.skip (numSamples);
    globals.env1 = current.env1;
    globals.env2 = current.env2;
    globals.env3 = current.env3;

    BlockBuffers buffers {
        bufMorphA.data(), bufLevelA.data(), bufMorphB.data(), bufLevelB.data(),
        bufSubLevel.data(), bufNoiseLin.data(), bufCutoff.data(), bufRes.data(),
        bufDrive.data(), bufEnvAmount.data(), driveComp
    };

    for (auto& v : voices)
    {
        if (! v.isActive())
            continue;
        v.startBlock (globals, numSamples);
        v.render (outL, outR, numSamples, buffers);
    }
}
} // namespace lumen
