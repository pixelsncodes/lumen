// lumen_render — headless render/verification harness (SPEC section 18).
//
//   lumen_render --preset init --note 60 --vel 100 --dur 2 --tail 2
//                --sr 48000 --out out.wav [--analyze] [--bench]
//                [--set param=value ...]
//
// --analyze writes <out>.json with peak_dbfs, rms_dbfs, dc_offset, nan_count,
// f0_hz, f0_cents_error, alias_floor_db, attack_ms_measured,
// release_ms_measured, realtime_factor.
// --bench renders 10 s of 8-voice chords and reports realtime_factor.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "Engine/SynthEngine.h"
#include "State/EngineBindings.h"
#include "State/ModState.h"

#include <chrono>
#include <cmath>
#include <iostream>

namespace
{
constexpr int kBlockSize = 512;

struct RenderOptions
{
    juce::String preset = "init";
    juce::String outPath = "out.wav";
    int note = 60;
    int velocity = 100;
    double durationSeconds = 2.0;
    double tailSeconds = 2.0;
    double sampleRate = 48000.0;
    double bpm = 120.0;
    bool analyze = false;
    bool bench = false;
    bool measureMod = false;
    std::vector<std::pair<juce::String, float>> overrides;      // --set id=value
    std::vector<juce::StringArray> modRoutes;                   // --mod src:dest:depth
    std::vector<juce::StringArray> macroMaps;                   // --macro n:dest:min:max
};

bool parseArguments (int argc, char* argv[], RenderOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const juce::String flag (argv[i]);
        const bool hasValue = i + 1 < argc;

        if (flag == "--analyze")                    { options.analyze = true; }
        else if (flag == "--bench")                 { options.bench = true; }
        else if (flag == "--measure-mod")           { options.measureMod = true; }
        else if (flag == "--bpm" && hasValue)       { options.bpm = juce::String (argv[++i]).getDoubleValue(); }
        else if ((flag == "--mod" || flag == "--macro") && hasValue)
        {
            juce::StringArray parts;
            parts.addTokens (juce::String (argv[++i]), ":", "");
            if ((flag == "--mod" && parts.size() != 3) || (flag == "--macro" && parts.size() != 4))
            {
                std::cerr << flag << ": expected " << (flag == "--mod" ? "source:dest:depth" : "macroIndex:dest:min:max") << "\n";
                return false;
            }
            (flag == "--mod" ? options.modRoutes : options.macroMaps).push_back (parts);
        }
        else if (flag == "--preset" && hasValue)    { options.preset = argv[++i]; }
        else if (flag == "--out" && hasValue)       { options.outPath = argv[++i]; }
        else if (flag == "--note" && hasValue)      { options.note = juce::String (argv[++i]).getIntValue(); }
        else if (flag == "--vel" && hasValue)       { options.velocity = juce::String (argv[++i]).getIntValue(); }
        else if (flag == "--dur" && hasValue)       { options.durationSeconds = juce::String (argv[++i]).getDoubleValue(); }
        else if (flag == "--tail" && hasValue)      { options.tailSeconds = juce::String (argv[++i]).getDoubleValue(); }
        else if (flag == "--sr" && hasValue)        { options.sampleRate = juce::String (argv[++i]).getDoubleValue(); }
        else if (flag == "--set" && hasValue)
        {
            const juce::String assignment (argv[++i]);
            const int eq = assignment.indexOfChar ('=');
            if (eq <= 0)
            {
                std::cerr << "--set expects param=value, got '" << assignment << "'\n";
                return false;
            }
            options.overrides.emplace_back (assignment.substring (0, eq),
                                            assignment.substring (eq + 1).getFloatValue());
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << flag << "\n";
            return false;
        }
    }

    if (options.preset != "init")
    {
        std::cerr << "Phase 2 supports only --preset init (got '" << options.preset << "')\n";
        return false;
    }

    if (options.note < 0 || options.note > 127 || options.velocity < 1 || options.velocity > 127
        || options.durationSeconds <= 0.0 || options.tailSeconds < 0.0 || options.sampleRate < 8000.0)
    {
        std::cerr << "Argument out of range\n";
        return false;
    }

    return true;
}

