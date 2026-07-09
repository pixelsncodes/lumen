# B-Phrase Coherence — External Analysis & Fix Plan (Variant C)

> Companion to `LUMEN_LUMENA_ANALYSIS_BRIEF.md` §5 and §10. This document answers the
> five open questions, replaces the A-vs-B binary with a third design (Variant C),
> and adds the measurement layer the open problem has been missing. It respects every
> constraint in brief §6. Nothing here reopens shelved material from
> `LUMENA_POP_DESIGN_REFERENCE.md` (§5 chords/arps stay untouched).

---

## 0. Verdict in one paragraph

The process is sound; the framing of the fix is not. The wander in bars 5–9 is not a
"too fresh vs. too repetitive" dial to be tuned between Variant A and Variant B — it is a
**missing relationship contract**. The current B-phrase teleports to a uniformly random
grid cell and adopts that cell's contour with no relation to motif A; the ear hears
*unrelated*, and unrelated is wander. Real pop B-sections are contrasting **under a
contract**: shared identity in one dimension (rhythm — which the engine already shares,
one template tiled everywhere), *controlled* contrast in another (contour direction),
and an **open ending** (degree 2/5, half-cadence function) so A″ reads as the answer.
Variant B (register-rein) treats the diagnosed *secondary* cause only, and its audible
footprint on the deliverable seed (5 pitches, one bar) is too small for an ear test to
adjudicate a 5-bar coherence problem. The fix is Variant C below, gated by an
objective coherence scoreboard modeled on the Phase 0 inversion gate.

---

## 1. Answers to the five open questions (brief §10)

**Q1 — Is register-rein alone plausibly sufficient?**
No — by the project's own diagnosis. §5.2 names the B-phrase teleport + fresh contour as
PRIMARY and register sprawl as SECONDARY; Variant B deliberately leaves the primary
untouched. Keep B's clamps as a *component* (they're clean and cheap), not as a
candidate fix. The principled way to answer "register vs. contour" without single-seed
ear tests is the coherence scoreboard in §3: register-centroid drift isolates the
register share of the wander; motif-recall distance isolates the contour share. Score
baseline vs. Variant B across 60 seeds and the decomposition falls out numerically.

**Q2 — Fresh AND coherent at once?**
Yes: freshness with a relationship. Three mechanisms, all stream-safe (§2):
related-region teleport (B's cell drawn from a window around A's cell — the image's own
local coherence supplies the relatedness), a deterministic contour *contrast* contract
(B = designated contrast class of A, magnitude still from the image), and an open-ended
B cadence (degree 2/5) so B functions as tension, not digression. The pitch detail of B
remains fully fresh — no note of motif A is reused — which is what keeps this out of
the Phase 3.5 "generated" failure mode.

**Q3 — `maybeOrnament` coupling?**
Leave it. The recommendation on the table is correct: determinism holds, the ~5%
rhythm-shifted seeds are re-baseline scope, and core surgery on an unaccepted audition
branch violates the isolation discipline. Phase 4.5 is the reserved slot to cure the
pitch-feeds-stream disease generally. Merging Variant C accepts the same ~5% rider
Variant B already carries.

**Q4 — Sequencing?**
Not "B first, then maybe A." Order: (1) metrics, read-only, zero risk; (2) score
baseline + Variant B across 60 seeds — expected result: B insufficient, now with
numbers; (3) build Variant C, score it; (4) spend the ear test on the top candidate
across the Mona Lisa fixture on 2–3 clean (pitch-only) seeds, not one. Variant A as
originally designed should not be built — its intent (coherence) is absorbed by C's
contract without C inheriting A's repetition risk.

