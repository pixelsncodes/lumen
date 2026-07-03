#pragma once

#include <juce_core/juce_core.h>

// Short hover descriptions for every user-facing control (SPEC 14 strings
// pass). One sentence, plain ASCII, no trailing period — they read as labels.
namespace tooltips
{
inline juce::String forParam (const juce::String& id)
{
    // --- Oscillators (A and B share suffix meanings) ---------------------
    if (id.startsWith ("osc") && id.length() > 4)
    {
        const juce::String osc = juce::String ("Osc ") + id[3];
        const auto suffix = id.substring (4);
        if (suffix == "Enabled")   return "Turn " + osc + " on or off";
        if (suffix == "Table")     return "Wavetable " + osc + " plays (Image = the Lens result)";
        if (suffix == "Morph")     return "Position across the wavetable frames - the main motion target";
        if (suffix == "Level")     return osc + " volume into the voice mix";
        if (suffix == "Pan")       return osc + " stereo position";
        if (suffix == "Semi")      return "Transpose " + osc + " in semitones (+/-24)";
        if (suffix == "Fine")      return "Fine tune " + osc + " in cents (+/-100)";
        if (suffix == "Unison")    return "Stacked voices per note (1-8) for width and thickness";
        if (suffix == "Detune")    return "Unison spread in cents - more = wider, blurrier";
        if (suffix == "Width")     return "Stereo spread of the unison voices";
        if (suffix == "Blend")     return "Balance of center vs detuned unison voices";
        if (suffix == "PhaseRand") return "Randomize each unison voice's start phase per note";
    }

    // --- Envelopes 1-3 ----------------------------------------------------
    if (id.startsWith ("env") && id.length() > 4)
    {
        const juce::String which = id[3] == '1' ? " (Env 1 drives the voice volume)"
                                 : id[3] == '2' ? " (Env 2 drives the filter)"
                                                : " (Env 3 is free to assign)";
        const auto suffix = id.substring (4);
        if (suffix == "Attack")  return "Time to reach full level after note-on" + which;
        if (suffix == "Decay")   return "Time to fall from peak to the sustain level";
        if (suffix == "Sustain") return "Level held while the key stays down";
        if (suffix == "Release") return "Fade-out time after the key is released";
        if (suffix == "Curve")   return "Bends the segments: left = snappy, right = soft";
    }

    // --- LFOs 1-3 -----------------------------------------------------------
    if (id.startsWith ("lfo") && id.length() > 4)
    {
        const auto suffix = id.substring (4);
        if (suffix == "Shape")   return "LFO waveform";
        if (suffix == "Sync")    return "Lock the rate to the host tempo";
        if (suffix == "Rate")    return "LFO speed in Hz (drag onto a knob to hear it move)";
        if (suffix == "SyncDiv") return "Tempo-synced rate (D = dotted, T = triplet)";
        if (suffix == "Phase")   return "Where in the cycle the LFO starts on each note";
        if (suffix == "Fade")    return "Fade the LFO in after note-on (poly mode)";
        if (suffix == "Mode")    return "Poly = restarts per note, Mono = one free-running LFO";
    }

    // --- Everything else ---------------------------------------------------
    struct Entry { const char* id; const char* text; };
    static constexpr Entry table[] = {
        { "macro1", "Tone macro - one knob, several mapped targets" },
        { "macro2", "Motion macro - drives the patch's movement" },
        { "macro3", "Space macro - reverb and delay travel" },
        { "macro4", "Texture macro - grit, noise and spread" },
        { "filterCutoff", "Filter cutoff frequency - drag the node in the spectrum too" },
        { "filterRes", "Resonance peak at the cutoff" },
        { "filterMode", "Filter type: low-pass, high-pass, band-pass or notch" },
        { "filterDrive", "Saturation into the filter (gain compensated)" },
        { "filterKeytrack", "Cutoff follows the played note (100% = 1:1)" },
        { "filterEnvAmount", "How far Env 2 opens (or closes) the cutoff" },
        { "subWave", "Sub oscillator waveform" },
        { "subOctave", "Sub plays one or two octaves below Osc A" },
        { "subLevel", "Sub oscillator volume" },
        { "noiseType", "White = bright, pink = darker noise" },
        { "noiseLevel", "Noise volume (-60 dB = off)" },
        { "voiceMode", "Poly = chords; Mono retriggers per note; Legato ties held notes" },
        { "glideTime", "Portamento: pitch slide time between notes (mono/legato)" },
        { "masterGain", "Final output volume" },
        { "driveEnabled", "Drive on/off" },
        { "driveAmount", "Tanh saturation amount" },
        { "driveTone", "Tilts what saturates: left = darker, right = brighter" },
        { "chorusEnabled", "Chorus on/off" },
        { "chorusRate", "Chorus wobble speed" },
        { "chorusDepth", "Chorus detune depth" },
        { "chorusMix", "Dry/wet balance of the chorus" },
        { "delayEnabled", "Delay on/off" },
        { "delaySync", "Lock the delay time to the host tempo" },
        { "delayTime", "Echo spacing in milliseconds (when not synced)" },
        { "delayDiv", "Tempo-synced echo spacing (D = dotted, T = triplet)" },
        { "delayFeedback", "How long the echoes repeat" },
        { "delayDamp", "Darkens each repeat (feedback low-pass)" },
        { "delayPingPong", "Echoes alternate left/right" },
        { "delayMix", "Dry/wet balance of the delay" },
        { "reverbEnabled", "Reverb on/off" },
        { "reverbSize", "Room size" },
        { "reverbDamp", "High-frequency absorption of the tail" },
        { "reverbWidth", "Stereo width of the reverb" },
        { "reverbMix", "Dry/wet balance of the reverb" },
    };
    for (const auto& entry : table)
        if (id == entry.id)
            return entry.text;
    return {};
}
} // namespace tooltips
