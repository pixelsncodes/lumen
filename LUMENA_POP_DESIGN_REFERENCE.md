# Pop Rhythm, Melody, and Chord Progression Ideas for LUMENA

> **This is a reference catalogue, not a plan.** The authoritative plan is
> `LUMENA_ROADMAP.md`. This doc is a menu the roadmap points at. Do **not**
> implement it end to end — most of it is either already solved or deliberately
> shelved. Build only what the roadmap phase names.

## What's adopted now (Phase 3.5)

- **§7.1–7.4** — phrase forms, motif generation, motif variation, phrase resolution.
  This is the core.
- **§3** — 2-bar rhythm structures. Pull **2–3 templates only**, not the whole list.
- **§4** — melody contours. Pull **2–3 only**.
- **§9** — probability model. Use as a **tuning reference** for motion vs
  repetition, not as spec.
- **§8** — strong-beat chord-tone rule. Already how the engine works; keep aligned.

## What's shelved (do NOT build from this doc)

- **§5** chord progressions and all arp material — **already solved**, chords/arps
  sound fine. Touching them is motion without progress.
- **§11** "6–8 of everything" template libraries — the roadmap explicitly defers
  large libraries. Note this doc contradicts itself (§11 says 6–8, then lists more).
  Start with 2–3.
- **§12** controls table — that's Phase 4/5 UI, not now.
- **§1, §6, §10, §13, §14** — background/philosophy. Read once, don't turn into tasks.

---

## Basic Concept

LUMENA should generate pop-friendly MIDI by combining three layers: a chord progression for musical structure, a rhythm pattern for groove, and a melody contour shaped by the image. The image should influence pitch movement, density, energy, and variation, while pop music rules keep the result catchy, repetitive, and usable.

---

## 1. Core Generation Stack

Use this hierarchy:

```text
Chord progression = musical safety frame
Rhythm template = genre feel
Image contour = pitch direction and density
Markov chain = musical glue
Chord snapping = correction
Seed = variation
```

A practical generation order:

```text
1. Detect image mood from brightness, saturation, hue, and contrast.
2. Pick a pop chord progression from the detected mood.
3. Pick a rhythm template from energy and image detail.
4. Pick a melody contour from the image brightness path.
5. Generate a theory-weighted melody degree.
6. Blend that melody degree toward the image target degree.
7. Snap strong beats to chord tones.
8. Repeat or vary the motif over 2 or 4 bars.
```

The most important pop rule:

```text
Pop melody = repetition + small variation + clear resolution
```


---

## 2. Common Pop Rhythm Patterns

Use a 16-step grid per bar:

```text
1 e & a 2 e & a 3 e & a 4 e & a
```

`X` = note start. `.` = empty step or sustain.

### 2.1 Quarter-Note Anchor

```text
X...X...X...X...
```

Use for big chorus lines, simple hooks, anthemic phrases, and stable melodic anchors.

### 2.2 Eighth-Note Pulse

```text
X.X.X.X.X.X.X.X.
```

Use for verses, pre-choruses, straight pop melodies, and simple synth or vocal lines.

### 2.3 Syncopated Pop Push

```text
X..X..X.X..X..X.
```

Use for modern pop hooks, K-pop inspired phrases, dance-pop phrases, and forward motion.

### 2.4 3-3-2 Rhythm

```text
X..X..X.X..X..X.
```

Feel:

```text
3 + 3 + 2
```

Use for catchy hooks, dance-pop, reggaeton-adjacent pop, Afrobeat-influenced pop, and bouncy melodies.

### 2.5 Dotted Eighth Plus Sixteenth Feel

```text
X..X.X..X..X.X..
```

Use for driving melodic hooks, energetic synth leads, and fast vocal-like ideas.

### 2.6 Offbeat Hook Rhythm

```text
..X...X...X...X.
```

Use for dance-pop, funk-pop, response phrases, and bouncy chorus sections.

### 2.7 Pickup Into Bar

Pickup:

```text
............X.X.
```

Resolution bar:

```text
X...X.X...X.....
```

Use for vocal-like phrases, natural phrase entry, and hooks that lead into the downbeat.

### 2.8 Short-Short-Long

```text
X.X.X.......X...
```

