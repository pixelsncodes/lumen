# Lumen / Lumena — Analysis Brief

**Purpose of this document.** A self-contained briefing for an external model asked to
analyse the current state of the project. It assumes no prior context. It covers what the
two products are, the end goal, the architecture, the phase history, the one musical problem
currently open, everything tried against it so far, what has and hasn't worked, the rules
that constrain any fix, the known-deferred backlog, and the specific open questions where an
outside read would be most useful. It is written to be honest about uncertainty: the marquee
item is *not* resolved, and the document says so plainly rather than presenting a tidy story.

---

## 1. What the two things are

**Lumen** is the parent audio plugin: APVTS parameters, the MelodyPanel UI, a
wavetable/Lens synth, and state versioning. It is the shippable product.

**Lumena** is a C++ melody engine that lives as a git submodule inside Lumen. It is where the
musical decisions happen. Almost all the work described here is in Lumena.

The product is an **image-driven music generator**: you give it an image, and it produces
playable MIDI whose *motion* (melody contour, rhythm density, phrase shape) derives from the
visual structure of that image, while a musical frame (chord progression + scale) keeps the
result consonant.

There are three musical layers:
1. **Chord progression** — a guardrailed pop harmonic frame (e.g. I–V–vi–IV and style
   variants).
2. **Arpeggio** — chord tones spelled as patterns.
3. **Melody** — the hard part, and the source of essentially every open problem.

---

## 2. The end goal (product thesis)

Move the product from *"generate a melody after glancing at an image"* to *"turn the visual
structure of an image into a playable musical phrase — without sacrificing musicality."*

The differentiator is explicitly **not** the color-to-key mapping (that idea is decades old
and already patented by others). The differentiator is: **deterministic, controllable,
image-drives-contour-within-a-musical-frame.** Lead with control and repeatability. The
positioning line the team uses:

> Lumena turns image structure into musical MIDI — melody, rhythm, chords, and arps shaped by
> color, brightness, and contrast, deterministically and under your control.

Two rules are treated as inviolable and everything is built to preserve them:

1. **Determinism.** Same image bytes + same settings + same seed = byte-identical MIDI.
2. **Musical skeleton stays.** The progression + scale quantization + strong-beat chord-tone
   snapping remain the harmonic frame. The image drives *motion within* that frame, not
   instead of it. This is what lets "Image Influence" go to 100% without turning to mush.

---

## 3. Core architecture

```
Harmony layer  (progression + scale)        -> guarantees consonance   [KEEP]
Image contour  (per-region, from the grid)  -> drives motion + rhythm  [BUILD]
Correction     (scale-quantize, chord-tone snap on strong beats) -> safety net
```

The single most important line in the whole design is the **blend model**:

```
imageTargetDegree = mapRegionContourToScaleDegree(region)
markovDegree      = theoryWeightedNext(previousDegree, chord)
finalDegree       = snapToScale( blend(markovDegree, imageTargetDegree, imageInfluence) )
```

The melody is a *walk* (not a random template): each note blends a theory-weighted next
degree toward an image-derived target, then snaps to scale, with chord-tone snapping on strong
beats. Rhythm comes from a template picked once per generation and tiled. Phrases are built as
a form (A A′ B A″) from a short motif that is then repeated / varied / resolved.

---

## 4. Phase history — what is done

The project runs in numbered phases. Each phase is its own tested checkpoint; the team does
**not** pin golden MIDI files, which is deliberately what lets phases stack and re-baseline.

- **Phase 0 — Safety net + measurement harness.** *Done.* Branching, a headless CLI harness
  (`lumena_demo`), a determinism regression test, and the "does the image matter?" experiment
  that produced the project's scoreboard: (a) same image across many seeds should stay
  recognizable; (b) different images with matched average hue/sat/luma should diverge. Success
  is these two numbers *inverting* from the pre-rework baseline.
- **Phase 1 — Correctness only.** *Closed.* Fixed harmony-drift-after-rests (drive harmony off
  absolute start beat, not a note-only clock), strong-beat detection via integer tick compare,
  one luma formula (Rec.709), valid degree metadata across Melody/Arp/Chord, de-overloaded the
  "complexity" knob, explicit grid resolution.
