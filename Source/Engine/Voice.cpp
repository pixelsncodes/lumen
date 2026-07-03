#include "Engine/Voice.h"

#include <algorithm>
#include <cmath>

namespace lumen
{
namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kCenterGain = 0.70710678f; // equal-power center for mono sources

    inline double nextRand01 (uint64_t& state) noexcept
    {
        // xorshift64* — deterministic, seeded once by the engine (no rand()).
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        const uint64_t z = state * 0x2545F4914F6CDD1DULL;
        return static_cast<double> (z >> 11) * (1.0 / 9007199254740992.0); // [0,1)
    }

    inline float whiteNoise (uint32_t& state) noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (static_cast<int32_t> (state)) * (1.0f / 2147483648.0f);
    }

    inline float readTable (const float* frame, double phase) noexcept
    {
        const float pos = static_cast<float> (phase) * static_cast<float> (Wavetable::kFrameLength);
        const int idx = static_cast<int> (pos);
        const float frac = pos - static_cast<float> (idx);
        return frame[idx] + frac * (frame[idx + 1] - frame[idx]);
    }

    // Unison lane placement (DECISIONS.md): offsets spread evenly in [-1, 1];
    // blend crossfades a center-weighted triangle profile against its inverse
    // (floored at 0.05 so no lane ever fully vanishes), then gains are
    // normalized by the amplitude sum so unison can never exceed a single
    // voice's coherent peak.
    struct LaneGains
    {
        float gainL[Voice::kMaxUnison] {};
        float gainR[Voice::kMaxUnison] {};
        double ratio[Voice::kMaxUnison] {};
    };

    LaneGains computeLanes (int unison, float detuneCents, float width, float blend, float pan)
    {
        LaneGains out;
        float weights[Voice::kMaxUnison] {};
        float weightSum = 0.0f;

        for (int k = 0; k < unison; ++k)
        {
            const float offset = unison == 1 ? 0.0f
                                             : -1.0f + 2.0f * static_cast<float> (k) / static_cast<float> (unison - 1);
            const float centerness = 1.0f - std::abs (offset);
            const float w = std::max (0.05f, (1.0f - blend) * centerness + blend * (1.0f - centerness));
            weights[k] = w;
            weightSum += w;

            out.ratio[k] = std::exp2 (static_cast<double> (offset * detuneCents) / 1200.0);

            const float panPos = std::clamp (pan + offset * width, -1.0f, 1.0f);
            const float angle = (panPos + 1.0f) * (kPi / 4.0f);
            out.gainL[k] = std::cos (angle);
            out.gainR[k] = std::sin (angle);
        }

        for (int k = 0; k < unison; ++k)
        {
            const float g = weights[k] / weightSum;
            out.gainL[k] *= g;
            out.gainR[k] *= g;
        }
        return out;
    }
} // namespace

void Voice::prepare (double sr, uint32_t noiseSeed)
{
    sampleRate = sr;
    noiseState = noiseSeed != 0 ? noiseSeed : 1;
    env1.setSampleRate (sr);
    env2.setSampleRate (sr);
    env3.setSampleRate (sr);
    filter.prepare (sr);
    for (int k = 0; k < 3; ++k)
        polyLfo[k].prepare (sr, noiseState ^ (0x1234567u * static_cast<uint32_t> (k + 1)));
    active = false;
}

