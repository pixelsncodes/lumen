# RC_REPORT.md — RC hardening + showcase packaging

Session on parent **`feature/phase5-minimal-ui`**; submodule
**`feature/clock-unification`** used read-only (zero submodule commits, no
pointer moves, no engine changes). Commits this session: `ad6872e`
(param-gap UI + roundtrip pin), `15a7524` (torture tests), `ba67b98`
(showcase gallery + harness export flags), plus this docs commit.

## 0. Status confirm — Phase 4.5-e/f (read-only) ✅

`feature/clock-unification` carries the complete e/f work: `da4d09d`
re-baseline(4.5-e) register continuity, `b5e4bd4` re-baseline(4.5-f) ending
sanity, `cada2d7` M4 metric (G4-verified), `1ae2bd6` addendum + refreshed
auditions. Gate results are recorded in `external/lumena/PHASE45_REPORT.md`
§11, including the decisive table, quoted verbatim:

> **60 seeds, Mona Lisa, protocol as §5 (all three engine states):**
>
> | state | M4 mean | M4 median | worst single jump | seeds > 10 st | worst terminal | bars histogram |
> |---|---|---|---|---|---|---|
> | old world (`pre-clock-unification`) | 3.62 | 3.63 | 17.33 | 16/60 | 7.25 b | {8:20, 9:24, 10:8, 11:3, 12:5} |
> | new world pre-fix (`9286a3b`) | 4.00 | 3.75 | 17.67 | 20/60 | 7.25 b | {8:14, 9:27, 10:10, 11:6, 12:3} |
> | **new world fixed (tip)** | **2.71** | **2.73** | **9.67** | **0/60** | **5.25 b** | {8:23, 9:22, 10:7, 11:5, 12:3} |

M2/reg_b also recorded there (4.35 old → 3.76 fixed, gate ≤ ~4.9 holds;
coupling-cure pin green; tripwire 0/60). The remaining Phase 4.5 gate is the
human ear test — checklist in `SHOWCASE.md`.

## 1. Param persistence gaps ✅ (`ad6872e`)

Clarification recorded for the brief's wording: `melodyDensity` and
`melodyLockHarmony` were **already APVTS-persisted** — the actual
pre-existing gap (`--check-params`) was UI reachability. Fixed: DENSITY is
the FEEL row's fifth knob; HARMONY is the third lock chip (chips relabelled
RHYTHM / PITCH / HARMONY to fit). **`--check-params`: 114/114 attached,
none missing.** New `MelodyParamRoundtripTest` pins all 16 melody params —
including Phase 5's `melodyLoopPlayback` and `melodyTranspose` — through a
full non-default state round-trip into a fresh processor. Panel screenshot
re-inspected (no clipping); `samples/phase5-ui/melody-panel-win.png`
refreshed.

## 2. pluginval ✅ — strictness 10, SUCCESS, nothing to fix

- Command: `Tools\pluginval.exe --strictness-level 10 --validate
  build\Lumen_artefacts\Release\VST3\Lumen.vst3`
- Level: **10 (maximum)** — passed cleanly on the first run; no lower level
  needed.
- Verdict line, verbatim: `SUCCESS` (exit 0).
- 25 test sections executed; grep for `fail|leak|warning|error` over the
  log: **0 hits**. Full output: `demo/rc/pluginval-strictness10.log`.
- Run against the Windows Release VST3 built at `15a7524` (identical
  plugin code to the branch tip — later commits touch only the test
  harness, gallery assets and docs). No crashes, no leaks, no
  param-handling issues → no fixes, nothing engine-implicating to log.

## 3. Torture wiring tests ✅ (`15a7524`) — all pass, zero fixes needed

`MelodyTortureTest` drives the real controller/player/engine with rendering
between actions (so voices genuinely release and stuck notes are detectable
via `activeVoiceCount()`):

| scenario | pass condition | result |
|---|---|---|
| REGENERATE ×3 during playback | transport alive, new material triggers, 0 stuck voices | pass |
| loop toggled every 10 blocks × 600 blocks | survives storm; one-shot ending honoured after; 0 stuck | pass |
| transpose swept −12..+12 every block in a loop | notes stay in MIDI range; every note-on finds its note-off | pass |
| state save/load mid-loop | melody, key, mood summary survive; 0 stuck voices | pass |
| rapid REGENERATE ×20 determinism | each take's (seed, steps) replays byte-identically via setSeed + applyState + generate | pass (20/20) |

All five passed on the first run — **no product-code changes were
required**. Windows suite after everything: **ALL TESTS PASSED**. WSL suite:
green except the known Windows-pinned wavetable golden (pre-existing,
passes on Windows).

## 4. Showcase assets ✅ (`ba67b98`) — `demo/showcase/`

Eight procedural images (deterministic `make_images.py`, no copyrighted
material) spanning the mood space, each with generated `.mid`, the plugin's
own generation-summary `.txt`, and a full editor screenshot. Detected:
D Major / Bb Harmonic Minor / C#-Db Major Pentatonic / E Minor / F# Dorian /
G Dorian / C Harmonic Minor / Ab Mixolydian — bright, vivid-dark,
washed-out, dark, dusky and warm buckets all represented, plus a
high/low-contrast pair for the density axis. Seeds pinned (5eed0001..08) via
the new harness flags `--melody-seed` / `--melody-export`, so every asset is
regenerable bit-for-bit; inventory + commands in `demo/showcase/README.md`.

## 5. SHOWCASE.md ✅

At the parent branch root: Windows build/install commands, the 5-minute
demo script (image order, knob moves, what to say the engine is doing),
honest caveats (both branches unmerged, pending human gates, host checks
outstanding), and both merge-readiness checklists (Phase 4.5 ear-test
items; Phase 5 visual items).

## Deviations / notes

- Task order was 0 → 1 → 3 → 2 → 4 → 5 (param gaps and torture tests
  before pluginval) so pluginval's verdict stands on the final plugin
  binaries; the brief's 0,2,1,3 ordering was a time-pressure priority and
  time did not run short.
- `demo/rc/` added for RC evidence (pluginval log).
- The gallery was generated with the real Windows `Lumen.exe` — the ship
  binary, not the Linux build.

## Session summary (SESSION_NOTES style)

RC hardening on `feature/phase5-minimal-ui` (submodule untouched):
4.5-e/f status confirmed with the M4 table quoted; `--check-params` closed
to 114/114 (DENSITY knob, HARMONY chip) with a 16-param state-roundtrip
pin; pluginval strictness 10 SUCCESS with zero findings (log committed);
five torture scenarios (regen/loop/transpose/save-load mid-playback +
rapid-regen determinism ×20) all green with no product fixes needed;
8-image mood-space gallery with MIDI + summaries + screenshots under
`demo/showcase/` (seeds pinned, regenerable); SHOWCASE.md demo kit at the
root. Windows suite ALL TESTS PASSED throughout. No merges, no pointer
moves. Remaining human gates: Phase 4.5 ear test, Phase 5 visual pass,
DAW host checks — checklists in SHOWCASE.md. STOPPED as commissioned.
