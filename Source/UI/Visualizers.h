#pragma once

#include <juce_dsp/juce_dsp.h>

#include "UI/Controls.h"
#include "UI/Theme.h"
#include "UI/WaterfallModel.h"

// SPEC section 15 visualizers. All audio data arrives through the
// processor's lock-free tap FIFO, drained once per UI frame by the editor
// into AudioHistory; the mod/level values come from atomics (UiTap /
// MeterAtomics). Nothing here ever touches the audio thread.

// Message-thread rolling history of the post-limiter mono signal.
class AudioHistory
{
public:
    static constexpr int kSize = 8192; // power of two

    void push (const float* data, int numSamples) noexcept;
    // Copies the most recent `numSamples` (oldest first) into dest.
    void latest (float* dest, int numSamples) const noexcept;
    juce::uint64 totalPushed() const noexcept { return total; }

private:
    float buffer[kSize] {};
    int writePos = 0;
    juce::uint64 total = 0;
};

// 3D wavetable stack: frame polylines with perspective offset, the current
// (possibly modulated) frame highlighted (SPEC 15).
class WavetableStackView final : public juce::Component
{
public:
    WavetableStackView (const UiShared& sharedContext, const juce::String& tableParamID,
                        const juce::String& morphParamID, juce::Colour accentColour);

    void paint (juce::Graphics& g) override;
    void animate();

private:
    float displayedMorph() const;

    UiShared shared;
    juce::Colour accent;
    std::atomic<float>* tableValue = nullptr;
    std::atomic<float>* morphValue = nullptr;
    int morphDest = -1;
    float lastMorph = -1.0f;
    int lastTable = -1;
};

// FFT spectrum (2048, Hann, log 20 Hz..20 kHz, -90..0 dB, peak hold) with
// the filter magnitude response drawn on top; the response node is
// draggable = cutoff/resonance (SPEC 15).
class SpectrumView final : public juce::Component
{
public:
    SpectrumView (const UiShared& sharedContext, const AudioHistory& historyRef);

    void paint (juce::Graphics& g) override;
    void animate (bool computeFft);

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder; // 2048
    static constexpr int kNumBins = kFftSize / 2;

    float xToHz (float x) const;
    float hzToX (float hz) const;
    float dbToY (float db) const;
    float filterResponseDb (float hz) const;
    void liveFilterValues (float& cutoffHz, float& res, int& mode) const;
    juce::Point<float> nodePosition() const;

    UiShared shared;
    const AudioHistory& history;

    juce::dsp::FFT fft { kFftOrder };
    float window[kFftSize];
    float fftData[kFftSize * 2] {};
    float magnitudesDb[kNumBins];
    float peakHoldDb[kNumBins];
    juce::uint64 lastTotal = 0;

    std::atomic<float>* modeValue = nullptr;
    std::unique_ptr<juce::ParameterAttachment> cutoffAttachment, resAttachment;
    float cutoffNatural = 20000.0f, resNatural = 0.12f;

    bool draggingNode = false, nodeHover = false;
    float dragStartRes = 0.0f;
    float dragStartY = 0.0f;
};

// WATERFALL_SPEC.md section 5 palette — neon yellow, exact values. Every
// colour derives from kAccentRgb via mix() (per-channel linear interp,
// half-to-even rounding), so retuning the whole surface is one hex change;
// the static_asserts pin the current derivation to the spec's table.
namespace lumen::waterfall
{
    constexpr juce::uint32 mixChannel (juce::uint32 a, juce::uint32 b, double t)
    {
        const double v = (double) a + ((double) b - (double) a) * t;
        const auto i = (juce::uint32) v;
        const double frac = v - (double) i;
        if (frac > 0.5) return i + 1;
        if (frac < 0.5) return i;
        return i % 2 == 0 ? i : i + 1;
    }

    constexpr juce::uint32 mixRgb (juce::uint32 a, juce::uint32 b, double t)
    {
        return (mixChannel ((a >> 16) & 0xff, (b >> 16) & 0xff, t) << 16)
             | (mixChannel ((a >> 8) & 0xff, (b >> 8) & 0xff, t) << 8)
             |  mixChannel (a & 0xff, b & 0xff, t);
    }

    // The one neon yellow: shared with the rest of the UI via theme::kAccentRgb.
    // The static_asserts pin the WATERFALL_SPEC section 5 table — retuning the
    // theme yellow means updating that table (and these asserts) too.
    constexpr juce::uint32 kAccentRgb   = theme::kAccentRgb;
    constexpr juce::uint32 kBrightRgb   = mixRgb (kAccentRgb, 0xFFFFFF, 0.5);
    constexpr juce::uint32 kDimRgb      = mixRgb (kAccentRgb, 0x04060A, 0.78);
    constexpr juce::uint32 kDarkBaseRgb = mixRgb (kDimRgb, 0x000000, 0.55);
    static_assert (kBrightRgb == 0xFCFF80, "WATERFALL_SPEC section 5: bright");
    static_assert (kDimRgb == 0x3A3D08, "WATERFALL_SPEC section 5: dim");
    static_assert (kDarkBaseRgb == 0x1A1B04, "WATERFALL_SPEC section 5: darkBase");