bool buildEngineParams (const RenderOptions& options, lumen::EngineParams& params)
{
    for (const auto& [id, value] : options.overrides)
    {
        if (! lumen::bindings::set (params, id, value))
        {
            std::cerr << "--set: unknown or non-engine parameter '" << id << "'\n";
            return false;
        }
    }

    params.bpm = options.bpm;

    int slotIndex = 0;
    for (const auto& route : options.modRoutes)
    {
        const int source = lumen::modstate::sourceFromToken (route[0]);
        const int dest = lumen::modstate::destFromToken (route[1]);
        if (source < 0 || dest < 0 || slotIndex >= lumen::mod::kNumSlots)
        {
            std::cerr << "--mod: bad route '" << route.joinIntoString (":") << "'\n";
            return false;
        }
        auto& slot = params.mod.slots[slotIndex++];
        slot.source = source;
        slot.dest = dest;
        slot.depth = route[2].getFloatValue();
        slot.enabled = true;
    }

    int macroSlot[4] {};
    for (const auto& map : options.macroMaps)
    {
        const int m = map[0].getIntValue() - 1; // 1-based on the CLI
        const int dest = lumen::modstate::destFromToken (map[1]);
        if (m < 0 || m >= 4 || dest < 0 || macroSlot[m] >= lumen::mod::kMaxMacroMaps)
        {
            std::cerr << "--macro: bad mapping '" << map.joinIntoString (":") << "'\n";
            return false;
        }
        auto& mm = params.mod.macroMaps[m][macroSlot[m]++];
        mm.dest = dest;
        mm.rangeMin = map[2].getFloatValue();
        mm.rangeMax = map[3].getFloatValue();
    }
    return true;
}

// Renders and returns wall-clock realtime factor (audio seconds / render seconds).
double renderNotes (lumen::SynthEngine& engine, juce::AudioBuffer<float>& output, double sampleRate,
                    const std::vector<std::pair<int, int>>& notes, // {midiNote, noteOffSample}
                    int velocity)
{
    const auto startTime = std::chrono::steady_clock::now();

    for (const auto& [note, offSample] : notes)
    {
        juce::ignoreUnused (offSample);
        engine.noteOn (note, static_cast<float> (velocity) / 127.0f);
    }

    // Sample-accurate note-offs: split rendering at each off sample so the
    // measured release aligns exactly with the requested duration.
    auto events = notes;
    std::sort (events.begin(), events.end(),
               [] (const auto& a, const auto& b) { return a.second < b.second; });

    const int totalSamples = output.getNumSamples();
    int position = 0;
    size_t nextEvent = 0;
    while (position < totalSamples)
    {
        while (nextEvent < events.size() && events[nextEvent].second <= position)
            engine.noteOff (events[nextEvent++].first);

        int end = juce::jmin (totalSamples, position + kBlockSize);
        if (nextEvent < events.size() && events[nextEvent].second < end)
            end = events[nextEvent].second;

        engine.render (output.getWritePointer (0, position),
                       output.getWritePointer (1, position), end - position);
        position = end;
    }

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - startTime;
    const double audioSeconds = totalSamples / sampleRate;
    return audioSeconds / std::max (1.0e-9, elapsed.count());
}

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    file.deleteFile();

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
        return false;

    juce::WavAudioFormat wavFormat;
    const auto writerOptions = juce::AudioFormatWriterOptions()
                                   .withSampleRate (sampleRate)
                                   .withNumChannels (buffer.getNumChannels())
                                   .withBitsPerSample (32)
                                   .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

    auto writer = wavFormat.createWriterFor (stream, writerOptions);
    if (writer == nullptr)
        return false;

    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

// ---------------------------------------------------------------------------
// Analysis
// ---------------------------------------------------------------------------

struct BasicStats
{
    float peak = 0.0f;
    double rms = 0.0;
    double dcOffset = 0.0;
    juce::int64 nanCount = 0;
};

BasicStats basicStats (const juce::AudioBuffer<float>& buffer)
{
    BasicStats s;
    double sum = 0.0, sumSq = 0.0;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            if (std::isnan (d[i]) || std::isinf (d[i]))
            {
                ++s.nanCount;
                continue;
            }
            s.peak = juce::jmax (s.peak, std::abs (d[i]));
            sum += d[i];
            sumSq += static_cast<double> (d[i]) * d[i];
        }
    }
    const double n = static_cast<double> (numChannels) * numSamples;
    s.dcOffset = sum / n;
    s.rms = std::sqrt (sumSq / n);
    return s;
}

std::vector<float> midChannel (const juce::AudioBuffer<float>& buffer)
{
    std::vector<float> mid (static_cast<size_t> (buffer.getNumSamples()));
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (1);
    for (size_t i = 0; i < mid.size(); ++i)
        mid[i] = 0.5f * (l[i] + r[i]);
    return mid;
}

