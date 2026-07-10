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
TESTS PASSED**; `--check-params` **114/114 attached**; pluginval strictness
10 **SUCCESS** (log: `demo/rc/pluginval-strictness10.log`).

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
