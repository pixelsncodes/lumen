#pragma once

#include <vector>

namespace lumen
{
// A wavetable: 1..256 frames of 2048 samples, each pre-rendered at 10
// band-limited mip levels (SPEC section 4). Level L keeps harmonics
// 1..min(1023, 1024 >> L). Frames store one extra wrap sample so the
// oscillator can linear-interpolate without a modulo.
class Wavetable
{
public:
    static constexpr int kFrameLength = 2048;
    static constexpr int kNumMips = 10;
    static constexpr int kStride = kFrameLength + 1;

    static constexpr int maxHarmonic (int mipLevel)
    {
        const int h = 1024 >> mipLevel;
        return h < 1023 ? h : 1023;
    }

    // sourceFrames = numFrames * 2048 floats, full-bandwidth. harmonicCap
    // trims the spectrum before the mip build (Lens uses 700, SPEC 13).
    // Every mip of a frame shares one normalization factor (level 0 peak ->
    // peakTarget) so mip transitions stay seamless.
    void build (const float* sourceFrames, int numFrames,
                int harmonicCap = 1023, float peakTarget = 1.0f);

    int getNumFrames() const noexcept { return numFrames; }
    bool isEmpty() const noexcept     { return numFrames == 0; }

    // 2049 floats: samples 0..2047 plus the wrap duplicate.
    const float* frameData (int frame, int mipLevel) const noexcept
    {
        return data.data() + (static_cast<size_t> (frame) * kNumMips
                              + static_cast<size_t> (mipLevel)) * kStride;
    }

    // Highest-resolution level whose top harmonic stays below 0.45 * sr
    // for a voice running at `cyclesPerSample` (SPEC section 4).
    static int mipForIncrement (double cyclesPerSample) noexcept
    {
        if (cyclesPerSample <= 0.0)
            return 0;

        const double maxAllowed = 0.45 / cyclesPerSample;
        for (int level = 0; level < kNumMips; ++level)
            if (static_cast<double> (maxHarmonic (level)) < maxAllowed)
                return level;

        return kNumMips - 1;
    }

private:
    int numFrames = 0;
    std::vector<float> data;
};
} // namespace lumen
