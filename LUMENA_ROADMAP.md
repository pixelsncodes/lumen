# LUMENA Improvement Roadmap

**Goal:** Move LUMENA from *"generate a melody after looking at an image"* to *"turn the visual structure of an image into a playable musical phrase"* — **without sacrificing musicality**.

## Two rules that never break

1. **Determinism:** same image bytes + same settings + same seed = identical MIDI. Every change below must preserve this.
2. **Musical skeleton stays:** the I–V–vi–IV (and style-based) chord progression + scale quantization + strong-beat chord-tone snapping remain the harmonic frame. The image drives *motion within* that frame, not instead of it. This is what lets Image Influence go to 100% without turning to mush.

## The core architecture (what every phase serves)

```
Harmony layer  (progression + scale)      -> guarantees consonance   [KEEP]
Image contour  (per-region, from the grid) -> drives motion + rhythm  [BUILD]
Correction     (scale-quantize, chord-tone snap on strong beats) -> safety net
```

The single most important change in this whole roadmap is the **blend model**:

```
imageTargetDegree = mapRegionContourToScaleDegree(region)
markovDegree      = theoryWeightedNext(previousDegree, chord)
finalDegree       = snapToScale( blend(markovDegree, imageTargetDegree, imageInfluence) )
```

Everything in Phases 1–2 exists to make that line real and correct.

---

## Phase 0 — Safety net + measurement harness

*Do before writing any feature code. This is what makes the rest low-risk.*

- Tag + branch both repos (submodule `v1.0-stable`, parent `lumena-pre-rework`; work on `feature/*` branches).
- Keep a built binary of the current plugin outside the repo as an A/B listening reference.
- Build `lumena_demo` (the CLI harness) so every change can be verified headless.
- Write the **"does the image matter?" experiment** and record baseline numbers now:
  - Same image, 10 different seeds -> how different are the melodies? (baseline: very different)
  - 10 different images with matched average hue/sat/luma, same seed -> how different? (baseline: nearly identical)
  - These two numbers are your scoreboard. Phase 2 succeeds when they *invert*: same image should stay recognizable across seeds; different images should diverge.
- Add a determinism regression test (same image+seed -> byte-identical MIDI) that runs in CI.

**Exit:** you can rebuild, measure, and roll back in one command.

---

## Phase 1 — Fix the foundation (correctness only)  — **CLOSED**

*No new behavior. Make the existing engine correct and tested. Shippable checkpoint.*

**Close-out status:**
- Bugs 1–5: **DONE** (submodule HEAD `9242e9c`, `feature/image-contour`).
- Bug 4b: deferred → **Phase 4**.
- Bug 6: **reclassified → Phase 2** (see resolution note below).

**Bug 6 resolution note:**
Bug 6 (arpeggioAmount, UI 'Complexity', MelodyGenerator.h:104) — NOT a Phase-1 correctness defect. Nothing miscomputed; values clamped; deterministic. Both probability gates draw RNG unconditionally (coin at :438, uni01 at :625 are left-operands), so the draw stream is independent of the param's value. Real issue = ONE param overloaded across 3 behaviors + cross-mode inconsistency:
   Feed A variety/min-spread :974 → :1011 (leaks into Freeform, not just Phrased)
   Feed B ornament rate :434 → :438 (prob = amount)
   Feed C leap probability :599 → :625 (prob = clampUnit × 0.4; leap magnitude ±2..3 fixed)
 Reclassified → PHASE 2 (unify semantics across modes). PHASE-4 tail: split into distinct musician controls + honest rename of arpeggioAmount.
 RNG-HYGIENE CARRY-FORWARD for Phase 2: because both gate draws are unconditional, a future 1→3 param split can stay byte-identical IF the draw sites at :438 and :625 are preserved as-is.

