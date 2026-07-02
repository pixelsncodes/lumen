# Lumen — Product & Technical Specification (v1.0)

This is the single source of truth for the Lumen synthesizer. If code and this spec disagree, the spec wins unless the user approved a deviation — record approved deviations in `DECISIONS.md`.

## 1. What Lumen is

A wavetable synthesizer inspired by Serum's oscillator quality, Vital's visible modulation, and the OP-1 Field's immediacy. Ships as a VST3 (for Ableton Live and Maschine 3) and a Windows standalone app. Signature feature: a fully local, deterministic image-to-tone engine ("Lens").

Design pillars — consult these whenever a decision isn't specified:

1. **Instant gratification.** A first-time user gets a good sound within 30 seconds.
2. **Everything visible.** Every parameter has a live visual consequence; every modulation is drawn.
3. **One great option.** One excellent filter, one excellent reverb — not five mediocre variants.
4. **The image is the signature.** The Lens engine is a first-class citizen of the UI, never a buried menu.

## 2. Targets & tech

- Language/toolchain: C++20, MSVC (Visual Studio 2022 Build Tools), CMake >= 3.25, Windows 10/11 x64.
- Framework: JUCE 8 fetched via CMake `FetchContent`. Pin the latest stable 8.x release tag during Phase 0 and record the tag in `CLAUDE.md`.
- Formats: `juce_add_plugin(... FORMATS VST3 Standalone)`.
- Plugin metadata: `COMPANY_NAME "Lumen Audio"`, `PLUGIN_MANUFACTURER_CODE Lumo`, `PLUGIN_CODE Lmn1`, product name `Lumen`, `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`.
- VST3 install: copy-after-build to `C:\Program Files\Common Files\VST3`. Phase 0 creates `tools\grant_vst3_write.ps1` (one-time elevated run grants the current user Modify rights on that folder so normal builds can copy). If copy fails, print a clear message with the manual copy path — never fail the build silently.
- Dependencies: JUCE + the C++ standard library only. Anything else requires user approval first.
- Editor window: base 1040x660, resizable 70–200% with fixed aspect ratio. Attach a `juce::OpenGLContext` for accelerated 2D rendering, but all drawing must go through normal `paint()` code so the software path is identical (required for `--screenshot`).

## 3. Signal flow

```
Osc A ─┐
Osc B ─┤
Sub   ─┼─ per-voice mix ─ SVF filter (drive) ─ voice amp (Env 1) ─ voice sum
Noise ─┘                                                            │
             Drive → Chorus → Delay → Reverb → Limiter → master gain/meter
```

Modulation (Env 1–3, LFO 1–3, Macros 1–4, MIDI sources) routes through the mod matrix to almost any continuous parameter.

## 4. Wavetable oscillators (A and B, identical)

- Frame format: 2048 samples per frame; a table holds 1–256 frames.
- Band-limiting: on table load, build 10 mip levels per frame via FFT — level `L` keeps harmonics `1..min(1023, floor(1024 / 2^L))`. At play time, pick the highest-resolution level whose top harmonic stays below `0.45 * sampleRate` for the voice's current frequency. Linear interpolation within a frame; equal-power crossfade between adjacent frames for morphing.
- Morph position (0–1) selects/interpolates frames. This is the primary modulation target.
- Unison: 1–8 voices per oscillator; detune 0–50 cents (symmetric spread), stereo width 0–1, blend 0–1 (center vs. detuned voices), phase-randomize on/off (on = random start phase per unison voice per note).
- Factory tables, generated analytically at build or first run and stored as binary data:
  - `Basic` — sine → triangle → saw → square morph, 64 frames
  - `PWM` — pulse-width sweep, 64 frames
  - `Harmonic Rise` — one harmonic added per frame, 64 frames
  - `Formant` — two moving resonant peaks, 64 frames
  - `Image` — whatever the Lens engine last produced (per oscillator)
- Per-osc parameters: on/off, table select, morph, level, pan, semitones ±24, fine ±100 cents, unison count/detune/width/blend, phase mode.

