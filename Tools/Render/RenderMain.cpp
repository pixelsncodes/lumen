// lumen_render — headless render/verification harness (SPEC section 18).
//
// Phase 1 scope: renders the temporary test voice to a WAV and, with
// --analyze, writes basic metrics (peak/rms/dc/nan) as JSON. The full metric
// set (f0, alias floor, attack/release timing, realtime factor) arrives in
// Phase 2 with the real engine.
//
//   lumen_render --preset init --note 60 --vel 100 --dur 2 --tail 2
//                --sr 48000 --out out.wav [--analyze]

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Engine/TestVoice.h"

#include <cmath>
#include <iostream>

namespace
{
struct RenderOptions
{
    juce::String preset = "init";
    juce::String outPath = "out.wav";
    int note = 60;
    int velocity = 100;
    double durationSeconds = 2.0;
    double tailSeconds = 2.0;
    double sampleRate = 48000.0;
    bool analyze = false;
};

bool parseArguments (int argc, char* argv[], RenderOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const juce::String flag (argv[i]);
        const bool hasValue = i + 1 < argc;

        if (flag == "--analyze")                    { options.analyze = true; }
        else if (flag == "--preset" && hasValue)    { options.preset = argv[++i]; }
        else if (flag == "--out" && hasValue)       { options.outPath = argv[++i]; }
        else if (flag == "--note" && hasValue)      { options.note = juce::String (argv[++i]).getIntValue(); }
        else if (flag == "--vel" && hasValue)       { options.velocity = juce::String (argv[++i]).getIntValue(); }
        else if (flag == "--dur" && hasValue)       { options.durationSeconds = juce::String (argv[++i]).getDoubleValue(); }
        else if (flag == "--tail" && hasValue)      { options.tailSeconds = juce::String (argv[++i]).getDoubleValue(); }
        else if (flag == "--sr" && hasValue)        { options.sampleRate = juce::String (argv[++i]).getDoubleValue(); }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << flag << "\n";
            return false;
        }
    }

    if (options.preset != "init")
    {
        std::cerr << "Phase 1 supports only --preset init (got '" << options.preset << "')\n";
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

void renderTestVoice (const RenderOptions& options, juce::AudioBuffer<float>& output)
{
    juce::Synthesiser synth;
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new lumen::TestVoice());
    synth.addSound (new lumen::TestSound());
    synth.setCurrentPlaybackSampleRate (options.sampleRate);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, options.note, (juce::uint8) options.velocity), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, options.note),
                   juce::roundToInt (options.durationSeconds * options.sampleRate));

    const int totalSamples = output.getNumSamples();
    constexpr int blockSize = 512;

    for (int position = 0; position < totalSamples; position += blockSize)
        synth.renderNextBlock (output, midi, position, juce::jmin (blockSize, totalSamples - position));
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

juce::String analyzeBuffer (const juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float peak = 0.0f;
    double sum = 0.0;
    double sumOfSquares = 0.0;
    juce::int64 nanCount = 0;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* data = buffer.getReadPointer (channel);
        for (int i = 0; i < numSamples; ++i)
        {
            const float sample = data[i];
            if (std::isnan (sample) || std::isinf (sample))
            {
                ++nanCount;
                continue;
            }
            peak = juce::jmax (peak, std::abs (sample));
            sum += sample;
            sumOfSquares += (double) sample * (double) sample;
        }
    }

    const auto totalCount = (double) numChannels * (double) numSamples;
    const double dcOffset = sum / totalCount;
    const double rms = std::sqrt (sumOfSquares / totalCount);

    auto* json = new juce::DynamicObject();
    json->setProperty ("peak_dbfs", juce::Decibels::gainToDecibels (peak, -144.0f));
    json->setProperty ("rms_dbfs", juce::Decibels::gainToDecibels (rms, -144.0));
    json->setProperty ("dc_offset", dcOffset);
    json->setProperty ("nan_count", nanCount);
    json->setProperty ("schema_phase", 1); // f0/alias/attack/release/realtime metrics arrive in Phase 2

    return juce::JSON::toString (juce::var (json));
}
} // namespace

int main (int argc, char* argv[])
{
    RenderOptions options;
    if (! parseArguments (argc, argv, options))
        return 2;

    const int totalSamples = juce::roundToInt ((options.durationSeconds + options.tailSeconds) * options.sampleRate);
    juce::AudioBuffer<float> buffer (2, totalSamples);
    buffer.clear();

    renderTestVoice (options, buffer);

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
        const auto jsonText = analyzeBuffer (buffer);
        const auto jsonFile = outFile.withFileExtension ("json");
        jsonFile.replaceWithText (jsonText);
        std::cout << jsonText << "\n";
    }

    return 0;
}