void Voice::startNote (int midiNote, float velocity, uint64_t& rngState,
                       bool phaseRandomA, bool phaseRandomB,
                       float glideFromSemis, float glideSeconds)
{
    note = midiNote;
    pitchTargetSemis = static_cast<double> (midiNote);
    if (glideFromSemis >= 0.0f && glideSeconds > 1.0e-4f
        && std::abs (static_cast<double> (glideFromSemis) - pitchTargetSemis) > 1.0e-6)
    {
        pitchSemis = static_cast<double> (glideFromSemis);
        glideSemisPerSample = (pitchTargetSemis - pitchSemis) / (static_cast<double> (glideSeconds) * sampleRate);
    }
    else
    {
        pitchSemis = pitchTargetSemis;
        glideSemisPerSample = 0.0;
    }
    velocityGain = std::clamp (velocity, 0.0f, 1.0f); // linear amplitude (DECISIONS.md)

    // Always (re)set all 8 lane phases so a unison-count increase mid-note
    // finds valid state. Fresh voices get a clean filter; stolen voices keep
    // theirs and the envelopes retrigger from their current level (no click).
    for (auto* osc : { &oscA, &oscB })
    {
        const bool random = osc == &oscA ? phaseRandomA : phaseRandomB;
        for (auto& lane : osc->lanes)
            lane.phase = random ? nextRand01 (rngState) : 0.0;
    }
    subPhase = 0.0;

    for (auto& lfo : polyLfo)
        lfo.retrigger();
    randomValue = 2.0f * static_cast<float> (nextRand01 (rngState)) - 1.0f;

    if (! active)
        filter.reset();

    env1.noteOn();
    env2.noteOn();
    env3.noteOn();
    active = true;
}

void Voice::retune (int midiNote, float glideSeconds)
{
    note = midiNote;
    pitchTargetSemis = static_cast<double> (midiNote);
    if (glideSeconds > 1.0e-4f && std::abs (pitchSemis - pitchTargetSemis) > 1.0e-6)
    {
        glideSemisPerSample = (pitchTargetSemis - pitchSemis) / (static_cast<double> (glideSeconds) * sampleRate);
    }
    else
    {
        pitchSemis = pitchTargetSemis;
        glideSemisPerSample = 0.0;
    }
}

void Voice::noteOff()
{
    env1.noteOff();
    env2.noteOff();
    env3.noteOff();
}

void Voice::kill()
{
    env1.reset();
    env2.reset();
    env3.reset();
    active = false;
}

void Voice::configureOsc (OscState& osc, const OscBlockGlobals& g, int numSamples,
                          float hzStart, float hzEnd,
                          double bendStart, double bendEnd)
{
    osc.enabled = g.enabled && g.table != nullptr && ! g.table->isEmpty();
    osc.table = g.table;
    osc.unison = std::clamp (g.unison, 1, kMaxUnison);
    if (! osc.enabled)
        return;

    const auto startLanes = computeLanes (osc.unison, g.detuneStart, g.widthStart, g.blendStart, g.panStart);
    const auto endLanes   = computeLanes (osc.unison, g.detuneEnd,   g.widthEnd,   g.blendEnd,   g.panEnd);

    const double baseStart = static_cast<double> (hzStart) * bendStart
        * std::exp2 ((static_cast<double> (g.semitones) + static_cast<double> (g.fineStart) / 100.0) / 12.0)
        / sampleRate;
    const double baseEnd = static_cast<double> (hzEnd) * bendEnd
        * std::exp2 ((static_cast<double> (g.semitones) + static_cast<double> (g.fineEnd) / 100.0) / 12.0)
        / sampleRate;

    const float invN = 1.0f / static_cast<float> (numSamples);

    for (int k = 0; k < osc.unison; ++k)
    {
        auto& lane = osc.lanes[k];
        const double incStart = baseStart * startLanes.ratio[k];
        const double incEnd   = baseEnd * endLanes.ratio[k];
        lane.inc = incStart;
        lane.incStep = (incEnd - incStart) / numSamples;
        lane.gainL = startLanes.gainL[k];
        lane.gainR = startLanes.gainR[k];
        lane.gainLStep = (endLanes.gainL[k] - startLanes.gainL[k]) * invN;
        lane.gainRStep = (endLanes.gainR[k] - startLanes.gainR[k]) * invN;
        lane.mip = Wavetable::mipForIncrement (std::max (incStart, incEnd));
    }
}