**Q5 — Is the phrase form the lever?**
The A A′ B A″ form is the right abstraction; do not redesign it. But **verify from the
code first**: how does the form tile past 4 bars? If an 8-bar generation is two
independent 4-bar phrases, each with its own teleports, the back half is unrelated to
the front half *by construction* and no per-B fix fully cures bars 5–9. If so, add a
**period rule**: the second phrase reuses the first phrase's motif A
(antecedent/consequent), differing only in cadence (first phrase ends open, second ends
closed on 1). One condition, not a form redesign. This is PRE-CODE TRACE item T-1 in §4.

---

## 2. Variant C — design (all interventions stream-safe)

Same discipline as Variant B: **no draw added, removed, or reordered.** Everything is a
post-draw remap, a draw-free logic change, or a pitch-only edit on emitted/pre-flatten
notes. The checkerboard canary must stay byte-identical (modulo the known ~5%
`maybeOrnament` seeds, which are re-baseline scope, not failures).

### C-1. Related-region teleport *(the primary fix)*
Site: `walkPhrase(newRegion=true)` → branch :363-371 (column draw :369, row draw :370).
Keep both draws verbatim. Post-draw, remap the drawn cell into a window centered on
motif A's cell:

```text
W      = window half-width (start: 2 cells on the 16×12 demo grid; scale for 8×8)
col'   = clamp(A.col + (drawnCol mod (2W+1)) − W, 0, gridW−1)
row'   = clamp(A.row + (drawnRow mod (2W+1)) − W, 0, gridH−1)
```

Uniformity of the remapped distribution is irrelevant; determinism and stream identity
are what matter, and both hold. The image still sources B's material — from a region
the image itself relates to A's. `W` is the one tunable: larger = fresher, smaller =
more coherent. Expose it as a compile-time constant for the audition; do NOT make it a
user control now (roadmap: not 18 knobs).

### C-2. Contour contrast contract *(draw-free)*
Site: `selectContour` :869-880 (no RNG — confirmed in the brief). Make B's contour
*class* a deterministic function of A's class:

```text
A = Rise  -> B = Fall
A = Fall  -> B = Rise
A = Arch  -> B = InvertedArch (or Fall if no inverted-arch class exists)
```

Contour *magnitude/detail* still comes from the remapped cell's brightness slope, so
the image keeps its hand on B. Deliberate contrast reads as development; random
contrast reads as wander. (Design reference §4.10: question/answer phrasing.)

### C-3. Open-ended B cadence *(pitch-only edit)*
B's final note is snapped toward scale degree 2 or 5 (half-cadence function; pick by
nearest, tie-break to 5). Coordinate with the 4c phrase-aware splice so the A″/final
cadence remains the *closed* ending (≥1.5-beat settle, chord-tone landing). Net shape:
B opens tension, A″ answers it — the return stops sounding like an arbitrary restart.
Implementation is pre-flatten via `PhraseNote` pitch edit or the same post-hoc note-list
mechanism 4c already uses; **no draws**.

### C-4. Keep Variant B's register clamps
Transpose band ±1 (post-draw clamp) and no consecutive octave lifts, exactly as built
on `feature/bphrase-register-rein @ 9086ae9`. C is built *on top of* B's branch, not
instead of it.

### C-5 (conditional). Period rule
Only if PRE-CODE TRACE T-1 confirms independent tiling past 4 bars: second phrase
reuses first phrase's motif A; only cadence differs (open → closed). Draw-count
neutrality must be verified in the trace before writing it — if the second phrase's
motif generation consumes draws, reuse must *still consume the same draws* (generate,
then discard/overwrite pitches) to preserve stream identity, same pattern as the
octave-lift draw kept verbatim in Variant B.

**Scope guard:** C does not touch chords, arps, rhythm templates, `maybeOrnament`,
`padToWholeBars`, or the two-clock split. Those stay on their scheduled passes.

---

## 3. Coherence scoreboard (the missing measurement layer)

Same philosophy as the Phase 0 inversion gate, applied *within* a piece. Read-only
addition to `lumena_demo`; no engine changes; zero re-baseline risk. All metrics
computed from emitted note lists.

