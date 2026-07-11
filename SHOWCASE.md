# SHOWCASE.md — demoing Lumen's image-to-melody engine today

The showcase state: parent **`feature/lumena-melody`** (merge `bf7d052`)
with the accepted Phase 4.5 engine at submodule pointer `e023c47`. Both
tracks are merged — released state. The two merge-readiness checklists at
the bottom are kept as the historical record; the merge record lives in
*Known caveats*.

## Build & install (Windows)

```bat
:: configure once (VS-bundled CMake; add -DGIT_EXECUTABLE=... on a fresh clone, see CLAUDE.md)
cmake -B build -G "Visual Studio 17 2022" -A x64

:: build everything
cmake --build build --config Release

:: verify
build\lumen_tests_artefacts\Release\lumen_tests.exe
build\Lumen_artefacts\Release\Standalone\Lumen.exe --check-params
Tools\pluginval.exe --strictness-level 10 --validate build\Lumen_artefacts\Release\VST3\Lumen.vst3

:: install the VST3 (one-time elevated write grant, then copy)
powershell -ExecutionPolicy Bypass -File tools\grant_vst3_write.ps1   :: elevated, once
xcopy /E /I /Y build\Lumen_artefacts\Release\VST3\Lumen.vst3 "C:\Program Files\Common Files\VST3\Lumen.vst3"
```

Current status of those gates on this branch: full Windows suite **ALL
TESTS PASSED**; `--check-params` **115/115 attached** (114 + the
post-release `melodyOctave`, see *Post-release additions*); pluginval
strictness 10 **SUCCESS** (log: `demo/rc/pluginval-strictness10.log`).

## The 5-minute demo script

Prep: standalone open (or VST3 in a DAW), `demo/showcase/` images at hand.

1. **(0:00) The premise.** "Lumen reads an image and composes from it —
   deterministically. Same image, same seed, same melody, every time."
   Load `demo/showcase/01-sunrise-bright.png` in LENS, toggle MELODY.
2. **(0:30) The detection.** Point at the GENERATED block: "It measured the
   image — hue 46° picks D on the circle of fifths, bright and saturated
   picks a major family. Nothing is hand-tagged; the readout is the actual
   provenance." (KEY: D Major, MOOD: bright (major).)
3. **(1:00) The music.** LOOP LENGTH → 4 bars, LOOP on, PLAY. While it
   loops: "Phrase form A A′ B C — a motif, a variation, a contrasting
   answer, a cadence home. The yellow trace is the walk across the image;
   the glowing cell is the note sounding right now."
4. **(1:45) The mood space.** Load `02-neon-dusk.png` (keep playing).
   "Vivid and dark flips it to Bb harmonic minor — the whole scale family
   follows the picture's mood." Let a pass play.
5. **(2:30) The knobs.** ENERGY up (busier, louder), DENSITY up on
   `07-ember-highcontrast.png` — "busy image regions subdivide the rhythm;
   flat regions stay calm." IMAGE knob: "how strongly the picture bends the
   pitch contour."
6. **(3:15) Control without losing the take.** TRANSPOSE +3 while looping —
   "pitch-only, the take itself never changes; back to 0 is the same
   melody." Then REGENERATE a couple of times: "new seed each press." Turn
   LOCK HARMONY on, REGENERATE again: "same chord progression, new tune on
   top."
7. **(4:15) Take it home.** DRAG MIDI straight into a DAW track (or SAVE
   .MID). "What you heard is exactly what exports — including the
   transpose." Save the project: "reload recalls the exact melody, seed and
   summary — the image itself is never needed again."
8. **(4:45) Close.** SEED readout: "everything you heard is reproducible
   from this hex number and this picture."

## Known caveats (be honest in the room)

None — released state.

**Merge record (2026-07-10, Phase 5 acceptance):**

- `feature/phase5-minimal-ui` merged into `feature/lumena-melody` as
  `bf7d052` (true merge, both parents preserved; human visual checklist
  PASS). The merge keeps the accepted Phase 4.5 engine at submodule
  pointer `e023c47` (moved in `6b83347` after the ear-verdict PASS) —
  both tracks are now on one line.
- Post-merge gates on the merged tree: full Windows suite green over 8
  consecutive runs; Windows Release build clean at /W4-as-errors; the
  three Phase 5 wiring groups (loop playback, generation summary,
  transpose), the melody-parameter state round-trip and the five-scenario
  transport torture group all green. The standing Linux-only wavetable
  golden exception (Windows-pinned hash) is unchanged and Windows-green.
- pluginval strictness 10 **SUCCESS** on the VST3 rebuilt from the merged
  tip — fresh log at `demo/rc/pluginval-strictness10.log`.
- One re-baseline, zero behavior changes (`c9ba498`): the torture test's
  fixed 600-block one-shot wait predated the engine's form-bar pad rule
  (lumena `4186417`), so ~1 in 4 random seeds outran it. Instrumentation
  over 15 fresh seeds showed every pass ends on its own (max 801 blocks)
  and every voice drains within its release tail — no transport bug; the
  wait is now a bounded wait-until-ended plus an explicit release drain.

## Post-release additions

Merged into `feature/lumena-melody` after the Phase 5 showcase (2026-07-11),
each on its own branch off `4d46813`, integrated in order with a gate
checkpoint after every merge; submodule pointer unchanged at `32972f6`.