Use for catchy motifs, repeated hooks, and simple memorable phrase cells.

### 2.9 Long-Short-Short

```text
X.......X.X.....
```

Use for answer phrases, resolutions, and phrase endings.

### 2.10 Chorus Chant Rhythm

```text
X...X.X...X.X...
```

Use for big chorus phrases, chant-like hooks, and repeated vocal motifs.


---

## 3. Two-Bar Rhythm Structures

Pop phrases usually work better when generated in 2-bar or 4-bar units instead of isolated 1-bar loops.

### 3.1 Call and Response

```text
Bar 1: X..X..X.X.......
Bar 2: X...X.X.....X...
```

Bar 1 asks a question. Bar 2 answers or resolves.

### 3.2 Hook Repetition

```text
Bar 1: X.X.X...X.X.....
Bar 2: X.X.X...X.......
```

Bar 1 creates the hook. Bar 2 repeats it with a simpler ending.

### 3.3 Build-Up Phrase

```text
Bar 1: X...X.......X...
Bar 2: X.X.X.X.X..X.X..
```

Bar 1 is sparse. Bar 2 increases activity.

Use for pre-chorus, rising energy, and brighter or more detailed image regions.


---

## 4. Common Pop Melody Patterns

Use scale degrees:

```text
1 2 3 4 5 6 7
```

In C major:

```text
1 = C
2 = D
3 = E
4 = F
5 = G
6 = A
7 = B
```

### 4.1 Stepwise Rise

```text
1 2 3 4
```

Use for building energy, verse-to-pre-chorus movement, and bright image regions.

### 4.2 Stepwise Fall

```text
5 4 3 2 1
```

Use for resolving phrases, darker or calmer regions, and phrase endings.

### 4.3 Arch Shape

```text
1 2 3 5 3 2 1
```

Use for natural vocal melody, pop hooks, and balanced phrase shapes.

### 4.4 Inverted Arch

```text
5 4 3 2 3 4 5
```

Use for restrained phrases, darker phrases, and pre-chorus tension.

### 4.5 Tonic Anchor

```text
1 1 3 2 1
```

Use for simple hooks, stable phrases, and strong tonal identity.

### 4.6 Chord-Tone Hook

Over I:

```text
1 3 5 3
```

Over vi:

```text
6 1 3 1
```

Over IV:

```text
4 6 1 6
```

Over V:

```text
5 7 2 7
```

Use for melodies that sit clearly on top of chords, hooks that sound stable, and strong chorus phrases.

### 4.7 Neighbor-Tone Motif

```text
3 2 3
5 4 5
1 2 1
```

Use for catchy small phrases, short motifs, and vocal-like movement.

### 4.8 Leap Then Resolve

```text
1 5 4 3
3 6 5 4
5 1 7 6
```

Rule:

```text
large leap -> resolve by step in the opposite direction
```

Use for emotional lifts, hook moments, and higher-energy phrases.

### 4.9 Repeated-Note Hook

```text
3 3 3 2 1
5 5 5 6 5
1 1 1 3 2
```

Use for memorable pop hooks, vocal-style phrasing, and rhythmic melodies.

Repeated notes are not a problem if the rhythm is strong.

### 4.10 Question and Answer

Question phrase:

```text
1 2 3 5
```

Answer phrase:

```text
5 4 2 1
```

Use for 2-bar phrases, 4-bar phrases, verse sections, and chorus structures.


---

## 5. Common Pop Chord Progressions

Use Roman numerals so the same algorithm works in any key.

### 5.1 Major Key Progressions

#### Classic Pop Loop

```text
I - V - vi - IV
```

Example in C:

```text
C - G - Am - F
```

Use for general pop, choruses, and stable emotional hooks.

#### Emotional Pop

```text
vi - IV - I - V
```

Example:

```text
Am - F - C - G
```

Use for emotional pop, melodic hooks, and modern ballad-pop.

#### Bright Chorus

```text
I - IV - V - I
```

Example in C:

```text
C - F - G - C
```

Use for bright pop, simple chorus sections, and positive or clear resolution.

#### Modern Loop

```text
I - vi - IV - V
```

Example in C:

```text
C - Am - F - G
```

