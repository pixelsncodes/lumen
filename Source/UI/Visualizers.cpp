#include "UI/Visualizers.h"

#include "Engine/FactoryTables.h"
#include "Engine/ModDestinations.h"
#include "State/ModState.h"
#include "UI/Theme.h"

#include <cmath>

using namespace lumen;

namespace
{
    constexpr float kMinDb = -90.0f;
    constexpr float kMaxDb = 0.0f;

    bool destHasModulation (const mod::Config* config, int dest)
    {
        if (config == nullptr || dest < 0)
            return false;
        for (const auto& slot : config->slots)
            if (slot.enabled && slot.dest == dest)
                return true;
        for (const auto& maps : config->macroMaps)
            for (const auto& map : maps)
                if (map.dest == dest)
                    return true;
        return false;
    }

    void drawWell (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (theme::well);
        g.fillRoundedRectangle (bounds, theme::wellRadius);
        g.setColour (theme::hairline);
        g.drawRoundedRectangle (bounds, theme::wellRadius, 1.0f);
    }
} // namespace

// ---------------------------------------------------------------------------
// AudioHistory
// ---------------------------------------------------------------------------

void AudioHistory::push (const float* data, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        buffer[writePos] = data[i];
        writePos = (writePos + 1) & (kSize - 1);
    }
    total += static_cast<juce::uint64> (numSamples);
}

void AudioHistory::latest (float* dest, int numSamples) const noexcept
{
    int readPos = (writePos - numSamples) & (kSize - 1);
    for (int i = 0; i < numSamples; ++i)
    {
        dest[i] = buffer[readPos];
        readPos = (readPos + 1) & (kSize - 1);
    }
}

// ---------------------------------------------------------------------------
// WavetableStackView
// ---------------------------------------------------------------------------

WavetableStackView::WavetableStackView (const UiShared& sharedContext,
                                        const juce::String& tableParamID,
                                        const juce::String& morphParamID,
                                        juce::Colour accentColour)
    : shared (sharedContext), accent (accentColour)
{
    tableValue = shared.apvts().getRawParameterValue (tableParamID);
    morphValue = shared.apvts().getRawParameterValue (morphParamID);
    morphDest = modstate::destFromToken (morphParamID);
    setInterceptsMouseClicks (false, false);
}

float WavetableStackView::displayedMorph() const
{
    // Base knob value normally; the live modulated value once the morph is
    // actually a modulation target (morph range is 0..1 linear, so the
    // normalized tap value IS the morph).
    if (destHasModulation (shared.processor.currentModConfig(), morphDest))
        return shared.tap.destNorm[morphDest].load (std::memory_order_relaxed);
    return morphValue != nullptr ? morphValue->load() : 0.0f;
}

void WavetableStackView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawWell (g, bounds);

    const auto& table = factory::forIndex (tableValue != nullptr
                                               ? juce::roundToInt (tableValue->load()) : 0);
    if (table.isEmpty())
        return;

    const int numFrames = table.getNumFrames();
    const float morph = juce::jlimit (0.0f, 1.0f, displayedMorph());
    const int currentFrame = juce::roundToInt (morph * static_cast<float> (numFrames - 1));

    const auto inner = bounds.reduced (10.0f, 8.0f);
    constexpr int kStride = 2;   // draw every 2nd frame (32 of 64)
    constexpr int kPoints = 96;

    // Perspective: frame 0 front bottom-left, higher frames recede up-right.
    const float depthX = inner.getWidth() * 0.28f;
    const float depthY = inner.getHeight() * 0.42f;
    const float waveW = inner.getWidth() - depthX;
    const float amp = inner.getHeight() * 0.20f;

    auto framePath = [&] (int frame)
    {
        const float t = static_cast<float> (frame) / static_cast<float> (numFrames - 1);
        const float ox = inner.getX() + depthX * t;
        const float oy = inner.getBottom() - amp - depthY * t;
        const float* data = table.frameData (frame, 0);

        juce::Path path;
        for (int p = 0; p < kPoints; ++p)
        {
            const float px = static_cast<float> (p) / (kPoints - 1);
            const int idx = juce::jmin (Wavetable::kFrameLength - 1,
                                        static_cast<int> (px * Wavetable::kFrameLength));
            const float y = oy - data[idx] * amp;
            if (p == 0)
                path.startNewSubPath (ox, y);
            else
                path.lineTo (ox + px * waveW, y);
        }
        return path;
    };

    // Back-to-front, deeper frames dimmer.
    for (int frame = numFrames - 1; frame >= 0; frame -= kStride)
    {
        if (frame == currentFrame)
            continue;
        const float t = static_cast<float> (frame) / static_cast<float> (numFrames - 1);
        g.setColour (accent.withAlpha (0.10f + 0.14f * (1.0f - t)));
        g.strokePath (framePath (frame), juce::PathStrokeType (1.0f));
    }

    g.setColour (accent);
    g.strokePath (framePath (currentFrame), juce::PathStrokeType (1.8f));
}

void WavetableStackView::animate()
{
    const float morph = displayedMorph();
    const int tableIndex = tableValue != nullptr ? juce::roundToInt (tableValue->load()) : 0;
    if (std::abs (morph - lastMorph) > 0.002f || tableIndex != lastTable)
    {
        lastMorph = morph;
        lastTable = tableIndex;
        repaint();
    }
}

