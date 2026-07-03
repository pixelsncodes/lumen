#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>

namespace lumen
{
// Message-thread analyser + history ring behind the Play view 3D spectral
// waterfall (WATERFALL_SPEC.md section 2). Emulates the Web Audio
// AnalyserNode the reference renderer was written against: linear-magnitude
// temporal smoothing (kSmoothing), then dB, then normalization into [0, 1]
// against the -100..-30 dB window. Column c samples ONE nearest FFT bin
// (no bucket averaging) on a 30 Hz..min(18 kHz, sr/2 - 1 kHz) log axis.
//
// No juce_gui includes — compiled headless by lumen_tests. All state is
// preallocated; nothing here allocates after construction.
class WaterfallModel
{
public:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder;  // 2048
    static constexpr int kNumBins = kFftSize / 2;    // 1024
    static constexpr int kColumns = 120;             // DEPTH_C
    // DEPTH_R. The view advances one row every OTHER ~60 fps frame
    // (WATERFALL_SPEC sections 2/6), so 22 rows still cover ~0.73 s.
    static constexpr int kRows = 22;

    // 0.8 is critical: it is what makes the ridges undulate instead of
    // flicker (the AnalyserNode smoothingTimeConstant default).
    static constexpr float kSmoothing = 0.8f;
    static constexpr float kFreqLo = 30.0f;
    static constexpr float kDbFloor = -100.0f;
    static constexpr float kDbCeil = -30.0f;

    WaterfallModel() { prepare (48000.0); }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        fHi = (float) std::min (18000.0, sampleRate * 0.5 - 1000.0);

        for (int c = 0; c < kColumns; ++c)
        {
            const float f = kFreqLo * std::pow (fHi / kFreqLo,
                                                (float) c / (float) (kColumns - 1));
            const auto idx = (int) std::lround ((double) f * kFftSize / sampleRate);
            binIndex[c] = std::clamp (idx, 1, kNumBins - 1);
        }

        for (int i = 0; i < kFftSize; ++i)
            window[i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                * (float) i / (float) kFftSize);

        std::fill (std::begin (smooth), std::end (smooth), 0.0f);
        for (auto& row : rows)
            std::fill (std::begin (row), std::end (row), 0.0f);
        front = 0;
    }

    // One FFT per row advance (every other animation frame, spec section 6)
    // over the most recent kFftSize samples — overlapping windows are
    // expected and desired. Pushes a new row at the front of the ring.
    void pushFrame (const float* samples) noexcept
    {
        for (int i = 0; i < kFftSize; ++i)
            fftData[i] = samples[i] * window[i];
        std::fill (fftData + kFftSize, fftData + 2 * kFftSize, 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData);

        // mag = sqrt(re^2 + im^2) / (N/2), smoothed on LINEAR magnitudes.
        constexpr float kMagScale = 1.0f / (float) kNumBins;
        for (int b = 0; b < kNumBins; ++b)
            smooth[b] = kSmoothing * smooth[b] + (1.0f - kSmoothing) * fftData[b] * kMagScale;

        float* row = pushRow();
        for (int c = 0; c < kColumns; ++c)
        {
            const float db = 20.0f * std::log10 (std::max (smooth[binIndex[c]], 1.0e-10f));
            row[c] = std::clamp ((db - kDbFloor) / (kDbCeil - kDbFloor), 0.0f, 1.0f);
        }
    }

    // Audio inactive (WATERFALL_SPEC section 6): advance an all-zero row so
    // the surface drains toward the horizon, while the analyser state keeps
    // decaying exactly as processed silence would decay it.
    void pushSilent() noexcept
    {
        float* row = pushRow();
        std::fill (row, row + kColumns, 0.0f);
        for (auto& s : smooth)
            s *= kSmoothing;
    }

    // Row j: 0 = newest (front, widest) .. kRows-1 = oldest (horizon).
    const float* row (int j) const noexcept { return rows[(size_t) ((front + j) % kRows)]; }
    float value (int j, int c) const noexcept { return row (j)[c]; }

    int binIndexFor (int c) const noexcept { return binIndex[c]; }
    float columnFrequency (int c) const noexcept
    {
        return kFreqLo * std::pow (fHi / kFreqLo, (float) c / (float) (kColumns - 1));
    }
    float freqHi() const noexcept { return fHi; }
    double currentSampleRate() const noexcept { return sampleRate; }

private:
    float* pushRow() noexcept
    {
        front = (front + kRows - 1) % kRows;
        return rows[(size_t) front];
    }

    juce::dsp::FFT fft { kFftOrder };
    double sampleRate = 48000.0;
    float fHi = 18000.0f;
    int front = 0;
    int binIndex[kColumns] {};
    float window[kFftSize] {};
    float fftData[kFftSize * 2] {};
    float smooth[kNumBins] {};
    float rows[kRows][kColumns] {};
};
} // namespace lumen
