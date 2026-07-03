#include "State/FactoryPresets.h"

#include "Lens/LensEngine.h"
#include "Lens/TestImages.h"
#include "State/EngineBindings.h"
#include "State/LensState.h"
#include "State/ModState.h"
#include "State/Parameters.h"

#include <cmath>

namespace lumen::presets
{
namespace
{
    // Fluent builder so each preset below reads as a patch sheet.
    struct Build
    {
        FactoryPreset p;

        Build (const char* name, const char* category)
        {
            p.name = name;
            p.category = category;
        }

        Build& set (const char* id, float value)
        {
            p.settings.push_back ({ id, value });
            return *this;
        }

        Build& route (const char* source, const char* dest, float depth)
        {
            p.routes.push_back ({ source, dest, depth });
            return *this;
        }

        Build& map (int macro, const char* dest, float span, float curve = 1.0f)
        {
            p.maps.push_back ({ macro, dest, span, curve });
            return *this;
        }

        // Stored macro knob positions (Tone / Motion / Space / Texture). The
        // rest position is the neutral point of every macro map, so the knobs
        // can sit anywhere without changing the designed sound (macroMapRange
        // pins a bit-exact zero offset there) — they just show the preset's
        // character and set where the sweeps start from.
        Build& macros (float tone, float motion, float space, float texture)
        {
            p.settings.push_back ({ lumen::params::macro1, tone });
            p.settings.push_back ({ lumen::params::macro2, motion });
            p.settings.push_back ({ lumen::params::macro3, space });
            p.settings.push_back ({ lumen::params::macro4, texture });
            return *this;
        }

        Build& lens (const char* image, int mode, int targetOsc = 0)
        {
            p.lens = { image, mode, targetOsc };
            return *this;
        }

        FactoryPreset done() { return std::move (p); }
    };

    // Choice indices, kept in sync with State/Parameters.cpp by the bank
    // validity unit test (every setting id exists and survives range snap).
    constexpr float kPwm = 1.0f, kRise = 2.0f, kFormant = 3.0f, kImage = 4.0f;
    constexpr float kLp12 = 0.0f, kLp24 = 1.0f, kBp12 = 3.0f;
    constexpr float kTriangle = 1.0f, kSawDown = 3.0f, kSquare = 4.0f, kSampleHold = 5.0f;
    constexpr float kSubSine = 0.0f, kSubTri = 1.0f, kSubSquare = 2.0f;
    constexpr float kPink = 1.0f;
    constexpr float kMono = 1.0f;
    // Delay sync divisions: base*3 + 0 straight / 1 dotted / 2 triplet.
    constexpr float kDiv12   = 9.0f;   // 1/2
    constexpr float kDiv12D  = 10.0f;  // 1/2 dotted
    constexpr float kDiv14   = 12.0f;  // 1/4
    constexpr float kDiv14T  = 14.0f;  // 1/4 triplet
    constexpr float kDiv18   = 15.0f;  // 1/8
    constexpr float kDiv18D  = 16.0f;  // 1/8 dotted
    constexpr float kDiv18T  = 17.0f;  // 1/8 triplet
    constexpr float kDiv116  = 18.0f;  // 1/16