// f0: coarse FFT peak near the nominal note frequency, refined by the phase
// advance between two FFTs offset by kPhaseHop samples (very high precision).
struct F0Result { double f0 = 0.0; double centsError = 0.0; bool valid = false; };

F0Result measureF0 (const std::vector<float>& mid, double sampleRate, double nominalHz,
                    int sustainStart, int sustainEnd)
{
    constexpr int kOrder = 16, kSize = 1 << kOrder, kPhaseHop = 4096;
    if (sustainEnd - sustainStart < kSize + kPhaseHop)
    {
        std::cerr << "f0: sustain segment too short (" << (sustainEnd - sustainStart) << ")\n";
        return {};
    }

    juce::dsp::FFT fft (kOrder);
    std::vector<juce::dsp::Complex<float>> inA (kSize), inB (kSize), a (kSize), b (kSize);

    for (int i = 0; i < kSize; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / kSize);
        inA[static_cast<size_t> (i)] = { mid[static_cast<size_t> (sustainStart + i)] * w, 0.0f };
        inB[static_cast<size_t> (i)] = { mid[static_cast<size_t> (sustainStart + kPhaseHop + i)] * w, 0.0f };
    }
    fft.perform (inA.data(), a.data(), false);
    fft.perform (inB.data(), b.data(), false);

    const double binHz = sampleRate / kSize;
    const int lo = juce::jmax (1, static_cast<int> (nominalHz * 0.943 / binHz));      // -1 semitone
    const int hi = juce::jmin (kSize / 2 - 1, static_cast<int> (nominalHz * 1.06 / binHz)); // +1 semitone

    int peakBin = lo;
    double peakMag = 0.0;
    for (int k = lo; k <= hi; ++k)
    {
        const double mag = std::norm (a[static_cast<size_t> (k)]);
        if (mag > peakMag) { peakMag = mag; peakBin = k; }
    }
    if (peakMag <= 0.0)
    {
        std::cerr << "f0: no spectral peak in [" << lo << ", " << hi << "] bins\n";
        return {};
    }

    const double phaseA = std::arg (a[static_cast<size_t> (peakBin)]);
    const double phaseB = std::arg (b[static_cast<size_t> (peakBin)]);

    // Unwrap the phase advance against the expectation from the peak bin.
    const double expected = 2.0 * juce::MathConstants<double>::pi * peakBin * kPhaseHop / kSize;
    double delta = phaseB - phaseA - expected;
    delta -= 2.0 * juce::MathConstants<double>::pi * std::round (delta / (2.0 * juce::MathConstants<double>::pi));

    const double f0 = (expected + delta) * sampleRate / (2.0 * juce::MathConstants<double>::pi * kPhaseHop);
    F0Result result;
    result.f0 = f0;
    result.centsError = 1200.0 * std::log2 (f0 / nominalHz);
    result.valid = true;
    return result;
}

// Alias floor: loudest non-harmonic partial above 500 Hz, dB relative to the
// fundamental (SPEC section 18). Blackman-Harris window (-92 dB sidelobes),
// +-6 bins exclusion around every harmonic of the measured f0.
double measureAliasFloor (const std::vector<float>& mid, double sampleRate, double f0,
                          int sustainStart, int sustainEnd)
{
    constexpr int kOrder = 16, kSize = 1 << kOrder;
    if (sustainEnd - sustainStart < kSize || f0 <= 0.0)
        return 0.0;

    juce::dsp::FFT fft (kOrder);
    std::vector<juce::dsp::Complex<float>> input (kSize), x (kSize);
    for (int i = 0; i < kSize; ++i)
    {
        const double t = 2.0 * juce::MathConstants<double>::pi * i / kSize;
        const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2.0 * t)
                         - 0.01168 * std::cos (3.0 * t);
        input[static_cast<size_t> (i)] = { mid[static_cast<size_t> (sustainStart + i)] * static_cast<float> (w), 0.0f };
    }
    fft.perform (input.data(), x.data(), false);

    const double binHz = sampleRate / kSize;
    auto magnitudeAt = [&x] (int k)
    {
        return static_cast<double> (std::abs (x[static_cast<size_t> (k)]));
    };

    // Fundamental magnitude: max over +-3 bins around f0.
    const int f0Bin = static_cast<int> (std::round (f0 / binHz));
    double fundamental = 0.0;
    for (int k = juce::jmax (1, f0Bin - 3); k <= juce::jmin (kSize / 2 - 1, f0Bin + 3); ++k)
        fundamental = juce::jmax (fundamental, magnitudeAt (k));
    if (fundamental <= 0.0)
        return 0.0;

    constexpr int kExclude = 6;
    double worstAlias = 0.0;
    int harmonic = 1;
    int nextHarmonicBin = static_cast<int> (std::round (f0 / binHz));

    for (int k = static_cast<int> (500.0 / binHz) + 1; k < kSize / 2; ++k)
    {
        while (nextHarmonicBin < k - kExclude)
            nextHarmonicBin = static_cast<int> (std::round (++harmonic * f0 / binHz));
        if (std::abs (k - nextHarmonicBin) <= kExclude)
            continue;
        worstAlias = juce::jmax (worstAlias, magnitudeAt (k));
    }

    return 20.0 * std::log10 (juce::jmax (1.0e-12, worstAlias / fundamental));
}

