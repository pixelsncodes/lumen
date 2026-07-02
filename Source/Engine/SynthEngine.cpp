#include "Engine/SynthEngine.h"

#include "Engine/FactoryTables.h"

#include <cmath>

namespace lumen
{
namespace
{
    constexpr double kSmoothingSeconds = 0.02; // SPEC section 12
    constexpr uint64_t kRngSeed = 0x4C756D656E325232ULL; // deterministic (no rand()/time())
    constexpr float kBendRangeSemis = 2.0f; // fixed until the settings panel (DECISIONS.md)

    float noiseDbToLinear (float db) noexcept
    {
        return db <= -59.95f ? 0.0f : std::pow (10.0f, db / 20.0f);
    }

    float noiseLinearToDb (float lin) noexcept
    {
        return lin <= 1.0e-3f ? -60.0f : 20.0f * std::log10 (lin);
    }

    void setTarget (juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& s,
                    float value, bool snap)
    {
        if (snap)
            s.setCurrentAndTargetValue (value);
        else
            s.setTargetValue (value);
    }
} // namespace

void SynthEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    maxBlock = std::max (16, maxBlockSize);
    rngState = kRngSeed;
    noteCounter = 0;
    primed = false;
    modWheel = aftertouch = pitchBend = 0.0f;

    // Force the deterministic factory-table build now (never on the audio thread).
    factory::get (TableChoice::basic);
    factory::get (TableChoice::pwm);
    factory::get (TableChoice::harmonicRise);
    factory::get (TableChoice::formant);

    for (int i = 0; i < kNumVoices; ++i)
    {
        voices[i].prepare (sr, 0x9E3779B9u ^ (static_cast<uint32_t> (i) * 2654435761u));
        voiceModPrimed[i] = false;
        for (auto& p : voicePolyPrev[i])
            p = 0.0f;
    }

    for (int k = 0; k < 3; ++k)
        monoLfo[k].prepare (sr, 0xC001D00Du ^ (static_cast<uint32_t> (k + 1) * 0x9E3779B9u));

    for (auto* buffer : { &bufMorphA, &bufLevelA, &bufMorphB, &bufLevelB,
                          &bufSubLevel, &bufNoiseLin, &bufCutoff, &bufRes,
                          &bufDrive, &bufEnvAmount })
        buffer->assign (static_cast<size_t> (maxBlock), 0.0f);
    for (auto& s : scratch)
        s.assign (static_cast<size_t> (maxBlock), 0.0f);

    for (auto* s : { &smoothA.morph, &smoothA.level, &smoothA.pan, &smoothA.fine,
                     &smoothA.detune, &smoothA.width, &smoothA.blend,
                     &smoothB.morph, &smoothB.level, &smoothB.pan, &smoothB.fine,
                     &smoothB.detune, &smoothB.width, &smoothB.blend,
                     &subLevel, &noiseLin, &cutoff, &res, &drive, &envAmount,
                     &keytrack, &bendSemis })
        s->reset (sr, kSmoothingSeconds);

    for (int d = 0; d < mod::kNumDests; ++d)
        destExponent[d] = mod::skewExponent (static_cast<mod::Dest> (d));

    // Measured tanh loudness compensation (DECISIONS.md), shared with the FX drive.
    FxChain::measureTanhCompensation (driveComp, 25);

    fx.prepare (sr, maxBlock);

    reset();
}

