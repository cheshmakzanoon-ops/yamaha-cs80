# Agent 11 — Winds recipe review

Scope: all 14 existing Winds programs at source `59d489d1a1f0065ae2a0f42a08c74edc43e94159`, with the final bank inventory independently resolving the fluent helper's repeated overrides. Current tine-only edits do not change these recipes. Read-only code/data audit; no listening or acoustic-match claim.

## Result

No additional definite wrong-model, octave-routing, disabled-pressure, filter-bypass or articulation wiring defect was found. The fourteen programs' final recipes agree with their explicit descriptions. The shared frozen-formant behavior is being investigated separately by agent 10.

| Existing program | Host index | Main body / register / size | Attack / release (ms) | Check |
|---|---:|---|---:|---|
| Flute | 374 | flute; 8'; size 1.00 | 26 / 110 | Flute body plus separately sustained air; pressure reaches both body color and air level. |
| Breathy Flute | 375 | flute; 8'; size 1.00 | 44 / 170 | Darker flute body, larger sustained air mix and longer release distinguish the breathy voice. |
| Pan Flute | 376 | flute; 8'; size 1.00 | 14 / 100 | Short air rank has zero sustain; main pitch remains held. Articulation matches description. |
| Recorder | 377 | flute; 8'; size 1.00 | 9 / 58 | Fast main attack/release; short zero-sustain tongue. Final aftertouch vibrato amount is zero. |
| Clarinet | 378 | clarinet; 8'; size 1.00 | 14 / 95 | Clarinet model and size1 retain the modeled odd/even register transition; weak short tongue support. |
| Bass Clarinet | 379 | clarinet; 16'; size 2.00 | 30 / 140 | Both ranks16ft; size2 lowers resonances/transition without adding an extra pitch transpose. |
| Oboe | 380 | oboe; 8'; size 1.00 | 13 / 105 | Oboe formants plus high-passed supporting rank; both ranks have actual pressure destinations. |
| English Horn | 381 | oboe; 8'; size 1.35 | 32 / 150 | Oboe-family body sizes1.35/1.30 lower the resonant bands at unchanged played pitch. |
| Bassoon | 382 | bassoon; 16'; size 1.00 | 20 / 95 | Both ranks16ft, bassoon model; support high-passed330Hz and lower mix retains main low body. |
| Soprano Reed | 383 | oboe; 8'; size 0.82 | 12 / 100 | Smaller oboe body0.82 and bright color; narrow-pulse support, small initial bend. Main oscillator literals are dormant at body mix1. |
| Ocarina | 384 | flute; 4'; size 1.00 | 24 / 90 | Both ranks4ft; mostly sine plus low-color flute body, quiet air only in rankII. No lower pitched rank. |
| Piccolo | 385 | flute; 2'; size 1.00 | 6 / 43 | Both ranks2ft as explicitly described; fast6ms/43ms main attack/release and separate air transient. |
| Whistle | 386 | flute; 4'; size 1.00 | 13 / 75 | 4ft main, body mix.28, rankII fully out of output mix,21ms glide and restrained initial bend. |
| Shakuhachi | 387 | flute; 8'; size 1.00 | 30 / 170 | Flute body;5ms noisy rank attack/310ms decay control with.10 sustain, restrained onset rise and live body brightness pressure. |

## Concrete dead-control finding

There are **16 active wind-body ranks with non-neutral `body*_decay` values**, but this control currently has no sound effect on these sustained models. `InstrumentBody::stringLoss()` returns zero for sustained bodies; onset growth and live timbre motion do not use `decayScale`, and pickupMotion is zero. These literals must not count as audible programming improvements. Do not fix this by making held winds fade away: describe the model-specific applicability, or separately design a coherent onset/transient role for the control. The affected ranks are:

- Flute, rank 1: 1.05
- Breathy Flute, rank 1: 1.20
- Pan Flute, rank 1: 0.80
- Recorder, rank 1: 0.75
- Bass Clarinet, rank 1: 1.25
- Bass Clarinet, rank 2: 1.10
- Oboe, rank 1: 0.95
- Oboe, rank 2: 0.80
- English Horn, rank 1: 1.20
- English Horn, rank 2: 1.15
- Bassoon, rank 1: 1.20
- Soprano Reed, rank 1: 0.85
- Ocarina, rank 1: 1.35
- Piccolo, rank 1: 0.65
- Whistle, rank 1: 1.10
- Shakuhachi, rank 1: 1.25

`Soprano Reed` main body mix is exactly 1. Its saw, sine/pulse and VCF-output-level literals therefore do not reach the output. This is an intentional full-body route, not a bug: HP/LP filters, envelopes, touch, rank level and modulation remain active. `Whistle` rank II is intentionally muted by mix0; editing that rank cannot affect its sound.

## Source checks

- `src/core/curated/Winds.inc`: final recipe overrides and descriptions; all fourteen names preserved.
- `src/core/CuratedLibrary.inc`: `wave`, `feet`, `level`, `eg`, `touch`, `body`, `bodySize`, `mix`, and `subAt` setter semantics.
- `src/core/Engine.cpp` near3282–3298: rank body follows panel HP/LP; pressure is multiplied by recipe afterBrill.
- `src/core/Engine.cpp` near3361: body blend replaces the oscillator path; main-rank output gain remains independent.
- `src/core/InstrumentBody.h`: model-specific harmonic bodies, clarinet equivalent register, sustained loss rule.
- `docs/preset-evidence/2026.11.4/development/final-bank/inventory.json`: final binary32 recipes, including repeated fluent overrides.

No production source, tests, commits or refs changed.
