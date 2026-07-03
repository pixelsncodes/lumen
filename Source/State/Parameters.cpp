#include "State/Parameters.h"

#include "Engine/EngineParams.h"
#include "Engine/Lfo.h"

namespace lumen::params
{
namespace
{
    using FloatParam  = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using BoolParam   = juce::AudioParameterBool;
    using IntParam    = juce::AudioParameterInt;
    using Attributes  = juce::AudioParameterFloatAttributes;
    using Layout      = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::NormalisableRange<float> logHzRange()
    {
        juce::NormalisableRange<float> range (20.0f, 20000.0f);
        range.setSkewForCentre (632.5f); // sqrt(20 * 20000): perceptually log sweep
        return range;
    }

    juce::NormalisableRange<float> logTimeRange (float minSeconds, float maxSeconds)
    {
        juce::NormalisableRange<float> range (minSeconds, maxSeconds);
        range.setSkewForCentre (std::sqrt (minSeconds * maxSeconds));
        return range;
    }

    juce::String hzToText (float v, int)
    {
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    }

    juce::String dbToText (float v, int)
    {
        return v <= -59.95f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB";
    }

    juce::String unitToText (float v, int)  { return juce::String (v, 2); }
    juce::String centsToText (float v, int) { return juce::String (v, 1) + " ct"; }

    juce::String secondsToText (float v, int)
    {
        return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                        : juce::String (v, 2) + " s";
    }

    juce::String percentToText (float v, int)
    {
        return juce::String (juce::roundToInt (v * 100.0f)) + " %";
    }

    juce::ParameterID pid (const char* id) { return juce::ParameterID (id, 1); }

    void addEnvelope (Layout& layout, const char* attackId, const char* decayId,
                      const char* sustainId, const char* releaseId, const char* curveId,
                      const juce::String& name, const lumen::EnvParams& defaults)
    {
        const auto secAttr = Attributes().withLabel ("s").withStringFromValueFunction (secondsToText);
        const auto unitAttr = Attributes().withStringFromValueFunction (unitToText);

        layout.add (std::make_unique<FloatParam> (pid (attackId), name + " Attack",
            logTimeRange (0.001f, 10.0f), defaults.attackSeconds, secAttr));
        layout.add (std::make_unique<FloatParam> (pid (decayId), name + " Decay",
            logTimeRange (0.001f, 10.0f), defaults.decaySeconds, secAttr));
        layout.add (std::make_unique<FloatParam> (pid (sustainId), name + " Sustain",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.sustain, unitAttr));
        layout.add (std::make_unique<FloatParam> (pid (releaseId), name + " Release",
            logTimeRange (0.005f, 15.0f), defaults.releaseSeconds, secAttr));
        layout.add (std::make_unique<FloatParam> (pid (curveId), name + " Curve",
            juce::NormalisableRange<float> (-1.0f, 1.0f), defaults.curve, unitAttr));
    }