## 5. Sub & noise

- Sub: sine / triangle / square at −1 or −2 octaves, level control. Tracks Osc A pitch (pre-detune).
- Noise: white / pink, level control.
- Both are summed into the voice pre-filter.

## 6. Filter

- Topology: trapezoidal/TPT state-variable filter (the Zavalishin/Simper SVF) — stable under fast modulation.
- Modes: LP12, LP24 (two cascaded stages), HP12, BP12, Notch.
- Parameters: cutoff 20 Hz–20 kHz (log), resonance 0–1 (mapped to Q roughly 0.5–12, soft-limited so self-oscillation stays controlled), drive 0–24 dB (tanh pre-filter, gain-compensated), key tracking 0–100%, Env 2 amount −1..+1 (bipolar, spanning ±5 octaves of cutoff).
- Env 2 is soft-wired to cutoff through this dedicated amount knob (the most common routing should be one knob, not a matrix trip). It remains matrix-assignable elsewhere too.

## 7. Envelopes (x3)

- ADSR with per-envelope curve: attack 1 ms–10 s (log), decay 1 ms–10 s (log), sustain 0–1, release 5 ms–15 s (log), curve −1..+1 (exp <-> log segment shaping).
- Env 1 → voice amplitude (fixed). Env 2 → filter via the soft-wired amount. Env 3 → free.
- Retrigger on every note-on; release runs to completion on note-off; a voice ends when its output falls below −90 dB.

## 8. LFOs (x3)

- Shapes: sine, triangle, saw up, saw down, square, sample & hold.
- Rate: free 0.01–40 Hz (log) or host-synced (4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, each with dotted and triplet variants).
- Phase offset 0–360 degrees, fade-in 0–5 s, mode poly (per voice, retriggers on note-on) or mono (global, free-running).
- Tempo read from the host playhead every block; fallback 120 BPM (the standalone exposes a tempo field in settings).

## 9. Modulation matrix & macros

- 24 slots: `{source, destination, depth −1..+1, enabled}`. Stored in plugin state (not host-automatable in v1 — the macros cover performance automation; note this in the manual).
- Sources: Env 1–3, LFO 1–3, Macro 1–4, velocity, mod wheel (CC1), channel aftertouch, pitch bend, note number (keytrack), random-per-note.
- Destinations: any continuous APVTS parameter tagged modulatable (all knobs; switches and choice parameters excluded).
- Combination: `final = clamp(base + sum(depth_i * source_i))` in normalized parameter space; per-voice for poly sources, per-block for global sources; always smoothed.
- UX (built in Phase 5): drag a source's grab-handle onto any knob to create a slot; a depth ring appears around the knob; right-click a knob to list, edit depth, or remove its modulations.
- Macros 1–4, default names Tone / Motion / Space / Texture (renamable per preset): each holds up to 8 internal mappings `{targetParam, min, max}` stored with the preset. Macros are ordinary APVTS parameters — automatable, MIDI-learnable — and are the first four VST3 parameters.

## 10. Effects (fixed order, each bypassable)

