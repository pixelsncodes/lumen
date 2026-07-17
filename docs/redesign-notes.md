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