// Amplitude envelope on a per-period grid, then interpolated threshold
// crossings: attack = time to 90% of global peak, release = time from
// note-off to -60 dB below the level at note-off.
struct EnvTiming { double attackMs = 0.0; double releaseMs = 0.0; bool valid = false; };

EnvTiming measureEnvelope (const std::vector<float>& mid, double sampleRate, double nominalHz,
                           int noteOffSample)
{
    const int window = juce::jmax (8, static_cast<int> (std::ceil (sampleRate / nominalHz)));
    const int numWindows = static_cast<int> (mid.size()) / window;
    if (numWindows < 4)
        return {};

    std::vector<double> env (static_cast<size_t> (numWindows), 0.0);
    double globalPeak = 0.0;
    for (int j = 0; j < numWindows; ++j)
    {
        double m = 0.0;
        for (int i = j * window; i < (j + 1) * window; ++i)
            m = juce::jmax (m, static_cast<double> (std::abs (mid[static_cast<size_t> (i)])));
        env[static_cast<size_t> (j)] = m;
        globalPeak = juce::jmax (globalPeak, m);
    }
    if (globalPeak <= 0.0)
        return {};

    EnvTiming result;
    auto windowTime = [window, sampleRate] (double j) { return (j + 0.5) * window / sampleRate; };

    // Attack: first crossing of 0.9 * peak, linear interpolation between windows.
    const double attackTarget = 0.9 * globalPeak;
    for (int j = 0; j < numWindows; ++j)
    {
        if (env[static_cast<size_t> (j)] >= attackTarget)
        {
            double t = windowTime (j);
            if (j > 0)
            {
                const double prev = env[static_cast<size_t> (j - 1)];
                const double frac = (attackTarget - prev) / juce::jmax (1.0e-12, env[static_cast<size_t> (j)] - prev);
                t = windowTime (j - 1) + frac * window / sampleRate;
            }
            result.attackMs = t * 1000.0;
            break;
        }
    }

    // Release: from the level at note-off down to -60 dB of that level.
    const int offWindow = juce::jmin (numWindows - 1, noteOffSample / window);
    const double offLevel = env[static_cast<size_t> (offWindow)];
    if (offLevel > 0.0)
    {
        const double releaseTarget = offLevel * 0.001; // -60 dB
        for (int j = offWindow + 1; j < numWindows; ++j)
        {
            if (env[static_cast<size_t> (j)] <= releaseTarget)
            {
                // Interpolate in log-amplitude between windows j-1 and j.
                const double prev = juce::jmax (1.0e-15, env[static_cast<size_t> (j - 1)]);
                const double curr = juce::jmax (1.0e-15, env[static_cast<size_t> (j)]);
                double frac = (std::log (releaseTarget) - std::log (prev))
                              / juce::jmin (-1.0e-12, std::log (curr) - std::log (prev));
                frac = juce::jlimit (0.0, 1.0, frac);
                const double t = windowTime (j - 1) + frac * window / sampleRate;
                result.releaseMs = (t - static_cast<double> (noteOffSample) / sampleRate) * 1000.0;
                break;
            }
        }
    }

    result.valid = result.attackMs > 0.0;
    return result;
}
// Modulation-rate measurement: spectral-centroid trajectory over the sustain,
// autocorrelated to find the dominant modulation period.
struct ModMeasure { double rateHz = 0.0; double periodSeconds = 0.0; double zipperRatio = 0.0; };