void SynthEngine::setParams (const EngineParams& params)
{
    current = params;
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
    voiceModPrimed[voice - voices] = false;
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
    fx.reset();
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

// ---------------------------------------------------------------------------
// Modulation plumbing
// ---------------------------------------------------------------------------

float SynthEngine::baseNaturalFor (int dest) const noexcept
{
    using D = mod::Dest;
    switch (static_cast<D> (dest))
    {
        case D::filterCutoff:    return current.filterCutoffHz;
        case D::filterRes:       return current.filterRes;
        case D::filterDrive:     return current.filterDriveDb;
        case D::filterKeytrack:  return current.filterKeytrack;
        case D::filterEnvAmount: return current.filterEnvAmount;
        case D::oscAMorph:   return current.oscA.morph;
        case D::oscALevel:   return current.oscA.level;
        case D::oscAPan:     return current.oscA.pan;
        case D::oscAFine:    return current.oscA.fineCents;
        case D::oscADetune:  return current.oscA.detuneCents;
        case D::oscAWidth:   return current.oscA.width;
        case D::oscABlend:   return current.oscA.blend;
        case D::oscBMorph:   return current.oscB.morph;
        case D::oscBLevel:   return current.oscB.level;
        case D::oscBPan:     return current.oscB.pan;
        case D::oscBFine:    return current.oscB.fineCents;
        case D::oscBDetune:  return current.oscB.detuneCents;
        case D::oscBWidth:   return current.oscB.width;
        case D::oscBBlend:   return current.oscB.blend;
        case D::subLevel:    return current.subLevel;
        case D::noiseLevel:  return current.noiseDb;
        case D::env1Attack:  return current.env1.attackSeconds;
        case D::env1Decay:   return current.env1.decaySeconds;
        case D::env1Sustain: return current.env1.sustain;
        case D::env1Release: return current.env1.releaseSeconds;
        case D::env1Curve:   return current.env1.curve;
        case D::env2Attack:  return current.env2.attackSeconds;
        case D::env2Decay:   return current.env2.decaySeconds;
        case D::env2Sustain: return current.env2.sustain;
        case D::env2Release: return current.env2.releaseSeconds;
        case D::env2Curve:   return current.env2.curve;
        case D::env3Attack:  return current.env3.attackSeconds;
        case D::env3Decay:   return current.env3.decaySeconds;
        case D::env3Sustain: return current.env3.sustain;
        case D::env3Release: return current.env3.releaseSeconds;
        case D::env3Curve:   return current.env3.curve;
        case D::delayMix:      return current.fx.delayMix;
        case D::reverbMix:     return current.fx.reverbMix;
        case D::masterGain:    return current.fx.masterGainDb;
        case D::driveAmount:   return current.fx.driveDb;
        case D::driveTone:     return current.fx.driveTone;
        case D::chorusRate:    return current.fx.chorusRateHz;
        case D::chorusDepth:   return current.fx.chorusDepth;
        case D::chorusMix:     return current.fx.chorusMix;
        case D::delayTime:     return current.fx.delayTimeMs;
        case D::delayFeedback: return current.fx.delayFeedback;
        case D::delayDamp:     return current.fx.delayDampHz;
        case D::reverbSize:    return current.fx.reverbSize;
        case D::reverbDamp:    return current.fx.reverbDamp;
        case D::reverbWidth:   return current.fx.reverbWidth;
        case D::count: break;
    }
    return 0.0f;
}

SynthEngine::Smoothed* SynthEngine::smootherFor (int dest) noexcept
{
    using D = mod::Dest;
    switch (static_cast<D> (dest))
    {
        case D::filterCutoff:    return &cutoff;
        case D::filterRes:       return &res;
        case D::filterDrive:     return &drive;
        case D::filterKeytrack:  return &keytrack;
        case D::filterEnvAmount: return &envAmount;
        case D::oscAMorph:  return &smoothA.morph;
        case D::oscALevel:  return &smoothA.level;
        case D::oscAPan:    return &smoothA.pan;
        case D::oscAFine:   return &smoothA.fine;
        case D::oscADetune: return &smoothA.detune;
        case D::oscAWidth:  return &smoothA.width;
        case D::oscABlend:  return &smoothA.blend;
        case D::oscBMorph:  return &smoothB.morph;
        case D::oscBLevel:  return &smoothB.level;
        case D::oscBPan:    return &smoothB.pan;
        case D::oscBFine:   return &smoothB.fine;
        case D::oscBDetune: return &smoothB.detune;
        case D::oscBWidth:  return &smoothB.width;
        case D::oscBBlend:  return &smoothB.blend;
        case D::subLevel:   return &subLevel;
        case D::noiseLevel: return &noiseLin; // holds LINEAR gain; converted at the call site
        case D::delayMix:      return &fx.smooth.delayMix;
        case D::reverbMix:     return &fx.smooth.reverbMix;
        case D::masterGain:    return &fx.smooth.masterGainDb;
        case D::driveAmount:   return &fx.smooth.driveDb;
        case D::driveTone:     return &fx.smooth.driveTone;
        case D::chorusRate:    return &fx.smooth.chorusRateHz;
        case D::chorusDepth:   return &fx.smooth.chorusDepth;
        case D::chorusMix:     return &fx.smooth.chorusMix;
        case D::delayTime:     return &fx.smooth.delayTimeMs;
        case D::delayFeedback: return &fx.smooth.delayFeedback;
        case D::delayDamp:     return &fx.smooth.delayDampHz;
        case D::reverbSize:    return &fx.smooth.reverbSize;
        case D::reverbDamp:    return &fx.smooth.reverbDamp;
        case D::reverbWidth:   return &fx.smooth.reverbWidth;
        default: return nullptr;              // env params have no smoother (block rate)
    }
}

int SynthEngine::bufferedIndexFor (int dest) noexcept
{
    using D = mod::Dest;
    switch (static_cast<D> (dest))
    {
        case D::oscAMorph:       return 0;
        case D::oscALevel:       return 1;
        case D::oscBMorph:       return 2;
        case D::oscBLevel:       return 3;
        case D::subLevel:        return 4;
        case D::noiseLevel:      return 5;
        case D::filterCutoff:    return 6;
        case D::filterRes:       return 7;
        case D::filterDrive:     return 8;
        case D::filterEnvAmount: return 9;
        default:                 return -1;
    }
}

void SynthEngine::applyGlobalModulation (int numSamples)
{
    using S = mod::Source;

    float values[mod::kNumSources] {};
    bool active[mod::kNumSources] {};

    for (int m = 0; m < 4; ++m)
    {
        values[static_cast<int> (S::macro1) + m] = current.macroValues[m];
        active[static_cast<int> (S::macro1) + m] = true;
    }
    values[static_cast<int> (S::modWheel)] = modWheel;     active[static_cast<int> (S::modWheel)] = true;
    values[static_cast<int> (S::aftertouch)] = aftertouch; active[static_cast<int> (S::aftertouch)] = true;
    values[static_cast<int> (S::pitchBend)] = pitchBend;   active[static_cast<int> (S::pitchBend)] = true;

    for (int k = 0; k < 3; ++k)
    {
        if (current.lfo[k].mono)
        {
            values[static_cast<int> (S::lfo1) + k] = monoLfo[k].value (current.lfo[k]);
            active[static_cast<int> (S::lfo1) + k] = true;
        }
        monoLfo[k].advance (current.lfo[k], current.bpm, numSamples);
    }

    for (auto& sum : globalNormSum) sum = 0.0f;
    for (auto& a : globalActive) a = false;
    numPolySlots = 0;

    for (int i = 0; i < mod::kNumSlots; ++i)
    {
        const auto& slot = current.mod.slots[i];
        if (! slot.enabled || slot.dest < 0 || slot.dest >= mod::kNumDests
            || slot.source < 0 || slot.source >= mod::kNumSources)
            continue;

        if (mod::isPolySource (static_cast<S> (slot.source), current.lfo))
        {
            polySlotIndices[numPolySlots++] = i;
        }
        else if (active[slot.source])
        {
            globalNormSum[slot.dest] += slot.depth * values[slot.source];
            globalActive[slot.dest] = true;
        }
    }

    // Macro mapping lists (SPEC section 9): offset = min + (max-min) * macro.
    for (int m = 0; m < mod::kNumMacros; ++m)
        for (const auto& map : current.mod.macroMaps[m])
            if (map.dest >= 0 && map.dest < mod::kNumDests)
            {
                globalNormSum[map.dest] += map.rangeMin
                    + (map.rangeMax - map.rangeMin) * current.macroValues[m];
                globalActive[map.dest] = true;
            }

    // Land every destination on its block-rate consumer.
    effEnv1 = current.env1;
    effEnv2 = current.env2;
    effEnv3 = current.env3;
    const bool snap = ! primed;

    for (int d = 0; d < mod::kNumDests; ++d)
    {
        const auto dest = static_cast<mod::Dest> (d);
        const float base = baseNaturalFor (d);
        float natural = base;
        if (globalActive[d])
        {
            const float n = mod::normalize (dest, base, destExponent[d]);
            natural = mod::denormalize (dest, mod::clampNorm (n + globalNormSum[d]), destExponent[d]);
        }

        if (auto* smoother = smootherFor (d))
        {
            setTarget (*smoother, dest == mod::Dest::noiseLevel ? noiseDbToLinear (natural) : natural, snap);
        }
        else
        {
            using D = mod::Dest;
            EnvParams* env = d <= static_cast<int> (D::env1Curve) ? &effEnv1
                           : d <= static_cast<int> (D::env2Curve) ? &effEnv2 : &effEnv3;
            const int field = (d - static_cast<int> (D::env1Attack)) % 5;
            if      (field == 0) env->attackSeconds = natural;
            else if (field == 1) env->decaySeconds = natural;
            else if (field == 2) env->sustain = natural;
            else if (field == 3) env->releaseSeconds = natural;
            else                 env->curve = natural;
        }
    }

    setTarget (bendSemis, pitchBend * kBendRangeSemis, snap);
    primed = true;
}

void SynthEngine::applyPolyModulation (int voiceIndex, VoiceBlockGlobals& globals,
                                       BlockBuffers& buffers, int numSamples)
{
    if (numPolySlots == 0)
    {
        voiceModPrimed[voiceIndex] = true;
        return;
    }

    using S = mod::Source;
    auto& voice = voices[voiceIndex];

    float values[mod::kNumSources] {};
    values[static_cast<int> (S::env1)] = voice.envValue (0);
    values[static_cast<int> (S::env2)] = voice.envValue (1);
    values[static_cast<int> (S::env3)] = voice.envValue (2);
    for (int k = 0; k < 3; ++k)
        if (! current.lfo[k].mono)
            values[static_cast<int> (S::lfo1) + k] = voice.polyLfoValue (k, current.lfo[k]);
    values[static_cast<int> (S::velocity)] = voice.velocityNorm();
    values[static_cast<int> (S::keytrack)] = voice.keytrackNorm();
    values[static_cast<int> (S::randomPerNote)] = voice.randomNorm();

    float polyNow[mod::kNumDests] {};
    int touched[mod::kNumSlots];
    int numTouched = 0;

    for (int i = 0; i < numPolySlots; ++i)
    {
        const auto& slot = current.mod.slots[polySlotIndices[i]];
        if (polyNow[slot.dest] == 0.0f)
        {
            bool seen = false;
            for (int t = 0; t < numTouched; ++t)
                seen = seen || touched[t] == slot.dest;
            if (! seen)
                touched[numTouched++] = slot.dest;
        }
        polyNow[slot.dest] += slot.depth * values[slot.source];
    }

    const float invN = 1.0f / static_cast<float> (numSamples);

    for (int t = 0; t < numTouched; ++t)
    {
        const int d = touched[t];
        const auto dest = static_cast<mod::Dest> (d);
        const float e = destExponent[d];
        const float now = polyNow[d];
        const float prev = voiceModPrimed[voiceIndex] ? voicePolyPrev[voiceIndex][d] : now;
        voicePolyPrev[voiceIndex][d] = now;

        const int bi = bufferedIndexFor (d);
        if (bi >= 0)
        {
            // Per-sample override: shared smoothed buffer + ramped poly sum.
            const float* src[10] = { buffers.morphA, buffers.levelA, buffers.morphB,
                                     buffers.levelB, buffers.subLevel, buffers.noiseLin,
                                     buffers.cutoffHz, buffers.res, buffers.driveDb,
                                     buffers.envAmount };
            const float* base = src[bi];
            const bool isNoise = dest == mod::Dest::noiseLevel;
            const float startNat = isNoise ? noiseLinearToDb (base[0]) : base[0];
            const float endNat   = isNoise ? noiseLinearToDb (base[numSamples - 1]) : base[numSamples - 1];
            const float n0 = mod::normalize (dest, startNat, e);
            const float n1 = mod::normalize (dest, endNat, e);
            const float nStep = (n1 - n0) * invN;
            const float pStep = (now - prev) * invN;

            float* out = scratch[bi].data();
            float n = n0, p = prev;
            for (int i = 0; i < numSamples; ++i)
            {
                float v = mod::denormalize (dest, mod::clampNorm (n + p), e);
                if (isNoise)
                    v = noiseDbToLinear (v);
                out[i] = v;
                n += nStep;
                p += pStep;
            }

            switch (bi)
            {
                case 0: buffers.morphA = out; break;
                case 1: buffers.levelA = out; break;
                case 2: buffers.morphB = out; break;
                case 3: buffers.levelB = out; break;
                case 4: buffers.subLevel = out; break;
                case 5: buffers.noiseLin = out; break;
                case 6: buffers.cutoffHz = out; break;
                case 7: buffers.res = out; break;
                case 8: buffers.driveDb = out; break;
                case 9: buffers.envAmount = out; break;
                default: break;
            }
        }
        else
        {
            // Block-ramped destinations: shift the start/end pairs the voice lerps.
            auto shift = [dest, e] (float& startVal, float& endVal, float prevSum, float nowSum)
            {
                startVal = mod::denormalize (dest, mod::clampNorm (
                               mod::normalize (dest, startVal, e) + prevSum), e);
                endVal = mod::denormalize (dest, mod::clampNorm (
                             mod::normalize (dest, endVal, e) + nowSum), e);
            };
            auto shiftOne = [dest, e] (float& value, float nowSum)
            {
                value = mod::denormalize (dest, mod::clampNorm (
                            mod::normalize (dest, value, e) + nowSum), e);
            };

            using D = mod::Dest;
            switch (dest)
            {
                case D::oscAPan:    shift (globals.oscA.panStart, globals.oscA.panEnd, prev, now); break;
                case D::oscAFine:   shift (globals.oscA.fineStart, globals.oscA.fineEnd, prev, now); break;
                case D::oscADetune: shift (globals.oscA.detuneStart, globals.oscA.detuneEnd, prev, now); break;
                case D::oscAWidth:  shift (globals.oscA.widthStart, globals.oscA.widthEnd, prev, now); break;
                case D::oscABlend:  shift (globals.oscA.blendStart, globals.oscA.blendEnd, prev, now); break;
                case D::oscBPan:    shift (globals.oscB.panStart, globals.oscB.panEnd, prev, now); break;
                case D::oscBFine:   shift (globals.oscB.fineStart, globals.oscB.fineEnd, prev, now); break;
                case D::oscBDetune: shift (globals.oscB.detuneStart, globals.oscB.detuneEnd, prev, now); break;
                case D::oscBWidth:  shift (globals.oscB.widthStart, globals.oscB.widthEnd, prev, now); break;
                case D::oscBBlend:  shift (globals.oscB.blendStart, globals.oscB.blendEnd, prev, now); break;
                case D::filterKeytrack: shiftOne (globals.keytrack, now); break;
                case D::env1Attack:  shiftOne (globals.env1.attackSeconds, now); break;
                case D::env1Decay:   shiftOne (globals.env1.decaySeconds, now); break;
                case D::env1Sustain: shiftOne (globals.env1.sustain, now); break;
                case D::env1Release: shiftOne (globals.env1.releaseSeconds, now); break;
                case D::env1Curve:   shiftOne (globals.env1.curve, now); break;
                case D::env2Attack:  shiftOne (globals.env2.attackSeconds, now); break;
                case D::env2Decay:   shiftOne (globals.env2.decaySeconds, now); break;
                case D::env2Sustain: shiftOne (globals.env2.sustain, now); break;
                case D::env2Release: shiftOne (globals.env2.releaseSeconds, now); break;
                case D::env2Curve:   shiftOne (globals.env2.curve, now); break;
                case D::env3Attack:  shiftOne (globals.env3.attackSeconds, now); break;
                case D::env3Decay:   shiftOne (globals.env3.decaySeconds, now); break;
                case D::env3Sustain: shiftOne (globals.env3.sustain, now); break;
                case D::env3Release: shiftOne (globals.env3.releaseSeconds, now); break;
                case D::env3Curve:   shiftOne (globals.env3.curve, now); break;
                default: break;
            }
        }
    }

    voiceModPrimed[voiceIndex] = true;
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void SynthEngine::renderChunk (float* outL, float* outR, int numSamples)
{
    applyGlobalModulation (numSamples);

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
    globals.env1 = effEnv1;
    globals.env2 = effEnv2;
    globals.env3 = effEnv3;
    globals.bendStart = std::exp2 (static_cast<double> (bendSemis.getCurrentValue()) / 12.0);
    globals.bendEnd = std::exp2 (static_cast<double> (bendSemis.skip (numSamples)) / 12.0);

    const BlockBuffers sharedBuffers {
        bufMorphA.data(), bufLevelA.data(), bufMorphB.data(), bufLevelB.data(),
        bufSubLevel.data(), bufNoiseLin.data(), bufCutoff.data(), bufRes.data(),
        bufDrive.data(), bufEnvAmount.data(), driveComp
    };

    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[vi];
        if (! v.isActive())
            continue;

        VoiceBlockGlobals voiceGlobals = globals;
        BlockBuffers voiceBuffers = sharedBuffers;
        applyPolyModulation (vi, voiceGlobals, voiceBuffers, numSamples);

        v.startBlock (voiceGlobals, numSamples);
        v.render (outL, outR, numSamples, voiceBuffers);
        v.advancePolyLfos (current.lfo, current.bpm, numSamples);
    }

    // Master FX bus (SPEC section 10). Runs after the voice sum; the
    // continuous FX targets were landed by applyGlobalModulation above.
    if (fxEnabled)
    {
        fx.setBlockParams (current.fx, current.bpm);
        fx.process (outL, outR, numSamples);
    }
}
} // namespace lumen