- **Tone audibility floor** (`feature/tone-audibility-floor`): a dark image
  now produces a DARK-sounding patch, not a near-silent one. The Tone macro
  is remapped `T = 0.45 + 0.55·T_raw²` (floor 0.45, γ 2.0) — monotone, so
  image ordering is preserved and bright inputs barely move; 02-neon-dusk
  and 04-forest-dark land above the 0.45 audibility floor. Patch/audio only:
  the MIDI layer is untouched (seed-2024 exports for all 8 gallery images
  byte-identical pre/post).
- **Full-res image persistence** (`feature/image-persistence`): the original
  source-encoded image bytes now ride in the saved state, so a reloaded
  project (or preset switch) shows and re-analyzes the full-quality image
  instead of the 64×64 thumbnail. The Lens image is **session-level** — it
  survives preset switches, and a switched-to preset's macros land exactly
  as stored (no silent re-derivation). State-tree field only; no automation
  parameter added.
- **SCAN / SPECTRAL glyphs**: the two Lens mode tabs are now directional-line
  glyphs `|` (Scan) and `—` (Spectral) with per-tab tooltips; same hit areas
  and behavior.
- **OCTAVE control** (`feature/octave-window`): a `melodyOctave` stepper
  (−2..+2, default 0) sits under TRANSPOSE. Effective pitch shift is
  `transpose + 12·octave`, applied post-generation and per-note clamped to
  MIDI 0..127 — pitch-only, exactly like Transpose: the stored take and seed
  never change, and 0/0 exports byte-identically to the pre-change render.
  This is the 115th parameter, appended at the end (first 8 untouched).
- **Moveable / resizable melody window**: the MELODY window drags by its
  title strip and resizes from a corner grip (aspect locked to the fixed
  580×476 layout, minimum = that size). Placement persists with the patch
  and is clamped on restore, so a stale/off-screen save can never strand it.
- **Keyboard lights from internal playback** (`feature/keyboard-playback-lights`):
  the on-screen keyboard now lights from the PLAY-button melody/chord/arp
  player, not just live host/hardware MIDI. The player publishes its exact
  sounding-note set on the same `LiveState` snapshot that drives the image-
  region highlight; the editor mirrors it onto the keyboard on channel 2,
  separate from live MIDI on channel 1, so a key held by both stays lit until
  both release (polyphonic in CHORDS/ARP, mono in MELODY, all keys clear on
  stop). UI-only — no new parameter; seed-2024 02-neon-dusk export
  byte-identical.
- **App icon**: the Lumen mark ships as `Assets/icon/lumen.ico` (16/32/48/64/
  128/256 px, transparency preserved) wired via CMake `ICON_BIG`/`ICON_SMALL`,
  so the standalone exe carries it (taskbar, alt-tab, exe file, window). The
  same art is embedded as `BinaryData` and rendered in the header beside the
  wordmark and in the gear menu's about line. Cosmetic — no param, no layout
  redesign; MIDI export byte-identical.

Integration gates (this merge, on the merged tip): full Windows suite green;
115/115 param round-trip; determinism (61 seeds × checkerboard, ×2,
byte-identical); octave-0/transpose-0 seed-2024 export hash matches the
pinned pre-change value; tone floor holds for 02-neon-dusk and 04-forest-dark;
pluginval strictness 10 SUCCESS on the rebuilt VST3.

## Merge-readiness checklist 1 — Phase 4.5 engine (ear test, human)

> **Done.** Ear verdict PASS; pointer moved to `e023c47` in `6b83347`.

From `external/lumena/PHASE45_REPORT.md` §7 + §11, on
`external/lumena/auditions/phase45/` (old vs new-fixed pairs, 4 seeds):

- [ ] Overall: does the new world hold together as well as the old —
      phrase form, B relatedness, cadences — while breathing better?
- [ ] Seed 58, bars 5–7: the old 13-semitone register teleport is gone;
      does the passage sound coherent now?
- [ ] Seed 30 ending: terminal drone 7.25 → 3.25 beats, one bar shorter —
      does the ending land instead of hanging?
- [ ] Seed 70, bar 6: the tied-anticipation showcase still sounds right
      (pushed note held through the bar line).
- [ ] Seeds 30 / 44 / 46 (per-note fold fallback): do the folded bars sound
      natural where a figure's interval shape was traded for register
      continuity?
- [ ] If accepted: merge `feature/clock-unification` →
      `feature/motif-phrasing` re-baseline, then the commissioned submodule
      pointer move (engine track owns both).

## Merge-readiness checklist 2 — Phase 5 UI (visual pass, human)

> **Done.** Visual checklist PASS; merged as `bf7d052` (see merge record).

The full list lives in `UI_SESSION_REPORT.md`; condensed:

- [ ] Loop: seamless repeat at the stated bar count; off-mid-flight
      finishes the pass; state reload keeps the chip.
- [ ] Summary: KEY/MOOD/FORM/SEED correct per image; survives save/reload;
      "(random draw)" in Random key mode.
- [ ] Transpose: ±12 clamps; no stuck notes mid-note; export matches ear;
      shape/seed never change.
- [ ] Regenerate/Mutate + all three LOCK chips behave (incl. new HARMONY).
- [ ] FEEL row: five knobs (incl. new DENSITY) render cleanly and work.
- [ ] Regression: image load, generation, MIDI drag/save, presets,
      automation (new params at the END of the list; first 8 untouched).
- [ ] Host checks in Ableton Live + Maschine 3.
- [ ] If accepted: merge `feature/phase5-minimal-ui` →
      `feature/lumena-melody`.