void Voice::startBlock (const VoiceBlockGlobals& globals, int numSamples)
{
    env1.setParameters (globals.env1);
    env2.setParameters (globals.env2);
    env3.setParameters (globals.env3);

    filterMode = globals.filterMode;
    subWave = globals.subWave;
    noiseType = globals.noiseType;
    keytrackFactor = std::exp2 (globals.keytrack * (static_cast<float> (note) - 60.0f) / 12.0f);

    // Advance the pitch glide across this block (SPEC 11). The float hz
    // expression matches the pre-glide noteHz math exactly, so a snapped
    // pitch (poly, glide 0) renders bit-identically to the old fixed pitch.
    const double semisStart = pitchSemis;
    if (glideSemisPerSample != 0.0)
    {
        double next = pitchSemis + glideSemisPerSample * static_cast<double> (numSamples);
        if ((glideSemisPerSample > 0.0 && next >= pitchTargetSemis)
            || (glideSemisPerSample < 0.0 && next <= pitchTargetSemis))
        {
            next = pitchTargetSemis;
            glideSemisPerSample = 0.0;
        }
        pitchSemis = next;
    }
    const float hzStart = 440.0f * std::exp2 ((static_cast<float> (semisStart) - 69.0f) / 12.0f);
    const float hzEnd = glideSemisPerSample == 0.0 && semisStart == pitchSemis
                            ? hzStart
                            : 440.0f * std::exp2 ((static_cast<float> (pitchSemis) - 69.0f) / 12.0f);

    configureOsc (oscA, globals.oscA, numSamples, hzStart, hzEnd, globals.bendStart, globals.bendEnd);
    configureOsc (oscB, globals.oscB, numSamples, hzStart, hzEnd, globals.bendStart, globals.bendEnd);

    // Sub tracks Osc A pitch pre-detune, -1 or -2 octaves (SPEC section 5).
    const double subScale = std::exp2 (-static_cast<double> (globals.subOctave));
    const double subStart = static_cast<double> (hzStart) * globals.bendStart
        * std::exp2 ((globals.oscA.semitones + static_cast<double> (globals.oscA.fineStart) / 100.0) / 12.0)
        * subScale / sampleRate;
    const double subEnd = static_cast<double> (hzEnd) * globals.bendEnd
        * std::exp2 ((globals.oscA.semitones + static_cast<double> (globals.oscA.fineEnd) / 100.0) / 12.0)
        * subScale / sampleRate;
    subInc = subStart;
    subIncStep = (subEnd - subStart) / numSamples;
}

float Voice::renderOscSample (OscState& osc, float morph, float& outR) noexcept
{
    const int numFrames = osc.table->getNumFrames();
    const float framePos = morph * static_cast<float> (numFrames - 1);
    int frameA = static_cast<int> (framePos);
    frameA = std::min (frameA, numFrames - 1);
    const int frameB = std::min (frameA + 1, numFrames - 1);
    const float frac = framePos - static_cast<float> (frameA);
    const float xfB = std::sqrt (frac);              // equal-power frame crossfade
    const float xfA = std::sqrt (1.0f - frac);

    float sumL = 0.0f, sumR = 0.0f;
    for (int k = 0; k < osc.unison; ++k)
    {
        auto& lane = osc.lanes[k];
        const float sA = readTable (osc.table->frameData (frameA, lane.mip), lane.phase);
        const float sB = readTable (osc.table->frameData (frameB, lane.mip), lane.phase);
        const float s = xfA * sA + xfB * sB;
        sumL += s * lane.gainL;
        sumR += s * lane.gainR;

        lane.phase += lane.inc;
        if (lane.phase >= 1.0)
            lane.phase -= 1.0;
        lane.inc += lane.incStep;
        lane.gainL += lane.gainLStep;
        lane.gainR += lane.gainRStep;
    }
    outR = sumR;
    return sumL;
}