ModMeasure measureModulation (const std::vector<float>& mid, double sampleRate,
                              int sustainStart, int sustainEnd)
{
    ModMeasure result;
    constexpr int kWin = 2048, kHop = 256;
    const int numFrames = (sustainEnd - sustainStart - kWin) / kHop;
    if (numFrames < 40)
        return result;

    juce::dsp::FFT fft (11);
    std::vector<float> frame (2 * kWin);
    std::vector<double> centroid (static_cast<size_t> (numFrames));
    const double binHz = sampleRate / kWin;
    const int loBin = static_cast<int> (100.0 / binHz);
    const int hiBin = juce::jmin (kWin / 2 - 1, static_cast<int> (16000.0 / binHz));

    for (int f = 0; f < numFrames; ++f)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);
        const int offset = sustainStart + f * kHop;
        for (int i = 0; i < kWin; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / kWin);
            frame[static_cast<size_t> (i)] = mid[static_cast<size_t> (offset + i)] * w;
        }
        fft.performRealOnlyForwardTransform (frame.data());

        double num = 0.0, den = 0.0;
        for (int k = loBin; k <= hiBin; ++k)
        {
            const double mag = std::hypot (frame[2 * static_cast<size_t> (k)],
                                           frame[2 * static_cast<size_t> (k) + 1]);
            num += k * binHz * mag;
            den += mag;
        }
        centroid[static_cast<size_t> (f)] = den > 0.0 ? num / den : 0.0;
    }

    double mean = 0.0;
    for (double c : centroid) mean += c;
    mean /= numFrames;
    double variance = 0.0;
    for (auto& c : centroid) { c -= mean; variance += c * c; }
    variance /= numFrames;

    if (variance > 1.0) // centroid actually moves
    {
        const double hopSeconds = kHop / sampleRate;
        const int minLag = juce::jmax (4, static_cast<int> (0.1 / hopSeconds));
        const int maxLag = juce::jmin (numFrames / 2, static_cast<int> (3.0 / hopSeconds));

        int bestLag = 0;
        double bestR = -1.0;
        std::vector<double> r (static_cast<size_t> (maxLag) + 1, 0.0);
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double sum = 0.0;
            for (int m = 0; m + lag < numFrames; ++m)
                sum += centroid[static_cast<size_t> (m)] * centroid[static_cast<size_t> (m + lag)];
            r[static_cast<size_t> (lag)] = sum / ((numFrames - lag) * variance);
            if (r[static_cast<size_t> (lag)] > bestR)
            {
                bestR = r[static_cast<size_t> (lag)];
                bestLag = lag;
            }
        }

        if (bestR > 0.2 && bestLag > minLag && bestLag < maxLag)
        {
            const double y0 = r[static_cast<size_t> (bestLag - 1)], y1 = r[static_cast<size_t> (bestLag)],
                         y2 = r[static_cast<size_t> (bestLag + 1)];
            const double denom = y0 - 2.0 * y1 + y2;
            const double delta = std::abs (denom) > 1.0e-12 ? 0.5 * (y0 - y2) / denom : 0.0;
            result.periodSeconds = (bestLag + delta) * hopSeconds;
            result.rateHz = 1.0 / result.periodSeconds;
        }
    }

    // Zipper detection: per-10ms peaks of the first difference. A smooth
    // sweep keeps them stationary; parameter steps spike isolated windows.
    const int window = static_cast<int> (0.010 * sampleRate);
    std::vector<double> diffPeaks;
    for (int start = sustainStart; start + window < sustainEnd; start += window)
    {
        double peak = 0.0;
        for (int i = start + 1; i < start + window; ++i)
            peak = juce::jmax (peak, std::abs (static_cast<double> (mid[static_cast<size_t> (i)])
                                               - mid[static_cast<size_t> (i - 1)]));
        diffPeaks.push_back (peak);
    }
    if (diffPeaks.size() >= 8)
    {
        auto sorted = diffPeaks;
        std::sort (sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];
        result.zipperRatio = median > 1.0e-9 ? sorted.back() / median : 0.0;
    }
    return result;
}
} // namespace