- **Phase 2 — Make the image actually drive the melody.** The core deliverable: the blend
  model in phrased mode, unified Image Influence semantics (0% = musical generator, 100% =
  strongly image-led). Ships only when the Phase 0 numbers invert.
- **Phase 3 — Image-driven rhythm + phrase contour.** Per-cell local contrast → rhythmic
  density; 2-bar/4-bar rhythm templates; phrase contour follows the region.
- **Phase 3.5 — Motif-based phrasing (the "generated-sounding" fix).** *Closed, accepted by
  ear.* The key win and the key lesson. Melody was competent but sounded "generated" — rhythm
  was nearly all even subdivisions, contour moved but didn't build and resolve, density
  *chopped* notes instead of *composing*. The fix: generate a phrase (form + motif + vary +
  resolve) instead of note-at-a-time, and — critically — feed the **motif contour and the
  variation choices from the image blend model, not from an RNG-picked template**. The
  inversion gate passed (correspondence climbed with Image Influence: −0.04 → +0.54 → +0.70).
  Accepted by ear as "musical, there's a rhythm to it." **Not perfect** — the over-the-bar
  coherence problem that is the current open item was already being chased here.
- **Phase 4 — Musical polish + production workflow.** Sequenced low-risk-first: **4a arps →
  4b locks → 4c cadences → (4.5 clock, held).**
  - **4a — Scale-aware chords & arps.** *Accepted by ear.* Blues ♭7 restored (in-scale min7,
    no chromatic dominant-3rds); arps resolve 1-3-5-8. (Size-3 blues *chords* still plain
    triads by agreement — ♭7 lands in the arp; optional follow-up.)
  - **4b — Locks + regeneration.** *Lock Harmony and Lock Rhythm→pitch accepted by ear.* Lock
    Rhythm / Lock Pitch / Regenerate / Mutate / Lock Harmony. All locking is a **post-hoc
    splice on emitted note lists** so generation stays byte-identical. This is where the
    single most load-bearing constraint was discovered (see §6).
  - **4c — Cadences / phrase endings.** *Accepted by ear* (harmonic-minor leading-tone cadence
    is the standout). Endings snap to a chord tone of the active harmony; phrase-final notes
    settle to ≥1.5 beats. Re-baselined: density-on emits fewer subdivided notes.
  - **4b fixes (mutate clamp + phrase-aware splice).** *Mechanically accepted.* Mutate now
    keeps onsets + rests fixed and is bar-count-invariant by construction; recombineLocked is
    phrase-aware so 4c resolutions land on phrase ends. These are correct but **do not fix the
    open musical problem** and were not meant to.

Everything in Phase 4 is **landed and tree-green**. Exactly one item is not accepted by ear.

---

## 5. THE OPEN PROBLEM — B-phrase pitch coherence

This is the marquee item and the reason for this brief.

### 5.1 Symptom (by ear, repeatedly)
Melodies read coherent for ~4 bars (the motif-A region), then **wander in bars ~5–9**.
Confirmed on base generate, on mutate-after, and on lock-pitch→new-rhythm (they all route
through the same generation path). Phase 4c's cadence landings **mask but do not fix** it — one
sample even drifted into sounding like a known folk tune once the splice cleaned up its
resolutions. It is *coherence over the phrase*, not a wrong-note or out-of-scale problem.

### 5.2 Root cause — it is PITCH/contour, not timing
A read-only diagnosis (which exists as code comments + the handoff, not as a standalone file;
it was re-verified directly against the code at the current tip) established:

- **Rhythm is coherent throughout and cannot be the cause.** One rhythm template is picked
  once in the constructor (`pickRhythmTemplate()`, single `discrete_distribution` draw) and
  tiled by `barAlignedDuration` against every note. The variation function's retime is a *dead
  write* at emission — copied notes stay `templated=true`, and emission uses
  `barAlignedDuration` and ignores the retimed length. So timing literally cannot desync.
