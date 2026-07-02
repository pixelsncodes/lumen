#include "Engine/FxChain.h"

#include <algorithm>
#include <cmath>

namespace lumen
{
namespace
{
    constexpr double kSmoothingSeconds = 0.02;      // SPEC section 12
    constexpr double kDelayTimeSmoothing = 0.1;     // tape-style time glide (DECISIONS.md)
    constexpr double kLookaheadSeconds = 0.0015;    // limiter, SPEC section 10
    constexpr double kLimiterRelease = 0.08;
    constexpr float  kCeilingDb = -0.3f;
    constexpr float  kChorusCentreMs = 13.5f;       // lines swing 7..20 ms
    constexpr float  kChorusSwingMs = 6.5f;
    constexpr float  kMaxDelayMs = 2000.0f;
    constexpr float  kTiltPivotHz = 800.0f;
    constexpr float  kTiltRangeDb = 6.0f;

    float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }

    float onePoleCoeff (double cutoffHz, double sampleRate) noexcept
    {
        return static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979323846 * cutoffHz / sampleRate));
    }

    void setTarget (FxChain::Smoothed& s, float value, bool snap)
    {
        if (snap)
            s.setCurrentAndTargetValue (value);
        else
            s.setTargetValue (value);
    }
} // namespace

void FxChain::measureTanhCompensation (float* table, int numEntries)
{
    constexpr int kProbe = 256;
    double rmsIn = 0.0;
    for (int i = 0; i < kProbe; ++i)
    {
        const double v = 0.25 * std::sin (2.0 * 3.14159265358979323846 * i / kProbe);
        rmsIn += v * v;
    }
    rmsIn = std::sqrt (rmsIn / kProbe);

    for (int d = 0; d < numEntries; ++d)
    {
        const double g = std::pow (10.0, d / 20.0);
        double rmsOut = 0.0;
        for (int i = 0; i < kProbe; ++i)
        {
            const double v = std::tanh (g * 0.25 * std::sin (2.0 * 3.14159265358979323846 * i / kProbe));
            rmsOut += v * v;
        }
        rmsOut = std::sqrt (rmsOut / kProbe);
        table[d] = static_cast<float> (rmsIn / std::max (1.0e-9, rmsOut));
    }
}

void FxChain::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    primed = false;

    measureTanhCompensation (driveComp, 25);
    tiltCoeff = onePoleCoeff (kTiltPivotHz, sr);

    const auto samplesFor = [sr] (double ms) { return static_cast<size_t> (std::ceil (ms * 0.001 * sr)) + 8; };
    chorusBufL.assign (samplesFor (kChorusCentreMs + kChorusSwingMs + 2.0), 0.0f);
    chorusBufR.assign (chorusBufL.size(), 0.0f);
    delayBufL.assign (samplesFor (kMaxDelayMs + 50.0), 0.0f);
    delayBufR.assign (delayBufL.size(), 0.0f);

    reverb.prepare ({ sr, static_cast<juce::uint32> (std::max (16, maxBlockSize)), 2 });
    wetL.assign (static_cast<size_t> (std::max (16, maxBlockSize)), 0.0f);
    wetR.assign (wetL.size(), 0.0f);

    lookahead = std::max (1, static_cast<int> (std::ceil (kLookaheadSeconds * sr)));
    ceiling = dbToGain (kCeilingDb);
    releaseAlpha = static_cast<float> (1.0 - std::exp (-1.0 / (kLimiterRelease * sr)));
    limDelayL.assign (static_cast<size_t> (lookahead), 0.0f);
    limDelayR.assign (static_cast<size_t> (lookahead), 0.0f);
    limTargets.assign (static_cast<size_t> (lookahead) + 1, 1.0f);
    limEnvHist.assign (static_cast<size_t> (lookahead) + 1, 1.0f);

    for (auto* s : { &smooth.driveDb, &smooth.driveTone,
                     &smooth.chorusRateHz, &smooth.chorusDepth, &smooth.chorusMix,
                     &smooth.delayFeedback, &smooth.delayDampHz, &smooth.delayMix,
                     &smooth.reverbSize, &smooth.reverbDamp, &smooth.reverbWidth, &smooth.reverbMix,
                     &smooth.masterGainDb,
                     &driveEnable, &chorusEnable, &delayEnable, &reverbEnable })
        s->reset (sr, kSmoothingSeconds);
    smooth.delayTimeMs.reset (sr, kDelayTimeSmoothing);

    reset();
}