- **Harmony drift after rests:** drive harmony/strong-beat decisions from the note's *absolute* start beat, not a separate clock that only advances on notes. `barIndex = floor(absBeat / beatsPerBar)`.
- **Strong-beat detection:** quantize to ticks, compare integers — no floating-point equality (`abs(localBeat - round(localBeat)) < 1e-3` is fragile once triplets/off-beats enter).
- **One luma formula** everywhere (pick Rec.709), retune the `chooseScaleType` thresholds against it. Rename the misleading `value` field that actually holds luma.
- **Degree metadata:** Melody, Arp, and Chord modes must all emit valid scale-degree data (Arp currently pushes all-zero, Chords pushes a non-degree). Use a separate struct for chord tones rather than overloading the melody field.
- **De-overload the complexity knob:** split the single field currently meaning leap-probability / ornament-amount / CLI "complexity" into distinct concepts (Motion, Ornament, Complexity).
- **Grid resolution:** make it an explicit setting so the plugin (8×8) and demo (16×12) test the same path.

**Exit:** all bugs from the code review fixed; tests green; output audibly unchanged from v1.0 except where a fix corrects a real error.

---

## Phase 2 — Make the image actually drive the melody (the core)

*This is the phase that delivers the product promise. Verify with the Phase 0 harness.*

- Compute a **per-region contour target** from the brightness grid you already calculate but barely use (region brightness -> target scale degree; brightness gradient -> ascending/descending intent).
- Implement the **blend model** in phrased mode: blend the Markov degree toward the image target by `imageInfluence`, then snap to scale and keep chord-tone snapping on strong beats. Freeform already does an absolute-brightness version; unify them.
- **Unify Image Influence semantics** across all modes to one meaning: *how closely the MIDI follows the image* (0% = musical generator, 100% = strongly image-led).
- Keep the progression running underneath the whole time — it's the correction layer, not a casualty.
- **Re-run the Phase 0 experiment.** Ship this phase only when the numbers invert (same image recognizable across seeds; different images diverge). If they don't, the blend isn't strong enough or the region features are too lossy — fix before moving on.

**Exit:** the image demonstrably, measurably shapes the note sequence, and it still sounds musical.

---

## Phase 3 — Image-driven rhythm + phrase contour

*Rhythm is currently the weakest link and the least image-driven. Fix that.*

- **Per-cell local contrast/detail** (cheap: variance within/near a cell — no ML, no full edge detection needed yet) -> rhythmic density. Smooth region = longer notes; busy region = shorter/syncopated.
- **2-bar and 4-bar rhythm templates** (plus pickup and cadence templates) so phrases stop feeling like one-bar loops.
- **Phrase contour follows the region**, not just per-cell nudges: region brightness rising -> phrase ascends; contrast spike -> leap/accent; flat region -> hold/repeat.
- Rhythm selection driven by `style + energy + complexity + image detail` together.

**Exit:** rhythm visibly derives from the image; phrases have shape and don't loop mechanically.

---

## Phase 3.5 — Motif-based phrasing (the "generated-sounding" fix)

*Finishes Phase 3's phrasing intent. This is the named next lever: rhythmic variety
and phrasing. Design catalogue: `LUMENA_POP_DESIGN_REFERENCE.md` (adopted sections
only). Output WILL change — this is a deliberate re-baseline, not a regression.*

**The problem being fixed:** melody is competent but "generated"-sounding. Rhythm is
almost all even subdivisions (busy passages = straight sixteenth runs, filler not
hooks). Contour moves but doesn't build and resolve. Density currently *chops*
existing notes instead of *composing* melodic content.

