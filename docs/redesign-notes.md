# Melody Side Panel Redesign — Phase 0 Survey Notes

Survey only, no code changes. Feeds Phases 1–5 in `PLAN.md`. Mockup reference:
`Assets/side-panel-mockup.png`.

Repos: plugin is this checkout (`lumen`); engine is the git submodule at
`external/lumena`, pinned at commit `03fcd8a`. **That pinned submodule is what
actually builds** (`CMakeLists.txt:20`, `add_subdirectory(external/lumena)`) —
it is *ahead of* the standalone checkout at `/mnt/c/Users/pixel/Projects/lumena`
(`main` @ `56f6ac3`), which lacks `RegenLocks`/`recombineLocked`/`mutate`
entirely. Treat `external/lumena` as authoritative; the standalone checkout is
a stale/diverged sibling.

---

## 1. Where MelodyPanel is created / shown / hidden

- Class: `Source/UI/MelodyPanel.h` / `.cpp` (759-line impl), a plain
  `juce::Component` — **not** a `DialogWindow`/`DocumentWindow`/separate OS
  window. It's an in-window overlay "card" added as a hidden child of the
  editor's `content` root:
  - Created: `Source/UI/PluginEditor.cpp:43`.
  - Added hidden: `PluginEditor.cpp:48` (`content.addChildComponent`, unlike
    `header`/`playView` which use `addAndMakeVisible`).
  - Its own draggable title-strip + resize-corner behavior is hand-rolled in
    `mouseDown`/`mouseDrag`/`mouseUp` (`MelodyPanel.cpp:438-496`), positioned
    via `setTransform(AffineTransform::scale(...).translated(...))` in
    `applyPlacement()` (`MelodyPanel.cpp:415-421`). Its own bounds stay fixed
    at `kBaseWidth=580 x kBaseHeight=476` (`MelodyPanel.h:48`); only the
    transform moves/scales it inside the editor's fixed content canvas.
  - Placement persisted via `Source/State/MelodyState.{h,cpp}`
    (`setWindowBounds`/`windowBounds`/`clampWindowBounds`).
- Toggle flag: `MelodyController::togglePanel()`/`setPanelActive()`/
  `isPanelActive()` — a plain bool (`Source/Melody/MelodyController.h:68-70,
  92`), **not** JUCE Component visibility. Set from the Lens toolbar's
  `melodyChip` (`LensIconToggle`, `Source/UI/LensPanel.cpp:417`).
- Actual `setVisible()` call happens in the editor's 60 Hz timer poll, not
  event-driven: `PluginEditor.cpp:345-358` compares
  `melodyPanel->isVisible()` against `processor.melodyController().isPanelActive()`
  each tick and flips visibility + `toFront()` on change, then calls
  `melodyPanel->animate()` while visible. `LensPanel::animate()`
  (`LensPanel.cpp:487-508`) independently polls the same flag a second time to
  sync the toolbar chip's on/off look.
- Close button: `x` `TextButton` in `MelodyPanel.cpp:236-239`, calls
  `melodyController().setPanelActive(false)`.

