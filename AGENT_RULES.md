# AGENT_RULES.md — Standing rules for all Lumena agent sessions

> Read this file at the start of every session. It replaces re-reading the full
> docs. Deep background, only if a task requires it: LUMENA_ROADMAP.md (plan),
> LUMEN_LUMENA_ANALYSIS_BRIEF.md (diagnosis), LUMENA_BPHRASE_FIX_PLAN.md
> (variant-C design), external/lumena/VARIANT_C_REPORT.md (current state).

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

## No-go zones (do not modify, even as "improvements")
- maybeOrnament, padToWholeBars, chord/progression code, arp code, rhythm
  template selection, the two-clock split. Phase 4.5 opens only when a brief
  explicitly commissions it.
- The RNG stream: one interleaved mt19937. Never add, remove, reorder, or
  split any draw. Interventions must be post-draw remaps, draw-free logic, or
  pitch-only edits. KNOWN TRAP: a conditional draw in stepNote fires on
  |gradient| > 0.02 along visited cells — changing which cells the walk
  visits perturbs the stream. Move read sites, never the walk.
- Never delete or weaken a test to make it pass.

## Hard gates (verify with commands after every commit)
- G1 Full LumenaTests suite green (current re-baselined count).
- G2 Determinism: checkerboard seed 2024 generated twice → byte-identical.
- G3 Canary vs the task's starting tag: checkerboard onsets, durations,
  velocities byte-identical. Pitch diffs allowed but dumped and logged.
  Timing shift = stream perturbed = revert and rework, never paper over.
- G4 Read-only/metrics commits: ALL generated MIDI fully byte-identical.
- Stream-exactness proof for pitch changes: 61 seeds × both fixtures with
  ornaments off → onsets/durations/velocities byte-identical, pitch-only.

## Fixtures (never swap jobs)
- checkerboard = determinism/regression only. Mona Lisa = taste/audition
  only. Metrics M1–M3 are taste metrics → Mona Lisa only.

## Known riders and caveats
- maybeOrnament pitch→draw coupling: any pitch change shifts rhythm on some
  seeds (~5–10%). Determinism holds; this is re-baseline scope, cured in
  Phase 4.5. State the percentage in merge/report text.
- M1 (motif recall) is known-flawed for deliberate contour contrast: report
  it, never gate on it. M2 (register distance), M3 (entropy inflation), and
  the over-coherence tripwire are the gating metrics.
- Test failures after intended changes: pins of old behavior → update in a
  commit prefixed "re-baseline:" stating what/why. Timing failures → real
  regression, fix or stop.

## Session discipline (Pro plan: small sessions)
- One deliverable per session, as named by the brief. Finish at a clean,
  committed, gated checkpoint, summarize in SESSION_NOTES.md, then STOP.
- Never merge, never pick audition winners, never retune taste parameters
  beyond what the brief allows. The human ear is the acceptance gate.
- If a gate fails twice on one step, or a task would require violating a
  rule above: STOP and write the situation into the task's report file
  instead of improvising around the constraint.