// ---------------------------------------------------------------------------
// SpectrumView
// ---------------------------------------------------------------------------

SpectrumView::SpectrumView (const UiShared& sharedContext, const AudioHistory& historyRef)
    : shared (sharedContext), history (historyRef)
{
    for (int i = 0; i < kFftSize; ++i)
        window[i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                            * static_cast<float> (i) / kFftSize);
    for (int b = 0; b < kNumBins; ++b)
    {
        magnitudesDb[b] = kMinDb;
        peakHoldDb[b] = kMinDb;
    }

    modeValue = shared.apvts().getRawParameterValue ("filterMode");

    auto& apvts = shared.apvts();
    if (auto* parameter = apvts.getParameter ("filterCutoff"))
    {
        cutoffNatural = parameter->convertFrom0to1 (parameter->getValue());
        cutoffAttachment = std::make_unique<juce::ParameterAttachment> (*parameter,
            [this] (float v) { cutoffNatural = v; repaint(); });
    }
    if (auto* parameter = apvts.getParameter ("filterRes"))
    {
        resNatural = parameter->convertFrom0to1 (parameter->getValue());
        resAttachment = std::make_unique<juce::ParameterAttachment> (*parameter,
            [this] (float v) { resNatural = v; repaint(); });
    }
    shared.registerAttachment ("filterCutoff");
    shared.registerAttachment ("filterRes");
}

float SpectrumView::xToHz (float x) const
{
    const float w = juce::jmax (1.0f, (float) getWidth() - 16.0f);
    const float t = juce::jlimit (0.0f, 1.0f, (x - 8.0f) / w);
    return 20.0f * std::pow (1000.0f, t);
}

float SpectrumView::hzToX (float hz) const
{
    const float w = juce::jmax (1.0f, (float) getWidth() - 16.0f);
    const float t = std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f);
    return 8.0f + t * w;
}

float SpectrumView::dbToY (float db) const
{
    const float h = juce::jmax (1.0f, (float) getHeight() - 12.0f);
    // Axis is -90..0 dB with a little headroom above 0 for resonant peaks.
    const float t = (juce::jlimit (kMinDb, 12.0f, db) - kMinDb) / (12.0f - kMinDb);
    return 6.0f + h * (1.0f - t);
}

void SpectrumView::liveFilterValues (float& cutoffHz, float& res, int& mode) const
{
    static const float cutoffExp = mod::skewExponent (mod::Dest::filterCutoff);
    static const float resExp = mod::skewExponent (mod::Dest::filterRes);

    cutoffHz = cutoffNatural;
    res = resNatural;
    const auto* config = shared.processor.currentModConfig();
    if (destHasModulation (config, static_cast<int> (mod::Dest::filterCutoff)))
        cutoffHz = mod::denormalize (mod::Dest::filterCutoff,
            shared.tap.destNorm[static_cast<int> (mod::Dest::filterCutoff)].load (std::memory_order_relaxed),
            cutoffExp);
    if (destHasModulation (config, static_cast<int> (mod::Dest::filterRes)))
        res = mod::denormalize (mod::Dest::filterRes,
            shared.tap.destNorm[static_cast<int> (mod::Dest::filterRes)].load (std::memory_order_relaxed),
            resExp);
    mode = modeValue != nullptr ? juce::roundToInt (modeValue->load()) : 1;
}

float SpectrumView::filterResponseDb (float hz) const
{
    float cutoffHz = 0.0f, res = 0.0f;
    int mode = 1;
    liveFilterValues (cutoffHz, res, mode);

    double sr = shared.processor.getSampleRate();
    if (sr <= 0.0)
        sr = 48000.0;

    // TPT SVF = bilinear transform of the analog prototype, so the drawn
    // response uses the analog transfer function on the prewarped axis —
    // identical to the live coefficients in Engine/SVF.h.
    const float pi = juce::MathConstants<float>::pi;
    const float g = std::tan (pi * juce::jlimit (20.0f, 20000.0f, cutoffHz) / (float) sr);
    const float w = std::tan (pi * juce::jlimit (1.0f, (float) sr * 0.499f, hz) / (float) sr);
    const float q = 0.5f * std::exp2 (4.585f * juce::jlimit (0.0f, 1.0f, res));
    const float k = juce::jmax (1.0f / 12.0f, 1.0f / q);

    const float s = w / g; // normalized jw
    auto stageMag = [s] (float damping, int m) -> float
    {
        const float re = 1.0f - s * s;       // denominator: (1 - s^2) + j*k*s
        const float im = damping * s;
        const float den = std::sqrt (re * re + im * im);
        switch (m)
        {
            case 2:  return (s * s) / den;             // HP12: |s^2| / |den|
            case 3:  return (damping * s) / den;       // BP12 unity: |k s| / |den|
            case 4:  return std::abs (1.0f - s * s) / den; // notch: |1 + s^2| / |den|
            default: return 1.0f / den;                // LP
        }
    };

    float magnitude = stageMag (k, mode == 4 ? 4 : (mode == 2 ? 2 : (mode == 3 ? 3 : 0)));
    if (mode == 1) // LP24: second Butterworth-damped stage (Engine/SVF.h)
        magnitude *= stageMag (1.41421356f, 0);

    return 20.0f * std::log10 (juce::jmax (1.0e-5f, magnitude));
}