    juce::StringArray syncDivNames()
    {
        juce::StringArray names;
        const char* bases[] = { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
        for (const char* base : bases)
        {
            names.add (base);
            names.add (juce::String (base) + " D");
            names.add (juce::String (base) + " T");
        }
        return names;
    }

    void addLfo (Layout& layout, const juce::String& prefix, const juce::String& name,
                 const lumen::LfoParams& defaults)
    {
        auto id = [&prefix] (const char* suffix) { return juce::ParameterID (prefix + suffix, 1); };

        layout.add (std::make_unique<ChoiceParam> (id ("Shape"), name + " Shape",
            juce::StringArray { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" },
            defaults.shape));
        layout.add (std::make_unique<BoolParam> (id ("Sync"), name + " Sync", defaults.sync));

        juce::NormalisableRange<float> rateRange (0.01f, 40.0f);
        rateRange.setSkewForCentre (std::sqrt (0.01f * 40.0f));
        layout.add (std::make_unique<FloatParam> (id ("Rate"), name + " Rate",
            rateRange, defaults.rateHz,
            Attributes().withLabel ("Hz").withStringFromValueFunction (
                [] (float v, int) { return juce::String (v, 2) + " Hz"; })));

        layout.add (std::make_unique<ChoiceParam> (id ("SyncDiv"), name + " Sync Division",
            syncDivNames(), defaults.syncDiv));
        layout.add (std::make_unique<FloatParam> (id ("Phase"), name + " Phase",
            juce::NormalisableRange<float> (0.0f, 360.0f, 1.0f), defaults.phaseDeg,
            Attributes().withLabel ("deg").withStringFromValueFunction (
                [] (float v, int) { return juce::String (juce::roundToInt (v)) + " deg"; })));
        layout.add (std::make_unique<FloatParam> (id ("Fade"), name + " Fade In",
            juce::NormalisableRange<float> (0.0f, 5.0f), defaults.fadeSeconds,
            Attributes().withLabel ("s").withStringFromValueFunction (secondsToText)));
        layout.add (std::make_unique<ChoiceParam> (id ("Mode"), name + " Mode",
            juce::StringArray { "Poly", "Mono" }, defaults.mono ? 1 : 0));
    }

    void addOscillator (Layout& layout, const juce::String& prefix, const juce::String& name,
                        const lumen::OscParams& defaults)
    {
        const auto unitAttr = Attributes().withStringFromValueFunction (unitToText);
        const auto centsAttr = Attributes().withLabel ("ct").withStringFromValueFunction (centsToText);
        const juce::StringArray tables { "Basic", "PWM", "Harmonic Rise", "Formant", "Image" };

        auto id = [&prefix] (const char* suffix) { return juce::ParameterID (prefix + suffix, 1); };

        layout.add (std::make_unique<BoolParam>  (id ("Enabled"), name + " On", defaults.enabled));
        layout.add (std::make_unique<ChoiceParam> (id ("Table"), name + " Table", tables, defaults.table));
        layout.add (std::make_unique<FloatParam> (id ("Morph"), name + " Morph",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.morph, unitAttr));
        layout.add (std::make_unique<FloatParam> (id ("Level"), name + " Level",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.level, unitAttr));
        layout.add (std::make_unique<FloatParam> (id ("Pan"), name + " Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f), defaults.pan, unitAttr));
        layout.add (std::make_unique<IntParam>   (id ("Semi"), name + " Semitones", -24, 24, defaults.semitones));
        layout.add (std::make_unique<FloatParam> (id ("Fine"), name + " Fine",
            juce::NormalisableRange<float> (-100.0f, 100.0f), defaults.fineCents, centsAttr));
        layout.add (std::make_unique<IntParam>   (id ("Unison"), name + " Unison", 1, 8, defaults.unison));
        layout.add (std::make_unique<FloatParam> (id ("Detune"), name + " Detune",
            juce::NormalisableRange<float> (0.0f, 50.0f), defaults.detuneCents, centsAttr));
        layout.add (std::make_unique<FloatParam> (id ("Width"), name + " Width",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.width, unitAttr));
        layout.add (std::make_unique<FloatParam> (id ("Blend"), name + " Blend",
            juce::NormalisableRange<float> (0.0f, 1.0f), defaults.blend, unitAttr));
        layout.add (std::make_unique<BoolParam>  (id ("PhaseRand"), name + " Phase Random", defaults.phaseRandom));
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Layout layout;
    const lumen::EngineParams defaults; // single source of default values

    const juce::NormalisableRange<float> unitRange (0.0f, 1.0f);
    const auto unitAttr = Attributes().withStringFromValueFunction (unitToText);
    const auto dbAttr = Attributes().withLabel ("dB").withStringFromValueFunction (dbToText);

    // --- Frozen first 8 (SPEC section 12) --------------------------------
    layout.add (std::make_unique<FloatParam> (pid (macro1), "Tone",    unitRange, 0.5f, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (macro2), "Motion",  unitRange, 0.5f, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (macro3), "Space",   unitRange, 0.3f, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (macro4), "Texture", unitRange, 0.2f, unitAttr));

    layout.add (std::make_unique<FloatParam> (pid (filterCutoff), "Filter Cutoff",
        logHzRange(), defaults.filterCutoffHz,
        Attributes().withLabel ("Hz").withStringFromValueFunction (hzToText)));

    layout.add (std::make_unique<FloatParam> (pid (filterRes), "Filter Resonance",
        unitRange, defaults.filterRes, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (delayMix), "Delay Mix",   unitRange, 0.0f,  unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (reverbMix), "Reverb Mix", unitRange, 0.12f, unitAttr));

    // --- Append-only from here -------------------------------------------
    layout.add (std::make_unique<FloatParam> (pid (masterGain), "Master Gain",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.01f), 0.0f, dbAttr));

    // Phase 2: oscillators, sub/noise, filter, envelopes
    addOscillator (layout, "oscA", "Osc A", defaults.oscA);
    addOscillator (layout, "oscB", "Osc B", defaults.oscB);

    layout.add (std::make_unique<ChoiceParam> (pid (subWave), "Sub Wave",
        juce::StringArray { "Sine", "Triangle", "Square" }, defaults.subWave));
    layout.add (std::make_unique<ChoiceParam> (pid (subOctave), "Sub Octave",
        juce::StringArray { "-1 oct", "-2 oct" }, defaults.subOctave - 1));
    layout.add (std::make_unique<FloatParam> (pid (subLevel), "Sub Level",
        unitRange, defaults.subLevel, unitAttr));

    layout.add (std::make_unique<ChoiceParam> (pid (noiseType), "Noise Type",
        juce::StringArray { "White", "Pink" }, defaults.noiseType));
    layout.add (std::make_unique<FloatParam> (pid (noiseLevel), "Noise Level",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), defaults.noiseDb, dbAttr));

    layout.add (std::make_unique<ChoiceParam> (pid (filterMode), "Filter Mode",
        juce::StringArray { "LP12", "LP24", "HP12", "BP12", "Notch" }, defaults.filterMode));
    layout.add (std::make_unique<FloatParam> (pid (filterDrive), "Filter Drive",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), defaults.filterDriveDb, dbAttr));
    layout.add (std::make_unique<FloatParam> (pid (filterKeytrack), "Filter Keytrack",
        unitRange, defaults.filterKeytrack,
        Attributes().withLabel ("%").withStringFromValueFunction (percentToText)));
    layout.add (std::make_unique<FloatParam> (pid (filterEnvAmount), "Filter Env Amount",
        juce::NormalisableRange<float> (-1.0f, 1.0f), defaults.filterEnvAmount, unitAttr));

    addEnvelope (layout, env1Attack, env1Decay, env1Sustain, env1Release, env1Curve, "Env 1", defaults.env1);
    addEnvelope (layout, env2Attack, env2Decay, env2Sustain, env2Release, env2Curve, "Env 2", defaults.env2);
    addEnvelope (layout, env3Attack, env3Decay, env3Sustain, env3Release, env3Curve, "Env 3", defaults.env3);

    // Phase 3: LFOs
    addLfo (layout, "lfo1", "LFO 1", defaults.lfo[0]);
    addLfo (layout, "lfo2", "LFO 2", defaults.lfo[1]);
    addLfo (layout, "lfo3", "LFO 3", defaults.lfo[2]);

    // Phase 4: effects. Continuous ranges mirror Engine/ModDestinations.h.
    const auto& fx = defaults.fx;
    const auto hzAttr = Attributes().withLabel ("Hz").withStringFromValueFunction (hzToText);

    layout.add (std::make_unique<BoolParam>  (pid (driveEnabled), "Drive On", fx.driveEnabled));
    layout.add (std::make_unique<FloatParam> (pid (driveAmount), "Drive Amount",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), fx.driveDb, dbAttr));
    layout.add (std::make_unique<FloatParam> (pid (driveTone), "Drive Tone",
        juce::NormalisableRange<float> (-1.0f, 1.0f), fx.driveTone, unitAttr));

    layout.add (std::make_unique<BoolParam>  (pid (chorusEnabled), "Chorus On", fx.chorusEnabled));
    juce::NormalisableRange<float> chorusRateRange (0.05f, 5.0f);
    chorusRateRange.setSkewForCentre (0.5f); // sqrt(0.05 * 5)
    layout.add (std::make_unique<FloatParam> (pid (chorusRate), "Chorus Rate",
        chorusRateRange, fx.chorusRateHz,
        Attributes().withLabel ("Hz").withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 2) + " Hz"; })));
    layout.add (std::make_unique<FloatParam> (pid (chorusDepth), "Chorus Depth",
        unitRange, fx.chorusDepth, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (chorusMix), "Chorus Mix",
        unitRange, fx.chorusMix, unitAttr));

    layout.add (std::make_unique<BoolParam>  (pid (delayEnabled), "Delay On", fx.delayEnabled));
    layout.add (std::make_unique<BoolParam>  (pid (delaySync), "Delay Sync", fx.delaySync));
    juce::NormalisableRange<float> delayTimeRange (1.0f, 2000.0f);
    delayTimeRange.setSkewForCentre (44.7213595f); // sqrt(1 * 2000)
    layout.add (std::make_unique<FloatParam> (pid (delayTime), "Delay Time",
        delayTimeRange, fx.delayTimeMs,
        Attributes().withLabel ("ms").withStringFromValueFunction (
            [] (float v, int) { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " ms"
                                                   : juce::String (v / 1000.0f, 2) + " s"; })));
    layout.add (std::make_unique<ChoiceParam> (pid (delayDiv), "Delay Sync Division",
        syncDivNames(), fx.delayDiv));
    layout.add (std::make_unique<FloatParam> (pid (delayFeedback), "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f), fx.delayFeedback,
        Attributes().withLabel ("%").withStringFromValueFunction (percentToText)));
    juce::NormalisableRange<float> delayDampRange (1000.0f, 16000.0f);
    delayDampRange.setSkewForCentre (4000.0f); // sqrt(1000 * 16000)
    layout.add (std::make_unique<FloatParam> (pid (delayDamp), "Delay Damping",
        delayDampRange, fx.delayDampHz, hzAttr));
    layout.add (std::make_unique<BoolParam>  (pid (delayPingPong), "Delay Ping-Pong", fx.delayPingPong));

    layout.add (std::make_unique<BoolParam>  (pid (reverbEnabled), "Reverb On", fx.reverbEnabled));
    layout.add (std::make_unique<FloatParam> (pid (reverbSize), "Reverb Size",
        unitRange, fx.reverbSize, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (reverbDamp), "Reverb Damping",
        unitRange, fx.reverbDamp, unitAttr));
    layout.add (std::make_unique<FloatParam> (pid (reverbWidth), "Reverb Width",
        unitRange, fx.reverbWidth, unitAttr));

    // Phase 7: voice modes + glide (SPEC section 11). Glide time is consumed
    // at note events (no audio-rate smoothing needed; the pitch slew IS the
    // smoothing), skewed so short portamento times get most of the throw.
    layout.add (std::make_unique<ChoiceParam> (pid (voiceMode), "Voice Mode",
        juce::StringArray { "Poly", "Mono", "Legato" }, defaults.voiceMode));
    juce::NormalisableRange<float> glideRange (0.0f, 2.0f);
    glideRange.setSkewForCentre (0.25f);
    layout.add (std::make_unique<FloatParam> (pid (glideTime), "Glide Time",
        glideRange, defaults.glideSeconds,
        Attributes().withLabel ("s").withStringFromValueFunction (secondsToText)));

    return layout;
}
} // namespace lumen::params