**Reusable patterns confirmed for the new panel** (per PLAN.md §"Follow
existing patterns"):
- `TabsBar` — `Source/UI/Controls.h:132-147` / `.cpp:423+`. Used for all
  mode/key/length/phrase/arp/loop tab strips; drives choice params directly
  via `setChoiceParam()`/`choiceParam()` (`MelodyPanel.cpp:498-513`), **not**
  `ComboBoxAttachment` — coverage for `--check-params` is done manually via
  `shared.registerAttachment(paramId)`.
- `styleChip()` — chip-coloring helper, but it's a **local anonymous-namespace
  helper in `MelodyPanel.cpp:95-101`**, not in `Controls.h`. Promote it there
  if the new panel needs it and the old file goes away.
- `LensIconToggle` — `Source/UI/LensPanel.h:78-105` / `.cpp:297-380`. This is
  the toolbar chip class; the new panel's open/close trigger is the existing
  `melodyChip` instance (`LensPanel.h:128-129`), no new toggle-icon code needed.
- `ModKnob` (`Controls.h:49-98`) and `ParamToggle` (`Controls.h:116-129`) are
  the two genuine `AudioProcessorValueTreeState`-Attachment-backed widgets in
  use (5 FEEL knobs, loop/lock toggles) — reuse directly.
- `animate()` idiom: every visible panel exposes a manual `animate()` called
  from the editor's single `Timer::timerCallback()` (`PluginEditor.cpp:339-365`).
  Inside, `MelodyPanel::animate()` (`MelodyPanel.cpp:697-758`) caches
  last-seen values and only repaints the changed sub-rect — follow this
  "poll + diff + targeted repaint" idiom in the new panel, don't invent
  push notifications.
- `UiShared` (`Controls.h:17-28`) is the standard constructor-context struct
  every widget/panel takes — use it unchanged.

---

## 2. Editor sizing & resize feasibility

`PluginEditor.cpp:13-14`: `kBaseWidth = 1040`, `kBaseHeight = 660`.
Constructor (`PluginEditor.cpp:67-72`):
```cpp
setResizable (true, true);
setResizeLimits (kBaseWidth*0.7, kBaseHeight*0.7, kBaseWidth*2, kBaseHeight*2);
constrainer->setFixedAspectRatio ((double) kBaseWidth / kBaseHeight);
setSize (kBaseWidth, kBaseHeight);
```
- Resizing **is** supported, but locked to the 1040:660 aspect ratio, scaled
  uniformly 70%–200% by the user dragging the corner.
- `resized()` (`PluginEditor.cpp:96-100`) does **not** re-layout children —
  it computes `scale = getWidth()/kBaseWidth` and applies one
  `content.setTransform(AffineTransform::scale(scale))` to the whole `content`
  component. All children (header, playView, deepView, melodyPanel) live at
  fixed logical 1040×660 coordinates and are scaled together as a unit.
- **No existing code path grows the editor's own window (`setSize`) when a
  panel opens.** MelodyPanel's current "resizing" only repositions itself
  *within* the fixed 1040×660 canvas via its own transform — it never touches
  `LumenAudioProcessorEditor::setSize`.

**Implication for Phase 2**: PLAN.md offers two options — widen the editor
on panel-open, or overlay on top of the existing layout. Given the aspect-
ratio-locked resize contract and the fact that host DAWs (Ableton, Maschine 3)
must react cleanly to programmatic resize, **overlay is the lower-risk
default** unless a quick host-resize spike proves clean. A widen-on-open
approach would need to (a) break or special-case the fixed aspect ratio while
the panel is open, and (b) verify VST3 hosts resize their view without
glitching — worth a small experiment at the start of Phase 2, not an
assumption. The current popup already overlays inside `content` at a fixed
inner size, so an overlay-docked side panel is the smaller diff.

---

## 3. Seed flow: generation → storage → SEED readout, and lumena's set-seed surface

### 3a. Seed generation (plugin side, not engine side)
- `external/lumena` (the engine) **never seeds anything internally** — every
  entry point (`generateMelody`, `mutate`, `KeySelector::detect`,
  `ScaleLibrary::randomScale`, the Markov chain) takes a caller-owned
  `std::mt19937&`. The engine is a pure function of (grid, scale, options,
  rng-state). **No `setSeed`/`getSeed`/`setSeedLocked` API exists in lumena at
  all** — confirmed by grep across `external/lumena/src`.
- The plugin generates the 64-bit melody seed itself:
  `MelodyController::makeSeed()` (`Source/Melody/MelodyController.cpp:205-212`) —
  two 32-bit draws from a plain time-seeded `juce::Random` member, XOR-packed
  into a `juce::uint64`. This is ordinary wall-clock randomness, **not**
  derived from image bytes (that's a separate, unrelated FNV-1a seed used only
  for Lens's visual oscillator phases — `Source/Lens/LensEngine.cpp:196-206` —
  never shown in the UI, don't confuse the two).
- Before use, the 64-bit seed is **folded down to 32 bits**:
  `std::mt19937 gen (seed ^ (seed >> 32))` (`MelodyController.cpp:295`). Any
  Phase-1 design that wants genuine 64-bit entropy needs to address this fold.

### 3b. Single shared RNG stream — no sub-seeds today
- `renderFresh()` (`MelodyController.cpp:279-322`) constructs **one**
  `std::mt19937` and threads it through key detection (random-mode hue/sat
  draws), then the entire `generateMelody()` call — harmony/progression,
  rhythm, pitch walk, and ornament draws all pull sequentially from that same
  stream. There is no per-domain (rhythm/pitch/harmony) sub-seed anywhere in
  engine or plugin.
- What the engine *does* have is post-hoc splice/recombination, not
  independent sub-streams:
  - `RegenLocks{rhythm, pitch, harmony}` (`external/lumena/src/melody/
    MelodyGenerator.h:270-274`) + `recombineLocked()` (lines 276-284, impl
    `.cpp:2467`): takes the timing track from whichever of two **already
    fully-generated** melodies the lock says, and the pitch track from the
    other. It never re-seeds a "just rhythm" or "just pitch" sub-stream —
    both candidates consumed the *entire* shared RNG stream during their own
    generation.
  - **`RegenLocks::harmony` is declared but dead** — grepped, zero uses inside
    `recombineLocked`'s implementation. Harmony locking actually works by
    feeding `MelodyOptions::progression` (the previous melody's chord roots)
    back into a fresh `generateMelody()` call so it voices over that
    progression instead of drawing a new one — see `MelodyController::
    regenerate()`, `carryProgression` (`MelodyController.cpp:377-378`).
  - `mutate()` (`MelodyGenerator.h:286-291`) perturbs an existing `Melody` by
    `amount`, honoring `RegenLocks` for which dimensions stay untouched — also
    not a sub-seed mechanism, just direct perturbation of the note list.
- **Known coupling bug** flagged in `LUMEN_LUMENA_ANALYSIS_BRIEF.md:259-268`:
  because it's one shared stream, changes to pitch-drawing logic can shift
  *rhythm* onsets on ~5% of seeds downstream, purely from RNG draw-count
  drift. A real sub-seed design (Phase 1's splitmix64 proposal) would fix this
  class of bug as a side effect, not just add a UI feature.

### 3c. Lock API that exists but is **dead code**
- `MelodyController::locked()`/`setLocked(bool)` (`MelodyController.h:42, 47`)
  and `reroll()` (`MelodyController.h:39`) are fully implemented — `reroll()`
  draws a new seed and regenerates *unless* `lockedFlag` is set
  (`MelodyController.cpp:358-364`) — but **zero UI code calls any of them**
  (grepped `Source/UI/*`). This is exactly the seed-lock/reroll surface
  PLAN.md Phase 1 asks for; it already exists at the controller layer and
  only needs UI wiring plus a real `setSeed(uint64)` added alongside it (that
  part genuinely doesn't exist).
- Separately, **`regenerate()` always draws a brand-new master seed**
  regardless of `lockedFlag` (`MelodyController.cpp:369`, `lockedFlag` isn't
  even read in this function) — so today the displayed SEED value changes on
  every Regenerate click even when Lock Rhythm+Pitch make the audible result
  identical. This inconsistency (seed readout says "new" while content says
  "same") needs a decision in Phase 1: either make Regenerate respect
  `lockedFlag` for whether it reseeds at all, or accept that seed = "last
  entropy draw," decoupled from "what you're currently hearing."
- `mutate()` never touches `seedValue` at all (deliberate, commented at
  `MelodyController.cpp:430-431`) — it perturbs the persisted note sequence,
  which is correct and should carry over unchanged into the redesign.

### 3d. Seed storage
- Persisted in a dedicated `MELODY` ValueTree sub-tree, **not** an ordinary
  APVTS parameter: `Source/State/MelodyState.{h,cpp}` — `seed()`/`setSeed()`
  (hex-string round-trip, `MelodyState.cpp:40-60`), `locked()`/`setLocked()`
  (same file). Schema documented at `MelodyState.h:23`.
- **Critical constraint for Phase 1's persistence plan**: the plugin persists
  the *generated note sequence itself* (`melodystate::storeSequence`), not
  just seed + params, specifically because Lens only ever persists a 64×64
  thumbnail of the source image (SPEC 13) — full-resolution pixels never
  survive a save. Regenerating purely from a stored seed after reload would
  sample a different/coarser brightness grid than the live session did, and
  would **not** reproduce the same melody byte-for-byte. This is a deliberate,
  documented design decision (`MelodyState.h:16-20`), not an oversight. Any
  Phase-1 "seed + lock + sub-seed overrides restore on load" plan must keep
  the sequence as the reload source of truth; the seed stays provenance
  metadata for display/regeneration-from-current-image only, not a substitute
  for the stored notes.
- Message-thread mirror: `MelodyController::seedValue`
  (`MelodyController.h:90`).
- Load-time-only CLI backdoor exists: `--melody-seed <hex>` in
  `Source/Standalone/StandaloneApp.cpp:197-209` writes directly into the
  ValueTree before `applyState()` runs (for reproducible showcase renders).
  This bypasses the controller and is not a live `setSeed()` call — it can't
  be reused as-is for an in-panel "type a seed, hit apply" control.

### 3e. Seed display (read-only today)
- Drawn in `MelodyPanel::paint()` (`MelodyPanel.cpp:635-641`): `KEY` / `MOOD`
  / `FORM` / `SEED` rows, SEED formatted as
  `juce::String::toHexString(int64(seed()))` — uppercase hex, no fixed width
  (short seeds render short; the mockup's "98c31442dc00d84a" is a full-width
  example, not guaranteed).
- Repaint-on-change only, via cached string diff in `animate()`
  (`MelodyPanel.cpp:702-713`).
- **No click/edit handler anywhere** — confirmed no `TextEditor` binding or
  click callback on the SEED value. Matches Phase 3's goal exactly: this is
  new work, not a port.
- The "SEED" section *label* in the current panel actually headers the
  **RHYTHM/PITCH/HARMONY lock toggles** (`MelodyPanel.cpp:592-597`), a
  different, already-wired mechanism — see §4 below. Don't conflate that
  section with the read-only SEED value row; they're visually adjacent but
  functionally separate today.

### 3f. Determinism test coverage (lumena)
`external/lumena/tests/` has a hand-rolled `CHECK`-macro harness (no gtest,
driven from `main.cpp`) with extensive same-seed-twice coverage already:
`MelodyGeneratorTests.cpp` (`test_reproducible`, phrased-cadence, per-note
source-cell, arpeggiator, splice-lock, and progression-determinism tests),
`MarkovTests.cpp:66-84`, `ScaleTests.cpp:163-181`, `KeySelectorTests.cpp:133-136`.
**Gap**: none of these test a master-seed → sub-seed derivation scheme,
because that scheme doesn't exist yet. Phase 1 needs new tests for (a) same
master seed twice ⇒ identical sub-seeds ⇒ identical per-domain output, and
(b) reroll one sub-seed only ⇒ only that domain changes. Note (b) is **not**
achievable with the current splice-after-full-generation design without
deeper engine changes (see the coupling bug in §3b) — flag this as a possible
scope/effort surprise for whoever picks up Phase 1.

---

## 4. Mockup controls: existing params vs. genuinely new

Existing `MelodyPanel` wiring (`Source/UI/MelodyPanel.cpp:217-392`), param IDs
from `Source/State/Parameters.h:128-157`:

| Mockup control | Status | Existing wiring |
|---|---|---|
| PLAY / LOOP | exists | `playButton` → `melodyController().play()/stop()`; `loopToggle` → `params::melodyLoopPlayback` |
| MODE (MELODY/CHORDS/ARP) | exists | `modeTabs` → `params::melodyMode` |
| KEY (FROM IMAGE/RANDOM) | exists | `keyModeTabs` → `params::melodyKeyMode` |
| LENGTH (8/16/32) | exists | `lengthTabs` → `params::melodyLength` |
| SHAPE (UP/DOWN/UP-DN/CONV/RAND) | exists, **Arp-only** | `arpPatternTabs` → `params::melodyArpPattern`; maps to engine `ArpPattern` enum, only consulted when `mode == Arpeggio` per engine doc comment — confirm the mockup intends SHAPE to show/apply only in ARP mode, not MELODY/CHORDS |
| FEEL knobs (ENERGY/COMPLEX/IMAGE/REPEAT/DENSITY) | exist | `energyKnob`/`complexityKnob`/`imageKnob`/`repetitionKnob`/`densityKnob` → `melodyEnergy`/`melodyComplexity`/`melodyImageInfluence`/`melodyRepetition`/`melodyDensity` |
| LOOP LENGTH (OFF/1/2/4/8) | exists | `loopTabs` → `params::melodyLoopLength` |
| REGENERATE | exists | → `melodyController().regenerate()` |
| MUTATE | exists | → `melodyController().mutate()` |
| EXPORT (DRAG MIDI / SAVE .MID) | exists | `dragMidi` custom drag source; `saveButton` → `melodyController().saveMidiFile()` |
| TRANSPOSE −/+ stepper | exists | custom stepper nudging `params::melodyTranspose`, manually registered for `--check-params` |
| OCTAVE −/+ stepper | exists | same pattern, `params::melodyOctave`, range ±2 |
| GENERATED readout (KEY/MOOD/FORM) | exists | painted in `paint()`, sourced from `melodyController().detectedKey()/moodText()/formText()` |
| SEED readout (display only) | exists | see §3e |
| SEED: RHYTHM/PITCH/HARMONY (mockup shows as **reroll buttons**) | **partially exists, different semantics** | Current `lockRhythm`/`lockPitch`/`lockHarmony` (`params::melodyLockRhythm/Pitch/Harmony`) are **lock toggles** that constrain what Regenerate/Mutate may change — they do not themselves trigger a reroll of just that domain. A true "reroll RHYTHM only" button is **new** (Phase 1 territory, and per §3b/§3f may need engine work beyond a UI button) |
| SEED edit (click hex, type new value) | **new** | no edit path exists anywhere; `MelodyController` has no `setSeed()` |
| SEED lock (padlock icon, mockup) | **exists at controller layer, unwired** | `MelodyController::locked()/setLocked()` fully implemented but zero UI callers — needs a UI toggle, not new controller logic |
| Close [x] | exists | closes the popup; port to panel as-is |
| Info readout under image (TRANSPOSE/OCTAVE steppers positioned there) | **layout-new** | the steppers/values themselves exist (see above) but currently live inside the popup, not under the main Lens image — Phase 3 is a relocation + restyle, not new logic |

---

## Key discrepancies from PLAN.md worth flagging before Phase 1 starts

1. **Sub-seed derivation (splitmix64) isn't a drop-in addition.** The engine's
   single shared `std::mt19937` stream means a naively-added sub-seed scheme
   can't guarantee "reroll one domain, others stay byte-identical" without
   either (a) restructuring generation to draw from three independent
   streams per domain, or (b) accepting the current splice-after-full-
   generation approximation (which already mostly works today via
   `RegenLocks`/`recombineLocked`, just without exposed per-domain reroll
   buttons). Recommend Phase 1 start by deciding explicitly which of these
   two it's doing — the smaller change is exposing UI for the existing
   lock+splice mechanism plus a real `setSeed()`; the larger change is
   restructuring the engine's RNG usage.
2. **Reload persistence must stay sequence-first, not seed-first** — the
   64×64 thumbnail limitation (SPEC 13) means seed-only persistence would
   silently produce different melodies after reload. PLAN.md's Phase 1 §3
   ("master seed, lock flag, sub-seed overrides... restore on load") should
   be read as *restoring the display/provenance*, not as the reload
   regeneration path — the stored `SEQ` stays authoritative.
3. **Two separate, confusable lock concepts already exist**: the coarse,
   currently-dead `MelodyController::lockedFlag` (gates `reroll()`) and the
   three live `RegenLocks` params (gate `regenerate()`/`mutate()` splicing).
   Phase 1 needs to either unify these or clearly document why both remain.
4. **`RegenLocks::harmony` is dead code in the engine** — harmony locking
   actually flows through `MelodyOptions::progression`. Don't build new UI
   assuming the `harmony` bool field does anything inside `recombineLocked`.
5. **Editor resize-on-panel-open is unproven.** Nothing in the current
   codebase resizes `LumenAudioProcessorEditor` dynamically; the fixed-aspect-
   ratio resize contract plus multi-host support (Ableton, Maschine 3) makes
   overlay the safer Phase 2 default absent a quick spike proving otherwise.
6. **SHAPE (ArpPattern) is Arp-mode-only per the engine's own doc comment** —
   confirm with the user whether the mockup's SHAPE row should be hidden in
   MELODY/CHORDS mode or whether it's meant to gain new meaning there.
7. The standalone `/mnt/c/Users/pixel/Projects/lumena` checkout is stale
   (missing `RegenLocks` entirely) — don't survey or prototype against it;
   use `external/lumena` inside this repo.

---

## Phase 1 (revised) — what was actually wired

Executed against overriding decisions from the user, not PLAN.md's original
splitmix64/sub-seed design (see discrepancy #1 above, now resolved this way):
RHYTHM/PITCH/HARMONY stay lock toggles (no per-domain reroll buttons), no
`external/lumena` changes, persistence stays sequence-first.

### Changes (`Source/Melody/MelodyController.{h,cpp}` only; no UI, no engine)

1. **`regenerate()` now honours the master lock.** Previously it unconditionally
   called `makeSeed()` (discrepancy #3c in Phase 0's notes — the seed readout
   changed on every click even when Lock Rhythm+Pitch made the audible result
   identical). Now: `const juce::uint64 newSeed = lockedFlag ? seedValue :
   makeSeed();`. Locked: byte-identical seed and — since params also unchanged
   — byte-identical output. Unlocked: draws fresh, same as before. The
   rhythm/pitch/harmony `RegenLocks` splice still runs afterward exactly as
   before; this only changes which seed the fresh candidate is generated at.
2. **`setSeed(juce::uint32 newSeed)` added.** Pins the seed and calls
   `generate()` (which persists via the existing `installSequence()` →
   `melodystate::setSeed()` path — same mechanism `reroll()` already used, no
   new persistence code needed). This is the method a future seed-edit UI
   (Phase 3) will call.
3. **`locked()`/`setLocked()`/`reroll()` are no longer dead code conceptually**
   — `setLocked()` was already fully implemented (Phase 0 found it, just
   unwired from any UI); `regenerate()` now actually consults it. Still no UI
   calls any of these yet — that's Phase 2/3's job. This phase only makes the
   controller-layer contract correct and ready to wire up.

### The 64-bit-vs-32-bit seed question (explicitly asked for in this task)

**Resolved: only 32 bits of the master seed ever functionally matter, and the
code now reflects that honestly.** Two independent reasons converge on this:

- `renderFresh()` seeds a `std::mt19937` via its single-value constructor,
  whose actual entropy ceiling is 32 bits (`std::mt19937::result_type` is a
  32-bit-ish unsigned integer) — no matter how wide a value you hand it, only
  32 bits of state result. This isn't a design choice in this codebase, it's
  an inherent property of `std::mt19937`'s API being used this way (both here
  and throughout `external/lumena`'s own tests).
- The **old** `makeSeed()` drew two independent 32-bit values (`hi`, `lo`) and
  packed them into a 64-bit `juce::uint64` as `(hi<<32)^lo`, but the old fold
  at use-time, `seed ^ (seed>>32)`, collapses back down to a single 32-bit
  value `hi^lo`. That's a many-to-one mapping: for any target 32-bit result,
  2^32 different-looking 64-bit `(hi,lo)` pairs produce it. A hypothetical
  64-bit hex edit box built on the old code would have been actively
  misleading — a user could type two visually distinct 16-hex-digit seeds and
  get byte-identical melodies with no indication why.

**Fix applied**: `makeSeed()` now draws a single 32-bit value and returns it
widened to `juce::uint64` (high 32 bits always zero). `setSeed()` takes
`juce::uint32` directly, so it's impossible to pass in a value with meaningful
high bits in the first place. `renderFresh()`'s fold was simplified from the
XOR-fold to a plain truncation — mathematically a no-op now (high bits are
always zero) but no longer implies high bits matter when they structurally
never will again. `seed()` still *returns* `juce::uint64` (kept for
source/binary compatibility with `MelodyState`'s existing hex64 persistence
format, `writeTempMidiFile()`'s hex filename, the `--melody-seed` CLI flag,
and the existing `MelodyGeneratorTests`/`TestsMain.cpp` call sites that already
type it as `juce::uint64` — none of that needed to change) — but the *value*
is now always ≤ `0xFFFFFFFF`, so `toHexString()` naturally renders at most 8
significant digits.

**Recommendation carried into Phase 2/3**: display and accept the seed as an
8-hex-digit (32-bit) value in the UI, not 16. If the edit box is sized for 16
digits "to match the mockup," either mask/clamp typed input to the low 32 bits
before calling `setSeed()`, or — simpler and preferred — just size the field
for 8 digits; nothing is lost since that's the entire entropy space. No UI
code was touched this phase, so this is a note for whoever builds Phase 2/3,
not something already handled.

### Persistence — verified, not newly built

Per the Phase 0 finding, master seed and master lock were **already**
persisted and restored correctly before this phase:
`installSequence()`/`setLocked()` call `melodystate::setSeed()`/`setLocked()`
on every change, and `MelodyController::applyState()` restores both from the
`MELODY` sub-tree. The three domain locks
(`melodyLockRhythm`/`melodyLockPitch`/`melodyLockHarmony`) are ordinary
`AudioParameterBool`s and already round-trip through `apvts.copyState()` /
`replaceState()` like every other parameter — no custom restore code exists
or is needed for them. **No new persistence plumbing was required for this
task's requirement #3** — it asked to "add" these to saved state, but they
were already there; this phase just confirmed it (and added a regression test
for the specific seed+lock combination, since existing coverage exercised
summary/param round-trips but not this exact pair — see below). Sequence-first
reload behavior (Phase 0 §3d) is untouched: `applyState()` still restores
`currentSeq` from the stored `SEQ` node, never re-derives it from the seed.

### Determinism verification

**Automated**: added `MelodySeedLockTest` to `Tools/Tests/TestsMain.cpp`
(registered alongside the existing `MelodyTortureTest`), covering:
`setSeed()` reproduces byte-identical note sequences when set to the same
value twice; locked `regenerate()` × 5 leaves both the seed and every note
unchanged; unlocked `regenerate()` draws a different seed; and seed + master
lock both survive an `apvts.copyState()`/`replaceState()`/`applyState()`
round-trip. Full suite result: `ALL TESTS PASSED` (no regressions in the
pre-existing "rapid regenerate x20" determinism test either).

**Manual, black-box**: ran the real Standalone exe twice —
`Lumen.exe --lens-image busy.png --melody --melody-seed 1234ABCD
--melody-export take1` / `take2` — same image, same seed, same (default)
params. Result: `take1.mid` and `take2.mid` SHA-256-identical
(`68c5b6f6...c3111156de7d495785889dbf1d546db2c06c3eb0`), and
`take1.txt`/`take2.txt` (key/mood/form/seed/notes summary) byte-identical.
Confirms the whole path — CLI seed pin → `renderFresh()`'s new truncation →
`generateMelody()` — end to end, not just through the unit-test harness.

### Surprises

1. **`mutate()`'s mutation-amount RNG was already silently discarding half of
   the old seed's entropy.** It builds its own throwaway RNG via
   `std::mt19937 mrng (static_cast<std::uint32_t> (makeSeed()))` — a direct
   truncation, not the `seed^(seed>>32)` fold `renderFresh()` used. Under the
   old two-draw `makeSeed()`, that truncation kept only `lo` and silently
   dropped `hi` entirely. The new single-draw `makeSeed()` doesn't change
   `mutate()`'s behavior at all (same effective value distribution) — it just
   removes a wasted second `rng.nextInt()` call that was never going anywhere.
2. **Locking now genuinely freezes REGENERATE's output**, not just its seed
   display. This resolves discrepancy #3c from the Phase 0 notes as a
   side-effect of the literal "wire the lock into regenerate" instruction —
   worth flagging to the user since it's a small behavior change beyond pure
   plumbing: previously REGENERATE always did *something* (new seed, possibly
   spliced against dimension locks); now, with the master lock on and no
   param changes, clicking REGENERATE is an audible no-op. That seems like
   the obviously-intended behavior for a "lock" control, but flagging it
   explicitly since it's a user-facing behavior change, not just new API
   surface.
3. **No engine changes were needed**, confirming Phase 0 discrepancy #1's
   smaller-diff path was sufficient: exposing the existing controller-layer
   lock/splice machinery (plus a real `setSeed()`) covered everything this
   task asked for without touching RNG-stream structure in
   `external/lumena`. The "reroll one domain independently" gap noted in
   Phase 0 §3f still exists (per the user's decision, it's explicitly out of
   scope — RHYTHM/PITCH/HARMONY remain locks, not rerolls).

---

## Phase 2 — Side panel component (Opus)

Executed against overriding decisions from the user (not PLAN.md §Phase 2's
"prefer widening" wording): **overlay, not window resize**. Editor sizing is
untouched — the panel is a docked overlay on the fixed 1040×660 content canvas.

### What was built

- New `Source/UI/MelodySidePanel.{h,cpp}` — a plain `juce::Component` docked to
  the right edge of the editor `content` at logical width
  `kPanelWidth = 316`, full content height (660). Added as a hidden child of
  `content` (`PluginEditor.cpp` — `addChildComponent`), shown/hidden by the
  existing 60 Hz poll of `MelodyController::isPanelActive()`
  (`PluginEditor.cpp` timer), exactly the mechanism the popup used. No
  `setTransform` gymnastics: a plain `setBounds` docks it, and the editor's
  whole-content scale transform (`resized()`) scales it with everything else.
  This is simpler than the popup's per-panel transform and needs no
  move/resize gesture code — documented deviation from the decision's
  "setTransform positioning" phrasing; the requirement it protects
  ("overlay, don't resize the editor") is fully met.
- Controls ported 1:1 from `MelodyPanel` with the **same param IDs and
  attachment patterns** (TabsBar choice-param drivers registered via
  `registerAttachment` for `--check-params`, `ParamToggle`, `ModKnob`,
  `styleChip`): close `[x]`, PLAY + LOOP, MODE, KEY, LENGTH, SHAPE
  (PHRASED/FREEFORM in Melody mode, UP/DOWN/UP-DN/CONV/RAND in Arp mode —
  same mode-driven swap as the popup, confirmed on screenshot), FEEL knobs
  (ENERGY/COMPLEX/IMAGE/REPEAT/DENSITY), LOOP LENGTH, SEED
  (RHYTHM/PITCH/HARMONY lock toggles — styled as the existing `ParamToggle`
  chips, which fill the accent colour when engaged so a locked lock reads
  obviously "on"), REGENERATE/MUTATE, EXPORT (DRAG MIDI drag-source ported
  from `MelodyPanel::MidiDragSource`, SAVE .MID). No new widget classes.
- **REGENERATE locked indicator**: `paintOverChildren` draws a dim scrim + a
  small padlock glyph over REGENERATE whenever
  `MelodyController::locked()` (the master seed lock) is engaged, polled in
  `animate()`. Forward-compatible: no UI toggles the master lock yet (that's
  the padlock next to the seed hex, Phase 3), but the indicator responds the
  moment `locked()` is true (e.g. restored from state) — signalling that a
  locked regenerate reproduces the same sequence (Phase 1 behaviour).

### Deliberately NOT in the side panel (per the user's decisions)

- **The Lens image + sampling-grid visualization** stays in the LENS panel.
  `LensImageView::paint` already overlays the grid/path/glow whenever
  `isPanelActive() || hasMelody()` (`LensPanel.cpp`), unchanged — so the Lens
  overlay keeps working with the side panel open or closed.
- **The GENERATED readout (KEY/MOOD/FORM/SEED) and the TRANSPOSE/OCTAVE
  steppers** are Phase 3 (they move under the Lens image). They were NOT
  moved — they still live in the old `MelodyPanel` popup's `paint()`/steppers.
  Consequence: because the popup is no longer shown (see below), the GENERATED
  readout is temporarily **not visible anywhere** between Phase 2 and Phase 3.
  This is intentional per "leave the readout where it lives, don't half-move
  it" + "that readout is Phase 3, not now." Phase 3 rebuilds it under the image.

### Popup retirement — partial, files kept

- The old `MelodyPanel` is **no longer instantiated or shown**: `PluginEditor`
  now owns a `MelodySidePanel` instead, and the Lens `melodyChip` toggles the
  same `isPanelActive()` flag the side panel polls. The melody chip wiring in
  `LensPanel.cpp` was untouched (it already called `togglePanel()`).
- `Source/UI/MelodyPanel.{h,cpp}` are **kept in the build** because the
  `melodygrid` namespace (grid/path/glow drawing) lives in that TU and is used
  by `LensPanel.cpp`. The `MelodyPanel` **class** is now dead code (defined,
  never constructed) but compiles clean under `/W4`.

### Leftovers for Phase 5 cleanup

1. **`MelodyPanel` class is dead** (only `melodygrid` in that TU is live).
   Phase 5 should extract `melodygrid` into its own small file (e.g.
   `Source/UI/MelodyGrid.{h,cpp}`) and delete the rest of `MelodyPanel`,
   including its floating-window move/resize gesture code and the summary/
   transpose/octave paint — after Phase 3 has relocated the readout.
2. **`melodystate::windowBounds` / `setWindowBounds` / `clampWindowBounds`**
   (in `State/MelodyState.{h,cpp}`) are now unused — they persisted the
   floating popup's placement, which a fixed dock doesn't need. Remove in
   Phase 5 along with any `MELODY` window-bounds attribute handling. The
   `#include "State/MelodyState.h"` was dropped from `PluginEditor.cpp`.
3. **Seed display formatting** (8 hex digits / 32-bit, per Phase 1's note): the
   side panel shows no seed, so nothing to format here. Phase 3's seed row
   under the image must render/accept **8 hex digits**; `seedValue` is already
   ≤ 32-bit so `toHexString` yields ≤ 8 significant digits, but Phase 3 should
   zero-pad to a fixed 8 for a stable readout.

### Verification

- Build: **VST3 + Standalone + tests all built clean** (Release, `/W4`
  warnings-as-errors — a warning would have failed the build).
- Tests: full `lumen_tests.exe` suite → `ALL TESTS PASSED` (includes the
  Phase 1 `MelodySeedLockTest` and the regenerate-determinism torture test).
- Screenshot: `Lumen.exe --screenshot ... --view play --lens-image busy.png
  --melody --melody-seed 1234ABCD` — side panel renders docked full-height on
  the right, all sections in mockup order, correct spacing, SHAPE showing the
  Melody-mode PHRASED/FREEFORM strip. Keyboard remains visible/functional to
  the left of the panel; the Lens (and its master knob/meter on the header
  right) sit underneath the docked panel while it is open — the accepted
  consequence of overlay-not-resize; everything is reachable again on close.

---

## Phase 3 — Info readout under the image (Sonnet)

Executed against the session's overriding decisions (not PLAN.md's original
Phase 3 wording): two layout fixes were bundled in with the readout build,
both driven by Phase 2's accepted "overlay, not resize" consequence colliding
with things Phase 3 needed to keep visible.

### Layout fix A — Lens column repositions around the open side panel

> **REVERTED in Phase 3b.** The shift/restore logic below no longer exists —
> the melody panel became a window extension, so it never overlaps the Lens
> column and the column stays put. `layoutLensColumn()` is now a static layout;
> the `sidePanelOpen`/`sidePanelOpenCache` poll and the `kLensOpenX` shift were
> deleted. The GENERATED readout under the image is unchanged and stays. See
> the Phase 3b section at the bottom of this file.

`PlayView::layoutLensColumn()` (`Source/UI/Cards.cpp`) now computes the Lens
panel's x each time the side panel's open/closed state changes (polled once
per `PlayView::animate()` tick, edge-triggered, same "poll + diff" idiom as
everything else in this codebase — not a new mechanism):
- Closed: unchanged normal dock, x=764 (same rect Phase 2 always used).
- Open: **x=16, flush against the left edge** — not merely "just clear of the
  side panel" as the decision's wording first suggests. The GENERATED readout
  (this phase's new component, directly below the Lens image) shares its row
  with the four macro knobs (Tone/Motion/Space/Texture, fixed at x 300-740).
  Sliding the column only as far as clearing the side panel (kPanelWidth=316
  from the right edge, i.e. x≈456) would have landed the readout squarely on
  top of those knobs — caught via an actual screenshot diff, not by
  inspection; see Surprises below. x=16 clears both the knob row and the side
  panel, at the cost of fully overlapping the waterfall/spectrogram on that
  side, which the decision explicitly permits.
- Restoring on close is automatic: the same edge-triggered poll fires the
  other direction when `isPanelActive()` goes false.

### Layout fix B — side panel ends above the keyboard

> **SUPERSEDED in Phase 3b.** The 522 height cap is gone — as a window
> extension the panel runs the full content height (660) and overlays nothing,
> so the keyboard stays fully visible with no cap needed. The internal spacing
> that was tightened to fit 522 has been loosened back to a roomy full-height
> layout matching the mockup. See the Phase 3b section at the bottom.

`MelodySidePanel` now docks at height 522 instead of the full content height
660 (`PluginEditor.cpp`: `kSidePanelHeight = 522`, chosen so its bottom edge
sits 10px above the keyboard's fixed y=532 in `PlayView`). All spacing inside
`MelodySidePanel::resized()` was tightened to fit ten sections (transport
through EXPORT) into the smaller height — padding 14→10, close-button
clearance 18→14, section gaps 14→9/8, FEEL knob row 62→54, action/export rows
trimmed a few px each. Verified by screenshot, not just arithmetic: every
section renders with no clipping, ~20px of slack remains at the bottom edge.

### GENERATED readout — new component, not a port-in-place

New `Source/UI/MelodyReadout.{h,cpp}`, a `juce::Component` living in
`PlayView` (not inside `LensPanel` itself — the mockup draws it as a plain,
unbordered text block sitting below the Lens card's frame, not inside it).
Positioned directly under the Lens image at whatever x `layoutLensColumn()`
currently has the Lens panel at, so the two always move together.

- **KEY / MOOD / FORM**: painted directly in `paint()` from
  `MelodyController::detectedKey()/moodText()/formText()`, same em-dash-for-
  empty styling as the old popup's summary block.
- **SEED**: a custom `juce::Label` subclass (`SeedLabel`) with
  `setEditable(true, false, true)` — single click to edit, and
  `lossOfFocusDiscardsChanges=true` so JUCE's own `Label::hideEditor` reverts
  cleanly (built-in, no revert code needed) if the user clicks away without
  pressing Return. `editorShown()` restricts input to 1-8 hex chars
  (`setInputRestrictions(8, "0123456789abcdefABCDEF")`); `textWasEdited()`
  parses via `getHexValue64()` and calls `MelodyController::setSeed()` only
  if the committed text is non-empty (empty commit = Return on a
  fully-deleted field — same as a discard, just routed through the "commit"
  path instead of the "escape" path per JUCE's `Label::textEditorReturnKeyPressed`,
  so it's handled explicitly rather than relying on the discard flag). After
  any commit the label always resyncs to `MelodyController::seed()` reformatted
  as 8 lowercase hex digits (`toHexString().paddedLeft('0', 8).toLowerCase()`),
  so what's on screen is always the controller's actual value, never raw
  user input.
- **Lock padlock**: `LockToggle`, a small custom `juce::Component` (not
  `ParamToggle` — the master seed lock is plain `MelodyController` state, not
  an APVTS bool param) bound directly to `locked()`/`setLocked()`. Same
  padlock glyph MelodySidePanel already draws over REGENERATE when locked
  (local copy of `drawLockGlyph`, same as the existing `styleChip` duplication
  pattern — promote both alongside the Phase 5 cleanup). Because both
  MelodySidePanel's REGENERATE indicator and this toggle read/write the same
  `MelodyController::lockedFlag` through the same getter/setter, toggling the
  lock from either side is guaranteed to update the other on the next
  `animate()` tick — single source of truth, no new synchronization code
  needed. (Verified by code inspection, not a live click-through — the
  `--screenshot` harness captures one static frame and has no CLI flag to
  pre-set the lock or simulate a click; a real interactive check in a DAW/
  standalone is still worth a quick look at the user gate.)
- **TRANSPOSE / OCTAVE steppers**: ported byte-for-byte in behavior from the
  old popup (`nudgeParam`/`intParam` helpers, same ±12 semitone / ±2 octave
  clamps, same "TRANSPOSE"/value-in-the-same-rect paint idiom). Both params
  are (re-)registered via `shared.registerAttachment()` here since the old
  `MelodyPanel` that used to register them is dead code (Phase 2) and never
  constructed — confirmed via `--check-params`: 115/115 attached, 0 missing.
- **Visibility**: the whole block hides via `setVisible(false)` when
  `hasMelody()` is false, shown otherwise — polled in `animate()`, edge-
  triggered. Caught a real bug here (see Surprises): must be added to
  `PlayView` with `addChildComponent`, not `addAndMakeVisible` — the latter
  unconditionally calls `setVisible(true)` on the child, overriding the
  constructor's initial hidden state.

### Dead popup readout/stepper paths — noted for Phase 5, not deleted

`Source/UI/MelodyPanel.cpp`'s summary-block paint code (`summaryArea`,
`transposeLabelArea`/`octaveLabelArea`, the GENERATED rows, the transpose/
octave stepper buttons and their `nudgeTranspose`/`nudgeOctave` lambdas) is
now **fully superseded** by `MelodyReadout` and has been dead since Phase 2
made the whole `MelodyPanel` class unconstructed. Left in place per the
existing Phase 2 leftover-for-Phase-5 note (`melodygrid` in the same TU is
still live, used by `LensPanel.cpp`) — Phase 5 should delete the summary/
stepper code (and the `summaryCache`/`transposeCache`/`octaveCache` members
that back it) in the same pass that extracts `melodygrid` into its own file.

### Surprises

1. **The layout-fix-A x target isn't "just clear the side panel"** — the
   decision's own wording undersold it. The readout's row is shared with the
   fixed macro-knob strip (x 300-740), which the side panel alone doesn't
   overlap but a merely-side-panel-clearing Lens shift (x≈456) does. Only
   caught by rendering an actual `--screenshot` and looking at it — the
   knobs and the readout's "GENERATED"/"F Dorian"/etc. text were overlapping
   in the first render. Fixed by moving the whole open-state column flush to
   the left edge (x=16) instead.
2. **`addAndMakeVisible` vs `addChildComponent` for a self-hiding child.**
   `MelodyReadout`'s constructor calls `setVisible(false)` so it starts
   hidden until a melody exists, but `PlayView`'s original
   `addAndMakeVisible(readout)` silently re-visibled it on add (JUCE's
   documented behavior, easy to forget) — the first closed-panel-no-melody
   screenshot showed the whole block rendered with dashes/blank SEED instead
   of nothing. Same fix pattern the codebase already uses for
   `deepView`/`melodySidePanel` (`addChildComponent`, not
   `addAndMakeVisible`) — should have matched it from the start.
3. Both surprises were caught by the "build, screenshot, actually look"
   verification step this repo's CLAUDE.md requires, not by code review —
   worth remembering neither would show up in a compile or the unit-test
   suite (no DSP touched this phase, so `lumen_tests`/`lumen_render --analyze`
   were unaffected and stayed green throughout).

### Verification

- Build: VST3 + Standalone + tests all built clean (Release, `/W4`
  warnings-as-errors).
- Tests: full `lumen_tests.exe` suite → `ALL TESTS PASSED` (no DSP changed
  this phase; this just confirms nothing else regressed).
- `--check-params`: `{"total_params":115,"attached":115,"missing":[]}`.
- `pluginval --strictness-level 10`: `SUCCESS`.
- Screenshots (`--view play --lens-image busy.png`, with/without
  `--melody`/`--melody-seed 1234ABCD`): (a) side panel closed, no melody —
  readout correctly absent, Lens panel at its normal x=764; (b) side panel
  open with a melody — Lens image + full GENERATED readout (KEY/MOOD/FORM/
  SEED "1234abcd"/padlock/TRANSPOSE/OCTAVE) fully visible at x=16, no
  overlap with the macro knobs, all ten side-panel sections visible with no
  clipping, keyboard fully visible below the shortened side panel.
- `lumen_render --preset init --analyze`: nan_count 0, metrics unchanged
  from prior phases (no DSP touched).

---

## Phase 3b — Panel becomes a window extension (not an internal overlay)

Goal (from the mockup `assets/side-panel-mockup.png`): the melody panel extends
the editor **to the right** as a separate full-height column. The base 1040×660
UI stays pixel-identical and nothing inside it moves. This replaces Phase 2's
"overlay on the fixed canvas" approach and removes both Phase 3 layout fixes,
which only existed to work around that overlay.

### What changed

1. **Panel is now a window extension, not an overlay.**
   - `content` (the scaled canvas in `PluginEditor`) is now `kBaseWidth +
     kPanelWidth` wide (1040 + 316 = 1356) × 660. The base UI still occupies
     logical x 0…1040 exactly as before; the panel lives in the strip at
     logical x = 1040, width 316, **full height 660** (`MelodySidePanel` bounds
     in the `PluginEditor` constructor).
   - When the panel opens, `LumenAudioProcessorEditor::setPanelOpen(true)`
     widens the editor by the panel width (316 × current scale), relocks the
     aspect ratio to `(kBaseWidth+kPanelWidth):kBaseHeight`, and rescales the
     resize limits to the wider logical width. On close it narrows back to the
     base width and restores the base aspect ratio/limits. Show/hide is still
     driven by the existing `isPanelActive()` poll in `timerCallback()` — only
     the geometry response is new; `setPanelOpen` is called on the same
     visibility-change edge.

2. **`resized()` now derives the content scale from HEIGHT, not width.**
   Previously `scale = getWidth()/kBaseWidth`. Now `scale = getHeight()/
   kBaseHeight`. Because opening the panel changes width but not height, keying
   the scale off height keeps the base canvas pixel-identical in both states,
   and the aspect lock keeps a corner-drag consistent (width is tied to height).
   This is also the host-safety mechanism — see "Known limitation" below.

3. **The content scale transform covers the panel region.** The panel is a
   child of `content`, so the single `content.setTransform(scale)` scales it
   with everything else. Panel controls scale with the rest of the UI across
   the 70–200 % zoom range — no separate transform, structurally guaranteed.

4. **Layout fix A reverted.** `PlayView::layoutLensColumn()` (`Cards.cpp`) is
   static again: the LENS panel + GENERATED readout dock at their fixed x=764
   and never move. The `sidePanelOpen` shift, `kLensOpenX`, the
   `sidePanelOpenCache` member, and the reposition-on-toggle poll in
   `PlayView::animate()` were all deleted. The GENERATED readout under the image
   (Phase 3's real deliverable) is untouched and stays.

5. **Layout fix B superseded.** `MelodySidePanel` runs the full 660 height again
   (the `kSidePanelHeight = 522` cap is gone). `MelodySidePanel::resized()`'s
   spacing was loosened from the tightened 522-fit values back to a roomy
   full-height layout (padding 16, section gaps 18, FEEL knob row 62) so the ten
   sections breathe across the whole height like the mockup. The keyboard stays
   fully visible because the panel no longer overlays anything.

### Known limitation — host must honour the programmatic resize (verify in DAW)

The panel strip is only visible if the host grows the plugin view when the
editor calls `setSize`. Most hosts do, but some ignore programmatic resizes.
The plugin is **safe** either way: because the content scale is derived from
height (not width), a host that ignores the widen leaves the base 1040×660 UI
fully correct and simply clips away the strip at logical x ≥ 1040 — the panel
just won't be visible; nothing is broken, misdrawn, or shrunk. On close the
editor narrows back regardless.

**User to verify** in Maschine 3 and Ableton Live: open the melody panel and
confirm the host view actually widens to reveal the strip (and narrows back on
close). If a host refuses to resize, the melody controls are unreachable in
that host until it does — that is the accepted degraded state, not a crash.
pluginval strictness 10 passes (SUCCESS) and does not itself exercise the
host-side resize, so this specific behaviour is a DAW-only check.

### Verification

- Build: VST3 + Standalone + tests all built clean (Release, `/W4`
  warnings-as-errors).
- Tests: full `lumen_tests.exe` suite → `ALL TESTS PASSED`.
- `--check-params`: `{"total_params":115,"attached":115,"missing":[]}`.
- `pluginval --strictness-level 10`: `SUCCESS`.
- `lumen_render --preset init --analyze`: nan_count 0 (no DSP touched).
- Screenshots (`--view play --lens-image busy.png`): (a) panel closed, no
  melody — editor is exactly 1040×660, base UI in its normal position, no
  readout; (b) panel open with a melody — editor is exactly **1356×660** (base
  widened by exactly 316), the full-height panel column renders all ten
  sections with roomy spacing matching the mockup, and the base region is
  **pixel-identical** to (a): a per-zone diff of the waterfall, keyboard, and
  header showed 0 differing pixels (a single AA-jitter pixel on one macro knob).
  The LENS image + GENERATED readout stay at x=764 in both states (fix A
  revert confirmed — the column no longer shifts).

---

## Phase 4 — SCAN/SPECTRAL icon fix (Sonnet, tiny)

Pure paint-code swap, exactly as scoped. The two mode-tab glyphs in
`LensPanel::LensPanel`'s `modeTabs` initializer (`LensPanel.cpp`) were
reversed: SCAN (tab index 0, `lens.mode() == 0`) was drawing `"|"` (vertical)
and SPECTRAL (index 1) was drawing the em dash (horizontal). Swapped the two
glyph strings in the initializer list so SCAN now draws the em dash
(horizontal — rows played as waveforms, beam travels down the image) and
SPECTRAL draws `"|"` (vertical — spectrogram columns, beam travels across).
Updated the adjacent comment to match. Nothing else touched: `setTabTooltips
({ "Scan", "Spectral" })` still pairs correctly with the (unchanged) tab
order, and the scanline-direction logic that actually drives the beam
(`LensImageView::paint`, `scanMode = lens.mode() == 0`) was left alone per the
task — that's mode *behavior*, not the icon.

Verified by screenshot: cropped and 4×-zoomed the mode-tab row
(`build/verify/lens_row_zoom.png`) and confirmed the **active** tab (SCAN, the
default) now shows the horizontal glyph, with SPECTRAL's vertical glyph
alongside, unselected.

---

## Phase 5 — Dead code removal, persistence QA, and interaction QA (Sonnet)

Executed together with Phase 4 in one session per the task's explicit
instruction to merge the two phases.

### Dead code removal

1. **`melodygrid` extracted to its own file first**, so `LensPanel.cpp` keeps
   compiling once the rest of `MelodyPanel` is gone (per the Phase 2/3
   leftover notes above). New `Source/UI/MelodyGrid.{h,cpp}` holds the
   `melodygrid` namespace (`DrawInfo`, `draw()`) verbatim — no logic changes.
   `LensPanel.cpp`'s include changed from `UI/MelodyPanel.h` to
   `UI/MelodyGrid.h`. `CMakeLists.txt` swapped `MelodyPanel.{h,cpp}` for
   `MelodyGrid.{h,cpp}` in both the header and source lists.
2. **`Source/UI/MelodyPanel.{h,cpp}` deleted outright** — grepped first
   (`grep -rn MelodyPanel Source Tools CMakeLists.txt`) and confirmed the only
   remaining hits after the `melodygrid` extraction were the class definition
   itself (now unused) and a few historical/provenance comments in
   `MelodySidePanel.{h,cpp}` and `MelodyReadout.h` ("ported from the old
   MelodyPanel popup...") — left alone, they're documentation of where the
   code came from, not dead code. This removes the popup class itself, its
   floating-window move/resize gesture code (`mouseDown`/`mouseDrag`/
   `mouseUp`/`mouseMove`, `applyPlacement`/`persistPlacement`,
   `inTitleStrip`/`inResizeCorner`), and the superseded summary/transpose/
   octave paint code (`paint()`'s GENERATED block, `TRANSPOSE`/`OCTAVE`
   readouts, `sectionLabels`) — all fully superseded by `MelodySidePanel` and
   `MelodyReadout` since Phase 2/3.
3. **`melodystate::windowBounds`/`setWindowBounds`/`clampWindowBounds`**
   removed from `Source/State/MelodyState.{h,cpp}` (impl + the four
   `kWinX/Y/W/H` `Identifier`s), along with the now-unused
   `<juce_graphics/juce_graphics.h>` include in `MelodyState.h` (nothing left
   in that header uses a graphics type). `MelodyWindowBoundsTest` removed from
   `Tools/Tests/TestsMain.cpp` along with its registration — it existed solely
   to cover these helpers.
4. **Phase 3 layout-fix A/B remnants**: none found. Grepped for
   `kLensOpenX`, `sidePanelOpenCache`, `kSidePanelHeight`, and
   `windowBounds` across `Source/` and `Tools/` — Phase 3b's revert (see
   above) already fully removed them; `PlayView::layoutLensColumn()` is
   confirmed static (fixed x=764, no open/closed branch) and
   `MelodySidePanel` has no height cap. Nothing left to clean up here.

### Persistence QA

Confirmed via existing tests rather than new ones, with one genuine gap
filled:

- **Master seed + master lock**: already covered end-to-end by
  `MelodySeedLockTest`'s "master lock + seed round-trip through saved state"
  case (`apvts.copyState()`/`replaceState()` + `applyState()`).
- **Domain locks (RHYTHM/PITCH/HARMONY), transpose, octave, and every other
  side-panel param** (mode, key mode, length, phrase, arp pattern, loop
  length, the five FEEL knobs, loop playback): already covered by
  `MelodyParamRoundtripTest`, which sets all of them to a non-default value,
  serializes, and restores into a fresh processor.
- **Gap found and filled**: nothing tested that the melody side panel's
  open/closed state is correctly *excluded* from persistence (per the task's
  explicit "panel starts closed on fresh load — open state is not
  persisted"). Confirmed by code inspection first —
  `MelodyController::panelActive` is a plain in-memory `bool` (`MelodyController.h`),
  never written to or read from the `MELODY` ValueTree subtree or any APVTS
  param — then pinned with a new regression test,
  `MelodyPanelOpenNotPersistedTest` (`TestsMain.cpp`): a fresh controller
  starts closed; opening it, saving state, and restoring that state into a
  **second, freshly-constructed** controller (simulating a real fresh plugin
  load) leaves the new controller closed; and calling `applyState()` on the
  *same* controller that had the panel open leaves it open (proving
  `applyState()` doesn't touch the flag either way — it's simply outside the
  persisted contract).

Full suite: `ALL TESTS PASSED`, including the new test and all pre-existing
Melody/State tests.

### Interaction QA

No DAW available in this environment, so verification split between
screenshot-driven checks (real, decisive) and code inspection (for the one
thing a static screenshot can't show — live interactive dragging):

- **Open/close + toggle modes + play**: `--view play --lens-image busy.png
  --melody --melody-seed 1234ABCD --notes 60,64,67` renders the side panel
  open, MODE/KEY/LENGTH/SHAPE tabs in their correct states, the waterfall
  active, and three keyboard keys highlighted — all together, no glitches.
  `--view deep` with the same flags confirms the compact LENS card (using the
  newly-extracted `MelodyGrid.cpp`) still draws the sampling-grid overlay
  correctly alongside the open side panel — the `melodygrid` extraction in
  this phase didn't regress its only other caller.
- **Base UI never shifts**: pixel-diffed the panel-closed and panel-open
  screenshots over the region that excludes the LENS/readout column (x
  0–762, which necessarily differs since one run has an image loaded and a
  melody generated and the other doesn't) — **1 differing pixel out of
  502,920** (a single AA-jitter pixel on a macro knob, same class of
  non-issue Phase 3b's own per-zone diff called out). The editor widened from
  exactly 1040×660 to exactly 1356×660 (base width + `kPanelWidth`), matching
  Phase 3b's original measurement.
- **Corner-drag resize, open and closed**: verified by code inspection, not a
  live drag — the screenshot harness has no way to synthesize a mouse-drag
  resize gesture, and this phase didn't touch `PluginEditor::resized()` or
  `setPanelOpen()`. Re-read both: `resized()` derives `content`'s scale from
  **height only**, and `setPanelOpen()` relocks the aspect ratio and rescales
  the resize limits to the current panel state on every open/close toggle.
  Because JUCE's `ComponentBoundsConstrainer` enforces the locked aspect
  ratio during an interactive drag, and the content scale never reads width
  directly, a corner-drag in either state necessarily produces a
  self-consistent uniform scale — this is the same mechanism Phase 3b
  screenshot-verified for the open/closed transition itself. **Still worth a
  few seconds of an actual drag in the standalone or a DAW at the user gate**,
  same spirit as Phase 3b's own "Known limitation" note about host-side
  resize honoring.
- **No dangling listeners**: this phase only deleted unreferenced code and
  extracted an already-shared free function; no listener registration/
  deregistration code was touched, so there's no new surface for a dangling
  listener. The existing "poll + diff" idiom used throughout (see Phase 0's
  survey) never registers component listeners in the first place, which is
  what keeps this class of bug from arising here.
- **Silent-at-idle smoke check**: `--view play` with no `--lens-image`/
  `--melody`/`--notes` shows the waterfall at its drained flat-surface idle
  state — confirms the standalone launches and produces no sound/activity
  indication with nothing playing.

### Verification

- Build: VST3 + Standalone + tests all built clean (Release, `/W4`
  warnings-as-errors) after reconfiguring CMake to pick up the new/removed
  source files.
- Tests: full `lumen_tests.exe` suite → `ALL TESTS PASSED` (36 "Melody"-tagged
  sub-tests in the log, including the three new `MelodyPanelOpenNotPersistedTest`
  cases).
- `--check-params`: `{"total_params":115,"attached":115,"missing":[]}`
  (unchanged from Phase 3 — this phase added/removed no params).
- `pluginval --strictness-level 10`: `SUCCESS`.
- Screenshots in `build/verify/`: `closed_no_melody.png`, `open_scan.png`,
  `open_deep.png`, `open_playing.png`, `silent_check.png`,
  `lens_row_zoom.png` (gitignored, Windows-visible).

This completes the `ui/side-panel-redesign` branch. Merging is left to the
user.

---

## Phase 6 — Panel polish: border, no-image state, lock indicator (`ui/panel-polish`)

Three fixes on top of the completed redesign, done together per the task's
explicit bundling.

### Fix A — remove the yellow panel border

`MelodySidePanel::paint()` drew `theme::neonYellow.withAlpha (0.5f)` /
`drawRoundedRectangle` around the whole panel after the background fill.
Deleted; the panel background/corner radius/section captions are otherwise
untouched. No border replaces it — matches the request ("border only").

### Fix B — no-image inactive state

New `MelodyController::hasImageSource()` (`Source/Melody/MelodyController.{h,cpp}`)
wraps `lens.displayImage (lens.target()).isValid()` — the exact same
condition `renderFresh()` already gates generation on, so the UI disables in
lockstep with when generation would actually no-op. Pure controller-layer
query, no engine/GUI coupling.

- **`MelodySidePanel`**: `animate()` polls `hasImageSource()` each tick
  (edge-triggered, same "poll + diff" idiom as the rest of the panel) and
  calls `setGenerationControlsEnabled(bool)`, which toggles `setEnabled()` on
  MODE, KEY, LENGTH, SHAPE (both tab strips), the five FEEL knobs, LOOP
  LENGTH, the three SEED domain locks (RHYTHM/PITCH/HARMONY), and REGENERATE/
  MUTATE. `juce::Component::isEnabled()` walks the parent chain, so disabling
  each container (`TabsBar`/`ModKnob`/`ParamToggle`/`TextButton`) is
  sufficient to both block clicks (JUCE's `Button`/`Slider` internals already
  check `isEnabled()`) and drive dimmed painting — no per-child wiring
  needed. PLAY, LOOP, DRAG MIDI, and SAVE .MID are never touched by this
  method, so a previously generated melody stays playable/exportable with no
  image loaded, per the requirement.
  - A small hint ("load an image to generate") is reserved in a fixed strip
    between the transport row and MODE, added via `resized()` — which is
    manually re-invoked from the same `animate()` edge trigger (nothing else
    calls `resized()` at runtime once the panel is docked, so this is a
    deliberate, one-off relayout call, not a per-frame cost). When active the
    strip collapses back to empty and the layout is byte-identical to before
    this phase — confirmed by screenshot.
- **`MelodyReadout`**: the SEED row's edit label and lock padlock
  (`seedLabel`/`lockToggle`) independently disable via the same
  `hasImageSource()` poll in `animate()`, gated inside the existing
  `hasMelody()`-active branch (so it only matters once a melody exists to
  show a readout for). KEY/MOOD/FORM and the TRANSPOSE/OCTAVE steppers are
  untouched — they're playback/export-adjacent, not generation triggers, so
  they stay usable exactly like PLAY/LOOP/EXPORT. `LockToggle` (a plain
  `juce::Component`, not a `Button`) didn't inherit JUCE's `isEnabled()`
  click-gating for free, so `mouseUp` now checks `isEnabled()` explicitly,
  and `paint()` dims via the same `withMultipliedAlpha` idiom
  `LensIconToggle` already used elsewhere in the codebase.

**Look-and-feel change enabling all of the above**: `LumenLookAndFeel`
overrides `drawButtonBackground`/`drawToggleButton` completely (replacing
JUCE's own disabled-state dimming), and neither previously consulted
`button.isEnabled()`. Both now multiply their fill/outline colours by 0.4
alpha when disabled — the one shared visual mechanism every `TextButton`,
`TabsBar` tab, and `ParamToggle` chip in the plugin now gets "for free" by
calling `setEnabled(false)`, not just this panel. `ModKnob::paint()`'s label
and `LumenLookAndFeel::drawRotarySlider`'s arc/needle already checked
`isEnabled()` (the rotary path predates this phase); `ModKnob::paint()`'s
text colour now also dims to `theme::textMuted` to match. Verified nothing
else in the plugin currently calls `setEnabled(false)` on a `Button`/
`ToggleButton` (grepped), so this is a pure addition with no other visual
surface affected.

### Fix C — seed lock no longer scrims REGENERATE

`MelodySidePanel::paintOverChildren()` previously drew a
`theme::panel.withAlpha(0.35f)` scrim across the whole REGENERATE button plus
a corner padlock whenever the master seed is locked. The scrim never actually
blocked clicks (it was an overlay paint in the parent, not a child
component — REGENERATE was always live underneath), but visually implied the
button was disabled. Replaced with a small circular padlock badge only (13px,
solid dark backing behind the glyph for legibility against the button's own
fill) — purely informational now, no dimming of the button itself.

Confirmed via the `MelodySeedLockTest` suite (`Tools/Tests/TestsMain.cpp`,
extended this phase) that locking never blocks or degrades REGENERATE:
"locked REGENERATE across a mode change: same seed, new (mode-driven)
sequence" pins the seed, switches MODE from Melody to Arp and regenerates,
switches back to Melody and regenerates again, and asserts the seed stayed
fixed across both calls while the note sequence changed both times (mode
genuinely drives different generation, even with an unchanged seed) — this
is the ARP -> MELODY case the task called out by name. Button hit-testing/
enabled state are unaffected by the lock by construction: nothing in
`setLocked()`/`lockedCache` ever touches `regenerateButton.setEnabled()`
(only Fix B's `hasImageSource()` gate does, independently).

### New test coverage (`Tools/Tests/TestsMain.cpp`, `MelodySeedLockTest`)

- `hasImageSource()` reflects load/remove/reload of the Lens image (osc 0) —
  pure controller-layer logic, testable headless in the existing
  `lumen_tests` harness (no `juce_gui_basics` dependency).
- The ARP<->MELODY locked-regenerate case described above.

**Enablement/dimming itself is not covered by a test** — `lumen_tests`
doesn't link `juce_gui_basics` or compile any `Source/UI/*.cpp` (checked:
`LUMEN_TESTS_SOURCES` in `CMakeLists.txt` has no UI sources, and no existing
test constructs a real `Component`), so `juce::Component::isEnabled()`/
paint-time dimming genuinely can't be exercised in that harness. Verified via
screenshot instead, per the task's own fallback instruction.

### CLI addition for verification

Added `--melody-lock` to the Standalone screenshot harness
(`Source/Standalone/StandaloneApp.cpp`) — engages the master seed lock after
`--melody-seed` pins it, so the REGENERATE padlock state is screenshot-able
and reproducible. Same spirit as the existing `--melody-seed`/`--melody-export`
dev-only flags; no prior way existed to reach this state without a live click.

### Verification

- Build: VST3 + Standalone + tests all built clean (Release, `/W4`
  warnings-as-errors).
- Tests: full `lumen_tests.exe` suite -> `ALL TESTS PASSED`, including the
  two new cases above.
- `--check-params`: `{"total_params":115,"attached":115,"missing":[]}`
  (unchanged — no params added/removed).
- `lumen_render --preset init --analyze`: nan_count 0, metrics unchanged
  (no DSP touched).
- `pluginval --strictness-level 10`: `SUCCESS`.
- Screenshots in `build/verify/` (gitignored, Windows-visible):
  - `no_image_dimmed.png` — panel open, no image: MODE/KEY/LENGTH/SHAPE/FEEL/
    LOOP LENGTH/SEED locks/REGENERATE/MUTATE all dimmed, hint text visible,
    PLAY/LOOP/EXPORT still bright and enabled, no yellow border.
  - `with_image_active.png` — panel open, image + melody loaded: everything
    fully active, no hint text, no border, GENERATED readout's SEED
    edit/lock enabled.
  - `locked_regen.png` / `regen_zoom.png` — master lock engaged
    (`--melody-lock`): REGENERATE shows only the small corner padlock, no
    scrim, button otherwise looks identical to unlocked.
  - `silent_check.png` — standalone launches clean with no image/melody/
    notes (idle smoke check).
- Pixel-scanned `with_image_active.png` for residual yellow border pixels
  along the panel boundary — none found; the only yellow near the edge is
  the pre-existing DRAG MIDI drag-handle outline (unrelated, confirmed by
  crop inspection).

---

## Phase 7 — MIDI export gating + a real restore-fidelity bug (`ui/panel-polish`)

Follow-up task: fix MIDI export gating consistency and diagnose a suspected
restore bug (`MelodyController::hasMelody()` possibly false after a state
reload, if restore repopulated the audio-thread player but not
`currentSeq`).

### Diagnosis: the suspected bug does not reproduce — a different, real one does

Added `MelodyRestoreExportTest` (`Tools/Tests/TestsMain.cpp`) that
generates a melody, serializes state, hands it to a **freshly-constructed**
`MelodyController` (a second object, not a re-`applyState()` on the same
instance — the existing `MelodySeedLockTest`/`MelodyPanelOpenNotPersistedTest`
coverage only exercised the latter, which can't catch a `currentSeq`-not-
restored bug because the same object's `currentSeq` is already populated
from before the "restore"), and checks `hasMelody()` plus an exact
byte-for-byte step comparison against the original.

- **`hasMelody()` is correctly `true` immediately after restore.**
  `MelodyController::applyState()` (`Source/Melody/MelodyController.cpp:533-552`)
  already does `currentSeq = restored;` when `melodystate::loadSequence()`
  succeeds — the suspected desync between "restore repopulates playback but
  not `currentSeq`" doesn't exist in this code path. `writeTempMidiFile()`/
  `saveMidiFile()`/`hasMelody()` all read `currentSeq` directly
  (`MelodyController.cpp:57,481-531`), so once `currentSeq` is right,
  everything downstream of it is right too.
- **But the restored *step data* didn't match the original, byte-for-byte** —
  caught by the test's `MelodySeedLockTest::sameNotes()` comparison, which
  failed on the very first run. Root cause: `MelodyState.cpp::storeSequence()`
  serializes each step's `velocity`/`startBeats`/`lengthBeats` via
  `juce::String`'s `operator<<`, which — for a bare `double`/`float` with no
  explicit decimal-place count — falls back to the **default C++ iostream
  precision of 6 significant digits** (`juce_String.cpp:481-505`:
  `numDecPlaces == 0` skips setting fixed-point precision entirely, so
  `o << n` uses whatever `std::ostream`'s default is). A generated beat
  position like `12.333333333333334` silently truncated to `"12.3333"` on
  every save. This was invisible to every prior persistence test because
  they all compared MIDI-exported *bytes* (480-PPQ tick-quantized, coarse
  enough to usually mask sub-tick drift) or reused the same live controller
  instance (never round-tripping through text at all) — never the raw stored
  `double`s against a fresh restore.
- **Fix**: `Source/State/MelodyState.cpp` now formats `velocity` with 9
  explicit decimal places and `startBeats`/`lengthBeats` with 15, via
  `juce::String (value, numberOfDecimalPlaces)` — forcing fixed-point
  formatting with enough digits to round-trip a `float` (~7 significant
  digits) and a `double` (~15-17 significant digits) exactly at the beat-
  position magnitudes this sequence ever reaches. CSV format/parser
  (`loadSequence`) is untouched — `getDoubleValue()`/`getFloatValue()` parse
  any decimal-place count, so old saved sessions still load fine.
- This is a genuinely different bug than the one suspected, but squarely in
  scope of "a restored melody must be exportable identically to a fresh
  one" — MIDI's tick quantization happened to mask it today, but nothing
  guaranteed that for every possible beat value, and `MelodyState.h`'s own
  documented contract ("a saved project recalls the *exact same* melody")
  was silently violated at the sub-tick level before this fix.

### MIDI export gating (`Source/UI/MelodySidePanel.{h,cpp}`)

Both export controls now disable/dim consistently on
`MelodyController::hasMelody()`, independent of the Phase 6 no-image gate
(`hasImageSource()`) — export doesn't need a live image, only a generated
sequence, so a restored melody with no image loaded keeps both enabled.

- New `exportActiveCache` member (`MelodySidePanel.h`), polled in
  `animate()` alongside the existing `generationActiveCache` edge-trigger:
  on change, calls `dragMidi.setEnabled()`/`saveButton.setEnabled()` and
  repaints.
- **`MidiDragSource`** (the DRAG MIDI handle) previously recomputed
  `hasMelody()` fresh in `paint()` only — the dim look was cosmetic, and
  `mouseDrag()` had no gate at all (it relied on `writeTempMidiFile()`
  happening to return an empty `juce::File{}` when there was nothing to
  export, which incidentally no-op'd the drag but wasn't an explicit
  contract). Now `paint()` reads `isEnabled()` (driven by the `animate()`
  poll above, single source of truth) and `mouseDrag()` explicitly checks
  `isEnabled()` first and returns before even considering starting a drag —
  no more relying on a downstream function's return value to prevent a
  drag gesture.
- **`saveButton`** (SAVE .MID) previously had no gating at all — always
  full brightness, `onClick` always opened the save dialog regardless of
  `hasMelody()` (confirmed by inspection before this fix: no `setEnabled()`
  call anywhere, no check inside the lambda). It now dims via the same
  `LumenLookAndFeel::drawButtonBackground`/text-alpha mechanism every other
  disabled button in the panel already uses (Phase 6), and — because JUCE's
  `Button` internals gate `onClick` on `isEnabled()` themselves
  (`juce_Button.cpp:297`) — `setEnabled(false)` alone is sufficient to stop
  the file chooser from opening; no redundant check needed inside the
  lambda, matching how `regenerateButton`/`mutateButton`/etc. are already
  gated in this file.

### Verification

- Build: VST3 + Standalone + tests all built clean (Release, `/W4`
  warnings-as-errors).
- Tests: full `lumen_tests.exe` suite -> `ALL TESTS PASSED` (151 sub-tests),
  including the new `MelodyRestoreExportTest` (hasMelody() after restore,
  byte-for-byte step match, `writeTempMidiFile()`/`saveMidiFile()` both
  produce non-empty output matching the original export).
- `--check-params`: `{"total_params":115,"attached":115,"missing":[]}`
  (unchanged).
- `lumen_render --preset init --analyze`: nan_count 0, metrics unchanged
  (no DSP touched).
- `pluginval --strictness-level 10`: `SUCCESS`.
- New Standalone CLI flag `--melody-remove-image` (drops the Lens image
  right after generating, simulating "melody exists, no live image" without
  needing a real saved project file) enabled three screenshots:
  - `export_none.png` — no image, no melody: DRAG MIDI **and** SAVE .MID
    both dim (previously SAVE .MID stayed full-brightness here regardless —
    visually confirms the SAVE .MID gating bug this phase fixed).
  - `export_with_image.png` — image + melody: both export controls fully
    active.
  - `export_no_image_has_melody.png` — melody generated, then image
    removed: MODE/KEY/LENGTH/SHAPE/FEEL/LOOP LENGTH/SEED locks/REGENERATE/
    MUTATE and the GENERATED readout's seed edit/lock all dim (Phase 6's
    image gate), while DRAG MIDI and SAVE .MID stay fully enabled (this
    phase's melody gate) — confirms the two gates are independent, exactly
    as required.

---

## Phase 8 — GENERATED readout visibility: found the actual desync (`ui/panel-polish`)

Task: make the GENERATED readout (KEY/MOOD/FORM/SEED + TRANSPOSE/OCTAVE)
visible if and only if `MelodyController::hasMelody()`, matching the Phase 7
export gate — reported as "can show stale content with an empty Lens and no
melody."

### Root cause: `MelodyReadout::animate()` was already `hasMelody()`-driven, but wasn't always being called

`MelodyReadout::animate()` (`Source/UI/MelodyReadout.cpp:245-255`) already
did exactly the right thing in isolation — `setVisible (m.hasMelody())`,
edge-triggered. The bug wasn't in that function; it was in **when** it gets
called. `MelodyReadout readout` is a private member of `PlayView`
(`Source/UI/Cards.h:293`), and the only call site was inside
`PlayView::animate()`, which the editor's timer only invokes when the DEEP
view is *not* showing:

```cpp
// PluginEditor.cpp, before this phase
if (deepView->isVisible())
    deepView->animate (tick % 2 == 0);
else
    playView->animate();   // <- readout.animate() lived in here
```

So `readout.animate()` — and therefore the entire visibility contract —
silently stopped running the instant the user switched to DEEP view, and
stayed stopped for as long as they remained there. Any `hasMelody()`
transition that happened while in DEEP (a preset switch clearing the
sequence, a project loading with `uiView="deep"` persisted and a melody
already in its saved state — `PluginEditor.cpp:75` restores the last-used
view on construction, so a session saved while in DEEP view opens directly
into DEEP, and `readout.animate()` would never run even once until the user
manually switched to PLAY) left `readout` frozen at whatever visibility/
content it had going into DEEP — exactly the "stale content" symptom
described. This is precisely the same class of bug Phase 3's "GENERATED
readout hides behind `addChildComponent` vs `addAndMakeVisible`" surprise
was, and the same category of fix Phase 7 applied to `MelodySidePanel`'s
export controls: a state that must hold *at all times*, driven by a poll
that wasn't actually running at all times.

The existing `melodySidePanel->animate()` call already avoided this exact
trap — it's a standalone block in `timerCallback()`, outside the DEEP/PLAY
`if`/`else`, polled unconditionally every tick regardless of which view is
showing (`PluginEditor.cpp:378-393`, predates this phase). `readout` just
hadn't been given the same treatment.

### Fix

- New `PlayView::animateReadout()` (`Cards.h`/`Cards.cpp`) — a thin wrapper
  that does only `readout.animate()`, split out of the rest of
  `PlayView::animate()` (waterfall/lensPanel/keyboard-follow, which
  genuinely are Play-view-only concerns and stay gated as before — no
  reason to spend CPU on keyboard-follow math while DEEP is showing).
- `readout.animate()`'s call site moved out of `PlayView::animate()`'s body.
- `PluginEditor::timerCallback()` now calls `playView->animateReadout()`
  unconditionally, right after the `if (deepView->isVisible()) ... else
  playView->animate();` block — mirroring the `melodySidePanel` poll's
  existing independence from the view split.

`readout` itself is still hidden while DEEP is showing (its parent,
`playView`, is `setVisible(false)` — nothing paints regardless), so this
costs nothing extra visually; the fix is purely about not letting the
*next* frame after switching back to PLAY render stale state, and about a
freshly-opened editor (DEEP-first, per persisted `uiView`) reflecting the
real `hasMelody()` state from its very first timer tick instead of only
once the user happens to switch views.

### New Standalone CLI flag for verification

`--melody-restore` (`Source/Standalone/StandaloneApp.cpp`): after
generating, calls `MelodyController::applyState()` again on the same
controller — exercising the actual `setStateInformation()`/
`loadPresetState()` restore path (`currentSeq` repopulated from the
persisted `SEQ` node) rather than just a fresh `generate()` call, so the
readout's post-restore visibility can be screenshotted directly instead of
only inferred from Phase 7's headless `MelodyRestoreExportTest`.

### Verification

- Build: VST3 + Standalone + tests all built clean (Release, `/W4`
  warnings-as-errors).
- Tests: full `lumen_tests.exe` suite -> `ALL TESTS PASSED` (no UI-visibility
  test added — `Source/UI/*.cpp` isn't linked into `lumen_tests`, same
  "screenshot, not unit test" fallback as every prior UI-only phase; the fix
  itself has no engine/controller-layer surface to test headlessly).
- `--check-params`: `{"total_params":115,"attached":115,"missing":[]}`
  (unchanged).
- `pluginval --strictness-level 10`: `SUCCESS`.
- Screenshots in `build/verify/`, all four required scenarios:
  - `readout_fresh_empty.png` — fresh instance, no image, no melody: no
    GENERATED block renders below the Lens panel at all (the empty region
    beneath LENS is bare, matching `--view play` with nothing else loaded).
  - `readout_generated.png` — image + melody generated: readout fully
    visible (KEY "F Dorian", MOOD, FORM, SEED "1234abcd", TRANSPOSE/OCTAVE).
  - `readout_no_image_has_melody.png` — melody generated, then
    `--melody-remove-image`: readout stays fully visible with the same
    content (matches Phase 6/7 — visibility never depended on the image).
  - `readout_restored.png` — melody generated, then `--melody-restore`
    (`applyState()` re-run against the persisted `SEQ`): readout visible
    with identical content, confirming the restore path (not just
    `generate()`) drives visibility correctly through the actual code path
    a project reload uses.

---

## Phase 9 — GENERATED readout visibility rule changed: image required (`ui/panel-polish`)

**Supersedes Phase 8's visibility condition.** Phase 8 made the readout
track `hasMelody()` alone (and fixed *when* that got polled — see above).
This phase changes *what* it's gated on, per a direct user decision: the
readout must be visible if and only if **`hasImageSource() && hasMelody()`**
— an image loaded in Lens *and* a melody. With no image, the whole block
(KEY/MOOD/FORM/SEED rows, TRANSPOSE/OCTAVE steppers) hides even if a melody
generated from a now-gone image is still alive and playable. Rationale: the
readout is provenance for the *current* image (KEY/MOOD/FORM/SEED describe
what was detected from it), so once that image is gone the readout would be
describing something no longer on screen — different intent from
PLAY/LOOP/export, which describe the *melody itself* and correctly don't
care whether an image is still loaded (Phases 6-7, untouched by this).

### Change

`Source/UI/MelodyReadout.cpp::animate()` — one-line condition change:

```cpp
// before (Phase 8)
const bool active = m.hasMelody();
// after (Phase 9)
const bool active = m.hasImageSource() && m.hasMelody();
```

Still driven from the same unconditional `PlayView::animateReadout()` poll
Phase 8 added (called every timer tick regardless of Play/Deep view) — only
the condition inside it changed, exactly as scoped.

### Cleanup: Phase 6's per-control image gate on SEED edit/lock is now dead code

Phase 6 had given `seedLabel`/`lockToggle` their *own* independent
`hasImageSource()`-driven enable/dim state (`imageActiveCache`,
`MelodyReadout.cpp`), because at the time the surrounding block could be
visible (`hasMelody()` true) while the image was absent — exactly the case
this phase now makes impossible. Since the whole readout can no longer be
visible without an image, that inner gate could never fire its "disabled"
branch again — removed rather than left as dead, misleading complexity:

- `imageActiveCache` member deleted (`MelodyReadout.h`).
- The `imageActive`/`seedLabel.setEnabled()`/`lockToggle.setEnabled()` block
  deleted from `animate()`.
- `LockToggle::paint()`/`mouseUp()` reverted to their pre-Phase-6 form (no
  `isEnabled()` dim multiplier, no `isEnabled()` click guard) — both are
  vestigial once the toggle can never exist in a disabled state, and keeping
  them would misleadingly imply an independent image-gating axis still
  exists inside the readout.

Nothing else in `MelodyReadout` changed — `refreshSeedText()`,
`summaryCache`, the `LockToggle`'s actual lock-toggling behavior, and the
TRANSPOSE/OCTAVE steppers are all untouched (they were never part of either
gate).

### Verification

- Build: VST3 + Standalone + tests all built clean (Release, `/W4`
  warnings-as-errors).
- Tests: full `lumen_tests.exe` suite -> `ALL TESTS PASSED` (no
  `MelodyController`/engine surface touched this phase — pure UI condition +
  dead-code removal — so no new headless test; matches the "screenshot, not
  unit test" precedent for `Source/UI/*.cpp` changes).
- `--check-params`: `{"total_params":115,"attached":115,"missing":[]}`
  (unchanged).
- `lumen_render --preset init --analyze`: nan_count 0, metrics unchanged
  (no DSP touched).
- `pluginval --strictness-level 10`: `SUCCESS`.
- Screenshots in `build/verify/`, all four required scenarios (reusing
  Phase 7/8's `--melody-remove-image`/`--melody-restore` CLI flags):
  - `readout9_fresh_empty.png` — fresh instance, no image, no melody:
    nothing renders below LENS (unchanged from Phase 8).
  - `readout9_with_image.png` — image + melody: readout fully visible
    (unchanged from Phase 8).
  - `readout9_no_image_has_melody.png` — melody generated, then
    `--melody-remove-image`: readout now **hidden** (the changed behavior —
    Phase 8 kept it visible here). Side panel confirms PLAY/LOOP/DRAG
    MIDI/SAVE .MID stay enabled regardless (Phases 6-7 untouched), while
    REGENERATE/MUTATE/RHYTHM/PITCH/HARMONY stay dimmed (Phase 6's separate
    image gate, also untouched).
  - `readout9_restored.png` — melody generated, then `--melody-restore`
    (`applyState()` re-run) with the image still present: readout visible,
    confirming the restore path satisfies the new AND condition correctly.