- **The wander has two pitch/contour sources:**
  1. **B-phrases (PRIMARY).** `walkPhrase(newRegion=true)` teleports to a **random grid cell**
     (two RNG draws: column then row) and then `selectContour` picks a **fresh Rise/Fall/Arch
     contour from that new cell's brightness slope, unrelated to motif A**. As bars 5–8
     accumulate more B-phrases, the ear hears less motif and more free walk.
  2. **`varyMotif` transpose + octave-lift (SECONDARY).** A″/A‴ drift in register: transpose
     was ±1..2 degrees, and an octave lift (gated on a draw < 0.25 and range room) lifts every
     note a full octave when it fires. Register sprawls.

### 5.3 The approach being run
Because this touches the **guardrailed core generate path** — the melody's character, the
thing Phases 0–3.5 earned — it is explicitly *not* being reshaped blind. The plan:

1. A **read-only re-confirm** of the two transforms against the current tip (done — see §5.4).
2. **Two audition variants, no commit to the core path until the user picks by ear:**
   - **Variant A — motif tie-back.** B-phrases / `varyMotif` stay closer to motif A's contour
     (more repetition, more coherence). **Risk:** over-correcting into repetitive,
     "generated"-sounding output — the exact thing Phase 3.5 fought to eliminate.
   - **Variant B — register-rein (the leading hypothesis).** Keep B genuinely *fresh*, but
     rein in the register/octave-lift sprawl (clamp the octave lift, tighten the transpose
     range). Keeps freshness, loses the diffuse-register wander.
3. The user auditions both on the **Mona Lisa** fixture; determinism is proven on the
   **checkerboard** fixture. **The user's ear is the acceptance test.** Pick the variant,
   *then* commit.

### 5.4 Where it stands right now (the live state)

**Read-only re-confirm: complete.** At tip (submodule `a9852a5`, parent `ac7d576`), engine
suite 22524/22524 green. The two transforms were confirmed against the code with real (not
approximate) line numbers; the RNG draw sites each transform consumes were mapped so a fix can
remap pitch *without* changing draw count or order. Baselines captured: checkerboard
`304db53d…` (regenerated twice, byte-identical), Mona Lisa `599b674d…`.

**Variant B (register-rein): built, on branch `feature/bphrase-register-rein @ 9086ae9`, NOT
merged.** Two post-draw clamps inside `varyMotif`, with the B-teleport and `selectContour`
left completely untouched so B stays fresh:
1. Transpose band tightened ±2 → ±1 degree (clamp on the already-drawn magnitude; direction
   free).