juce::Point<float> SpectrumView::nodePosition() const
{
    float cutoffHz = 0.0f, res = 0.0f;
    int mode = 1;
    liveFilterValues (cutoffHz, res, mode);
    return { hzToX (cutoffHz), dbToY (filterResponseDb (cutoffHz)) };
}

void SpectrumView::animate (bool computeFft)
{
    if (computeFft && history.totalPushed() != lastTotal)
    {
        lastTotal = history.totalPushed();

        float samples[kFftSize];
        history.latest (samples, kFftSize);
        for (int i = 0; i < kFftSize; ++i)
            fftData[i] = samples[i] * window[i];
        std::fill (fftData + kFftSize, fftData + kFftSize * 2, 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData);

        const float scale = 4.0f / kFftSize; // Hann coherent gain 0.5 -> 2/N * 2
        for (int b = 0; b < kNumBins; ++b)
        {
            const float db = 20.0f * std::log10 (juce::jmax (1.0e-9f, fftData[b] * scale));
            // Fast attack, slow decay, plus a peak-hold trace.
            magnitudesDb[b] = db > magnitudesDb[b]
                ? db : juce::jmax (kMinDb, magnitudesDb[b] - 2.5f);
            peakHoldDb[b] = db > peakHoldDb[b]
                ? db : juce::jmax (kMinDb, peakHoldDb[b] - 0.35f);
        }
    }
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawWell (g, bounds);

    double sr = shared.processor.getSampleRate();
    if (sr <= 0.0)
        sr = 48000.0;

    // Grid: octave-ish frequency lines + 30 dB lines.
    g.setColour (theme::hairline.withAlpha (0.6f));
    for (const float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine (juce::roundToInt (hzToX (f)), 6.0f, bounds.getHeight() - 6.0f);
    for (const float db : { -60.0f, -30.0f, 0.0f })
        g.drawHorizontalLine (juce::roundToInt (dbToY (db)), 8.0f, bounds.getWidth() - 8.0f);

    // Spectrum + peak hold.
    const int columns = juce::jmax (32, (int) bounds.getWidth() - 16);
    auto binDb = [&] (const float* data, float hz)
    {
        const float bin = juce::jlimit (0.0f, (float) (kNumBins - 2),
                                        hz / (float) (sr * 0.5) * (float) kNumBins);
        const int b0 = (int) bin;
        return juce::jmap (bin - (float) b0, data[b0], data[b0 + 1]);
    };

    juce::Path spectrumPath, holdPath;
    for (int c = 0; c < columns; ++c)
    {
        const float x = 8.0f + (float) c / (float) (columns - 1) * (bounds.getWidth() - 16.0f);
        const float hz = xToHz (x);
        const float y = dbToY (binDb (magnitudesDb, hz));
        const float yHold = dbToY (binDb (peakHoldDb, hz));
        if (c == 0)
        {
            spectrumPath.startNewSubPath (x, y);
            holdPath.startNewSubPath (x, yHold);
        }
        else
        {
            spectrumPath.lineTo (x, y);
            holdPath.lineTo (x, yHold);
        }
    }

    g.setColour (theme::textMuted.withAlpha (0.5f));
    g.strokePath (holdPath, juce::PathStrokeType (1.0f));

    juce::Path fill (spectrumPath);
    fill.lineTo (bounds.getWidth() - 8.0f, bounds.getHeight() - 6.0f);
    fill.lineTo (8.0f, bounds.getHeight() - 6.0f);
    fill.closeSubPath();
    g.setColour (theme::accentFilter.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (theme::accentFilter.withAlpha (0.75f));
    g.strokePath (spectrumPath, juce::PathStrokeType (1.2f));

    // Filter magnitude response from the live coefficients.
    juce::Path curve;
    constexpr int kCurvePoints = 96;
    for (int p = 0; p < kCurvePoints; ++p)
    {
        const float x = 8.0f + (float) p / (kCurvePoints - 1) * (bounds.getWidth() - 16.0f);
        const float y = dbToY (filterResponseDb (xToHz (x)));
        if (p == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }
    g.setColour (theme::accentFilter);
    g.strokePath (curve, juce::PathStrokeType (1.8f));

    // Draggable cutoff/resonance node.
    const auto node = nodePosition();
    const float r = (nodeHover || draggingNode) ? 6.5f : 5.0f;
    g.setColour (theme::accentFilter.withAlpha (0.25f));
    g.fillEllipse (node.x - r - 3.0f, node.y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
    g.setColour (theme::accentFilter);
    g.fillEllipse (node.x - r, node.y - r, r * 2.0f, r * 2.0f);
    g.setColour (theme::well);
    g.fillEllipse (node.x - r * 0.45f, node.y - r * 0.45f, r * 0.9f, r * 0.9f);
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    if (nodePosition().getDistanceFrom (e.position) < 14.0f)
    {
        draggingNode = true;
        dragStartRes = resNatural;
        dragStartY = e.position.y;
        if (cutoffAttachment != nullptr)
            cutoffAttachment->beginGesture();
        if (resAttachment != nullptr)
            resAttachment->beginGesture();
    }
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingNode)
        return;

    const float hz = juce::jlimit (20.0f, 20000.0f, xToHz (e.position.x));
    if (cutoffAttachment != nullptr)
        cutoffAttachment->setValueAsPartOfGesture (hz);

    const float res = juce::jlimit (0.0f, 1.0f,
        dragStartRes + (dragStartY - e.position.y) / ((float) getHeight() * 0.7f));
    if (resAttachment != nullptr)
        resAttachment->setValueAsPartOfGesture (res);
    repaint();
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    if (! draggingNode)
        return;
    draggingNode = false;
    if (cutoffAttachment != nullptr)
        cutoffAttachment->endGesture();
    if (resAttachment != nullptr)
        resAttachment->endGesture();
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const bool over = nodePosition().getDistanceFrom (e.position) < 14.0f;
    if (over != nodeHover)
    {
        nodeHover = over;
        setMouseCursor (over ? juce::MouseCursor::PointingHandCursor
                             : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

// ---------------------------------------------------------------------------
// WaterfallView (WATERFALL_SPEC.md)
// ---------------------------------------------------------------------------

WaterfallView::WaterfallView (const UiShared& sharedContext, const AudioHistory& historyRef)
    : shared (sharedContext), history (historyRef)
{
    setInterceptsMouseClicks (false, false);
    // Reserve once; Path::clear() keeps the storage afterwards (spec section 7).
    ridge.preallocateSpace (WaterfallModel::kColumns * 3 + 8);
    fillPath.preallocateSpace (WaterfallModel::kColumns * 3 + 16);
}

void WaterfallView::animate (bool audioActive)
{
    const double sr = shared.processor.getSampleRate();
    const double effectiveRate = sr > 0.0 ? sr : 48000.0;
    if (effectiveRate != preparedRate)
    {
        preparedRate = effectiveRate;
        model.prepare (effectiveRate);
        rebuildFloorImage(); // fHi (top tick span) depends on the sample rate
    }

    if (audioActive)
    {
        history.latest (sampleBuf, WaterfallModel::kFftSize);
        model.pushFrame (sampleBuf);
    }
    else
    {
        model.pushSilent();
    }
    repaint();
}

void WaterfallView::resized()
{
    rebuildFloorImage();
}

// Floor + labels cached to an image, regenerated on resize/sr change only
// (spec section 7). Everything is derived from w/h (spec sections 3-4).
void WaterfallView::rebuildFloorImage()
{
    const int wi = getWidth(), hi = getHeight();
    if (wi <= 0 || hi <= 0)
        return;

    floorImage = juce::Image (juce::Image::ARGB, wi, hi, true);
    juce::Graphics g (floorImage);

    const float w = (float) wi, h = (float) hi;
    const float cx = w / 2.0f, halfW = w * 0.47f, groundY = h * 0.84f;

    // Baseline: 1 px at groundY, accent @ 0.28, spanning cx +/- halfW.
    g.setColour (waterfall::accent.withAlpha (0.28f));
    g.fillRect (juce::Rectangle<float> (cx - halfW, groundY, halfW * 2.0f, 1.0f));

    // Ticks: 1-2-5 series within [fLo, fHi], height h*0.018 below the baseline.
    const float fLo = WaterfallModel::kFreqLo, fHi = model.freqHi();
    const float logLo = std::log (fLo), logRange = std::log (fHi) - std::log (fLo);
    const float tickH = h * 0.018f;
    auto tickX = [&] (float f)
    {
        return cx + ((std::log (f) - logLo) / logRange - 0.5f) * halfW * 2.0f;
    };
    for (const float f : { 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        if (f >= fLo && f <= fHi)
        {
            g.setColour (waterfall::accent.withAlpha (0.16f));
            g.fillRect (juce::Rectangle<float> (tickX (f) - 0.5f, groundY + 1.0f, 1.0f, tickH));
        }

    // Labels: 100 / 1k / 10k centered under their ticks, "Hz" right-aligned
    // at cx + halfW. Mono face (DECISIONS.md: system default monospace — the
    // UI has no embedded mono face), size max(8, h*0.028).
    const float fontSize = juce::jmax (8.0f, h * 0.028f);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              fontSize, juce::Font::plain)));
    const int labelY = juce::roundToInt (groundY + 1.0f + tickH + 2.0f);
    const int labelH = juce::roundToInt (fontSize + 2.0f);
    const std::pair<const char*, float> labels[] = { { "100", 100.0f }, { "1k", 1000.0f },
                                                     { "10k", 10000.0f } };
    g.setColour (waterfall::accent.withAlpha (0.5f));
    for (const auto& [text, f] : labels)
        if (f >= fLo && f <= fHi)
            g.drawText (text, juce::roundToInt (tickX (f)) - 30, labelY, 60, labelH,
                        juce::Justification::centred);
    g.setColour (waterfall::accent.withAlpha (0.65f));
    g.drawText ("Hz", juce::roundToInt (cx + halfW) - 60, labelY, 60, labelH,
                juce::Justification::centredRight);
}

void WaterfallView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (waterfall::background);
    g.fillRoundedRectangle (bounds, theme::wellRadius);

    // Floor/axis before the rows (spec section 4).
    if (floorImage.isValid())
        g.drawImageAt (floorImage, 0, 0);

    // Projection constants (spec section 3), all relative to w/h.
    const float w = bounds.getWidth(), h = bounds.getHeight();
    const float cx = w / 2.0f, halfW = w * 0.47f;
    const float groundY = h * 0.84f, horizonY = h * 0.30f;
    constexpr float kGain = 1.2f; // clamp any future gain parameter to <= 2.2
    const float hScale = h * 0.36f * kGain;

    constexpr int kRows = WaterfallModel::kRows;
    constexpr int kColumns = WaterfallModel::kColumns;

    // Back to front; each opaque fill occludes the rows behind it.
    for (int j = kRows - 1; j >= 0; --j)
    {
        const float depth = (float) j / (float) (kRows - 1);
        const float scale = 1.0f - depth * 0.42f;
        const float baseY = groundY + (horizonY - groundY) * depth;
        const float* row = model.row (j);

        float minY = baseY;
        for (int c = 0; c < kColumns; ++c)
        {
            const float n = (float) c / (float) (kColumns - 1);
            float hv = row[c];
            hv = hv * hv * 0.55f + hv * 0.45f; // soft expander — do not omit
            xs[c] = cx + (n - 0.5f) * halfW * 2.0f * scale;
            ys[c] = baseY - hv * hScale * scale;
            minY = juce::jmin (minY, ys[c]);
        }

        ridge.clear();
        fillPath.clear();
        ridge.startNewSubPath (xs[0], ys[0]);
        fillPath.startNewSubPath (xs[0], ys[0]);
        for (int c = 1; c < kColumns; ++c)
        {
            ridge.lineTo (xs[c], ys[c]);
            fillPath.lineTo (xs[c], ys[c]);
        }
        fillPath.lineTo (xs[kColumns - 1], baseY);
        fillPath.lineTo (xs[0], baseY);
        fillPath.closeSubPath();

        // 1. FILL — fully opaque vertical gradient; this is the occlusion.
        const auto topCol = waterfall::mix (waterfall::accent, waterfall::dim, depth * 0.55f);
        juce::ColourGradient gradient (topCol, 0.0f, juce::jmin (minY, baseY - 1.0f),
                                       waterfall::darkBase, 0.0f, baseY, false);
        gradient.addColour (0.5, waterfall::mix (topCol, waterfall::darkBase, 0.7f));
        g.setGradientFill (gradient);
        g.fillPath (fillPath);

        // 2. GLOW stroke, front half only.
        if (depth < 0.5f)
        {
            g.setColour (waterfall::accent.withAlpha (0.14f * (1.0f - depth * 2.0f)));
            g.strokePath (ridge, juce::PathStrokeType (juce::jmax (2.5f, h * 0.013f)));
        }

        // 3. MAIN stroke — near-white front fading to pure accent at the back.
        g.setColour (waterfall::mix (waterfall::bright, waterfall::accent, depth * 0.5f));
        g.strokePath (ridge, juce::PathStrokeType (juce::jmax (1.0f, h * 0.004f)));
    }

    g.setColour (theme::hairline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), theme::wellRadius, 1.0f);
}

// ---------------------------------------------------------------------------
// ScopeView
// ---------------------------------------------------------------------------

ScopeView::ScopeView (const AudioHistory& historyRef, juce::Colour accentColour)
    : history (historyRef), accent (accentColour)
{
    setInterceptsMouseClicks (false, false);
}

void ScopeView::animate()
{
    if (history.totalPushed() != lastTotal)
    {
        lastTotal = history.totalPushed();
        repaint();
    }
}

void ScopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawWell (g, bounds);

    constexpr int kWindow = 2048;
    float samples[kWindow * 2];
    history.latest (samples, kWindow * 2);

    // Rising zero-cross trigger in the first half for a stable trace.
    int trigger = 0;
    for (int i = 1; i < kWindow; ++i)
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f)
        {
            trigger = i;
            break;
        }

    const auto inner = bounds.reduced (6.0f, 5.0f);
    const float midY = inner.getCentreY();

    g.setColour (theme::hairline);
    g.drawHorizontalLine (juce::roundToInt (midY), inner.getX(), inner.getRight());

    juce::Path trace;
    const int points = juce::jmax (32, (int) inner.getWidth());
    for (int p = 0; p < points; ++p)
    {
        const float t = static_cast<float> (p) / (points - 1);
        const int idx = trigger + static_cast<int> (t * (kWindow - 1));
        const float y = midY - juce::jlimit (-1.0f, 1.0f, samples[idx]) * inner.getHeight() * 0.48f;
        const float x = inner.getX() + t * inner.getWidth();
        if (p == 0)
            trace.startNewSubPath (x, y);
        else
            trace.lineTo (x, y);
    }
    g.setColour (accent);
    g.strokePath (trace, juce::PathStrokeType (1.3f));
}

// ---------------------------------------------------------------------------
// EnvelopeEditor
// ---------------------------------------------------------------------------

EnvelopeEditor::EnvelopeEditor (const UiShared& sharedContext, int envIndex, juce::Colour accentColour)
    : shared (sharedContext), env (envIndex), accent (accentColour)
{
    const juce::String prefix = "env" + juce::String (env + 1);
    auto& apvts = shared.apvts();

    attackValue  = apvts.getRawParameterValue (prefix + "Attack");
    decayValue   = apvts.getRawParameterValue (prefix + "Decay");
    sustainValue = apvts.getRawParameterValue (prefix + "Sustain");
    releaseValue = apvts.getRawParameterValue (prefix + "Release");
    curveValue   = apvts.getRawParameterValue (prefix + "Curve");

    auto makeAttachment = [&] (const juce::String& id)
    {
        auto* parameter = apvts.getParameter (id);
        shared.registerAttachment (id);
        return std::make_unique<juce::ParameterAttachment> (*parameter,
            [this] (float) { repaint(); });
    };
    attackAttachment  = makeAttachment (prefix + "Attack");
    decayAttachment   = makeAttachment (prefix + "Decay");
    sustainAttachment = makeAttachment (prefix + "Sustain");
    releaseAttachment = makeAttachment (prefix + "Release");
}

float EnvelopeEditor::widthFracToSeconds (float frac, float maxSeconds) const
{
    const float t = juce::jmax (0.0f, (frac - 0.05f) / 0.9f);
    return juce::jlimit (0.001f, maxSeconds, maxSeconds * t * t);
}

float EnvelopeEditor::secondsToWidthFrac (float seconds, float maxSeconds) const
{
    return 0.05f + 0.9f * std::sqrt (juce::jlimit (0.0f, 1.0f, seconds / maxSeconds));
}

EnvelopeEditor::Layout EnvelopeEditor::computeLayout() const
{
    Layout l;
    l.area = getLocalBounds().toFloat().reduced (8.0f, 7.0f);

    // Segment widths: sqrt-scaled times + a fixed sustain shelf, normalized.
    const float wA = secondsToWidthFrac (attackValue->load(), 10.0f);
    const float wD = secondsToWidthFrac (decayValue->load(), 10.0f);
    const float wR = secondsToWidthFrac (releaseValue->load(), 15.0f);
    const float wS = 0.30f;
    const float sum = wA + wD + wS + wR;

    l.xAttack = l.area.getX() + l.area.getWidth() * (wA / sum);
    l.xDecay = l.xAttack + l.area.getWidth() * (wD / sum);
    l.xSustainEnd = l.xDecay + l.area.getWidth() * (wS / sum);
    l.xRelease = l.area.getRight();
    return l;
}

juce::Point<float> EnvelopeEditor::nodePoint (Node node, const Layout& l) const
{
    const float sustain = sustainValue->load();
    const float yTop = l.area.getY();
    const float yBottom = l.area.getBottom();
    const float ySustain = yBottom - sustain * (yBottom - yTop);

    switch (node)
    {
        case Node::attack:  return { l.xAttack, yTop };
        case Node::decay:   return { l.xDecay, ySustain };
        case Node::sustain: return { (l.xDecay + l.xSustainEnd) * 0.5f, ySustain };
        case Node::release: return { l.xRelease, yBottom };
        case Node::none:    break;
    }
    return {};
}

EnvelopeEditor::Node EnvelopeEditor::hitTest (juce::Point<float> position) const
{
    const auto l = computeLayout();
    for (const auto node : { Node::attack, Node::decay, Node::sustain, Node::release })
        if (nodePoint (node, l).getDistanceFrom (position) < 11.0f)
            return node;
    return Node::none;
}

void EnvelopeEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawWell (g, bounds);

    const auto l = computeLayout();
    const float curve = curveValue->load();
    const float sustain = sustainValue->load();
    const float attackPow = 2.0f * std::pow (3.0f, -curve);
    const float decayPow = std::pow (3.0f, -curve);

    const float yTop = l.area.getY();
    const float yBottom = l.area.getBottom();
    auto levelToY = [yTop, yBottom] (float level)
    {
        return yBottom - level * (yBottom - yTop);
    };

    juce::Path path;
    path.startNewSubPath (l.area.getX(), yBottom);
    constexpr int kSegmentPoints = 24;
    for (int p = 1; p <= kSegmentPoints; ++p) // attack: x^p rise
    {
        const float t = static_cast<float> (p) / kSegmentPoints;
        path.lineTo (juce::jmap (t, l.area.getX(), l.xAttack),
                     levelToY (std::pow (t, attackPow)));
    }
    for (int p = 1; p <= kSegmentPoints; ++p) // decay: (1-x)^q fall to sustain
    {
        const float t = static_cast<float> (p) / kSegmentPoints;
        const float level = sustain + (1.0f - sustain) * std::pow (1.0f - t, decayPow);
        path.lineTo (juce::jmap (t, l.xAttack, l.xDecay), levelToY (level));
    }
    path.lineTo (l.xSustainEnd, levelToY (sustain));
    for (int p = 1; p <= kSegmentPoints; ++p) // release
    {
        const float t = static_cast<float> (p) / kSegmentPoints;
        path.lineTo (juce::jmap (t, l.xSustainEnd, l.xRelease),
                     levelToY (sustain * std::pow (1.0f - t, decayPow)));
    }

    juce::Path fill (path);
    fill.lineTo (l.area.getRight(), yBottom);
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (path, juce::PathStrokeType (1.6f));

    // Live envelope level from the newest voice (engine tap).
    const float live = shared.tap.sourceValue[env].load (std::memory_order_relaxed);
    if (live > 0.001f)
    {
        g.setColour (accent.withAlpha (0.45f));
        g.drawHorizontalLine (juce::roundToInt (levelToY (live)), l.area.getX(), l.area.getRight());
    }

    for (const auto node : { Node::attack, Node::decay, Node::sustain, Node::release })
    {
        const auto point = nodePoint (node, l);
        const bool hot = node == hoverNode || node == dragNode;
        const float r = hot ? 5.0f : 3.6f;
        g.setColour (hot ? theme::textPrimary : accent);
        g.fillEllipse (point.x - r, point.y - r, r * 2.0f, r * 2.0f);
    }
}