    inline const juce::Colour accent     { 0xff000000 | kAccentRgb };
    inline const juce::Colour bright     { 0xff000000 | kBrightRgb };
    inline const juce::Colour dim        { 0xff000000 | kDimRgb };
    inline const juce::Colour darkBase   { 0xff000000 | kDarkBaseRgb };
    inline const juce::Colour background { 0xff070708 };

    // Runtime per-channel linear interp for the depth-dependent paint mixes.
    inline juce::Colour mix (juce::Colour a, juce::Colour b, float t)
    {
        auto channel = [t] (juce::uint8 x, juce::uint8 y)
        {
            return (juce::uint8) juce::roundToInt ((float) x + ((float) y - (float) x) * t);
        };
        return { channel (a.getRed(), b.getRed()), channel (a.getGreen(), b.getGreen()),
                 channel (a.getBlue(), b.getBlue()) };
    }
} // namespace lumen::waterfall

// Play view 3D spectral waterfall (WATERFALL_SPEC.md): pseudo-3D ridgeline
// surface, newest spectrum row at the front, rows stacked with linear
// perspective and drawn back-to-front, each closed to its own baseline and
// filled opaquely (the opaque fill IS the hidden-line removal). Replaces the
// scope as the Play view "audio active" visual; the Deep view scope strip is
// unchanged.
class WaterfallView final : public juce::Component
{
public:
    WaterfallView (const UiShared& sharedContext, const AudioHistory& historyRef);

    void paint (juce::Graphics& g) override;
    void resized() override;
    // One history row per UI frame (spec section 6): an FFT of the latest
    // tap samples while audio is active, an all-zero drain row otherwise.
    void animate (bool audioActive);

private:
    void rebuildFloorImage();

    UiShared shared;
    const AudioHistory& history;
    lumen::WaterfallModel model;
    double preparedRate = 48000.0;

    // Preallocated scratch — paint never allocates (spec section 7); the
    // two Paths keep their storage across clear() calls.
    float sampleBuf[lumen::WaterfallModel::kFftSize] {};
    float xs[lumen::WaterfallModel::kColumns] {};
    float ys[lumen::WaterfallModel::kColumns] {};
    juce::Path ridge, fillPath;
    juce::Image floorImage; // baseline + ticks + labels, rebuilt on resize only
};

// Post-limiter scope, 2048-sample window, rising zero-cross trigger (SPEC 15).
class ScopeView final : public juce::Component
{
public:
    ScopeView (const AudioHistory& historyRef, juce::Colour accentColour);

    void paint (juce::Graphics& g) override;
    void animate();

private:
    const AudioHistory& history;
    juce::Colour accent;
    juce::uint64 lastTotal = 0;
};

// ADSR editor: curve redrawn from parameter values, draggable breakpoints
// write parameters with proper gestures (SPEC 15).
class EnvelopeEditor final : public juce::Component
{
public:
    EnvelopeEditor (const UiShared& sharedContext, int envIndex, juce::Colour accentColour);

    void paint (juce::Graphics& g) override;
    void animate();

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    enum class Node { none, attack, decay, sustain, release };

    struct Layout
    {
        float xAttack, xDecay, xSustainEnd, xRelease; // segment end x positions
        juce::Rectangle<float> area;
    };
    Layout computeLayout() const;
    Node hitTest (juce::Point<float> position) const;
    juce::Point<float> nodePoint (Node node, const Layout& l) const;
    float widthFracToSeconds (float frac, float maxSeconds) const;
    float secondsToWidthFrac (float seconds, float maxSeconds) const;

    UiShared shared;
    int env;
    juce::Colour accent;

    std::atomic<float>* attackValue = nullptr;
    std::atomic<float>* decayValue = nullptr;
    std::atomic<float>* sustainValue = nullptr;
    std::atomic<float>* releaseValue = nullptr;
    std::atomic<float>* curveValue = nullptr;

    std::unique_ptr<juce::ParameterAttachment> attackAttachment, decayAttachment,
                                               sustainAttachment, releaseAttachment;

    Node dragNode = Node::none, hoverNode = Node::none;
    float lastLive = -1.0f;
    float lastParams[5] { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
};

// LFO shape display with a live phase marker.
class LfoView final : public juce::Component
{
public:
    LfoView (const UiShared& sharedContext, int lfoIndex, juce::Colour accentColour);

    void paint (juce::Graphics& g) override;
    void animate();

private:
    float shapeValue (float x) const; // one display cycle, -1..1

    UiShared shared;
    int lfo;
    juce::Colour accent;
    std::atomic<float>* shape = nullptr;
    std::atomic<float>* phaseDeg = nullptr;
    float lastPhase = -1.0f;
    float lastShape = -1.0f;
};

// Stereo peak + RMS meter, 300 ms peak hold (SPEC 15).
class MeterView final : public juce::Component
{
public:
    explicit MeterView (const UiShared& sharedContext);

    void paint (juce::Graphics& g) override;
    void animate();

private:
    UiShared shared;
    float holdDb[2] { -60.0f, -60.0f };
    juce::uint32 holdTime[2] { 0, 0 };
};