void FxChain::reset()
{
    std::fill (chorusBufL.begin(), chorusBufL.end(), 0.0f);
    std::fill (chorusBufR.begin(), chorusBufR.end(), 0.0f);
    std::fill (delayBufL.begin(), delayBufL.end(), 0.0f);
    std::fill (delayBufR.begin(), delayBufR.end(), 0.0f);
    std::fill (limDelayL.begin(), limDelayL.end(), 0.0f);
    std::fill (limDelayR.begin(), limDelayR.end(), 0.0f);
    std::fill (limTargets.begin(), limTargets.end(), 1.0f);
    std::fill (limEnvHist.begin(), limEnvHist.end(), 1.0f);
    limBoxSum = static_cast<double> (limEnvHist.size());
    limReleaseEnv = 1.0f;
    chorusPos = delayPos = limPos = limHistPos = 0;
    chorusPhase = 0.0;
    tiltStateL = tiltStateR = dampStateL = dampStateR = 0.0f;
    reverb.reset();
    meter = {};
}

void FxChain::setBlockParams (const FxParams& params, double bpm)
{
    const bool snap = ! primed;
    primed = true;

    setTarget (driveEnable,  params.driveEnabled  ? 1.0f : 0.0f, snap);
    setTarget (chorusEnable, params.chorusEnabled ? 1.0f : 0.0f, snap);
    setTarget (delayEnable,  params.delayEnabled  ? 1.0f : 0.0f, snap);
    setTarget (reverbEnable, params.reverbEnabled ? 1.0f : 0.0f, snap);

    delaySync = params.delaySync;
    delayPingPong = params.delayPingPong;

    // Host-synced delay overrides the (already modulation-landed) time target;
    // the shared smoother turns division/tempo changes into a tape-style glide.
    if (delaySync)
    {
        const double beats = lfoSyncBeats (params.delayDiv);
        const double ms = beats * 60000.0 / std::max (1.0, bpm);
        setTarget (smooth.delayTimeMs, static_cast<float> (std::clamp (ms, 1.0, static_cast<double> (kMaxDelayMs))), snap);
    }
}

float FxChain::readHermite (const std::vector<float>& buffer, int writePos, float delaySamples) noexcept
{
    const int size = static_cast<int> (buffer.size());
    float pos = static_cast<float> (writePos) - delaySamples;
    if (pos < 0.0f)
        pos += static_cast<float> (size);

    const int i1 = static_cast<int> (pos);
    const float t = pos - static_cast<float> (i1);
    const int i0 = i1 > 0 ? i1 - 1 : size - 1;
    const int i2 = i1 + 1 < size ? i1 + 1 : 0;
    const int i3 = i2 + 1 < size ? i2 + 1 : 0;

    const float xm1 = buffer[static_cast<size_t> (i0)];
    const float x0  = buffer[static_cast<size_t> (i1)];
    const float x1  = buffer[static_cast<size_t> (i2)];
    const float x2  = buffer[static_cast<size_t> (i3)];

    const float c = 0.5f * (x1 - xm1);
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + 0.5f * (x2 - x0);
    const float b = w + a;
    return ((((a * t) - b) * t + c) * t + x0);
}

