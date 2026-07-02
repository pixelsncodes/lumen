# PHASES.md — Lumen build order and acceptance gates

Rules of engagement: one phase per session works best. Claude Code verifies every acceptance criterion itself (render metrics, screenshots, tests, pluginval) and shows the numbers. Items marked **User gate** are the only things the user must do by hand. pluginval strictness: 8 for Phases 1–3, 10 from Phase 4 onward.

Each phase ends with: all criteria green → commit → tag `phase-N` → summary of what was built, verification results, open items in `DECISIONS.md`, and exactly what the user should test.

---

## Phase 0 — Toolchain & repo

Tasks: check for and (with approval) install via winget: VS 2022 Build Tools with the C++ workload, CMake, Git. `git init` + sensible `.gitignore`. CMake skeleton fetching the pinned JUCE 8.x tag (record the tag in CLAUDE.md). Create `tools/grant_vst3_write.ps1` (grants the current user Modify rights on `C:\Program Files\Common Files\VST3`; user runs it once elevated). Download the pluginval Windows release into `tools/`.

Accept:
- [ ] `cmake` configure + Release build of a minimal JUCE console app succeeds (x64).
- [ ] Repo initialized with first commit; `pluginval --version` runs.

## Phase 1 — Skeleton plugin

Tasks: `juce_add_plugin` (VST3 + Standalone) with SPEC section 2 metadata. APVTS containing the frozen first-8 parameters plus master gain. A temporary test voice (band-limited saw, fixed 5 ms / 200 ms envelope) so the plugin is audible. Copy-after-build to the VST3 folder. Standalone `--screenshot` and `--version` flags. Skeleton `lumen_render` and `lumen_tests` targets. Record real artefact paths in CLAUDE.md.

Accept:
- [ ] Clean Release build; pluginval strictness 8 passes on the VST3.
- [ ] `Lumen.vst3` present in `C:\Program Files\Common Files\VST3`.
- [ ] Standalone launches; `--screenshot` produces a non-black PNG at the expected size.
- [ ] `lumen_render` writes a valid WAV; `lumen_tests` runs green.

**User gate:** plugin appears and makes sound in Ableton Live, Maschine 3, and standalone; Launchkey Mini plays notes into the standalone.

## Phase 2 — Core engine

Tasks: wavetable class + mip pipeline + the four factory tables. Osc A/B with unison. Sub + noise. SVF (all modes, drive, key tracking). Three ADSRs (Env 1 → amp, Env 2 → filter amount). 16-voice manager with stealing. Velocity. Smoothing on every continuous parameter. Full `--analyze` metrics implemented in `lumen_render`.

Accept (all via `lumen_render --analyze` unless noted):
- [ ] `f0_cents_error` <= 2 at MIDI 33, 45, 57, 69, 81, 93 (saw table, unison 1).
- [ ] `alias_floor_db` <= −50 at MIDI 84 and <= −60 at MIDI 60 (saw).
- [ ] `|dc_offset|` < 0.001 and `nan_count` = 0 in every render.
- [ ] Measured attack/release within ±10% of set values at 10 ms, 100 ms, 1 s.
- [ ] Unison 8: stable output, peak <= −1 dBFS at default levels.
- [ ] `--bench` `realtime_factor` >= 8 (Release, 8 voices).
- [ ] All unit tests green (mip levels, SVF stability sweep, envelope timing).

**User gate:** play every factory table in Live; report anything that sounds wrong or dull.

## Phase 3 — Modulation

Tasks: LFO 1–3 (all shapes, host sync, poly/mono, fade-in). Mod matrix engine + state storage. Macro system with per-preset mapping lists. Sources: velocity, mod wheel, aftertouch, pitch bend, keytrack, random-per-note. Per-voice vs. global application. (Drag-drop UX arrives in Phase 5 — for now the matrix is editable via a plain temporary list panel.)

Accept:
- [ ] Unit tests for matrix combination math and clamping.
- [ ] Render with LFO → cutoff shows periodic spectral movement matching the set rate within 5%.
- [ ] Host-sync test at 120 BPM: 1/4 modulation period = 0.5 s within 2%.
- [ ] Macro render-diff test: moving each macro changes exactly its mapped parameters.
- [ ] Slow cutoff sweep renders artifact-free (no zipper discontinuities in the spectrum).

