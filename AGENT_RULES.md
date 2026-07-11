# AGENT_RULES.md — Standing rules for all Lumena agent sessions

> Read this file at the start of every session. It replaces re-reading the full
> docs. Deep background, only if a task requires it: LUMENA_ROADMAP.md (plan),
> LUMEN_LUMENA_ANALYSIS_BRIEF.md (diagnosis), LUMENA_BPHRASE_FIX_PLAN.md
> (variant-C design), external/lumena/PHASE45_REPORT.md (current state),
> external/lumena/VARIANT_C_REPORT.md (pre-4.5 state).

## Repo safety (absolute)
- Parent Lumen repo is READ-ONLY. Never modify, stage, or commit parent files.
  Never update the submodule pointer. Never rebuild/install the VST binary.
  (Exception: only when a brief explicitly commissions the single
  submodule-pointer commit after human acceptance.)
- All work happens inside external/lumena, on feature branches. Tag starting
  points. Never force-push, rewrite history, or delete branches/tags.
- All files you create (reports, auditions/, session notes) live INSIDE
  external/lumena. SESSION_NOTES.md is in the submodule — append there.
- The parent suite's failing wavetable SHA-256 golden (Windows-pinned Lens
  hash) is pre-existing and unrelated. Ignore it.

## Current invariants (post-Phase-4.5 clock — this is the law)
- **G2 Same-seed determinism.** Same image + same settings + same seed =
  byte-identical MIDI. Verify by rendering twice (×2) and comparing bytes —
  minimum: checkerboard seed 2024 generated twice → byte-identical; both
  fixtures for anything touching generation.
- **Pitch never touches the stream.** Pitch-domain inputs must NEVER
  influence RNG draw consumption or the timing/dynamics stream. Pinned
  permanently by the suite test `test_pitch_domain_never_shifts_timing`
  (tests/MelodyGeneratorTests.cpp). If a change trips that pin, the change
  is wrong — revert and rework, never re-pin it.
- **Musical invariants.** Bar counts as specified; the cadence lands on a
  chord tone; phrase form holds; strict monophony; everything on the
  960-tick grid.
- **G1 Suite.** Full LumenaTests suite green (current re-baselined count)
  after every commit. Never delete or weaken a test to make it pass. Test failures
  after intended changes: pins of old behavior → update in a commit prefixed
  "re-baseline:" stating what/why. Determinism or pin failures → real
  regression, fix or stop.

## No-go zones (do not modify, even as "improvements")
- chord/progression code, arp code, rhythm template selection.
- padToWholeBars' stated form-bar rule and maybeOrnament's cured,
  pin-protected behavior — changes only when a brief explicitly commissions
  them.

## Gating metrics
- **GATES (fail = stop):**
  - **M2 — B-phrase register distance.** Released ~3.76, vs the 5.40
    no-fix baseline. Must not regress toward the baseline.
  - **M4 — mean adjacent-bar register-centroid jump, bars 1–8.** Released
    median 2.73. Hard gate: no seed exceeds a 10-semitone single jump.
- **REPORT-ONLY:** M1 (motif recall) — known-flawed for deliberate contour
  contrast; state it in reports, never gate on it.
- **DESCRIPTIVE-ONLY:** M3 (entropy) — report the number, draw no
  conclusion from it: its "collapse" once endorsed the C-2 flattening the
  ear rejected.

## Fixtures (never swap jobs)
- checkerboard = determinism/regression only. Mona Lisa = taste/audition
  only. Metrics M1–M4 are taste metrics → Mona Lisa only.

## Environment
- WSLg builds AND runs the Windows Lumen.exe, the Windows test suite, and
  pluginval. Agents do full Windows verification themselves — do not defer
  builds, suite runs, or pluginval to the human.
- Quirk: the Windows exe cannot write to WSL-only paths (`/home`, `/tmp`,
  …). Pass Windows paths (`C:\...`) for all output files it writes.

## Session discipline (Pro plan: small sessions)
- One deliverable per session, as named by the brief. Finish at a clean,
  committed, gated checkpoint, summarize in SESSION_NOTES.md, then STOP.
- Never merge, never pick audition winners, never retune taste parameters
  beyond what the brief allows. The human ear is the acceptance gate.
- If a gate fails twice on one step, or a task would require violating a
  rule above: STOP and write the situation into the task's report file
  instead of improvising around the constraint.

## Historical — pre-Phase-4.5 clock (retained for reading old branches and reports; NOT current law)

Everything below enforced stream identity on the old interleaved RNG under
the pre-4.5 two-clock split: draw-count preservation, post-draw clamps and
remaps, and canary/stream-exactness proofs. Phase 4.5's clock unification
cured the coupling these rules guarded against; the pin test above replaces
them. Keep this section for reading old branches, reports, and commit
messages only.

### No-go zones (old world)
- maybeOrnament, padToWholeBars, chord/progression code, arp code, rhythm
  template selection, the two-clock split. Phase 4.5 opens only when a brief
  explicitly commissions it.
- The RNG stream: one interleaved mt19937. Never add, remove, reorder, or
  split any draw. Interventions must be post-draw remaps, draw-free logic, or
  pitch-only edits. KNOWN TRAP: a conditional draw in stepNote fires on
  |gradient| > 0.02 along visited cells — changing which cells the walk
  visits perturbs the stream. Move read sites, never the walk.

### Hard gates (old world)
- G3 Canary vs the task's starting tag: checkerboard onsets, durations,
  velocities byte-identical. Pitch diffs allowed but dumped and logged.
  Timing shift = stream perturbed = revert and rework, never paper over.
- G4 Read-only/metrics commits: ALL generated MIDI fully byte-identical.
- Stream-exactness proof for pitch changes: 61 seeds × both fixtures with
  ornaments off → onsets/durations/velocities byte-identical, pitch-only.

### Known riders and caveats (old world)
- maybeOrnament pitch→draw coupling: any pitch change shifts rhythm on some
  seeds (~5–10%). Determinism holds; this is re-baseline scope, cured in
  Phase 4.5. State the percentage in merge/report text.
- M1 (motif recall) is known-flawed for deliberate contour contrast: report
  it, never gate on it. M2 (register distance), M3 (entropy inflation), and
  the over-coherence tripwire are the gating metrics. (Superseded by the
  Gating metrics section above: M4 added, M3 demoted to descriptive-only.)