2. No consecutive octave lifts (the lift draw is kept verbatim; a lift applies only if it
   rolled, the previous variation didn't lift, and range allows).

Engine suite still 22524/22524. **Determinism canary passed on the deliverable seed (2024):**
checkerboard byte-identical to baseline; Mona Lisa 37→37 notes, onsets + durations
byte-identical, velocities byte-identical, **exactly 5 pitches changed**, all one contiguous
run in bar 4 pulled *down* toward home register:

| idx | onset | old → new | Δ semitones |
|-----|-------|-----------|-------------|
| 15 | 13.00b | F5 (77) → D#5 (75) | −2 |
| 16 | 13.50b | G5 (79) → F5 (77) | −2 |
| 17 | 14.00b | D#4 (63) → D4 (62) | −1 |
| 18 | 14.75b | D#4 (63) → D4 (62) | −1 |
| 19 | 15.50b | D#4 (63) → D4 (62) | −1 |

So on this seed the entire audible change is those 5 notes in one bar. This is a *subtle*
intervention by design — register-rein is a small lever.

**Awaiting the ear test.** The user is auditioning baseline vs. Variant B on seed 2024. The
score render was inspected first but is the wrong instrument for this (dynamics are bucketed
into unreliable marks; the trailing "9th bar" is a known padding artifact — see §7), so the
raw MIDI diff above was used to confirm the mechanical change is clean. The **discriminating
question the ear must answer** is *where* the residual off-ness lives:
- If it's **bar 4** (the reined run stepping down / splitting across two register tiers), then
  B is on the right track and needs a small smoothing tweak to keep the run on one tier.
- If it's **bars 5–9 still wandering** (bar 4 fine, drift later), then register was the wrong
  lever alone and the wander is **contour** — the diagnosis's *primary* suspect, which B
  deliberately did not touch — which points to commissioning **Variant A**.
- If it **can't be localized on one seed**, render 2–3 more clean (pitch-only) seeds so the
  decision isn't resting on a single 5-note change in a single bar.

---

## 6. Rules that never break (constraints on any fix)

Any proposed solution has to respect all of these, or it is out of scope:

- **Same-seed determinism.** Same image + settings + seed = identical MIDI. Anything that
  isn't the intended change must be byte-identical to the prior commit.
- **The RNG stream is interleaved.** Pitch, rhythm, and harmony all draw from **one**
  `mt19937`. It must **never be split, and draws must never be reordered.** Locking works only
  because it is a post-hoc splice on emitted notes, not a re-roll of one axis — you cannot move
  one axis without perturbing the other while they share the stream. Draw sites are frozen with
  "preserved for stream identity" comments. This is the single most load-bearing constraint in
  the codebase.
- **Single accumulator.** The flatten-loop beat is the sole authoritative emitted timeline. No
  parallel beat counter is allowed (until the deliberate clock unification in Phase 4.5). Feed
  timing pre-flatten via `PhraseNote.lengthBeats` / note-count edits.
- **960 PPQ grid.** Safe subdivisions only: 2/3/5-smooth denominators up to 1/64. No
  7-tuplets, nothing finer than 1/64.
- **Transpose (a Phase 5 feature) is a playback/output shift, NOT a regeneration input.**
  Routing it into generation would break same-seed determinism.
- **Two fixtures, two jobs, never swapped.** *Checkerboard* = determinism/regression (uniform
  contrast → exact reproducible counts; its density range collapses, so it is useless for
  taste). *Graded image (Mona Lisa)* = taste/audition (varied contrast → full register, richer
  harmony). Never run determinism on an audition fixture; never audition on the checkerboard.
  Synthesised vivid-dark fixtures exist for auditioning specific scales the standard two don't
  exercise.

---

## 7. Known issues / deferred backlog (real, not blocking, scheduled deliberately)

These are all understood and intentionally *not* being fixed inside the B-phrase pass, because
each one re-baselines or destabilises something and deserves isolation:

- **`padToWholeBars` never trims — it only rounds up.** So `loopBars` is a *floor*: an 8-bar
  loop renders as **9 bars**, and the extra bar is often near-empty (this is the sagging
  trailing bar visible on the scores). Latent, real, its own pass — fixing it re-baselines
  generation, so it must not be bundled into the B-phrase work.
- **A pitch→draw coupling in `maybeOrnament`.** Ornaments fire on a coin draw, and both an
  early-return branch and a short-circuited direction draw key off the *site note's scale
  degree*. So **any pitch change** (Variant B, Variant A, or simply a different seed) can, on
  some seeds, push an ornament site across a boundary and cause an extra draw / a different
  ornament figure → a downstream rhythm+pitch cascade. Measured on Variant B: a 60-seed sweep
  found 51/60 clean pitch-only, but **3/60 (~5%) also shifted onsets/durations** through this
  coupling. **Determinism still holds** (same seed ⇒ same output) — this is a *re-baseline
  scope* question, not a bug. It is the same family as the two-clock / bug-4b "pitch feeds the
  stream" issue, and the recommendation on the table is to leave `maybeOrnament` untouched and
  let the Phase 4.5 clock work neutralise the coupling generally, rather than doing core
  surgery on an unaccepted audition branch. Consequence: *merging* any B-phrase pitch change is
  a pitch re-baseline that also moves rhythm on ~5% of seeds.
- **State-schema persistence for two in-memory carriers.** The Lock-Harmony progression and the
  phrase-aware-splice phrase starts are session-memory only; after a plugin reload the first
  regenerate under a lock draws fresh until a generation re-seeds them. Fixing needs a
  state-version bump + migration; folds into the Phase 5 UI-binding work (locks have no UI yet
  either).
- **Size-3 blues chords** are still plain triads (♭7 only in the arp). Small optional
  follow-up.
- **All UI is parked to Phase 5.** The `Lumen.exe --screenshot` inspection can't run under WSL,
  so Phase 4 stays engine-only. Lock params and a Mutate button exist; full UI binding is
  Phase 5.

**Notation caveat (why scores mislead here):** score editors bucket velocity into coarse marks,
so the dense `f`/`mp`/`ppp` scatter on a rendered score is a rendering artifact and dynamics
**cannot** be judged from the page — only by ear or a raw-number dump. Scores are reliable for
notes, rhythm, and density, not dynamics.

---

## 8. Phase 4.5 — the isolated risky foundation (held, on purpose)

After the B-phrase item is accepted by ear, the last Phase-4 step is **clock unification**:
merge the generation-time clock and the real emitted timeline (the two-clock split kept since
Phase 3). This clears bug-4b and **unblocks true tied-anticipation across bar lines** (deferred
since Phase 3.5). It is isolated and last because it is **the one change that destabilises the
determinism harness everything else rests on** — it will perturb earlier baselines and requires
re-verifying same-seed determinism across *all* prior phases after the merge. It has its own
re-baseline event and its own rollback story. It is not opened until B-phrase is accepted.

Note the throughline: bug-4b, the `maybeOrnament` coupling, and true tied-anticipation are all
the **same underlying two-clock / pitch-feeds-stream disease**, and Phase 4.5 is the reserved
slot to cure it in one deliberate place.

---

## 9. What's working vs. what isn't (honest read)

**Working:**
- The determinism discipline. Byte-identical same-seed reproduction, the interleaved-stream
  invariant, the post-hoc splice design for locks, and the checkerboard/Mona fixture split have
  all held across every phase. Phases stack cleanly because of it.
- The image-drives-motion thesis is *measurably* real — the Phase 0 inversion gate passed at
  Phase 3.5 (correspondence rises with Image Influence; same image clusters across seeds;
  different images diverge).
- Harmony, arps, cadences, and the producer workflow (lock/regenerate/mutate) are all accepted
  by ear.
- The engine suite is fully green (22524/22524), and the process (read-only trace → scoped
  variants → ear test → commit) has repeatedly caught "mechanically green but musically wrong."

**Not working / unresolved:**
- **Over-the-phrase coherence.** Bars 5–9 wander. This has been the known soft spot since
  Phase 3.5 and is the one item still failing the ear test. The register-rein variant (B) is a
  small, clean, subtle change whose sufficiency is genuinely unknown until auditioned — and the
  diagnosis itself predicts it may only be a *partial* fix, because it deliberately leaves the
  *primary* suspect (fresh B-phrase contour) untouched.
- **The tension at the heart of the fix.** The project fought hard (Phase 3.5) to stop sounding
  "generated." The most obvious coherence fix — tie B-phrases back to motif A (Variant A) —
  risks walking straight back into that failure mode. So "make it more coherent" and "keep it
  fresh" are in direct tension, and the whole two-variant structure exists precisely because
  the team doesn't yet know where the right point on that axis is. **This is the crux an
  outside analysis is most likely to add value on.**

---

## 10. Open questions where outside analysis would help most

1. **Is register-rein alone plausibly sufficient, or is the wander fundamentally contour?**
   Given the diagnosis (B-phrase teleport = primary, register drift = secondary), how much of
   the perceived "wander" is register vs. contour? Is there a principled way to answer this
   *without* relying solely on single-seed ear tests?
2. **How to make B-phrases fresh AND coherent at once.** Is there a middle design between
   "fresh random contour from a teleported cell" and "tie back to motif A" — e.g. constraining
   the teleported cell to be *near* A's cell, blending the new brightness slope toward A's,
   reusing A's contour *class* while keeping fresh pitch detail — that buys coherence without
   the repetitive "generated" feel? What's the least-repetitive change that still reads as
   development rather than free walk?
3. **The `maybeOrnament` pitch→draw coupling.** Is "leave it, accept ~5% rhythm-shifted seeds
   as part of a deliberate re-baseline, cure it generally in Phase 4.5" the right call, or is
   there a reason to neutralise it before merging any pitch change?
4. **Sequencing.** Given the tension in (2) and the risk asymmetry (Variant A's failure —
   oversmoothing — is expensive and hard to notice; Variant B's failure — the wander persists —
   is cheap and informative), is "B first, A only if B leaves the wander" the right order, or
   should both be built and compared side-by-side regardless?
5. **Whether the phrase form itself is the lever.** All of the above operates within the fixed
   A A′ B A″ form. Is the form the right abstraction, or is the coherence problem better
   attacked at the level of how many B-phrases appear and where, rather than what each B-phrase
   does?

---

## 11. Tooling / environment

- Builds headless under WSL / build-linux. **No soundfont installed** — audio cannot be
  rendered in-environment. Samples export as MIDI; the user auditions by ear on their own
  machine (DAW / soundfont). This is why "the user's ear" is a genuinely separate acceptance
  gate from "the suite is green."
- Engine suite (LumenaTests): **22524/22524 green** at the current tip. (The count dropped from
  ~24k earlier in Phase 4 because density-on melodies now emit fewer subdivided notes — no
  tests were skipped.)
- Parent suite (lumen_tests): green **except one pre-existing, unrelated wavetable SHA-256
  golden** — a Windows-pinned Lens hash that differs under Linux floating point. It is not the
  melody engine and is deliberately left alone.
- No golden-MIDI pins anywhere — this is intentional and is what lets phases re-baseline.

## 12. Key files, symbols, and reference points

- **`MelodyGenerator.cpp`** — the engine. Notable sites (verified at tip `a9852a5`):
  `pickRhythmTemplate()` ~:322 (single rhythm-template draw ~:972); `barAlignedDuration`
  ~:1251 (tiles the template, ignores retimed length ~:1248-1252); B-phrase teleport
  `walkPhrase(newRegion=true)` :1193 → branch :363-371 (column draw :369, row draw :370);
  `selectContour` :869-880 (no RNG); `varyMotif` :447-513 (transpose pick :477 / uni01 :478,
  octave lift draw :493, Flowing-mode draws :507/:510); `maybeOrnament` ~:564-568 (the
  pitch→draw coupling); flatten-loop accumulator :1213/:1293/:1308.
- **Fixtures:** `checkerboard` (determinism), `Mona Lisa` (audition), plus synthesised
  vivid-dark fixtures (e.g. a blues-green and a harmonic-minor image) for scale auditions.
- **Docs of record:** `LUMENA_ROADMAP.md` (authoritative plan), `LUMENA_PHASE4_TAIL_HANDOFF.md`
  (the Phase-4 tail handoff), and `SESSION_NOTES.md` (running log; the STEP-0 re-confirm and
  the Variant B build + diff are appended there).
- **Current branches:** work tip on submodule `a9852a5` (`feature/lumena-melody` /
  `phase1-complete-24-ga9852a5`), parent `ac7d576`. Variant B lives un-merged on
  `feature/bphrase-register-rein @ 9086ae9`.

## 13. Glossary

- **Motif A / A′ / B / A″** — the phrase form. A is the seed motif; A′/A″ are varied restatements;
  B is a contrasting phrase. The "wander" is the accumulation of B-phrases (and register-drifted
  A-variants) in the back half.
- **Blend model** — the line that blends a theory-weighted next degree toward an image-derived
  target degree by Image Influence, then snaps to scale.
- **Interleaved RNG stream** — pitch, rhythm, and harmony all draw from one `mt19937` in a fixed
  order; the central invariant.
- **Single accumulator** — the flatten-loop beat as the one authoritative emitted timeline.
- **Re-baseline** — a deliberate, isolated commit where output is *meant* to change; permitted
  because there are no golden-MIDI pins.
- **The canary** — because a clean pitch change consumes the same draws in the same order, note
  onsets + durations on the checkerboard must stay byte-identical to baseline; if rhythm shifts,
  the stream was perturbed and the change is stopped rather than papered over.