void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    dragNode = hitTest (e.position);
    switch (dragNode)
    {
        case Node::attack:  attackAttachment->beginGesture(); break;
        case Node::decay:   decayAttachment->beginGesture();
                            sustainAttachment->beginGesture(); break;
        case Node::sustain: sustainAttachment->beginGesture(); break;
        case Node::release: releaseAttachment->beginGesture(); break;
        case Node::none:    break;
    }
}

void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode == Node::none)
        return;

    const auto l = computeLayout();
    const float width = l.area.getWidth();
    const float sustainFromY = juce::jlimit (0.0f, 1.0f,
        (l.area.getBottom() - e.position.y) / juce::jmax (1.0f, l.area.getHeight()));

    // Convert the dragged x into this segment's width fraction of the well,
    // then invert the sqrt time scaling. Segment shares shift as times
    // change; using the current layout keeps the drag stable enough.
    const float wS = 0.30f;
    const float wA = secondsToWidthFrac (attackValue->load(), 10.0f);
    const float wD = secondsToWidthFrac (decayValue->load(), 10.0f);
    const float wR = secondsToWidthFrac (releaseValue->load(), 15.0f);
    const float sum = wA + wD + wS + wR;

    if (dragNode == Node::attack)
    {
        const float frac = (e.position.x - l.area.getX()) / width * sum;
        attackAttachment->setValueAsPartOfGesture (widthFracToSeconds (frac, 10.0f));
    }
    else if (dragNode == Node::decay)
    {
        const float frac = (e.position.x - l.xAttack) / width * sum;
        decayAttachment->setValueAsPartOfGesture (widthFracToSeconds (frac, 10.0f));
        sustainAttachment->setValueAsPartOfGesture (sustainFromY);
    }
    else if (dragNode == Node::sustain)
    {
        sustainAttachment->setValueAsPartOfGesture (sustainFromY);
    }
    else if (dragNode == Node::release)
    {
        // Release's right edge is pinned; drag left edge = sustain shelf end.
        const float frac = (l.xRelease - e.position.x) / width * sum;
        releaseAttachment->setValueAsPartOfGesture (widthFracToSeconds (frac, 15.0f));
    }
    repaint();
}