```text
M1  Motif recall      Per bar k: normalized edit distance between bar 1's
                      symbol sequence (contour symbols or interval classes)
                      and bar k's. Report front-half mean (bars 1–4) vs
                      back-half mean (bars 5–8), and dist(A″) vs dist(B).
M2  Register drift    Per-bar mean pitch (centroid). Report max back-half
                      excursion from the front-half centroid band.
M3  Interval entropy  Shannon entropy of interval classes over a sliding
                      2-bar window. Report back-half minus front-half.
```

**Protocol:** 60-seed sweep (existing tooling), Mona Lisa fixture (taste fixture —
these are taste metrics; never the checkerboard, whose collapsed range makes M1–M3
meaningless, per the Phase 3.5 finding). Score three arms: baseline, Variant B,
Variant C.

**Expected signatures:**
- Baseline: M1 back-half ≫ front-half; M3 positive. (This *validates the metrics* —
  if baseline doesn't show the wander numerically, fix the metrics before trusting
  them on candidates.)
- Variant B: M2 improves; M1/M3 barely move. (Confirms Q1 numerically.)
- Variant C: M1 back-half within ~1.5× front-half; dist(A″) < dist(B); M3 ≈ 0;
  M2 holds B's gain.

**Over-coherence guard (the Phase 3.5 fear, quantified):** if C drives M1's B-phrase
distance *below* the A′ distance — B more similar to A than A's own variation — C has
oversmoothed; widen `W` or relax C-2 before auditioning. This is the numeric tripwire
for the "generated" failure mode.

Numbers select the audition candidate. **The ear remains the acceptance gate** — on
2–3 clean pitch-only seeds, not one.

---

## 4. Execution sequence

```text
T-1  PRE-CODE TRACE (read-only): how does the phrase form tile past 4 bars?
     Independent phrases, or one form spanning the loop? Also map any draws a
     period rule would need to keep. Report before writing C-5.
S-1  Metrics: add M1–M3 to lumena_demo. Read-only. No baseline risk.
S-2  Score baseline + Variant B, 60 seeds, Mona Lisa. Validates metrics
     (baseline must show the wander) and answers Q1 with numbers.
S-3  Build Variant C on top of feature/bphrase-register-rein:
     C-1 remap -> C-2 contour contract -> C-3 open B cadence
     (-> C-5 only if T-1 demands it). Canary after each step:
     checkerboard byte-identical on the deliverable seed; suite green.
S-4  Score C, 60 seeds. Gate: expected signatures in §3, including the
     over-coherence tripwire. Tune W if needed (one knob, few iterations).
S-5  Ear test: baseline vs C on Mona Lisa, 2–3 clean pitch-only seeds.
     User's ear is the acceptance test, as always.
S-6  Merge as a deliberate pitch re-baseline (accepting the known ~5%
     maybeOrnament rhythm-shift rider, same as Variant B's scope).
S-7  Only after acceptance: open Phase 4.5 clock unification, per plan.
```

Rollback story: C lives on its own branch until S-5 passes; W and the C-2 mapping are
the only tunables; each of C-1/C-2/C-3 is a separately revertible commit.

---

## 5. What to keep, what to change (process read)

**Keep, unchanged:** determinism discipline, interleaved-stream invariant, post-draw
clamp technique, two-fixture split, per-phase re-baselines, pre-code traces,
"user's ear = acceptance," the Phase 4.5 isolation of the clock work, and the
deferral list (design reference §5 chords/arps stay closed — motion without progress).

**Change:**
1. Retire the A-vs-B binary. The axis was wrong; the contract is the design.
2. Never again spend an ear test on a sub-audible diff. If the mechanical change is
   5 notes in one bar, the ear test is pre-decided to be inconclusive; metrics first.
3. Any musical property that persists across seeds gets a metric before it gets a
   variant. That is the Phase 0 lesson, re-applied.
