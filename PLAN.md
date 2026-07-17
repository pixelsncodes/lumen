# Lumen — Melody Side Panel Redesign (PLAN.md)

## Goal
Replace the popup MELODY window with a right-docked **side panel** + an **info
readout under the Lens image**, add a **visible / editable / lockable seed**,
and fix the SCAN/SPECTRAL icon orientation.

Reference mockup: `assets/side-panel-mockup.png` (put the screenshot there
before starting). Match it, don't reinvent it.

## Repos
- Plugin (JUCE/C++): `~/projects/lumen`  (Windows: C:\Users\pixel\Projects\lumen)
- Engine:            `~/projects/lumena` (Windows: C:\Users\pixel\Projects\lumena)

If working from WSL, prefer the Linux-filesystem clones for speed; push/pull
between them and the Windows folders via git, not file copying.

## Working agreement (read at the start of EVERY session)
- **One phase per session.** Finish the phase, build, stop. `/clear` between
  phases.
- All work happens on branch `ui/side-panel-redesign` (Phase 0 creates it).
  Never commit to main/master.
- Keep diffs minimal. Do not reformat or rewrite files you aren't changing.
- Follow existing patterns in the codebase (TabsBar strips, styleChip,
  choiceParam attachments, LensIconToggle, animate() sync). Read the existing
  MelodyPanel before writing the new panel — most controls port over.
- After every phase: build the plugin (or `cmake --build` the relevant
  target), fix warnings you introduced, commit with a clear message, then STOP
  and ask the user to load it in their DAW and eyeball it.
- If a phase reveals the plan is wrong about how the code actually works,
  say so and propose the smaller correct change — don't silently improvise a
  big refactor.

## Model guidance (user is on Pro plan)
- Phases 0, 4: **Sonnet** (mechanical).
- Phases 1, 2: **Opus** (architecture/state — get these right).
- Phases 3, 5: **Sonnet**, escalate to Opus only if layout math gets hairy.

---

## Phase 0 — Branch + survey (Sonnet, small session)
1. In BOTH repos: `git status` (must be clean or stash), then
   `git checkout -b ui/side-panel-redesign`.
2. Survey and write findings to `docs/redesign-notes.md` (commit it):
   - Where MelodyPanel popup is created/shown/hidden (PluginEditor, overlay?).
   - Current editor window size and whether it can grow, or whether the side
     panel must overlay on top of the existing layout.
   - How seed currently flows: where is it generated, stored, displayed
     (the `SEED 98c3…` readout), and whether lumena exposes set-seed at all.
   - Which controls in the mockup already exist as params (mode, key-from-
     image/random, length, shape, feel knobs, loop length) vs. which are new
     (seed edit/lock, per-domain reroll RHYTHM/PITCH/HARMONY, MUTATE if
     absent).
3. NO code changes beyond the notes file in this phase.

## Phase 1 — Seed model in the engine + controller (Opus)
Goal: seed becomes explicit, settable, lockable, with sub-seeds.
1. Lumena engine: ensure generation takes a 64-bit master seed and derives
   three sub-seeds (rhythm, pitch, harmony) from it deterministically
   (e.g. splitmix64 of master ^ domain constant). Same master seed + same
   image + same params ⇒ identical output.
2. API surface (engine or MelodyController, wherever generation options
   live today):
   - `setSeed(uint64)`, `getSeed()`, `setSeedLocked(bool)`, `isSeedLocked()`
   - `rerollSeed()` — new random master seed (no-op visual if locked? NO:
     reroll button always rerolls; the LOCK only governs REGENERATE).
   - `rerollDomain(Rhythm|Pitch|Harmony)` — rerolls one sub-seed, keeps the
     others. Store sub-seed overrides alongside the master.
   - REGENERATE: if locked → reuse current seed(s); if unlocked → new master.
   - MUTATE: keep the seed, perturb parameters (or derive child seed —
     match whatever mutate currently means; document the choice in the notes
     file).
3. Persistence: master seed, lock flag, and any sub-seed overrides go into
   plugin state (ValueTree/APVTS) and restore on load.
4. Unit-test determinism in lumena if it has a test harness (same seed twice
   ⇒ byte-identical MIDI/event list). Commit.

## Phase 2 — Side panel component (Opus)
Goal: right-docked panel replacing the popup, per the mockup.
1. New `Source/UI/MelodySidePanel.{h,cpp}` in lumen. Port controls from the
   existing MelodyPanel; reuse its attachments/patterns. Layout top-to-bottom
   per mockup:
   - Close [x], PLAY / LOOP buttons
   - MODE: MELODY / CHORDS / ARP tabs
   - KEY: FROM IMAGE / RANDOM
   - LENGTH: 8 / 16 / 32
   - SHAPE: UP / DOWN / UP-DN / CONV / RAND
   - FEEL: ENERGY, COMPLEX, IMAGE, REPEAT, DENSITY knobs
   - LOOP LENGTH: OFF / 1 / 2 / 4 / 8
   - SEED: RHYTHM / PITCH / HARMONY reroll buttons
   - REGENERATE / MUTATE
   - EXPORT: DRAG MIDI / SAVE .MID
2. Show/hide: clicking the melody icon in the Lens toolbar toggles the panel
   (slide-in from the right or instant show — pick what JUCE does cheaply;
   don't build an animation framework). Decide per Phase-0 notes whether the
   editor widens (setSize) when open or the panel overlays; prefer widening
   if the host handles resize cleanly.
3. Delete/retire the popup window path once the panel has parity — but only
   at the END of this phase, after a successful build with the panel doing
   everything the popup did.
4. Keyboard viz + Lens overlay behavior from the previous work must keep
   functioning with the panel open or closed. Commit.

## Phase 3 — Info readout under the image (Sonnet)
Goal: GENERATED block below the Lens image when melody is active.
1. Rows: KEY, MOOD, FORM, SEED — then TRANSPOSE and OCTAVE steppers with
   −/+ buttons and value display (0 st / 0 oct defaults), matching mockup.
2. SEED row is special:
   - Shows hex seed; click to edit (text editor accepting hex, validate,
     apply via setSeed → regenerates with that seed).
   - Lock toggle (padlock icon) next to it bound to setSeedLocked.
   - Updates live after REGENERATE/MUTATE/reroll.
3. Appears when melody is active, hides otherwise; the Lens image shrinks or
   the panel area grows to make room — follow Phase-0 layout findings.
4. Build, commit.

## Phase 4 — SCAN/SPECTRAL icon fix (Sonnet, tiny)
In the Lens SCAN/SPECTRAL toggle icons:
- SCAN icon = **horizontal** line
- SPECTRAL icon = **vertical** line
(Currently reversed.) Pure paint-code swap in the icon drawing; touch nothing
else. Build, commit.

## Phase 5 — Persistence + QA pass (Sonnet)
1. Save/reload in a host: panel-open state (decide: persist or always start
   closed — default closed is fine), seed, lock, transpose, octave, all panel
   params restore correctly.
2. Resize/repaint sanity: open/close panel repeatedly, toggle modes, drag
   knobs — no layout glitches, no dangling listeners (check animate() syncs).
3. Kill any dead code from the popup path; grep for the old class name.
4. Update README/CHANGELOG briefly. Final commit. Leave merging the branch
   to the user.