void EnvelopeEditor::mouseUp (const juce::MouseEvent&)
{
    switch (dragNode)
    {
        case Node::attack:  attackAttachment->endGesture(); break;
        case Node::decay:   decayAttachment->endGesture();
                            sustainAttachment->endGesture(); break;
        case Node::sustain: sustainAttachment->endGesture(); break;
        case Node::release: releaseAttachment->endGesture(); break;
        case Node::none:    break;
    }
    dragNode = Node::none;
    repaint();
}

void EnvelopeEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto over = hitTest (e.position);
    if (over != hoverNode)
    {
        hoverNode = over;
        setMouseCursor (over != Node::none ? juce::MouseCursor::PointingHandCursor
                                           : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EnvelopeEditor::animate()
{
    const float live = shared.tap.sourceValue[env].load (std::memory_order_relaxed);
    const float params[5] = { attackValue->load(), decayValue->load(), sustainValue->load(),
                              releaseValue->load(), curveValue->load() };
    bool changed = std::abs (live - lastLive) > 0.004f;
    for (int i = 0; i < 5; ++i)
        changed = changed || params[i] != lastParams[i];
    if (changed)
    {
        lastLive = live;
        for (int i = 0; i < 5; ++i)
            lastParams[i] = params[i];
        repaint();
    }
}

// ---------------------------------------------------------------------------
// LfoView
// ---------------------------------------------------------------------------

LfoView::LfoView (const UiShared& sharedContext, int lfoIndex, juce::Colour accentColour)
    : shared (sharedContext), lfo (lfoIndex), accent (accentColour)
{
    const juce::String prefix = "lfo" + juce::String (lfo + 1);
    shape = shared.apvts().getRawParameterValue (prefix + "Shape");
    phaseDeg = shared.apvts().getRawParameterValue (prefix + "Phase");
    setInterceptsMouseClicks (false, false);
}

float LfoView::shapeValue (float x) const
{
    x -= std::floor (x);
    switch (shape != nullptr ? juce::roundToInt (shape->load()) : 0)
    {
        case 0: return std::sin (juce::MathConstants<float>::twoPi * x);
        case 1: return x < 0.25f ? 4.0f * x : (x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f);
        case 2: return 2.0f * x - 1.0f;
        case 3: return 1.0f - 2.0f * x;
        case 4: return x < 0.5f ? 1.0f : -1.0f;
        default:
        {
            // Display-only deterministic S&H staircase (8 steps).
            static constexpr float steps[8] = { 0.62f, -0.35f, 0.91f, 0.12f,
                                                -0.78f, 0.44f, -0.15f, -0.94f };
            return steps[juce::jlimit (0, 7, (int) (x * 8.0f))];
        }
    }
}

void LfoView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawWell (g, bounds);

    const auto inner = bounds.reduced (8.0f, 7.0f);
    const float midY = inner.getCentreY();
    const float offset = (phaseDeg != nullptr ? phaseDeg->load() : 0.0f) / 360.0f;

    g.setColour (theme::hairline);
    g.drawHorizontalLine (juce::roundToInt (midY), inner.getX(), inner.getRight());

    juce::Path path;
    constexpr int kPoints = 128;
    for (int p = 0; p < kPoints; ++p)
    {
        const float t = static_cast<float> (p) / (kPoints - 1);
        const float y = midY - shapeValue (t + offset) * inner.getHeight() * 0.42f;
        if (p == 0)
            path.startNewSubPath (inner.getX(), y);
        else
            path.lineTo (inner.getX() + t * inner.getWidth(), y);
    }
    g.setColour (accent);
    g.strokePath (path, juce::PathStrokeType (1.5f));

    // Live phase marker + value dot.
    const float phase = shared.tap.lfoPhase[lfo].load (std::memory_order_relaxed);
    const float px = inner.getX() + (phase - std::floor (phase)) * inner.getWidth();
    g.setColour (accent.withAlpha (0.35f));
    g.drawVerticalLine (juce::roundToInt (px), inner.getY(), inner.getBottom());
    const float py = midY - shapeValue (phase + offset) * inner.getHeight() * 0.42f;
    g.setColour (theme::textPrimary);
    g.fillEllipse (px - 2.5f, py - 2.5f, 5.0f, 5.0f);
}

void LfoView::animate()
{
    const float phase = shared.tap.lfoPhase[lfo].load (std::memory_order_relaxed);
    const float currentShape = shape != nullptr ? shape->load() : 0.0f;
    if (std::abs (phase - lastPhase) > 0.004f || currentShape != lastShape)
    {
        lastPhase = phase;
        lastShape = currentShape;
        repaint();
    }
}

// ---------------------------------------------------------------------------
// MeterView
// ---------------------------------------------------------------------------

MeterView::MeterView (const UiShared& sharedContext)
    : shared (sharedContext)
{
    setInterceptsMouseClicks (false, false);
}

void MeterView::animate()
{
    repaint();
}

void MeterView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawWell (g, bounds);

    const auto& levels = shared.processor.meterLevels();
    const float peaks[2] = { levels.peakL.load (std::memory_order_relaxed),
                             levels.peakR.load (std::memory_order_relaxed) };
    const float rms[2] = { levels.rmsL.load (std::memory_order_relaxed),
                           levels.rmsR.load (std::memory_order_relaxed) };

    auto toDb = [] (float linear)
    {
        return juce::jlimit (-60.0f, 0.0f, juce::Decibels::gainToDecibels (linear, -60.0f));
    };
    auto dbToFrac = [] (float db) { return (db + 60.0f) / 60.0f; };

    const auto now = juce::Time::getMillisecondCounter();
    auto inner = bounds.reduced (4.0f, 4.0f);
    const float barHeight = (inner.getHeight() - 2.0f) * 0.5f;

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto bar = juce::Rectangle<float> (inner.getX(),
                                                 inner.getY() + (barHeight + 2.0f) * (float) channel,
                                                 inner.getWidth(), barHeight);
        const float peakDb = toDb (peaks[channel]);

        // 300 ms peak hold (SPEC 15).
        if (peakDb >= holdDb[channel] || now - holdTime[channel] > 300)
        {
            holdDb[channel] = peakDb;
            holdTime[channel] = now;
        }

        g.setColour (theme::hairline);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (theme::accentFilter.withAlpha (0.55f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * dbToFrac (toDb (rms[channel]))), 2.0f);
        g.setColour (theme::accentFilter);
        const float peakX = bar.getX() + bar.getWidth() * dbToFrac (peakDb);
        g.fillRect (juce::Rectangle<float> (peakX - 1.0f, bar.getY(), 2.0f, bar.getHeight()));

        if (holdDb[channel] > -59.0f)
        {
            g.setColour (theme::textPrimary.withAlpha (0.8f));
            const float holdX = bar.getX() + bar.getWidth() * dbToFrac (holdDb[channel]);
            g.fillRect (juce::Rectangle<float> (holdX - 0.75f, bar.getY(), 1.5f, bar.getHeight()));
        }
    }
}