int main (int argc, char* argv[])
{
    RenderOptions options;
    if (! parseArguments (argc, argv, options))
        return 2;

    lumen::EngineParams params;
    if (! buildEngineParams (options, params))
        return 2;

    lumen::SynthEngine engine;
    engine.prepare (options.sampleRate, kBlockSize);
    engine.setParams (params);

    if (options.bench)
    {
        auto* json = new juce::DynamicObject();
        // 10 s of an 8-note chord (SPEC section 18), release at 8 s.
        const int totalSamples = static_cast<int> (10.0 * options.sampleRate);
        juce::AudioBuffer<float> buffer (2, totalSamples);
        buffer.clear();

        std::vector<std::pair<int, int>> notes;
        const int offSample = static_cast<int> (8.0 * options.sampleRate);
        for (int note : { 36, 43, 48, 55, 60, 64, 67, 72 })
            notes.emplace_back (note, offSample);

        const double realtimeFactor = renderNotes (engine, buffer, options.sampleRate, notes, options.velocity);
        json->setProperty ("realtime_factor", realtimeFactor);
        json->setProperty ("bench_voices", 8);
        json->setProperty ("schema_phase", 2);

        const auto stats = basicStats (buffer);
        json->setProperty ("peak_dbfs", juce::Decibels::gainToDecibels (stats.peak, -144.0f));
        json->setProperty ("nan_count", stats.nanCount);

        const auto jsonText = juce::JSON::toString (juce::var (json));
        std::cout << jsonText << "\n";
        const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile (options.outPath);
        outFile.withFileExtension ("json").replaceWithText (jsonText);
        return 0;
    }

    const int noteOffSample = static_cast<int> (options.durationSeconds * options.sampleRate);
    const int totalSamples = static_cast<int> ((options.durationSeconds + options.tailSeconds) * options.sampleRate);
    juce::AudioBuffer<float> buffer (2, totalSamples);
    buffer.clear();

    const double realtimeFactor = renderNotes (engine, buffer, options.sampleRate,
                                               { { options.note, noteOffSample } }, options.velocity);

    const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile (options.outPath);
    if (! writeWav (outFile, buffer, options.sampleRate))
    {
        std::cerr << "Failed to write WAV: " << outFile.getFullPathName() << "\n";
        return 1;
    }
    std::cout << "Wrote " << outFile.getFullPathName() << " (" << totalSamples << " samples, "
              << options.sampleRate << " Hz, 2ch float)\n";

    if (options.analyze)
    {
        const auto stats = basicStats (buffer);
        const auto mid = midChannel (buffer);
        const double nominalHz = 440.0 * std::exp2 ((options.note - 69.0) / 12.0);

        // Sustain segment: skip the first 25% of the held note (attack +
        // smoothing settle), end at note-off.
        const int sustainStart = noteOffSample / 4;
        const auto f0 = measureF0 (mid, options.sampleRate, nominalHz, sustainStart, noteOffSample);
        const double aliasFloor = f0.valid
            ? measureAliasFloor (mid, options.sampleRate, f0.f0, sustainStart, noteOffSample)
            : 0.0;
        const auto timing = measureEnvelope (mid, options.sampleRate, nominalHz, noteOffSample);

        auto* json = new juce::DynamicObject();
        json->setProperty ("peak_dbfs", juce::Decibels::gainToDecibels (stats.peak, -144.0f));
        json->setProperty ("rms_dbfs", juce::Decibels::gainToDecibels (stats.rms, -144.0));
        json->setProperty ("dc_offset", stats.dcOffset);
        json->setProperty ("nan_count", stats.nanCount);
        json->setProperty ("f0_hz", f0.valid ? f0.f0 : 0.0);
        json->setProperty ("f0_cents_error", f0.valid ? f0.centsError : 999.0);
        json->setProperty ("alias_floor_db", aliasFloor);
        json->setProperty ("attack_ms_measured", timing.attackMs);
        json->setProperty ("release_ms_measured", timing.releaseMs);
        json->setProperty ("realtime_factor", realtimeFactor);
        json->setProperty ("schema_phase", 3);

        if (options.measureMod)
        {
            const auto modResult = measureModulation (mid, options.sampleRate,
                                                      static_cast<int> (0.3 * options.sampleRate),
                                                      noteOffSample);
            json->setProperty ("mod_rate_hz", modResult.rateHz);
            json->setProperty ("mod_period_s", modResult.periodSeconds);
            json->setProperty ("zipper_ratio", modResult.zipperRatio);
        }

        const auto jsonText = juce::JSON::toString (juce::var (json));
        outFile.withFileExtension ("json").replaceWithText (jsonText);
        std::cout << jsonText << "\n";
    }

    return 0;
}