Use for pop verses, softer chorus phrases, and familiar harmonic motion.

#### Softer Verse

```text
I - iii - vi - IV
```

Example in C:

```text
C - Em - Am - F
```

Use for softer verses, dreamy pop, and lower-energy sections.

#### Lift Into Chorus

```text
IV - V - vi - V
```

Example in C:

```text
F - G - Am - G
```

Use for pre-chorus, build-up sections, and transition into a brighter chorus.

### 5.2 Minor Key Progressions

#### Common Minor Pop

```text
i - VI - III - VII
```

Example in A minor:

```text
Am - F - C - G
```

Use for darker pop, cinematic pop, and emotional hooks.

#### Dark Pop

```text
i - VII - VI - VII
```

Example in A minor:

```text
Am - G - F - G
```

Use for dark sections, suspense, and serious or heavier moods.

#### Tense Minor

```text
i - iv - VI - V
```

Example in A minor:

```text
Am - Dm - F - E
```

Use for dark tension, harmonic minor flavor, and strong pull back to i.

#### Cinematic Minor

```text
i - VI - iv - V
```

Example in A minor:

```text
Am - F - Dm - E
```

Use for cinematic melodies, dark emotional progressions, and strong phrase endings.


---

## 6. Chord Progression Algorithm for LUMENA

A simple deterministic chord progression algorithm:

```text
1. Detect key and scale from the image.
2. Detect brightness, saturation, and contrast.
3. Classify mood as bright, dark, soft, saturated, tense, or calm.
4. Pick a progression family using weighted scoring.
5. Choose chord rhythm.
6. Apply variation every 4 or 8 bars.
7. Use chord tones as melody anchors on strong beats.
```

### 6.1 Image Mood to Progression Family

| Image Condition | Progression Type |
|---|---|
| Bright and saturated | I-V-vi-IV, I-IV-V-I |
| Bright and low saturation | I-vi-IV-V, I-iii-vi-IV |
| Dark and saturated | i-VI-III-VII, i-iv-VI-V |
| Dark and low saturation | i-VII-VI-VII, i-VI-iv-V |
| High contrast | More chord changes |
| Low contrast | Slower harmonic rhythm |
| High saturation | More colorful extensions |
| Low saturation | Simpler triads |

### 6.2 Weighted Progression Picker

Example data structure:

```cpp
struct ProgressionTemplate {
    std::vector<int> degrees;
    float brightWeight;
    float darkWeight;
    float energyWeight;
    float tensionWeight;
};
```

Example templates:

```cpp
I_V_vi_IV      = {1, 5, 6, 4};
vi_IV_I_V      = {6, 4, 1, 5};
I_vi_IV_V      = {1, 6, 4, 5};
I_IV_V_I       = {1, 4, 5, 1};
i_VI_III_VII   = {1, 6, 3, 7};
i_VII_VI_VII   = {1, 7, 6, 7};
i_iv_VI_V      = {1, 4, 6, 5};
```

Scoring:

```text
score =
  brightWeight * imageBrightness +
  darkWeight * (1 - imageBrightness) +
  energyWeight * energy +
  tensionWeight * contrast
```

Then pick deterministically using the seeded RNG.


---

## 7. Melody Generation Algorithm for Pop

A good pop melody generator should be phrase-based.

### 7.1 Choose Phrase Form

Common forms:

```text
A A' B A''
A B A B
A A B C
Call Response Call Resolution
```

Recommended 4-bar structure:

```text
Bar 1: Motif A
Bar 2: Variation A'
Bar 3: Contrast B
Bar 4: Resolution A''
```

### 7.2 Generate Motif

A motif should usually be short:

```text
3 to 6 notes
```

Build it from:

```text
rhythm pattern + pitch contour pattern
```

Example:

```text
Rhythm: X.X.X...X.......
Pitch:  1 1 3 2
```

### 7.3 Vary Motif

| Variation | Example |
|---|---|
| Repeat exactly | 1 1 3 2 |
| Change ending | 1 1 3 5 |
| Move up one scale degree | 2 2 4 3 |
| Same rhythm, new contour | 5 5 4 3 |
| Shorten ending | 1 1 3 |
| Add pickup | 5 into 1 |