    std::vector<FactoryPreset> makeBank()
    {
        using namespace lumen::params;
        std::vector<FactoryPreset> bank;
        bank.reserve (kNumPresets);

        // ================= BASS =========================================
        // Sub Zero — pure deep sine sub with a triangle floor; thump from a
        // short filter-envelope knock. Tone opens it into a clean synth bass.
        bank.push_back (Build ("Sub Zero", "Bass")
            .macros (0.15f, 0.20f, 0.10f, 0.10f)
            .set (oscAMorph, 0.0f).set (oscAUnison, 1.0f).set (oscALevel, 0.9f)
            .set (subWave, kSubTri).set (subLevel, 0.35f)
            .set (filterMode, kLp24).set (filterCutoff, 400.0f).set (filterRes, 0.05f)
            .set (env1Attack, 0.003f).set (env1Decay, 0.4f).set (env1Sustain, 0.9f).set (env1Release, 0.15f)
            .set (filterEnvAmount, 0.35f).set (env2Decay, 0.18f).set (env2Sustain, 0.2f).set (env2Release, 0.12f)
            .set (driveEnabled, 1.0f).set (driveAmount, 2.0f).set (driveTone, -0.2f)
            .set (reverbMix, 0.0f)
            .set (lfo1Rate, 0.6f)
            .route ("velocity", "filterCutoff", 0.15f)
            .route ("lfo1", "filterCutoff", 0.05f)
            .map (0, "filterCutoff", 0.55f).map (0, "filterDrive", 0.3f)
            .map (1, "lfo1Rate", 0.4f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.5f).map (3, "driveAmount", 0.35f)
            .done());

        // Rubber — elastic resonant pluck bass: closed LP24 kicked open by a
        // fast envelope; velocity digs into the filter.
        bank.push_back (Build ("Rubber", "Bass")
            .macros (0.30f, 0.45f, 0.15f, 0.25f)
            .set (oscAMorph, 0.6f).set (oscAUnison, 1.0f).set (oscALevel, 0.8f)
            .set (subWave, kSubSine).set (subLevel, 0.25f)
            .set (filterMode, kLp24).set (filterCutoff, 260.0f).set (filterRes, 0.4f)
            .set (filterEnvAmount, 0.6f)
            .set (env2Attack, 0.001f).set (env2Decay, 0.09f).set (env2Sustain, 0.0f).set (env2Release, 0.09f)
            .set (env1Attack, 0.002f).set (env1Decay, 0.35f).set (env1Sustain, 0.7f).set (env1Release, 0.12f)
            .set (driveEnabled, 1.0f).set (driveAmount, 4.0f)
            .set (reverbMix, 0.06f)
            .set (lfo1Rate, 6.0f)
            .route ("velocity", "filterCutoff", 0.25f)
            .route ("lfo1", "oscAFine", 0.03f)
            .map (0, "filterCutoff", 0.6f).map (0, "driveTone", 0.4f)
            .map (1, "lfo1Rate", 0.5f).map (1, "env2Decay", 0.35f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "oscAMorph", 0.35f).map (3, "noiseLevel", 0.4f)
            .done());

        // Neon Growl — formant bass with an LFO chewing the morph; drive on
        // both the filter and the bus for bite.
        bank.push_back (Build ("Neon Growl", "Bass")
            .macros (0.40f, 0.60f, 0.15f, 0.55f)
            .set (oscATable, kFormant).set (oscAMorph, 0.35f)
            .set (oscAUnison, 2.0f).set (oscADetune, 8.0f).set (oscAWidth, 0.4f).set (oscALevel, 0.9f)
            .set (masterGain, 3.0f)
            .set (subWave, kSubSine).set (subLevel, 0.4f)
            .set (filterMode, kLp24).set (filterCutoff, 750.0f).set (filterRes, 0.3f)
            .set (filterDrive, 6.0f)
            .set (filterEnvAmount, 0.4f).set (env2Decay, 0.25f).set (env2Sustain, 0.35f)
            .set (env1Attack, 0.004f).set (env1Decay, 0.5f).set (env1Sustain, 0.8f).set (env1Release, 0.18f)
            .set (driveEnabled, 1.0f).set (driveAmount, 6.0f).set (driveTone, 0.2f)
            .set (reverbMix, 0.05f)
            .set (lfo1Rate, 1.8f)
            .route ("lfo1", "oscAMorph", 0.3f)
            .route ("velocity", "filterCutoff", 0.2f)
            .map (0, "filterCutoff", 0.6f).map (0, "driveAmount", 0.3f)
            .map (1, "lfo1Rate", 0.5f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "oscADetune", 0.5f).map (3, "noiseLevel", 0.3f)
            .done());

        // Deep Field — wide slow-PWM bass pad over a sine floor; chorus and
        // a drifting morph keep the low end moving without mud.
        bank.push_back (Build ("Deep Field", "Bass")
            .macros (0.35f, 0.35f, 0.30f, 0.40f)
            .set (oscATable, kPwm).set (oscAMorph, 0.35f)
            .set (oscAUnison, 4.0f).set (oscADetune, 14.0f).set (oscAWidth, 0.85f).set (oscALevel, 1.0f)
            .set (masterGain, 6.0f)
            .set (subWave, kSubSine).set (subLevel, 0.45f)
            .set (filterMode, kLp24).set (filterCutoff, 520.0f).set (filterRes, 0.15f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.35f).set (chorusDepth, 0.4f).set (chorusMix, 0.3f)
            .set (env1Attack, 0.02f).set (env1Decay, 0.6f).set (env1Sustain, 0.85f).set (env1Release, 0.35f)
            .set (reverbMix, 0.1f)
            .set (lfo1Rate, 0.25f)
            .route ("lfo1", "oscAMorph", 0.35f)
            .map (0, "filterCutoff", 0.55f)
            .map (1, "lfo1Rate", 0.5f).map (1, "chorusRate", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "oscAWidth", 0.4f).map (3, "oscADetune", 0.45f)
            .done());

        // Knuckle — short punchy knock bass: harmonic-rise wave, LP12, hard
        // filter knock, velocity into cutoff and bus drive.
        bank.push_back (Build ("Knuckle", "Bass")
            .macros (0.45f, 0.25f, 0.10f, 0.50f)
            .set (oscATable, kRise).set (oscAMorph, 0.3f)
            .set (oscAUnison, 1.0f).set (oscALevel, 0.8f)
            .set (masterGain, 3.0f)
            .set (subWave, kSubSquare).set (subLevel, 0.3f)
            .set (filterMode, kLp12).set (filterCutoff, 900.0f).set (filterRes, 0.25f)
            .set (filterDrive, 4.0f)
            .set (filterEnvAmount, 0.55f).set (env2Decay, 0.06f).set (env2Sustain, 0.1f)
            .set (env1Attack, 0.001f).set (env1Decay, 0.3f).set (env1Sustain, 0.6f).set (env1Release, 0.1f)
            .set (driveEnabled, 1.0f).set (driveAmount, 8.0f).set (driveTone, 0.1f)
            .set (reverbMix, 0.04f)
            .set (lfo1Rate, 4.0f)
            .route ("velocity", "filterCutoff", 0.3f)
            .route ("velocity", "driveAmount", 0.2f)
            .route ("lfo1", "filterCutoff", 0.06f)
            .map (0, "filterCutoff", 0.6f).map (0, "driveTone", 0.5f)
            .map (1, "lfo1Rate", 0.5f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.45f).map (3, "driveAmount", 0.3f)
            .done());

        // Tape Bass — soft lo-fi bass: dark tilt, slow chorus wow, pink hiss
        // and a lazy pitch drift, like a worn cassette.
        bank.push_back (Build ("Tape Bass", "Bass")
            .macros (0.25f, 0.30f, 0.15f, 0.45f)
            .set (oscAMorph, 0.42f).set (oscAUnison, 1.0f).set (oscALevel, 0.85f)
            .set (subWave, kSubTri).set (subLevel, 0.3f)
            .set (filterMode, kLp12).set (filterCutoff, 650.0f).set (filterRes, 0.1f)
            .set (driveEnabled, 1.0f).set (driveAmount, 3.0f).set (driveTone, -0.5f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.28f).set (chorusDepth, 0.55f).set (chorusMix, 0.25f)
            .set (noiseType, kPink).set (noiseLevel, -44.0f)
            .set (env1Attack, 0.008f).set (env1Decay, 0.45f).set (env1Sustain, 0.85f).set (env1Release, 0.16f)
            .set (reverbMix, 0.06f)
            .set (lfo1Rate, 0.4f)
            .route ("lfo1", "oscAFine", 0.03f)
            .map (0, "filterCutoff", 0.5f).map (0, "driveTone", 0.4f)
            .map (1, "lfo1Rate", 0.5f).map (1, "chorusDepth", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.5f).map (3, "chorusMix", 0.3f)
            .done());

        // ================= LEADS ========================================
        // Laser — zap lead: a fast free envelope drops brightness and pitch
        // on every note; ping-pong eighths echo the zap.
        bank.push_back (Build ("Laser", "Leads")
            .macros (0.70f, 0.65f, 0.35f, 0.30f)
            .set (oscATable, kRise).set (oscAMorph, 0.9f)
            .set (oscAUnison, 2.0f).set (oscADetune, 10.0f).set (oscAWidth, 0.5f).set (oscALevel, 0.75f)
            .set (masterGain, 3.0f)
            .set (filterMode, kLp24).set (filterCutoff, 9000.0f).set (filterRes, 0.2f)
            .set (env3Attack, 0.001f).set (env3Decay, 0.16f).set (env3Sustain, 0.0f).set (env3Release, 0.1f)
            .set (env1Attack, 0.001f).set (env1Decay, 0.4f).set (env1Sustain, 0.75f).set (env1Release, 0.12f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv18).set (delayPingPong, 1.0f)
            .set (delayFeedback, 0.3f).set (delayMix, 0.22f)
            .set (reverbMix, 0.1f)
            .set (lfo1Rate, 6.0f)
            .route ("env3", "oscAMorph", -0.65f)
            .route ("env3", "oscAFine", -0.25f)
            .route ("lfo1", "oscAMorph", 0.15f)
            .map (0, "filterCutoff", 0.5f).map (0, "filterDrive", 0.3f)
            .map (1, "lfo1Rate", 0.5f)
            .map (2, "delayMix", 0.5f, 1.6f).map (2, "reverbMix", 0.35f, 1.6f)
            .map (3, "oscADetune", 0.5f).map (3, "oscAWidth", 0.35f)
            .done());

        // Glass Whistle — near-sine whistle with delayed vibrato and a
        // breath-noise bed; Texture adds air.
        bank.push_back (Build ("Glass Whistle", "Leads")
            .macros (0.65f, 0.40f, 0.45f, 0.20f)
            .set (oscAMorph, 0.03f).set (oscAUnison, 1.0f).set (oscALevel, 0.8f)
            .set (filterMode, kLp12).set (filterCutoff, 8000.0f).set (filterRes, 0.05f)
            .set (noiseLevel, -52.0f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.4f).set (chorusDepth, 0.3f).set (chorusMix, 0.15f)
            .set (env1Attack, 0.04f).set (env1Decay, 0.3f).set (env1Sustain, 0.9f).set (env1Release, 0.25f)
            .set (reverbMix, 0.22f).set (reverbSize, 0.6f)
            .set (lfo1Rate, 5.2f).set (lfo1Fade, 0.5f)
            .route ("lfo1", "oscAFine", 0.035f)
            .map (0, "filterCutoff", 0.45f).map (0, "filterRes", 0.2f)
            .map (1, "lfo1Rate", 0.4f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.55f).map (3, "chorusDepth", 0.4f)
            .done());

        // Saw Hero — seven-lane supersaw with a sub-octave saw bed; quarter
        // delay and a slow filter shimmer. The anthem patch.
        bank.push_back (Build ("Saw Hero", "Leads")
            .macros (0.75f, 0.35f, 0.45f, 0.60f)
            .set (oscAMorph, 0.72f)
            .set (oscAUnison, 7.0f).set (oscADetune, 16.0f).set (oscAWidth, 0.95f)
            .set (oscABlend, 0.65f).set (oscALevel, 0.8f)
            .set (masterGain, 4.0f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 0.72f).set (oscBSemi, -12.0f)
            .set (oscBLevel, 0.35f).set (oscBUnison, 2.0f).set (oscBDetune, 6.0f)
            .set (filterMode, kLp24).set (filterCutoff, 7000.0f).set (filterRes, 0.15f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv14).set (delayFeedback, 0.4f).set (delayMix, 0.18f)
            .set (reverbMix, 0.18f)
            .set (env1Attack, 0.004f).set (env1Decay, 0.5f).set (env1Sustain, 0.9f).set (env1Release, 0.2f)
            .set (lfo1Rate, 0.8f)
            .route ("lfo1", "filterCutoff", 0.05f)
            .map (0, "filterCutoff", 0.55f).map (0, "filterDrive", 0.3f)
            .map (1, "lfo1Rate", 0.5f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.4f, 1.6f)
            .map (3, "oscADetune", 0.6f).map (3, "oscABlend", 0.3f)
            .done());

        // Vapor — hazy PWM lead: slow duty sweep, deep chorus, washed
        // quarter echoes. Soft attack, dreamwave colors.
        bank.push_back (Build ("Vapor", "Leads")
            .macros (0.45f, 0.40f, 0.60f, 0.35f)
            .set (oscATable, kPwm).set (oscAMorph, 0.35f)
            .set (oscAUnison, 2.0f).set (oscADetune, 12.0f).set (oscAWidth, 0.7f).set (oscALevel, 1.0f)
            .set (masterGain, 6.0f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 0.1f).set (oscBSemi, -12.0f).set (oscBLevel, 0.5f)
            .set (filterMode, kLp24).set (filterCutoff, 4500.0f).set (filterRes, 0.2f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.4f).set (chorusDepth, 0.5f).set (chorusMix, 0.25f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv14).set (delayFeedback, 0.45f).set (delayMix, 0.2f)
            .set (reverbMix, 0.25f).set (reverbSize, 0.7f)
            .set (noiseType, kPink).set (noiseLevel, -50.0f)
            .set (env1Attack, 0.04f).set (env1Decay, 0.5f).set (env1Sustain, 0.85f).set (env1Release, 0.5f)
            .set (lfo1Rate, 0.5f)
            .route ("lfo1", "oscAMorph", 0.2f)
            .map (0, "filterCutoff", 0.5f).map (0, "filterRes", 0.2f)
            .map (1, "lfo1Rate", 0.5f).map (1, "chorusDepth", 0.3f)
            .map (2, "reverbMix", 0.55f, 1.6f).map (2, "delayMix", 0.4f, 1.6f)
            .map (3, "oscADetune", 0.5f).map (3, "noiseLevel", 0.4f)
            .done());

        // Chrome — metallic formant lead with a hard square underlay,
        // driven filter and triplet echoes; vibrato fades in late.
        bank.push_back (Build ("Chrome", "Leads")
            .macros (0.70f, 0.45f, 0.30f, 0.55f)
            .set (oscATable, kFormant).set (oscAMorph, 0.75f)
            .set (oscAUnison, 2.0f).set (oscADetune, 9.0f).set (oscAWidth, 0.5f).set (oscALevel, 0.9f)
            .set (masterGain, 4.5f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 1.0f).set (oscBFine, 6.0f).set (oscBLevel, 0.4f)
            .set (filterMode, kLp12).set (filterCutoff, 4500.0f).set (filterRes, 0.35f)
            .set (filterDrive, 6.0f)
            .set (driveEnabled, 1.0f).set (driveAmount, 8.0f).set (driveTone, 0.3f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv18T).set (delayFeedback, 0.3f).set (delayMix, 0.18f)
            .set (filterEnvAmount, 0.3f).set (env2Decay, 0.12f).set (env2Sustain, 0.4f)
            .set (env1Attack, 0.002f).set (env1Decay, 0.4f).set (env1Sustain, 0.8f).set (env1Release, 0.15f)
            .set (reverbMix, 0.12f)
            .set (lfo1Rate, 5.5f).set (lfo1Fade, 0.6f)
            .route ("lfo1", "oscAFine", 0.03f)
            .map (0, "filterCutoff", 0.55f).map (0, "driveAmount", 0.4f)
            .map (1, "lfo1Rate", 0.4f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "oscBLevel", 0.4f).map (3, "oscADetune", 0.4f)
            .done());

        // Solar Flare — big aggressive lead: full harmonic-rise stack over a
        // detuned PWM sub-layer, hot drive, air noise, twin LFOs burning.
        bank.push_back (Build ("Solar Flare", "Leads")
            .macros (0.80f, 0.60f, 0.50f, 0.70f)
            .set (oscATable, kRise).set (oscAMorph, 1.0f)
            .set (oscAUnison, 5.0f).set (oscADetune, 22.0f).set (oscAWidth, 0.9f).set (oscALevel, 0.85f)
            .set (masterGain, 6.0f)
            .set (oscBEnabled, 1.0f).set (oscBTable, kPwm).set (oscBMorph, 0.3f)
            .set (oscBSemi, -12.0f).set (oscBLevel, 0.55f).set (oscBUnison, 2.0f).set (oscBDetune, 8.0f)
            .set (filterMode, kLp24).set (filterCutoff, 6000.0f).set (filterRes, 0.25f)
            .set (filterDrive, 5.0f)
            .set (driveEnabled, 1.0f).set (driveAmount, 10.0f).set (driveTone, 0.1f)
            .set (noiseLevel, -38.0f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv14).set (delayFeedback, 0.35f).set (delayMix, 0.15f)
            .set (reverbMix, 0.25f).set (reverbSize, 0.7f)
            .set (env1Attack, 0.005f).set (env1Decay, 0.6f).set (env1Sustain, 0.85f).set (env1Release, 0.3f)
            .set (lfo1Rate, 6.5f).set (lfo2Rate, 0.2f)
            .route ("lfo1", "oscAMorph", 0.12f)
            .route ("lfo2", "filterCutoff", 0.08f)
            .map (0, "filterCutoff", 0.5f).map (0, "driveAmount", 0.4f)
            .map (1, "lfo1Rate", 0.5f).map (1, "lfo2Rate", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "noiseLevel", 0.5f).map (3, "oscADetune", 0.4f)
            .done());

        // ================= PADS =========================================
        // Neon Tide — THE default patch: wide morphing saws over a
        // fine-detuned PWM bed and sine floor, two counter-phase tide LFOs,
        // ping-pong halves and a big bright hall.
        bank.push_back (Build ("Neon Tide", "Pads")
            .macros (0.55f, 0.45f, 0.55f, 0.35f)
            .set (oscAMorph, 0.55f)
            .set (oscAUnison, 5.0f).set (oscADetune, 13.0f).set (oscAWidth, 0.9f)
            .set (oscABlend, 0.55f).set (oscALevel, 0.85f)
            .set (masterGain, 5.0f)
            .set (oscBEnabled, 1.0f).set (oscBTable, kPwm).set (oscBMorph, 0.45f)
            .set (oscBFine, 9.0f).set (oscBLevel, 0.6f).set (oscBUnison, 2.0f)
            .set (oscBDetune, 7.0f).set (oscBWidth, 0.8f)
            .set (subWave, kSubSine).set (subLevel, 0.3f)
            .set (filterMode, kLp24).set (filterCutoff, 2400.0f).set (filterRes, 0.12f)
            .set (env1Attack, 0.35f).set (env1Decay, 1.0f).set (env1Sustain, 0.85f).set (env1Release, 1.6f)
            .set (filterEnvAmount, 0.25f)
            .set (env2Attack, 0.8f).set (env2Decay, 1.2f).set (env2Sustain, 0.6f).set (env2Release, 1.0f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.3f).set (chorusDepth, 0.45f).set (chorusMix, 0.35f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv12).set (delayPingPong, 1.0f)
            .set (delayFeedback, 0.45f).set (delayDamp, 6000.0f).set (delayMix, 0.14f)
            .set (reverbSize, 0.8f).set (reverbDamp, 0.35f).set (reverbMix, 0.3f)
            .set (noiseType, kPink).set (noiseLevel, -52.0f)
            .set (lfo1Rate, 0.13f).set (lfo2Rate, 0.09f).set (lfo2Phase, 90.0f)
            .route ("lfo1", "oscAMorph", 0.4f)
            .route ("lfo2", "oscBMorph", 0.3f)
            .map (0, "filterCutoff", 0.5f).map (0, "filterRes", 0.15f)
            .map (1, "lfo1Rate", 0.45f).map (1, "lfo2Rate", 0.45f).map (1, "chorusRate", 0.25f)
            .map (2, "reverbMix", 0.55f, 1.6f).map (2, "delayMix", 0.4f, 1.6f).map (2, "reverbSize", 0.3f)
            .map (3, "oscADetune", 0.5f).map (3, "noiseLevel", 0.45f).map (3, "oscAWidth", 0.25f)
            .done());

        // Slow Aurora — a 16-second harmonic sunrise: the rise table sweeps
        // up one partial at a time under a huge soft hall.
        bank.push_back (Build ("Slow Aurora", "Pads")
            .macros (0.40f, 0.25f, 0.65f, 0.25f)
            .set (oscATable, kRise).set (oscAMorph, 0.2f)
            .set (oscAUnison, 4.0f).set (oscADetune, 12.0f).set (oscAWidth, 0.85f).set (oscALevel, 0.85f)
            .set (masterGain, 5.5f)
            .set (subWave, kSubSine).set (subLevel, 0.25f)
            .set (filterMode, kLp24).set (filterCutoff, 3000.0f).set (filterRes, 0.15f)
            .set (filterEnvAmount, 0.2f).set (env2Attack, 1.5f).set (env2Decay, 2.0f).set (env2Sustain, 0.7f)
            .set (env1Attack, 0.9f).set (env1Decay, 1.5f).set (env1Sustain, 0.8f).set (env1Release, 2.2f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.25f).set (chorusDepth, 0.4f).set (chorusMix, 0.3f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv12D).set (delayFeedback, 0.4f).set (delayMix, 0.1f)
            .set (reverbSize, 0.85f).set (reverbDamp, 0.3f).set (reverbMix, 0.38f)
            .set (noiseLevel, -55.0f)
            .set (lfo1Rate, 0.06f)
            .route ("lfo1", "oscAMorph", 0.55f)
            .map (0, "filterCutoff", 0.5f)
            .map (1, "lfo1Rate", 0.6f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "oscADetune", 0.45f).map (3, "noiseLevel", 0.4f)
            .done());

        // Warm Fog — dark blanket pad: closed LP24, pink-noise fog, slow
        // breathing filter. Nothing bright survives.
        bank.push_back (Build ("Warm Fog", "Pads")
            .macros (0.20f, 0.30f, 0.55f, 0.45f)
            .set (oscAMorph, 0.3f)
            .set (oscAUnison, 4.0f).set (oscADetune, 10.0f).set (oscAWidth, 0.75f).set (oscALevel, 0.85f)
            .set (masterGain, 4.0f)
            .set (subWave, kSubSine).set (subLevel, 0.35f)
            .set (noiseType, kPink).set (noiseLevel, -40.0f)
            .set (filterMode, kLp24).set (filterCutoff, 950.0f).set (filterRes, 0.08f)
            .set (env1Attack, 0.6f).set (env1Decay, 1.0f).set (env1Sustain, 0.9f).set (env1Release, 1.8f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.2f).set (chorusDepth, 0.5f).set (chorusMix, 0.3f)
            .set (reverbSize, 0.7f).set (reverbDamp, 0.7f).set (reverbMix, 0.3f)
            .set (lfo1Rate, 0.1f)
            .route ("lfo1", "filterCutoff", 0.07f)
            .map (0, "filterCutoff", 0.45f).map (0, "filterRes", 0.12f)
            .map (1, "lfo1Rate", 0.5f).map (1, "chorusDepth", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.5f).map (3, "oscADetune", 0.35f)
            .done());

        // Choir Ghost — formant vowels drifting under a dark choir floor;
        // church-sized hall, breath in the highs.
        bank.push_back (Build ("Choir Ghost", "Pads")
            .macros (0.35f, 0.35f, 0.65f, 0.30f)
            .set (oscATable, kFormant).set (oscAMorph, 0.45f)
            .set (oscAUnison, 4.0f).set (oscADetune, 9.0f).set (oscAWidth, 0.8f).set (oscALevel, 0.85f)
            .set (masterGain, 5.0f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 0.1f).set (oscBSemi, -12.0f).set (oscBLevel, 0.4f)
            .set (filterMode, kLp24).set (filterCutoff, 1900.0f).set (filterRes, 0.2f)
            .set (env1Attack, 0.5f).set (env1Decay, 1.2f).set (env1Sustain, 0.85f).set (env1Release, 1.9f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.3f).set (chorusDepth, 0.35f).set (chorusMix, 0.3f)
            .set (reverbSize, 0.85f).set (reverbDamp, 0.4f).set (reverbMix, 0.36f)
            .set (noiseType, kPink).set (noiseLevel, -54.0f)
            .set (lfo1Rate, 0.08f)
            .route ("lfo1", "oscAMorph", 0.45f)
            .map (0, "filterCutoff", 0.5f)
            .map (1, "lfo1Rate", 0.55f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.5f).map (3, "oscADetune", 0.4f)
            .done());

        // Polar Drift — icy wide pad: bright thin PWM lanes, a glassy sine
        // an octave up, cold ping-pong halves and a dark-damped hall.
        bank.push_back (Build ("Polar Drift", "Pads")
            .macros (0.65f, 0.30f, 0.60f, 0.40f)
            .set (oscATable, kPwm).set (oscAMorph, 0.7f)
            .set (oscAUnison, 6.0f).set (oscADetune, 18.0f).set (oscAWidth, 1.0f)
            .set (oscABlend, 0.6f).set (oscALevel, 0.75f)
            .set (masterGain, 5.0f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 0.05f).set (oscBSemi, 12.0f).set (oscBLevel, 0.35f)
            .set (filterMode, kLp24).set (filterCutoff, 5500.0f).set (filterRes, 0.18f)
            .set (env1Attack, 0.4f).set (env1Decay, 1.0f).set (env1Sustain, 0.85f).set (env1Release, 2.0f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv12).set (delayPingPong, 1.0f)
            .set (delayFeedback, 0.5f).set (delayDamp, 4000.0f).set (delayMix, 0.18f)
            .set (reverbSize, 0.8f).set (reverbDamp, 0.2f).set (reverbMix, 0.32f)
            .set (noiseLevel, -50.0f)
            .set (lfo1Rate, 0.11f).set (lfo2Rate, 0.07f)
            .route ("lfo1", "oscAMorph", 0.35f)
            .route ("lfo2", "oscBFine", 0.04f)
            .map (0, "filterCutoff", 0.5f).map (0, "filterRes", 0.15f)
            .map (1, "lfo1Rate", 0.5f).map (1, "lfo2Rate", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.4f, 1.6f)
            .map (3, "oscABlend", 0.35f).map (3, "noiseLevel", 0.45f)
            .done());

        // Amber Haze — mellow dusk pad: warm tilted drive, a formant vowel
        // glowing underneath, everything slightly out of focus.
        bank.push_back (Build ("Amber Haze", "Pads")
            .macros (0.30f, 0.35f, 0.50f, 0.40f)
            .set (oscAMorph, 0.48f)
            .set (oscAUnison, 4.0f).set (oscADetune, 11.0f).set (oscAWidth, 0.8f).set (oscALevel, 0.8f)
            .set (masterGain, 4.0f)
            .set (oscBEnabled, 1.0f).set (oscBTable, kFormant).set (oscBMorph, 0.3f).set (oscBLevel, 0.3f)
            .set (filterMode, kLp12).set (filterCutoff, 1600.0f).set (filterRes, 0.1f)
            .set (filterDrive, 2.0f)
            .set (driveEnabled, 1.0f).set (driveAmount, 3.0f).set (driveTone, -0.35f)
            .set (env1Attack, 0.3f).set (env1Decay, 0.9f).set (env1Sustain, 0.88f).set (env1Release, 1.5f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.3f).set (chorusDepth, 0.4f).set (chorusMix, 0.35f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv14).set (delayFeedback, 0.35f).set (delayMix, 0.08f)
            .set (reverbSize, 0.7f).set (reverbDamp, 0.5f).set (reverbMix, 0.28f)
            .set (noiseType, kPink).set (noiseLevel, -50.0f)
            .set (lfo1Rate, 0.14f)
            .route ("lfo1", "oscBMorph", 0.3f)
            .map (0, "filterCutoff", 0.5f).map (0, "driveTone", 0.35f)
            .map (1, "lfo1Rate", 0.5f).map (1, "chorusRate", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "noiseLevel", 0.45f).map (3, "oscADetune", 0.4f)
            .done());

        // ================= KEYS / PLUCKS ================================
        // Dial Tone — two pure sines a fourth apart, straight off the hook;
        // Texture beats them against each other.
        bank.push_back (Build ("Dial Tone", "Keys")
            .macros (0.50f, 0.55f, 0.25f, 0.15f)
            .set (oscAMorph, 0.0f).set (oscAUnison, 1.0f).set (oscALevel, 0.7f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 0.0f).set (oscBSemi, 4.0f).set (oscBLevel, 0.55f)
            .set (filterMode, kLp12).set (filterCutoff, 4000.0f)
            .set (env1Attack, 0.002f).set (env1Decay, 0.5f).set (env1Sustain, 0.55f).set (env1Release, 0.18f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv18).set (delayFeedback, 0.25f).set (delayMix, 0.15f)
            .set (reverbMix, 0.15f)
            .set (lfo1Shape, kSquare).set (lfo1Rate, 3.5f)
            .route ("lfo1", "oscALevel", 0.12f)
            .map (0, "filterCutoff", 0.5f)
            .map (1, "lfo1Rate", 0.5f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "oscBFine", 0.3f).map (3, "noiseLevel", 0.35f)
            .done());

        // Marble Pluck — hard bright click-pluck: resonant filter snap,
        // per-note pitch dust, tight eighth echoes.
        bank.push_back (Build ("Marble Pluck", "Keys")
            .macros (0.60f, 0.30f, 0.35f, 0.30f)
            .set (oscATable, kRise).set (oscAMorph, 0.35f)
            .set (oscAUnison, 2.0f).set (oscADetune, 6.0f).set (oscAWidth, 0.5f).set (oscALevel, 1.0f)
            .set (masterGain, 4.5f)
            .set (filterMode, kLp24).set (filterCutoff, 1400.0f).set (filterRes, 0.3f)
            .set (filterEnvAmount, 0.6f)
            .set (env2Attack, 0.001f).set (env2Decay, 0.05f).set (env2Sustain, 0.0f).set (env2Release, 0.05f)
            .set (env1Attack, 0.001f).set (env1Decay, 0.4f).set (env1Sustain, 0.0f).set (env1Release, 0.22f)
            .set (env1Curve, -0.4f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv18).set (delayFeedback, 0.3f).set (delayMix, 0.18f)
            .set (reverbMix, 0.2f).set (reverbSize, 0.55f)
            .route ("velocity", "filterCutoff", 0.3f)
            .route ("random", "oscAFine", 0.015f)
            .map (0, "filterCutoff", 0.55f)
            .map (1, "delayFeedback", 0.4f).map (1, "env2Decay", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "oscADetune", 0.5f).map (3, "noiseLevel", 0.35f)
            .done());

        // Music Box — tiny tines: a sine two octaves up rings over the
        // fundamental; keytracked brightness, darker low notes.
        bank.push_back (Build ("Music Box", "Keys")
            .macros (0.55f, 0.20f, 0.50f, 0.15f)
            .set (oscAMorph, 0.08f).set (oscAUnison, 1.0f).set (oscALevel, 0.7f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 0.12f).set (oscBSemi, 24.0f).set (oscBLevel, 0.35f)
            .set (filterMode, kLp12).set (filterCutoff, 6000.0f)
            .set (env1Attack, 0.001f).set (env1Decay, 0.9f).set (env1Sustain, 0.0f).set (env1Release, 0.6f)
            .set (env1Curve, -0.5f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv18D).set (delayFeedback, 0.25f).set (delayMix, 0.1f)
            .set (reverbMix, 0.3f).set (reverbSize, 0.6f)
            .set (lfo1Rate, 0.3f)
            .route ("keytrack", "filterCutoff", 0.3f)
            .route ("velocity", "oscALevel", 0.25f)
            .route ("lfo1", "oscBFine", 0.02f)
            .map (0, "filterCutoff", 0.45f)
            .map (1, "lfo1Rate", 0.4f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "oscBLevel", 0.4f).map (3, "noiseLevel", 0.3f)
            .done());

        // Soft EP — mellow electric piano: sine body + octave tine that
        // rises with velocity, classic chorus, slow mono autopan.
        bank.push_back (Build ("Soft EP", "Keys")
            .macros (0.40f, 0.50f, 0.35f, 0.30f)
            .set (oscAMorph, 0.15f).set (oscAUnison, 1.0f).set (oscALevel, 0.7f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 0.05f).set (oscBSemi, 12.0f).set (oscBLevel, 0.28f)
            .set (filterMode, kLp12).set (filterCutoff, 2800.0f)
            .set (env1Attack, 0.002f).set (env1Decay, 0.8f).set (env1Sustain, 0.3f).set (env1Release, 0.3f)
            .set (env1Curve, -0.3f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.35f).set (chorusDepth, 0.4f).set (chorusMix, 0.45f)
            .set (reverbMix, 0.18f)
            .set (lfo1Rate, 4.0f).set (lfo1Mode, kMono)
            .route ("velocity", "filterCutoff", 0.35f)
            .route ("velocity", "oscBLevel", 0.3f)
            .route ("lfo1", "oscAPan", 0.25f)
            .map (0, "filterCutoff", 0.5f)
            .map (1, "lfo1Rate", 0.45f).map (1, "chorusDepth", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "chorusMix", 0.4f).map (3, "oscBLevel", 0.35f)
            .done());

        // Pixel Pluck — 8-bit blip: thin pulse, sixteenth ping-pong, a
        // little per-note duty scatter for chip charm.
        bank.push_back (Build ("Pixel Pluck", "Keys")
            .macros (0.65f, 0.55f, 0.20f, 0.45f)
            .set (oscATable, kPwm).set (oscAMorph, 0.3f)
            .set (oscAUnison, 1.0f).set (oscALevel, 1.0f)
            .set (subWave, kSubSquare).set (subLevel, 0.35f)
            .set (masterGain, 6.0f)
            .set (filterMode, kLp24).set (filterCutoff, 8000.0f)
            .set (env1Attack, 0.001f).set (env1Decay, 0.3f).set (env1Sustain, 0.35f).set (env1Release, 0.09f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv116).set (delayPingPong, 1.0f)
            .set (delayFeedback, 0.5f).set (delayMix, 0.35f)
            .set (reverbMix, 0.08f)
            .set (lfo1Shape, kSquare).set (lfo1Rate, 8.0f)
            .route ("random", "oscAMorph", 0.06f)
            .route ("lfo1", "oscAMorph", 0.1f)
            .map (0, "filterCutoff", 0.5f)
            .map (1, "lfo1Rate", 0.5f).map (1, "delayFeedback", 0.3f)
            .map (2, "reverbMix", 0.4f, 1.6f).map (2, "delayMix", 0.4f, 1.6f)
            .map (3, "oscAMorph", 0.3f).map (3, "noiseLevel", 0.3f)
            .done());

        // Kalimba Dust — thumb piano with a soft thump and dusty pink air;
        // per-note pitch scatter keeps repeats organic.
        bank.push_back (Build ("Kalimba Dust", "Keys")
            .macros (0.50f, 0.30f, 0.40f, 0.35f)
            .set (oscATable, kFormant).set (oscAMorph, 0.12f)
            .set (oscAUnison, 1.0f).set (oscALevel, 1.0f)
            .set (masterGain, 3.0f)
            .set (filterMode, kLp12).set (filterCutoff, 3200.0f).set (filterRes, 0.15f)
            .set (filterEnvAmount, 0.3f)
            .set (env2Attack, 0.003f).set (env2Decay, 0.04f).set (env2Sustain, 0.0f).set (env2Release, 0.05f)
            .set (env1Attack, 0.003f).set (env1Decay, 0.75f).set (env1Sustain, 0.0f).set (env1Release, 0.45f)
            .set (env1Curve, -0.5f)
            .set (noiseType, kPink).set (noiseLevel, -48.0f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv18D).set (delayFeedback, 0.25f).set (delayMix, 0.1f)
            .set (reverbMix, 0.25f).set (reverbSize, 0.5f).set (reverbDamp, 0.6f)
            .route ("random", "oscAFine", 0.02f)
            .route ("velocity", "filterCutoff", 0.25f)
            .map (0, "filterCutoff", 0.5f)
            .map (1, "delayFeedback", 0.35f).map (1, "env2Decay", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.5f).map (3, "oscAMorph", 0.25f)
            .done());

        // Bell Garden — glassy bells: a nineteenth partial rings against
        // the fundamental with a slow beat shimmer; long triplet tails.
        bank.push_back (Build ("Bell Garden", "Keys")
            .macros (0.60f, 0.25f, 0.60f, 0.20f)
            .set (oscAMorph, 0.04f).set (oscAUnison, 1.0f).set (oscALevel, 0.6f)
            .set (oscBEnabled, 1.0f).set (oscBMorph, 0.06f).set (oscBSemi, 19.0f).set (oscBLevel, 0.4f)
            .set (filterMode, kLp12).set (filterCutoff, 7000.0f)
            .set (env1Attack, 0.001f).set (env1Decay, 1.4f).set (env1Sustain, 0.0f).set (env1Release, 1.1f)
            .set (env1Curve, -0.6f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv14T).set (delayFeedback, 0.35f).set (delayMix, 0.12f)
            .set (reverbMix, 0.35f).set (reverbSize, 0.75f)
            .set (noiseLevel, -55.0f)
            .set (lfo1Rate, 0.25f)
            .route ("lfo1", "oscBFine", 0.03f)
            .route ("velocity", "oscBLevel", 0.3f)
            .map (0, "filterCutoff", 0.45f)
            .map (1, "lfo1Rate", 0.45f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "oscBLevel", 0.45f).map (3, "noiseLevel", 0.3f)
            .done());

        // ================= TEXTURES =====================================
        // Static Bloom — pink-noise swell blooming through a resonant
        // band-pass on a slow LFO; the tone underneath is barely there.
        bank.push_back (Build ("Static Bloom", "Textures")
            .macros (0.25f, 0.40f, 0.60f, 0.75f)
            .set (noiseType, kPink).set (noiseLevel, -10.0f)
            .set (masterGain, 5.0f)
            .set (oscAMorph, 0.5f).set (oscALevel, 0.5f)
            .set (oscADetune, 12.0f).set (oscAWidth, 0.8f)
            .set (filterMode, kLp12).set (filterCutoff, 800.0f).set (filterRes, 0.5f)
            .set (env1Attack, 1.2f).set (env1Decay, 1.0f).set (env1Sustain, 0.85f).set (env1Release, 2.4f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.2f).set (chorusDepth, 0.5f).set (chorusMix, 0.35f)
            .set (reverbSize, 0.85f).set (reverbDamp, 0.3f).set (reverbMix, 0.42f)
            .set (lfo1Rate, 0.09f)
            .route ("lfo1", "filterCutoff", 0.35f)
            .map (0, "filterCutoff", 0.5f).map (0, "filterRes", 0.25f)
            .map (1, "lfo1Rate", 0.55f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.5f).map (3, "oscADetune", 0.4f)
            .done());

        // Scanline — Lens-built (factory stripe image, Spectral mode): a
        // fixed harmonic stack {2,4,8,16,32,64} droning like a CRT; a
        // saw-down LFO sweeps the filter like the beam, a square one
        // flickers the level.
        bank.push_back (Build ("Scanline", "Textures")
            .macros (0.45f, 0.60f, 0.40f, 0.55f)
            .lens ("stripes", 1 /* spectral */, 0)
            .set (oscATable, kImage).set (oscAMorph, 0.5f)
            .set (oscAUnison, 3.0f).set (oscADetune, 10.0f).set (oscAWidth, 0.8f).set (oscALevel, 0.9f)
            .set (subWave, kSubSine).set (subLevel, 0.25f)
            .set (masterGain, 6.0f)
            .set (filterMode, kLp24).set (filterCutoff, 4000.0f).set (filterRes, 0.2f)
            .set (env1Attack, 0.6f).set (env1Decay, 1.0f).set (env1Sustain, 0.85f).set (env1Release, 1.6f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv18).set (delayFeedback, 0.4f).set (delayMix, 0.15f)
            .set (reverbMix, 0.25f).set (reverbSize, 0.7f)
            .set (noiseLevel, -46.0f)
            .set (lfo1Shape, kSawDown).set (lfo1Rate, 0.4f)
            .set (lfo2Shape, kSquare).set (lfo2Rate, 6.0f)
            .route ("lfo1", "filterCutoff", 0.25f)
            .route ("lfo2", "oscALevel", 0.08f)
            .map (0, "filterCutoff", 0.5f)
            .map (1, "lfo1Rate", 0.5f).map (1, "lfo2Rate", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "oscADetune", 0.5f).map (3, "noiseLevel", 0.4f)
            .done());

        // Radio Sky — shortwave band-scanning: sample & hold hops the
        // formant vowel like tuning stations, white static behind a
        // resonant band-pass drifting across the dial.
        bank.push_back (Build ("Radio Sky", "Textures")
            .macros (0.40f, 0.70f, 0.45f, 0.80f)
            .set (oscATable, kFormant).set (oscAMorph, 0.5f)
            .set (oscAUnison, 1.0f).set (oscALevel, 1.0f)
            .set (noiseLevel, -3.0f)
            .set (masterGain, 6.0f)
            .set (filterMode, kBp12).set (filterCutoff, 1600.0f).set (filterRes, 0.35f)
            .set (env1Attack, 0.4f).set (env1Decay, 1.0f).set (env1Sustain, 0.8f).set (env1Release, 1.2f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv14).set (delayFeedback, 0.55f)
            .set (delayDamp, 3000.0f).set (delayMix, 0.22f)
            .set (reverbMix, 0.3f)
            .set (lfo1Shape, kSampleHold).set (lfo1Rate, 2.5f)
            .set (lfo2Rate, 0.07f)
            .route ("lfo1", "oscAMorph", 0.45f)
            .route ("lfo2", "filterCutoff", 0.3f)
            .map (0, "filterCutoff", 0.5f).map (0, "filterRes", 0.2f)
            .map (1, "lfo1Rate", 0.5f).map (1, "lfo2Rate", 0.35f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "noiseLevel", 0.5f).map (3, "delayFeedback", 0.3f)
            .done());

        // Machine Hum — the room tone of an imaginary server hall: low PWM
        // drone an octave down, sub floor, slow duty churn and an 8 Hz
        // throb.
        bank.push_back (Build ("Machine Hum", "Textures")
            .macros (0.15f, 0.45f, 0.30f, 0.50f)
            .set (oscATable, kPwm).set (oscAMorph, 0.6f).set (oscASemi, -12.0f)
            .set (oscAUnison, 2.0f).set (oscADetune, 5.0f).set (oscAWidth, 0.4f).set (oscALevel, 0.85f)
            .set (masterGain, 4.0f)
            .set (subWave, kSubSine).set (subLevel, 0.45f)
            .set (filterMode, kLp24).set (filterCutoff, 480.0f).set (filterRes, 0.2f)
            .set (filterDrive, 5.0f)
            .set (noiseType, kPink).set (noiseLevel, -42.0f)
            .set (env1Attack, 0.7f).set (env1Decay, 1.0f).set (env1Sustain, 0.9f).set (env1Release, 1.4f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.15f).set (chorusDepth, 0.6f).set (chorusMix, 0.25f)
            .set (reverbMix, 0.18f).set (reverbDamp, 0.8f)
            .set (lfo1Shape, kTriangle).set (lfo1Rate, 0.3f)
            .set (lfo2Rate, 8.0f)
            .route ("lfo1", "oscAMorph", 0.2f)
            .route ("lfo2", "oscALevel", 0.05f)
            .map (0, "filterCutoff", 0.45f).map (0, "filterDrive", 0.3f)
            .map (1, "lfo1Rate", 0.5f).map (1, "lfo2Rate", 0.3f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "noiseLevel", 0.45f).map (3, "oscADetune", 0.35f)
            .done());

        // Wind Tunnel — white noise through a howling band-pass: one LFO
        // gusts the cutoff, another leans on the resonance; a faint
        // detuned tone whistles inside the airstream.
        bank.push_back (Build ("Wind Tunnel", "Textures")
            .macros (0.30f, 0.55f, 0.55f, 0.85f)
            .set (noiseLevel, 0.0f)
            .set (masterGain, 6.0f)
            .set (oscAMorph, 0.4f).set (oscALevel, 0.3f)
            .set (oscADetune, 20.0f).set (oscAWidth, 1.0f)
            .set (filterMode, kBp12).set (filterCutoff, 900.0f).set (filterRes, 0.3f)
            .set (env1Attack, 0.9f).set (env1Decay, 1.0f).set (env1Sustain, 0.85f).set (env1Release, 2.0f)
            .set (reverbSize, 0.8f).set (reverbMix, 0.35f)
            .set (lfo1Rate, 0.16f).set (lfo2Rate, 0.11f)
            .route ("lfo1", "filterCutoff", 0.45f)
            .route ("lfo2", "filterRes", 0.2f)
            .map (0, "filterCutoff", 0.5f).map (0, "filterRes", 0.25f)
            .map (1, "lfo1Rate", 0.55f).map (1, "lfo2Rate", 0.35f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.3f, 1.6f)
            .map (3, "oscALevel", 0.35f).map (3, "oscADetune", 0.3f)
            .done());

        // Photograph — Lens-built (factory gradient image, Scan mode): each
        // frame is one harmonic of the source rows, so the slow morph LFO
        // develops the picture as a rising partial glide.
        bank.push_back (Build ("Photograph", "Textures")
            .macros (0.50f, 0.20f, 0.55f, 0.30f)
            .lens ("gradient", 0 /* scan */, 0)
            .set (oscATable, kImage).set (oscAMorph, 0.08f)
            .set (oscAUnison, 3.0f).set (oscADetune, 9.0f).set (oscAWidth, 0.8f).set (oscALevel, 1.0f)
            .set (subWave, kSubSine).set (subLevel, 0.3f)
            .set (masterGain, 6.0f)
            .set (filterMode, kLp24).set (filterCutoff, 8000.0f).set (filterRes, 0.15f)
            .set (env1Attack, 0.8f).set (env1Decay, 1.2f).set (env1Sustain, 0.85f).set (env1Release, 2.0f)
            .set (chorusEnabled, 1.0f).set (chorusRate, 0.25f).set (chorusDepth, 0.4f).set (chorusMix, 0.3f)
            .set (delayEnabled, 1.0f).set (delayDiv, kDiv12).set (delayFeedback, 0.4f).set (delayMix, 0.1f)
            .set (reverbSize, 0.8f).set (reverbMix, 0.35f)
            .set (noiseType, kPink).set (noiseLevel, -52.0f)
            .set (lfo1Rate, 0.06f)
            .route ("lfo1", "oscAMorph", 0.12f)
            .map (0, "filterCutoff", 0.5f)
            .map (1, "lfo1Rate", 0.6f)
            .map (2, "reverbMix", 0.5f, 1.6f).map (2, "delayMix", 0.35f, 1.6f)
            .map (3, "oscADetune", 0.45f).map (3, "noiseLevel", 0.4f)
            .done());

        // Init — the shipped starting point: engine defaults + the stock
        // modulation set (seeded by ensureTrees, mirrored by
        // applyInitModDefaults). No explicit settings on purpose.
        {
            Build init ("Init", "Textures");
            init.p.initMods = true;
            bank.push_back (init.done());
        }

        jassert (static_cast<int> (bank.size()) == kNumPresets);
        return bank;
    }
} // namespace

const std::vector<FactoryPreset>& bank()
{
    static const std::vector<FactoryPreset> presets = makeBank();
    return presets;
}

const juce::StringArray& categories()
{
    static const juce::StringArray order { "Bass", "Leads", "Pads", "Keys", "Textures" };
    return order;
}

const FactoryPreset* find (const juce::String& name)
{
    for (const auto& preset : bank())
        if (name.equalsIgnoreCase (preset.name))
            return &preset;
    return nullptr;
}

float macroPosition (const FactoryPreset& preset, int macroIndex)
{
    static constexpr const char* ids[4] = { params::macro1, params::macro2,
                                            params::macro3, params::macro4 };
    static constexpr float defaults[4] = { 0.5f, 0.5f, 0.3f, 0.2f };
    const int m = juce::jlimit (0, 3, macroIndex);
    for (const auto& setting : preset.settings)
        if (juce::String (setting.id) == ids[m])
            return juce::jlimit (0.0f, 1.0f, setting.value);
    return defaults[m];
}

void macroMapRange (const FactoryPreset& preset, const MacroMap& map,
                    float& rangeMin, float& rangeMax)
{
    const float position = macroPosition (preset, map.macro);
    // `shaped` mirrors mod::macroMapOffset exactly (same float pow), which
    // computes offset = min + (max - min) * shaped. The at-rest offset must
    // be EXACTLY 0.0f (a mapped destination renders denormalize(normalize(
    // base) + offset), so only a bit-exact zero keeps the designed sound
    // bit-identical across recalibrations). min = -(d * shaped) cancels by
    // construction; the only failure mode is (min + d) - min not rounding
    // back to d, cured by trying the span an ulp at a time.
    const float shaped = map.curve == 1.0f ? position : std::pow (position, map.curve);
    float d = map.span;
    for (int guard = 0; guard < 16; ++guard)
    {
        const float atRest = d * shaped;
        rangeMin = -atRest;
        rangeMax = rangeMin + d;
        if ((rangeMax - rangeMin) * shaped == atRest)
            break;
        d = std::nextafter (d, 0.0f);
    }
    jassert ((rangeMax - rangeMin) * shaped == -rangeMin); // offset == 0 exactly
    jassert (rangeMin >= -1.0f && rangeMax <= 1.0f);       // spans stay well inside +-1
}

void applyToEngine (const FactoryPreset& preset, EngineParams& params)
{
    for (const auto& setting : preset.settings)
    {
        [[maybe_unused]] const bool known = bindings::set (params, setting.id, setting.value);
        jassert (known); // covered by the bank validity unit test
    }

    params.mod = {};
    if (preset.initMods)
    {
        modstate::applyInitModDefaults (params.mod);
        return;
    }

    int slot = 0;
    for (const auto& route : preset.routes)
    {
        if (slot >= mod::kNumSlots)
            break;
        auto& s = params.mod.slots[slot++];
        s.source = modstate::sourceFromToken (route.source);
        s.dest = modstate::destFromToken (route.dest);
        s.depth = route.depth;
        s.enabled = s.source >= 0 && s.dest >= 0;
        jassert (s.enabled); // covered by the bank validity unit test
    }

    int mapCount[mod::kNumMacros] {};
    for (const auto& map : preset.maps)
    {
        const int m = juce::jlimit (0, mod::kNumMacros - 1, map.macro);
        const int dest = modstate::destFromToken (map.dest);
        if (dest < 0 || mapCount[m] >= mod::kMaxMacroMaps)
        {
            jassertfalse; // covered by the bank validity unit test
            continue;
        }
        auto& mm = params.mod.macroMaps[m][mapCount[m]++];
        mm.dest = dest;
        macroMapRange (preset, map, mm.rangeMin, mm.rangeMax);
        mm.curve = map.curve;
    }
}

std::vector<float> buildLensFrames (const FactoryPreset& preset)
{
    if (! preset.hasLens())
        return {};
    const auto analysis = lens::analyzeImage (lens::testimages::byName (preset.lens.image));
    if (! analysis.valid)
        return {};
    return lens::buildFrames (analysis, preset.lens.mode == 1 ? lens::Mode::spectral
                                                              : lens::Mode::scan);
}

juce::ValueTree buildState (const FactoryPreset& preset,
                            const juce::AudioProcessorValueTreeState& apvts)
{
    juce::ValueTree state (apvts.state.getType());
    state.setProperty ("stateVersion", params::kStateVersion, nullptr);
    state.setProperty ("presetName", juce::String (preset.name), nullptr);
    state.setProperty ("presetCategory", juce::String (preset.category), nullptr);
    state.setProperty ("presetAuthor", juce::String (kFactoryAuthor), nullptr);

    // Every parameter explicitly (a PARAM child APVTS::replaceState will
    // apply), defaults + overrides snapped onto each range's grid so the
    // stored value is exactly what the parameter will hold.
    for (auto* raw : apvts.processor.getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (raw);
        if (parameter == nullptr)
            continue;

        float value = parameter->convertFrom0to1 (parameter->getDefaultValue());
        for (const auto& setting : preset.settings)
            if (parameter->paramID == setting.id)
                value = parameter->convertFrom0to1 (parameter->convertTo0to1 (setting.value));

        juce::ValueTree node ("PARAM");
        node.setProperty ("id", parameter->paramID, nullptr);
        node.setProperty ("value", static_cast<double> (value), nullptr);
        state.appendChild (node, nullptr);
    }

    // Matrix + macro trees, complete (all 24 slots / 4 macros) so loading
    // never re-seeds the Init defaults over a factory patch. Init itself
    // omits both: modstate::ensureTrees seeds the stock set on load.
    if (! preset.initMods)
    {
        juce::ValueTree matrix ("MODMATRIX");
        for (int i = 0; i < mod::kNumSlots; ++i)
        {
            juce::ValueTree slot ("SLOT");
            if (i < static_cast<int> (preset.routes.size()))
            {
                const auto& route = preset.routes[static_cast<size_t> (i)];
                slot.setProperty ("source", route.source, nullptr);
                slot.setProperty ("dest", route.dest, nullptr);
                slot.setProperty ("depth", static_cast<double> (route.depth), nullptr);
                slot.setProperty ("enabled", true, nullptr);
            }
            else
            {
                slot.setProperty ("source", "lfo1", nullptr);
                slot.setProperty ("dest", "", nullptr);
                slot.setProperty ("depth", 0.0, nullptr);
                slot.setProperty ("enabled", false, nullptr);
            }
            matrix.appendChild (slot, nullptr);
        }
        state.appendChild (matrix, nullptr);

        juce::ValueTree macros ("MACROS");
        for (int m = 0; m < mod::kNumMacros; ++m)
        {
            juce::ValueTree macro ("MACRO");
            macro.setProperty ("index", m, nullptr);
            for (const auto& map : preset.maps)
            {
                if (map.macro != m)
                    continue;
                float rangeMin = 0.0f, rangeMax = 0.0f;
                macroMapRange (preset, map, rangeMin, rangeMax);
                juce::ValueTree node ("MAP");
                node.setProperty ("dest", map.dest, nullptr);
                node.setProperty ("min", static_cast<double> (rangeMin), nullptr);
                node.setProperty ("max", static_cast<double> (rangeMax), nullptr);
                node.setProperty ("curve", static_cast<double> (map.curve), nullptr);
                macro.appendChild (node, nullptr);
            }
            macros.appendChild (macro, nullptr);
        }
        state.appendChild (macros, nullptr);
    }

    // LENS settings always present (stable round-trips); frames + thumbnail
    // only for the two Lens-built presets — generated, never a source path.
    lensstate::setMode (state, preset.lens.mode);
    lensstate::setChroma (state, false);
    lensstate::setTarget (state, preset.hasLens() ? preset.lens.targetOsc : 0);
    if (preset.hasLens())
    {
        const auto analysis = lens::analyzeImage (lens::testimages::byName (preset.lens.image));
        const auto frames = lens::buildFrames (analysis, preset.lens.mode == 1
                                                             ? lens::Mode::spectral
                                                             : lens::Mode::scan);
        const auto thumbPng = lens::encodePng (lens::makeThumbnail (analysis));
        lensstate::storeImage (state, preset.lens.targetOsc, frames, thumbPng,
                               analysis.seed, preset.lens.image);
    }

    return state;
}
} // namespace lumen::presets
