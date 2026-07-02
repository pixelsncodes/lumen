#pragma once

#include <juce_dsp/juce_dsp.h>

#include "UI/Controls.h"

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