**The change:** in Phrased mode, stop generating note-at-a-time. Generate a phrase:
pick a phrase form (A A' B A''), build a short motif (3–6 notes), then
repeat / vary / resolve it across the phrase. Add anticipation and syncopation so
lines breathe.

- Pull **2–3 rhythm templates** and **2–3 melody contours** from the reference doc.
  Not the full library.
- Density should add **passing/neighbor tones** (melodic content), not even chops.

**The one risk that decides success — image must feed the *motif*:**
Current architecture feeds the image in per-note (density chops notes; blend picks
per-note degree). A template motif + RNG variation can accidentally cut the image
out entirely. **The motif contour and the variation choices must come from the image
blend model, not from a template picked by RNG alone.** If unsure how the current
code wires this, answer that from the code *before* writing anything.

**Rules that don't break (from the build handoff):**
- Same-seed determinism holds. Byte-identical vs v1.0 **will** break — intended.
  Land as an isolated, deliberate re-baseline commit.
- Feed new timing **pre-flatten** as `PhraseNote.lengthBeats` / note-count edits.
  Never add a parallel beat counter — the flatten-loop beat is the sole timeline.
- Anticipations lean on the deferred two-clock pass-2 reconciliation. If anticipated
  notes land mistimed, **flag it — don't paper over it.** That's the signal the
  Phase 4 clock work stops being deferrable.
- Stay on the 960 PPQ grid: safe subdivisions only (2/3/5-smooth up to 1/64).

**Exit:**
- Re-run the **Phase 0 inversion experiment**. Same image must stay recognizable
  across seeds; different images must diverge. If templates override the image,
  stop and fix the blend before continuing — this is the go/no-go gate.
- Rhythm shows real variety (not even chops); phrases build and resolve.
- Engine + parent suites green; regenerate the 3 sample MIDIs; audition by ear.

---

## Phase 4 — Musical polish + production workflow

*What turns it from a toy into something a producer keeps output from.*

- **Scale-aware chords & arps:** stop flattening Blues/Harmonic-Minor detections to plain triads. Arps spell chord tones (1-3-5-8 patterns), not scale runs; harmonic minor raises the 7th in V, etc.
- **Better cadences / phrase endings.**
- **Locks + regeneration:** Lock Rhythm (new pitches), Lock Pitch (new rhythm), Lock Harmony, Regenerate (new seed, same settings), Mutate (small changes). Producers almost never accept the first take — this is high-value.
- Finalize the **~6–8 musician-facing controls**: Image Influence, Rhythm Source (Groove/Image/Hybrid), Style, Energy, Complexity, Density, Motion, Repetition. Not 18.

**Exit:** usable in a real session; expressive scales survive into chords/arps.

---

## Phase 5 — Make it visible (UI)

*Only after the engine is right. Cheapest-first.*

- **Generation summary** first (cheap, high perceived value): `Detected: F Dorian · medium brightness, high saturation · Phrase A A' B A''`. Proves the image mattered.
- **Image playback overlay** using the grid-cell provenance you already store (light up the region that produced the current note). This is your genuinely distinctive UX and it's low-cost because the data exists.
- Piano-roll preview + region color-coding **last** — real UI work, lowest priority.

**Exit:** the image-to-MIDI relationship is tangible to the user.

---

## Explicitly deferred to v2+ (do not build now)

Dominant-color k-means clustering · full 8-mode scan-path zoo (ship 2–3 max) · bassline generation · multi-stem MIDI pack export · 18-control panel · large progression-style library (2–3 styles is plenty for now). These are real ideas, just not what makes the next version strong.

---

## Minimum viable next release

If you want the tightest possible scope that still meaningfully improves LUMENA:

1. Phase 1 correctness fixes (harmony timing, degree metadata, one luma).
2. Phase 2 blend model + unified Image Influence — **the** feature.
3. Phase 3: 2-bar templates + image-driven rhythm density.
4. Phase 3.5: motif-based phrasing (image feeds the motif) — the "generated-sounding" fix.
5. Phase 4: scale-aware arps + Lock Rhythm + Regenerate.
5. Phase 5: generation summary + image playback overlay.

Everything is verified against the Phase 0 harness, committed as per-phase checkpoints on the branch, and merged to stable only when its exit criteria hold.

## Positioning (honest version)

> LUMENA turns image structure into musical MIDI — melody, rhythm, chords, and arps shaped by color, brightness, and contrast, deterministically and under your control.

The differentiator is **deterministic, controllable, image-drives-contour-within-a-musical-frame** — not the color-to-key mapping (that idea is decades old and already patented by others). Lead with control and repeatability, not novelty of the color mapping.