- **Drive** — tanh waveshaper: amount 0–24 dB, tone tilt −1..+1 (±6 dB tilt pivoting at 800 Hz), output gain-compensated.
- **Chorus** — two modulated delay lines (7–20 ms): rate 0.05–5 Hz, depth 0–1, mix 0–1.
- **Delay** — stereo: time free 1–2000 ms or synced divisions (same set as LFOs), feedback 0–95% with damping lowpass 1–16 kHz, ping-pong toggle, mix (this is frozen parameter #7).
- **Reverb** — wrap `juce::dsp::Reverb` as size, damping, width, mix (frozen parameter #8). Post-v1 upgrade path: custom FDN. Do not build that now.
- **Limiter** — always on, not user-facing: ceiling −0.3 dBFS, ~1.5 ms lookahead (or `juce::dsp::Limiter`), ~80 ms release.
- Master gain −inf..+6 dB; stereo peak/RMS meter tap (FIFO to UI).

## 11. Voices & global

- 16 voices. Steal the quietest releasing voice; otherwise the oldest.
- Voice modes: poly / mono / legato; glide 0–2 s (applies in mono/legato).
- Pitch bend range ±2 semitones default, settable 0–24 in the settings panel.
- `juce::ScopedNoDenormals` in `processBlock`; flush-to-zero behavior verified by tests.

## 12. Parameters (APVTS)

- All parameters live in one `AudioProcessorValueTreeState` with versioned `ParameterID`s, human-readable names ("Osc A Morph", never "oa_mrph"), correct units, and value-to-text lambdas.
- Creation order = VST3 order. **The first 8 are frozen** (they land on Maschine's first knob page):

| # | ID | Name | Range | Default |
|---|----|------|-------|---------|
| 1 | `macro1` | Tone | 0–1 | 0.5 |
| 2 | `macro2` | Motion | 0–1 | 0.5 |
| 3 | `macro3` | Space | 0–1 | 0.3 |
| 4 | `macro4` | Texture | 0–1 | 0.2 |
| 5 | `filterCutoff` | Filter Cutoff | 20–20000 Hz, log | 20000 |
| 6 | `filterRes` | Filter Resonance | 0–1 | 0.12 |
| 7 | `delayMix` | Delay Mix | 0–1 | 0.0 |
| 8 | `reverbMix` | Reverb Mix | 0–1 | 0.12 |

- Every continuous parameter is smoothed over roughly 20 ms (`SmoothedValue` or per-sample ramps). Zipper noise is a bug, not a style.
- Remaining parameters follow sections 4–11. Where a range or default is unspecified, choose the most musical option and record it in `DECISIONS.md`.

## 13. Lens — the image-to-tone engine

Fully local and deterministic: the same image file must always produce bit-identical wavetables and identical patch changes. No network, no ML models, no `rand()`, no `time()`.

Pipeline:

1. **Decode** with `juce::ImageFileFormat` (PNG and JPEG minimum). Bilinear-downscale to fit 512x512; keep a 256x256 analysis copy.
2. **Seed**: FNV-1a 64-bit hash over the downscaled ARGB bytes. All "random" phases derive from this seed.
3. **Scan mode** (default): for frame `i` in 0..63 — sample row `y = round((i + 0.5) / 64 * (h − 1))`; resample that row to 2048 points (linear); value `= 2 * luma − 1` where `luma = 0.2126 R + 0.7152 G + 0.0722 B`; subtract the mean (DC removal); if peak < 1e-4, substitute `0.05 * sin` (silence guard for flat rows); normalize peak to 0.9. Cap harmonics at 700 during the mip build to tame extreme buzz. Morphing the wavetable now travels down the image.
4. **Spectral mode**: for frame `i` (column band centered at `x = (i + 0.5) / 64 * w`) — for harmonic `h` = 1..256, sample brightness in a 3x3 patch at `(x, y(h))` where `y` maps `h` logarithmically with the image top = high frequencies; amplitude `= brightness^1.5`; phase = seeded uniform in `[0, 2π)`; inverse FFT to a 2048-sample frame; normalize to 0.9. (This is the image-as-spectrogram idea of the 1950s ANS synthesizer.)
5. **Chroma mapping** (toggle: "Set patch from colors") — computed on the 256x256 copy in HSV. Let `Hm` = circular mean hue (deg), `Sm` = mean saturation (0–1), `Vm` = mean value (0–1), `sigV` = luma standard deviation normalized 0–1, `E` = mean Sobel edge magnitude normalized 0–1, `sigH` = hue dispersion 0–1. Apply exactly:
   - Filter mode: `Hm` in [0,70) or [310,360) → LP24 (warm); [70,170) → BP12; [170,310) → LP12.
   - `filterCutoff = 250 * 2^(5.5 * Vm)` Hz. `filterRes = 0.05 + 0.55 * Sm`.
   - Osc A detune `= 3 + 30 * Sm` cents; unison `= 1 + round(5 * Sm)`.
   - Env 1 attack `= 400 * (2/400)^Vm` ms (bright image → snappy). Env 1 release `= 150 + 1350 * (1 − Vm)` ms.
   - Drive `= 20 * E` dB. Noise level `= −60 + 42 * E` dB.
   - LFO 1 → Osc A morph: depth `= 0.5 * sigH`, rate `= 0.15 + 4 * sigV` Hz, sine, poly.
   - `reverbMix = 0.10 + 0.35 * (1 − E)`. Macro 4 (Texture) default `= E`.
   These constants are the starting point; the user tunes them by ear in Phase 6.
6. Result loads into the selected oscillator's `Image` table slot and the UI switches its visualizer to the image view.

State: presets and DAW state store the **generated wavetable data plus a 64x64 PNG thumbnail (base64)** — never a path to the original file — so projects recall bit-exactly on any machine even if the photo is gone.

UI: drop an image anywhere on the window; the Lens panel shows the image with an animated scanline synced to the live morph position; Scan/Spectral toggle; "Set patch from colors" toggle; oscillator target selector (A/B).

## 14. UI

Two views in the same window (toggle top-right):

- **Play view**: preset strip; one large visualizer (scope / wavetable / image, auto-selected by context); Lens drop zone; the four macro knobs, large and color-coded; a 2-octave mouse-playable keyboard.
- **Deep view** (top to bottom):

```
[ header: logo | preset < Neon Tide > + browser | Play/Deep | out meter ]
[ Osc A card: 3D table + morph + knobs ][ Osc B card (same) ][ Sub/Noise ]
[ Filter card: spectrum + draggable filter curve ][ FX rack: 4 strips    ]
[ Env card (tabs 1-3, draggable breakpoints) ][ LFO card ][ Lens card    ]
[ footer: Macros 1-4 | mod-source grab handles | scope strip             ]
```

Component behavior (every knob/slider): vertical drag; Shift = fine (x0.1); double-click = reset to default; mouse-wheel steps; value tooltip while dragging; right-click menu = MIDI Learn / Clear MIDI / list & remove modulations. Modulated knobs draw an animated arc: static arc = base value, moving indicator = live modulated value. Automation gestures use proper begin/end notifications.

Look — OP-1 Field-inspired but original (no copied artwork or trade dress):

- Panels `#1C1C1F`, display wells `#0F0F12`, hairlines `#2A2A2E`–`#38383E`.
- Text `#E8E6E3` primary / `#9A9AA2` secondary / `#6F6F76` muted.
- Accents: orange `#FF7A45` (Osc A, Macro 1) · blue `#4AA8FF` (Osc B, Macro 2) · green `#3DDC84` (filter + FX, Macro 3) · warm gray `#C9C9CF` (modulation, Macro 4). A module's controls, visualizer traces, and mod arcs share its accent color.
- Typeface: Inter (SIL OFL — verify the license file ships with it), embedded via BinaryData; sizes 12/13/15/22. Flat design, 8–10 px corner radius, no gradients, no skeuomorphic shadows.

## 15. Visualizers

All visualizers read audio/mod data through lock-free FIFOs filled on the audio thread; they never lock or allocate against it.

- **3D wavetable stack**: up to 64 frame polylines with perspective offset; the current (possibly modulated) frame highlighted; morph animates.
- **Spectrum**: FFT 2048, Hann window, ~30 Hz update, log axis 20 Hz–20 kHz, −90..0 dB, peak-hold with decay. The filter's magnitude response (from live coefficients) is drawn on top; its node is draggable = cutoff/resonance.
- **Scope**: post-limiter, 2048-sample window, rising zero-cross trigger for a stable trace.
- **Envelope/LFO editors**: redraw from parameter values each frame; dragging breakpoints writes parameters.
- **Meter**: peak + RMS, 300 ms peak hold.
- Budget: 60 fps with 8 sounding voices; degrade to 30 fps gracefully rather than ever starving audio.

## 16. Presets & state

- Format: ValueTree → XML, one file per preset at `Documents/Lumen/Presets/<Category>/<Name>.lumen`. State carries a version int (1). Loading newer states: ignore unknown keys. Loading older: default missing keys.
- Save dialog (name, category, author); browser popup grouped by category; `<` `>` step through presets.
- `getStateInformation` / `setStateInformation` carry the full patch including image wavetable + thumbnail + matrix + macro mappings.
- Factory bank — 32 presets, designed and verified by rendered metrics (no clipping; RMS between −20 and −8 dBFS at velocity 100):
  - Bass: Sub Zero, Rubber, Neon Growl, Deep Field, Knuckle, Tape Bass
  - Leads: Laser, Glass Whistle, Saw Hero, Vapor, Chrome, Solar Flare
  - Pads: **Neon Tide (default patch)**, Slow Aurora, Warm Fog, Choir Ghost, Polar Drift, Amber Haze
  - Keys/Plucks: Dial Tone, Marble Pluck, Music Box, Soft EP, Pixel Pluck, Kalimba Dust, Bell Garden
  - Textures: Static Bloom, Scanline, Radio Sky, Machine Hum, Wind Tunnel, Photograph, Init
  - `Photograph` and `Scanline` are built through the Lens engine from procedurally generated images (a vertical gradient and a stripe pattern synthesized by the render tool) — factory proof of the feature, fully deterministic.

## 17. MIDI

- Note on/off with velocity, sustain (CC64), mod wheel (CC1), channel aftertouch, pitch bend.
- MIDI Learn: right-click any knob → Learn → move a hardware control → bound. The map persists globally at `%APPDATA%/Lumen/midi_map.xml` (not per preset). This is how the Launchkey Mini's knobs get mapped.
- Standalone: JUCE audio/MIDI settings panel; all MIDI inputs enabled by default; WASAPI as the default backend (the Focusrite Scarlett works out of the box). Optional ASIO build: CMake option `LUMEN_ASIO=ON` plus a user-supplied Steinberg ASIO SDK path; the SDK must never be committed to the repo.

## 18. Verification harness — how Claude Code hears and sees

- **`lumen_render`** (console target linking the engine, no GUI):
  `--preset <name|path|init> --note 60 --vel 100 --dur 2 --tail 2 --sr 48000 --out out.wav [--analyze] [--bench] [--image <png>] [--mode scan|spectral] [--set param=value ...] [--table-dump frames.csv]`
  `--analyze` writes `out.json` with: `peak_dbfs, rms_dbfs, dc_offset, nan_count, f0_hz, f0_cents_error, alias_floor_db` (loudest non-harmonic partial above 500 Hz, dB relative to the fundamental), `attack_ms_measured` (time to 90% of peak), `release_ms_measured` (to −60 dB), `realtime_factor`.
  `--bench` renders 10 s of 8-voice chords and reports `realtime_factor`.
- **Standalone flags**: `--screenshot <png> [--view play|deep] [--preset <name>]` — launch, wait ~700 ms for first paints and timers, snapshot the editor, exit 0. Claude Code opens and inspects the PNG itself after every UI change.
- **`lumen_tests`** (JUCE UnitTest): mip-level correctness, SVF stability sweep (modulate cutoff 20 Hz–20 kHz at audio rate — output must stay bounded), envelope timing, matrix math and clamping, image determinism (fixed generated test image → known wavetable checksum), state round-trip.
- **pluginval**: Windows release downloaded into `tools/`; run against the built VST3 before any phase is declared complete (strictness per `PHASES.md`).

## 19. Non-goals for v1 (park these — do not build without asking)

MPE, effect reordering, a wavetable draw/editor, macOS/Linux builds (JUCE keeps the door open), theming/skins, arpeggiator/sequencer, oversampled FX, custom FDN reverb, NKS integration (requires an NI partnership; plain VST3 paging is the v1 answer).
