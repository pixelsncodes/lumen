# Lumen

A wavetable synthesizer with a deterministic image-to-tone engine ("Lens").
Ships as a VST3 and a Windows standalone app. C++20 / JUCE 8.

## Quick start

1. Load **Lumen** in your host (or run the standalone). It opens on the
   *Slow Aurora* pad in the **Play** view: four macro knobs, a spectral
   waterfall, a Lens drop zone and a mouse-playable keyboard.
2. Turn **Tone / Motion / Space / Texture** — every preset maps all four.
3. Click the preset name in the header to browse the 32-preset factory bank.
4. Drop any photo onto the window. The image becomes a wavetable and (with
   **COLORS** on) sets the patch from the image's colors.
5. Press **DEEP** (top right) for the full editor.

## Signal flow

```
Osc A ─┐
Osc B ─┤
Sub   ─┼─ per-voice mix ─ SVF filter (drive) ─ voice amp (Env 1) ─ voice sum
Noise ─┘                                                            │
             Drive → Chorus → Delay → Reverb → Limiter → master gain/meter
```

Env 1 always drives voice volume; Env 2 is soft-wired to filter cutoff via
the **Env 2** amount knob on the filter; Env 3 is free. LFOs 1–3, the macros
and MIDI sources route anywhere through the modulation system below.

## The controls contract

Every knob behaves the same way:

- **Drag vertically** to change; hold **Shift** for fine adjustment.
- **Double-click** resets to the default; **mouse-wheel** steps.
- A value bubble shows while dragging; hovering shows a short tooltip.
- **Drag a source chip** (ENV/LFO/MAC, footer of the Deep view) onto any knob
  to modulate it. The dim arc shows the reachable span, the moving dot the
  live value.
- **Right-click** a knob to list/edit/remove its modulations, reset it, or
  bind hardware via MIDI Learn.

The modulation matrix itself (24 slots) is stored with the preset and is not
host-automatable in v1 — automate the four macros instead; they are ordinary
parameters and the first four the host sees.

## Voice modes & glide

The **VOICE** section (Deep view, top right): **Poly** plays chords (16
voices), **Mono** is one voice with last-note priority that retriggers on
every note, **Legato** ties overlapping notes without retriggering. **Glide**
slides the pitch between successive notes over the set time (mono/legato).
Factory examples: *Rubber* and *Laser* (mono + glide), *Neon Growl* (legato).

## Presets

- Factory bank: 32 presets in Bass / Leads / Pads / Keys / Textures — always
  present, never on disk.
- **Save Preset…** (bottom of the browser) asks only for a name and writes
  `Documents/Lumen/Presets/User/<Name>.lumen`. Everything you saved shows in
  one **User** section after the factory bank (any `.lumen` file anywhere
  under `Documents/Lumen/Presets/` is picked up on the next browser open).
- Presets store the full patch — including any Lens wavetable and thumbnail —
  so they recall bit-exactly even if the source photo is gone.

## Lens — image to tone

Drop a PNG/JPEG anywhere on the window (or click the dashed drop zone).

- **SCAN** reads image rows as waveforms — morphing travels down the image.
  **SPECTRAL** reads it as a spectrogram (top = high frequencies).
- **A/B** picks the destination oscillator.
- **COLORS** is on by default: a drop also *blends* the patch from the image —
  filter, envelope, drive, noise, macro positions — layered on top of the
  current sound rather than resetting it. Dropping onto a patch you like is
  safe: the **×** button removes the image and restores exactly the sound you
  had before the first drop.
- Every note also journeys through the image (a slow morph sweep wired to
  Env 3); the **Motion** macro sets how fast.
- Same image bytes → same sound, always, on any machine. Nothing is uploaded.

## MIDI Learn

Right-click any knob → **MIDI Learn** → move a hardware control: bound. The
same menu shows **Clear MIDI (CC n)** to unbind. The map is global — saved at
`%APPDATA%/Lumen/midi_map.xml`, shared by every instance and project, and it
survives restarts. Mod wheel (CC1) always drives the mod-wheel source even
when learned.

## Standalone notes

- The gear icon opens the audio/MIDI settings. Device, driver type and sample
  rate persist across restarts (`%APPDATA%/Lumen/Lumen.settings`); all MIDI
  inputs are enabled by default. WASAPI is the default backend.
- The header is the title bar: drag it to move the window; the buttons past
  the meter minimize/close.

## If something looks stale

Rescan pattern: reopen the preset browser to pick up new `.lumen` files;
delete `%APPDATA%/Lumen/Lumen.settings` to reset audio devices to defaults,
or `%APPDATA%/Lumen/midi_map.xml` to clear all MIDI bindings. If the host
cached an old plugin scan, rescan `C:\Program Files\Common Files\VST3`.

## Building

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Verification harness (see `SPEC.md` §18): `lumen_render` (headless renders +
analysis JSON), `lumen_tests` (unit suite), `Lumen.exe --screenshot`, and
pluginval at strictness 10.
