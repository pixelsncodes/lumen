#pragma once

#include <juce_graphics/juce_graphics.h>

#include <cstdint>
#include <vector>

// Lens — the image-to-tone engine (SPEC section 13). Fully local and fully
// deterministic: results depend only on the image bytes. The one seed is an
// FNV-1a 64-bit hash over the downscaled ARGB pixels; every "random" phase
// derives from it through a fixed xorshift64* sequence. No rand(), no
// time(), no global state.
//
// This layer is pure analysis (juce_graphics + juce_dsp + stdlib). Table
// ownership/swapping lives in LensController (plugin) or the harness.

namespace lumen::lens
{
inline constexpr int kNumFrames = 64;
inline constexpr int kFrameLength = 2048;   // == Wavetable::kFrameLength
inline constexpr int kHarmonicCap = 700;    // mip-build cap for Lens tables (SPEC 13.3)
inline constexpr int kMaxHarmonics = 256;   // spectral mode harmonic count (SPEC 13.4)
inline constexpr float kPeakTarget = 0.9f;  // per-frame normalization target
inline constexpr int kThumbSize = 64;       // persisted thumbnail edge (SPEC 13)

enum class Mode { scan = 0, spectral = 1 };

// Decoded + downscaled source: `working` fits 512x512 (aspect kept, never
// upscaled), `analysis` is the exact 256x256 chroma copy (SPEC 13.1). The
// seed is hashed over the working image's dimensions + ARGB bytes.
struct Analysis
{
    juce::Image working;
    juce::Image chromaCopy;
    uint64_t seed = 0;
    bool valid = false;
};

Analysis analyzeImage (const juce::Image& source);
Analysis analyzeImageFile (const juce::File& file);
Analysis analyzeImageData (const void* data, size_t size);

// 64 frames x 2048 samples per SPEC 13.3 (scan) / 13.4 (spectral), each
// frame DC-free and peak-normalized to 0.9 (silence-guarded flat rows get
// 0.05 * sin). This float block is exactly what gets checksummed and stored
// in presets/DAW state.
std::vector<float> buildFrames (const Analysis& analysis, Mode mode);

// SPEC 13.5 statistics, computed on the 256x256 copy in HSV.
struct ChromaStats
{
    float hueMeanDeg = 0.0f; // Hm: saturation-weighted circular mean hue, degrees
    float satMean = 0.0f;    // Sm
    float valMean = 0.0f;    // Vm
    float lumaSigma = 0.0f;  // sigV: 2 * stddev(luma), clamped 0..1
    float edgeMean = 0.0f;   // E: mean Sobel magnitude / 4, clamped 0..1
    float hueSigma = 0.0f;   // sigH: circular hue dispersion (1 - R), 0..1
};

ChromaStats chromaStats (const Analysis& analysis);

// The exact SPEC 13.5 patch targets derived from the stats.
struct PatchTargets
{
    int   filterMode = 1;         // FilterMode index: LP24 / BP12 / LP12 by hue
    float cutoffHz = 20000.0f;    // 250 * 2^(5.5 * Vm)
    float res = 0.12f;            // 0.05 + 0.55 * Sm
    float detuneCents = 10.0f;    // 3 + 30 * Sm (target osc)
    int   unison = 1;             // 1 + round(5 * Sm)
    float attackSeconds = 0.005f; // 400 * (2/400)^Vm ms
    float releaseSeconds = 0.2f;  // 150 + 1350 * (1 - Vm) ms
    float driveDb = 0.0f;         // 20 * E (the Drive effect)
    float noiseDb = -60.0f;       // -60 + 42 * E
    float lfoDepth = 0.0f;        // LFO 1 -> target osc morph: 0.5 * sigH
    float lfoRateHz = 0.15f;      // 0.15 + 4 * sigV, sine, poly
    float reverbMix = 0.12f;      // 0.10 + 0.35 * (1 - E)
    float macro4 = 0.2f;          // Texture default = E
};

PatchTargets patchTargetsFor (const ChromaStats& stats);

// 64x64 thumbnail + PNG bytes for state storage (never a path, SPEC 13).
juce::Image makeThumbnail (const Analysis& analysis);
juce::MemoryBlock encodePng (const juce::Image& image);

// SHA-256 hex of arbitrary bytes (determinism checksums, juce_cryptography).
juce::String sha256Hex (const void* data, size_t size);
} // namespace lumen::lens