Do not vary everything at once. Pop relies on recognition.

### 7.4 Resolve Phrase

Phrase endings usually land on:

```text
1, 3, or 5
```

For stronger resolution:

```text
end on 1
```

For open-ended phrasing:

```text
end on 2, 5, or 6
```


---

## 8. Strong-Beat Melody Rule

In 4/4:

```text
Strong beats: 1 and 3
Weak beats: 2 and 4
Offbeats: &, e, a
```

Rule:

```text
if note starts on beat 1 or 3:
    prefer chord tone
else:
    allow passing tone, neighbor tone, or tension tone
```

This helps generated melodies stay musical.

---

## 9. Pop Melody Probability Model

For each next note:

```text
60% stepwise motion
20% repeat same note
15% small leap, third or fourth
5% bigger leap, fifth or octave
```

After a leap:

```text
80% chance next note moves stepwise in the opposite direction
```

For phrase endings:

```text
50% resolve to tonic
30% resolve to chord third
20% resolve to chord fifth or open tone
```

For hooks:

```text
increase repeated notes
increase rhythmic repetition
decrease random leaps
```


---

## 10. Mapping Pop Rules to Image Features

| Image Feature | Musical Result |
|---|---|
| Brightness rises | Melody ascends |
| Brightness falls | Melody descends |
| High contrast | More rhythmic density and accents |
| Low contrast | Longer notes and simpler rhythms |
| High saturation | More energy, more ornament, richer harmony |
| Low saturation | Simpler melody, simpler harmony |
| Bright regions | Higher register or stronger velocity |
| Dark regions | Lower register, rests, or longer notes |
| Repeating visual pattern | Repeated rhythmic or melodic motif |
| Smooth region | Sustained notes |
| Detailed region | Faster notes or syncopation |

---

## 11. Minimal Pop System for LUMENA

For a first useful implementation, keep the system small:

```text
1. 6 to 8 rhythm templates.
2. 6 to 8 melody contour templates.
3. 8 to 10 chord progression templates.
4. 4 phrase forms.
5. Strong-beat chord-tone correction.
6. Image Influence blend.
7. Lock Rhythm and Regenerate.
```

Avoid adding too many styles or controls before the core behavior sounds good.

---

## 12. Recommended Controls

| Control | Meaning |
|---|---|
| Image Influence | How much the image controls the MIDI |
| Style | Pop, Dark Pop, Bright Pop, Cinematic, Lo-fi |
| Energy | Velocity and rhythmic activity |
| Density | Number of notes |
| Motion | Stepwise vs leap-heavy |
| Repetition | Motif reuse |
| Rhythm Source | Groove, Image, or Hybrid |
| Lock Rhythm | Keep rhythm while changing notes |
| Regenerate | Same image and settings, new seed |
| Mutate | Similar result with small changes |


---

## 13. Practical Implementation Notes

### 13.1 Do Not Generate One Note at a Time Without a Plan

Weak approach:

```text
pick random next note
pick random duration
repeat
```

Better approach:

```text
pick phrase form
pick rhythm template
pick contour
generate motif
repeat or vary motif
resolve phrase
```

### 13.2 Keep Repetition

Generated melodies often fail because they avoid repetition. Pop needs repetition.

Recommended:

```text
Bar 1 = motif
Bar 2 = motif with changed ending
Bar 3 = contrast
Bar 4 = resolution based on motif
```

### 13.3 Keep the Chord Progression Underneath

The image should drive motion inside a musical frame.

Recommended model:

```text
image controls contour and density
harmony controls safety and resolution
rhythm controls groove
seed controls variation
```

### 13.4 Treat the First Version as a Small Template Engine

Do not try to solve all pop melody generation at once.

The first version can be:

```text
template selected by image mood + deterministic variation + image contour blend
```

That is enough to make the result feel musical and image-derived.

---

## 14. Final Recommendation

For LUMENA, the pop engine should not be fully random. It should use common pop phrase structures, rhythm templates, and chord progression families as a musical frame. The image should then shape the contour, density, variation, and energy.

The strongest design principle:

```text
Every phrase should be both image-derived and musically explainable.
```

That gives LUMENA a useful identity:

```text
visual structure -> pop phrase -> editable MIDI
```