void FxChain::process (float* left, float* right, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;

    const float invN = 1.0f / static_cast<float> (numSamples);

    // --- Drive: +-6 dB tilt at 800 Hz into tanh, loudness-compensated ------
    {
        const float e0 = driveEnable.getCurrentValue();
        const float e1 = driveEnable.skip (numSamples);
        const float d0 = smooth.driveDb.getCurrentValue();
        const float d1 = smooth.driveDb.skip (numSamples);
        const float t0 = smooth.driveTone.getCurrentValue();
        const float t1 = smooth.driveTone.skip (numSamples);

        if (e0 > 0.0f || e1 > 0.0f)
        {
            const float g0 = dbToGain (d0), g1 = dbToGain (d1);
            const int ci0 = std::clamp (static_cast<int> (d0), 0, 24);
            const int ci1 = std::clamp (static_cast<int> (d1), 0, 24);
            const float c0 = driveComp[ci0] + (driveComp[std::min (24, ci0 + 1)] - driveComp[ci0]) * (d0 - static_cast<float> (ci0));
            const float c1 = driveComp[ci1] + (driveComp[std::min (24, ci1 + 1)] - driveComp[ci1]) * (d1 - static_cast<float> (ci1));
            const float lo0 = dbToGain (-t0 * kTiltRangeDb), lo1 = dbToGain (-t1 * kTiltRangeDb);
            const float hi0 = dbToGain (t0 * kTiltRangeDb),  hi1 = dbToGain (t1 * kTiltRangeDb);

            float e = e0, g = g0, c = c0, lo = lo0, hi = hi0;
            const float eS = (e1 - e0) * invN, gS = (g1 - g0) * invN, cS = (c1 - c0) * invN;
            const float loS = (lo1 - lo0) * invN, hiS = (hi1 - hi0) * invN;

            for (int i = 0; i < numSamples; ++i)
            {
                const float xl = left[i], xr = right[i];
                tiltStateL += tiltCoeff * (xl - tiltStateL);
                tiltStateR += tiltCoeff * (xr - tiltStateR);
                const float wetl = std::tanh (g * (lo * tiltStateL + hi * (xl - tiltStateL))) * c;
                const float wetr = std::tanh (g * (lo * tiltStateR + hi * (xr - tiltStateR))) * c;
                left[i]  = xl + e * (wetl - xl);
                right[i] = xr + e * (wetr - xr);
                e += eS; g += gS; c += cS; lo += loS; hi += hiS;
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i) // keep the tilt state warm for re-enable
            {
                tiltStateL += tiltCoeff * (left[i] - tiltStateL);
                tiltStateR += tiltCoeff * (right[i] - tiltStateR);
            }
        }
    }

    // --- Chorus: two 7-20 ms modulated lines, quadrature LFOs --------------
    {
        const float e0 = chorusEnable.getCurrentValue();
        const float e1 = chorusEnable.skip (numSamples);
        const float m0 = smooth.chorusMix.getCurrentValue();
        const float m1 = smooth.chorusMix.skip (numSamples);
        const float dep0 = smooth.chorusDepth.getCurrentValue();
        const float dep1 = smooth.chorusDepth.skip (numSamples);
        const float rate = smooth.chorusRateHz.skip (numSamples);

        const double phaseInc = static_cast<double> (rate) / sampleRate;
        const float msToSamples = static_cast<float> (sampleRate * 0.001);
        float mix = m0 * e0;
        const float mixS = (m1 * e1 - mix) * invN;
        float dep = dep0;
        const float depS = (dep1 - dep0) * invN;

        for (int i = 0; i < numSamples; ++i)
        {
            chorusBufL[static_cast<size_t> (chorusPos)] = left[i];
            chorusBufR[static_cast<size_t> (chorusPos)] = right[i];

            const float lfoL = std::sin (static_cast<float> (chorusPhase) * 6.2831853f);
            const float lfoR = std::sin ((static_cast<float> (chorusPhase) + 0.25f) * 6.2831853f);
            const float delL = (kChorusCentreMs + kChorusSwingMs * dep * lfoL) * msToSamples;
            const float delR = (kChorusCentreMs + kChorusSwingMs * dep * lfoR) * msToSamples;

            const float wl = readHermite (chorusBufL, chorusPos, delL);
            const float wr = readHermite (chorusBufR, chorusPos, delR);
            left[i]  += mix * (wl - left[i]);
            right[i] += mix * (wr - right[i]);

            if (++chorusPos >= static_cast<int> (chorusBufL.size()))
                chorusPos = 0;
            chorusPhase += phaseInc;
            if (chorusPhase >= 1.0)
                chorusPhase -= 1.0;
            mix += mixS; dep += depS;
        }
    }

    // --- Delay: stereo, damped feedback, optional ping-pong ----------------
    {
        const float e0 = delayEnable.getCurrentValue();
        const float e1 = delayEnable.skip (numSamples);
        const float m0 = smooth.delayMix.getCurrentValue();
        const float m1 = smooth.delayMix.skip (numSamples);
        const float fb0 = smooth.delayFeedback.getCurrentValue();
        const float fb1 = smooth.delayFeedback.skip (numSamples);
        const float dampHz = smooth.delayDampHz.skip (numSamples);
        const float dampCoeff = onePoleCoeff (dampHz, sampleRate);

        const float msToSamples = static_cast<float> (sampleRate * 0.001);
        const float maxDelaySamples = static_cast<float> (delayBufL.size()) - 8.0f;
        float mix = m0 * e0;
        const float mixS = (m1 * e1 - mix) * invN;
        float fb = fb0;
        const float fbS = (fb1 - fb0) * invN;

        for (int i = 0; i < numSamples; ++i)
        {
            const float delSamples = std::clamp (smooth.delayTimeMs.getNextValue() * msToSamples,
                                                 4.0f, maxDelaySamples);
            const float wl = readHermite (delayBufL, delayPos, delSamples);
            const float wr = readHermite (delayBufR, delayPos, delSamples);

            dampStateL += dampCoeff * (wl - dampStateL);
            dampStateR += dampCoeff * (wr - dampStateR);

            const float xl = left[i], xr = right[i];
            if (delayPingPong)
            {
                const float mono = 0.5f * (xl + xr);
                delayBufL[static_cast<size_t> (delayPos)] = mono + fb * dampStateR;
                delayBufR[static_cast<size_t> (delayPos)] = fb * dampStateL;
            }
            else
            {
                delayBufL[static_cast<size_t> (delayPos)] = xl + fb * dampStateL;
                delayBufR[static_cast<size_t> (delayPos)] = xr + fb * dampStateR;
            }

            left[i]  = xl + mix * (wl - xl);
            right[i] = xr + mix * (wr - xr);

            if (++delayPos >= static_cast<int> (delayBufL.size()))
                delayPos = 0;
            mix += mixS; fb += fbS;
        }
    }

    // --- Reverb: juce::dsp::Reverb, wet-only, external dry/wet mix ---------
    {
        const float e0 = reverbEnable.getCurrentValue();
        const float e1 = reverbEnable.skip (numSamples);
        const float m0 = smooth.reverbMix.getCurrentValue();
        const float m1 = smooth.reverbMix.skip (numSamples);

        juce::Reverb::Parameters rp;
        rp.roomSize = smooth.reverbSize.skip (numSamples);
        rp.damping = smooth.reverbDamp.skip (numSamples);
        rp.width = smooth.reverbWidth.skip (numSamples);
        rp.wetLevel = 1.0f / 3.0f; // cancels juce::Reverb's internal 3x wet scale
        rp.dryLevel = 0.0f;
        reverb.setParameters (rp);

        std::copy_n (left, numSamples, wetL.data());
        std::copy_n (right, numSamples, wetR.data());
        float* channels[2] = { wetL.data(), wetR.data() };
        juce::dsp::AudioBlock<float> block (channels, 2, static_cast<size_t> (numSamples));
        juce::dsp::ProcessContextReplacing<float> context (block);
        reverb.process (context);

        float mix = m0 * e0;
        const float mixS = (m1 * e1 - mix) * invN;
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  += mix * (wetL[static_cast<size_t> (i)] - left[i]);
            right[i] += mix * (wetR[static_cast<size_t> (i)] - right[i]);
            mix += mixS;
        }
    }

    // --- Limiter (always on): lookahead sliding-min + boxcar, then release -
    {
        const int window = lookahead + 1;
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = std::max (std::abs (left[i]), std::abs (right[i]));
            limTargets[static_cast<size_t> (limHistPos)] = a > ceiling ? ceiling / a : 1.0f;

            float slidingMin = 1.0f;
            for (const float t : limTargets)
                slidingMin = std::min (slidingMin, t);

            if (slidingMin <= limReleaseEnv)
                limReleaseEnv = slidingMin;
            else
                limReleaseEnv += (slidingMin - limReleaseEnv) * releaseAlpha;

            limBoxSum += static_cast<double> (limReleaseEnv)
                       - static_cast<double> (limEnvHist[static_cast<size_t> (limHistPos)]);
            limEnvHist[static_cast<size_t> (limHistPos)] = limReleaseEnv;
            if (++limHistPos >= window)
                limHistPos = 0;

            const float gain = static_cast<float> (limBoxSum / static_cast<double> (window));

            const float outl = limDelayL[static_cast<size_t> (limPos)] * gain;
            const float outr = limDelayR[static_cast<size_t> (limPos)] * gain;
            limDelayL[static_cast<size_t> (limPos)] = left[i];
            limDelayR[static_cast<size_t> (limPos)] = right[i];
            if (++limPos >= lookahead)
                limPos = 0;

            left[i]  = std::clamp (outl, -ceiling, ceiling);
            right[i] = std::clamp (outr, -ceiling, ceiling);
        }
    }

    // --- Master gain + meter tap -------------------------------------------
    {
        const float db0 = smooth.masterGainDb.getCurrentValue();
        const float db1 = smooth.masterGainDb.skip (numSamples);
        float g = db0 <= -59.95f ? 0.0f : dbToGain (db0);
        const float g1 = db1 <= -59.95f ? 0.0f : dbToGain (db1);
        const float gS = (g1 - g) * invN;

        float peakL = 0.0f, peakR = 0.0f;
        double sumL = 0.0, sumR = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] *= g;
            right[i] *= g;
            peakL = std::max (peakL, std::abs (left[i]));
            peakR = std::max (peakR, std::abs (right[i]));
            sumL += static_cast<double> (left[i]) * left[i];
            sumR += static_cast<double> (right[i]) * right[i];
            g += gS;
        }
        meter.peakL = peakL;
        meter.peakR = peakR;
        meter.rmsL = static_cast<float> (std::sqrt (sumL / numSamples));
        meter.rmsR = static_cast<float> (std::sqrt (sumR / numSamples));
    }
}
} // namespace lumen