**User gate:** in Maschine, page 1 knobs = Macros 1–4 + cutoff/res/delay/reverb mixes; wiggle everything, confirm smoothness.

## Phase 4 — Effects

Tasks: drive, chorus, delay (with sync), reverb wrapper, always-on limiter, master gain, meter taps, per-effect bypass.

Accept:
- [ ] All-bypassed output nulls against the pre-FX signal (difference <= −80 dB at −12 dBFS material, limiter not engaged).
- [ ] Delay sync: 1/4 at 120 BPM = 500 ms echo spacing within ±2 ms (measured from render).
- [ ] Torture render (every effect maxed, 8 voices): peak <= −0.29 dBFS, `nan_count` = 0.
- [ ] pluginval strictness 10 passes (and from now on).

**User gate:** sweep each effect by ear; call out anything metallic, thin, or noisy.

## Phase 5 — UI & visualizers (the big one)

Tasks: full Deep + Play views per SPEC section 14. OpenGL-accelerated rendering with identical software path. 3D wavetable stack, spectrum + draggable filter curve, scope, envelope/LFO breakpoint editors, animated mod arcs, drag-and-drop modulation, meters, on-screen keyboard, resizing 70–200%, Inter embedded. Screenshots wired for both views.

Accept:
- [ ] `--screenshot` of both views matches SPEC section 14 layout and palette (Claude Code inspects the PNGs and says what it checked).
- [ ] Every APVTS parameter is reachable in the UI and automates with correct gesture begin/end (pluginval clean).
- [ ] Frame-time instrumentation: <= 16.7 ms average with 8 sounding voices (debug HUD toggle); zero audio dropouts logged during a 60 s UI stress wiggle.
- [ ] Control contract everywhere: double-click reset, Shift fine, wheel steps, right-click menu, drag tooltip.

**User gate:** look-and-feel review in Live, Maschine, and standalone at multiple window sizes. Expect 1–3 iteration rounds — describe issues in plain words ("knob labels too small at 100%", "spectrum feels laggy").

## Phase 6 — Lens (image engine)

Tasks: decode + downscale, Scan and Spectral modes, chroma mapping, seeded phases, silence guards, wavetable + thumbnail persistence, drop-anywhere, animated scanline, oscillator target selector, toggles. `lumen_render --image` support plus procedural test-image generation (vertical gradient, stripes, checker).

Accept:
- [ ] Determinism: same file → identical wavetable SHA-256 across two runs and after a state round-trip.
- [ ] Gradient image, Scan mode: spectral centroid changes monotonically across the morph range.
- [ ] Stripe image, Spectral mode: harmonic peaks land on the expected bins (stripe count ±1).
- [ ] Chroma unit tests: synthetic solid-color images hit the exact SPEC section 13 target values.
- [ ] State round-trip restores the sound bit-exactly with the original image file deleted.
- [ ] Screenshot shows the Lens panel with image + scanline.

**User gate:** drop your own photos; judge musicality. The mapping constants in SPEC section 13 get tuned to taste here.

## Phase 7 — Presets, MIDI Learn, polish, release

Tasks: preset system + browser + save dialog. The 32-preset factory bank (designed against render metrics; `Photograph` and `Scanline` built through Lens from procedural images). MIDI Learn with the global persisted map. Standalone settings persistence. Default patch = Neon Tide. Tooltips and strings pass. `README.md` mini-manual (signal flow, controls contract, rescan instructions). Version 1.0.0.

Accept:
- [ ] Every factory preset renders clip-free with RMS between −20 and −8 dBFS at velocity 100, loadable by name in `lumen_render`.
- [ ] State version round-trip and unknown-key tolerance tested.
- [ ] MIDI Learn map survives restart (covered by a unit test on the map file).
- [ ] pluginval strictness 10; tag `v1.0.0`.

**User gate:** full play-test — Launchkey mapped via MIDI Learn, Maschine macro page, Scarlett in standalone. Build the punch list; then ship.

---

## Paste-prompt template (Phases 2–7)

> Read CLAUDE.md, SPEC.md and PHASES.md in full. Execute Phase N (<name>) completely. Verify every Phase N acceptance criterion yourself with the harness and show me the actual numbers/screenshots. Then run pluginval at the required strictness, commit, tag phase-N, and report: what you built, verification results, anything new in DECISIONS.md, and exactly what I should test by ear or eye.

(The Phase 0+1 kickoff prompt lives in START_HERE.md.)