void Voice::render (float* outL, float* outR, int numSamples, const BlockBuffers& buffers)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float e1 = env1.next();
        const float e2 = env2.next();
        env3.next();

        if (! env1.isActive())
        {
            active = false;
            return;
        }

        float l = 0.0f, r = 0.0f;

        if (oscA.enabled)
        {
            float or_ = 0.0f;
            const float ol = renderOscSample (oscA, buffers.morphA[i], or_);
            l += ol * buffers.levelA[i];
            r += or_ * buffers.levelA[i];
        }
        if (oscB.enabled)
        {
            float or_ = 0.0f;
            const float ol = renderOscSample (oscB, buffers.morphB[i], or_);
            l += ol * buffers.levelB[i];
            r += or_ * buffers.levelB[i];
        }

        // Sub: sine / triangle / polyBLEP square (SPEC section 5)
        {
            const float ph = static_cast<float> (subPhase);
            float s;
            if (subWave == static_cast<int> (SubWave::sine))
                s = std::sin (2.0f * kPi * ph);
            else if (subWave == static_cast<int> (SubWave::triangle))
                s = 4.0f * std::abs (ph - 0.5f) - 1.0f;
            else
            {
                s = ph < 0.5f ? 1.0f : -1.0f;
                const float dt = static_cast<float> (subInc);
                auto blep = [dt] (float t) noexcept
                {
                    if (t < dt)          { const float x = t / dt;          return x + x - x * x - 1.0f; }
                    if (t > 1.0f - dt)   { const float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
                    return 0.0f;
                };
                s += blep (ph);
                float ph2 = ph + 0.5f;
                if (ph2 >= 1.0f) ph2 -= 1.0f;
                s -= blep (ph2);
            }
            const float sub = s * buffers.subLevel[i] * kCenterGain;
            l += sub;
            r += sub;

            subPhase += subInc;
            if (subPhase >= 1.0)
                subPhase -= 1.0;
            subInc += subIncStep;
        }

        // Noise: white / pink (Kellet economy filter)
        {
            const float w = whiteNoise (noiseState);
            float n = w;
            if (noiseType == static_cast<int> (NoiseType::pink))
            {
                pinkB0 = 0.99765f * pinkB0 + w * 0.0990460f;
                pinkB1 = 0.96300f * pinkB1 + w * 0.2965164f;
                pinkB2 = 0.57000f * pinkB2 + w * 1.0526913f;
                n = (pinkB0 + pinkB1 + pinkB2 + w * 0.1848f) * 0.2f;
            }
            const float noise = n * buffers.noiseLin[i] * kCenterGain;
            l += noise;
            r += noise;
        }

        // Drive: tanh pre-filter, gain-compensated from the table the engine
        // measured at prepare time (SPEC section 6).
        {
            const float db = buffers.driveDb[i];
            if (db > 0.01f)
            {
                const float g = std::exp2 (db * 0.16609640474f); // 10^(db/20)
                const int di = std::min (23, static_cast<int> (db));
                const float dfrac = db - static_cast<float> (di);
                const float comp = buffers.driveComp[di] + dfrac * (buffers.driveComp[di + 1] - buffers.driveComp[di]);
                l = std::tanh (g * l) * comp;
                r = std::tanh (g * r) * comp;
            }
        }

        // Filter: cutoff = smoothed knob * keytrack * Env2 (+-5 octaves span)
        {
            const float envOct = 5.0f * buffers.envAmount[i] * e2;
            const float fc = buffers.cutoffHz[i] * keytrackFactor * std::exp2 (envOct);
            filter.setPerSample (fc, buffers.res[i]);
            l = filter.processSample (l, filterMode, 0);
            r = filter.processSample (r, filterMode, 1);
        }

        const float amp = e1 * velocityGain;
        outL[i] += l * amp;
        outR[i] += r * amp;
    }
}
} // namespace lumen
