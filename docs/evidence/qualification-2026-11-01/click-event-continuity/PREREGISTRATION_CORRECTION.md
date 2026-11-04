# Exact click-continuity gate: early-release non-vacuity correction

**Correction draft 2026-11-01. Not executable until independently committed.**

The original pre-registered 72-field continuity proposal was published alone as
`98c8b738b9fb6f96f7006b4f2cb032176c285c66` from source baseline
`840496fbf30ea3666fb2e861d0662c7deccdb3c2`, with contract file SHA-256
`fd529f554189d0ff29da833d39fa338930eacc5bf25f7430749ccf260f7e6e65`.
The first source-only validation, fully archived at
`/tmp/cs80-click-next/baseline-run.log` SHA-256
`67b5f67d2f59840816a828c36a5edb37798274d8a243ea1e9337e7dff6eaa959`,
returned **5,817 checks, nine failures**. Every failure was the **fixture
precondition** that both physical Funky 1 + Funky 2 rank filter capacitor
proxies must still be nonzero at the *original 1.40 s note-off*. All 72-field
zero-time comparisons, PCM exact-prefix controls, and both Bass assignment and
WSC cache-restoration controls passed. This red run is retained; it has not been
relabeled as a pass, and no failing assertion has been weakened numerically.

## Why the precondition was impossible at this event

`src/core/FactoryPresets.cpp::hardwareTonePatch` tone-index 9 gives **both
ranks** a provisional 15 ms filter attack, 300 ms filter decay, IL=.25 and
AL=.85. `src/core/Dsp.h::FilterEnv::tickReconstruction` settles Decay toward
zero with the declared `ln(1000)` release/decay law, and exactly selects the
Sustain zero value once the absolute residual is below **0.0008** model units.
The original single note is held from the quantized 0.20 s onset to the
quantized 1.40 s off, about **1.2 seconds**: vastly longer than the declared
decay. Zero filter capacitor proxies there are the model's expected normal
state. Requiring both to be nonzero at that endpoint was a scientifically
invalid **fixture non-vacuity assumption**, not evidence of a click mechanism.
These are software-model constants and source-labelled provisional controls;
no physical CS-80 envelope transition is claimed as measured.

## Exact, reviewable correction before any revised evaluation

1. **Retain the entire original 1.40 s paired Funky note-off fixture:** same
   rank-specific tone-selector route, render/event rounding, all 72 named
   continuation fields per rank, zero-bit event boundary acceptance, omitted
   control prefix, rank-II VCA coupling nonzero. The official phrase note-off
   ratio must still be <=1 and remains open; its event/window cannot change.
2. **Add an independent early-release mutation-sensitivity fixture** with
   precisely the same sources and onset at 0.20 s, but note-off at **0.22 s**
   after ~20 ms held. The source model's 15 ms attack has completed and
   300 ms decay has only just started: both filter capacitor charges and both
   amplitude EG charges should still be nonzero. Freeze the precondition
   **all four charges !=0** at all rates/qualities plus nonzero rank-II VCA
   coupling, then require the same zero-bit 72-field continuity before/after.
3. Mutate the actual noteOff path to clear each amplitude/filter charge or
   rank-II coupling, one mutation at a time. At least the new early fixture
   must fail for each mutation. Do not use the impossible 1.40 s charge
   precondition as a fake mutation failure. Preserve separate failure logs.
4. Preserve Bass physical-PANEL at 48/96, sounding-tail Bass, standalone WSC
   cache-restart controls, and the original phase/time compatibility gates.

**No relaxation of `phrase_retrigger_max=1.0`, `phrase_note_off_max=1.0`, the
zero-bit continuation thresholds, or the original audible phrase fixture.**
The amendment adds a distinguishable event and moves only the impossible
*nonzero-state mutation precondition* to that additional event. This is a
test-feasibility correction justified by the frozen software law, not a
post-hoc sound or click metric calibration. Publish the amended JSON on its
own commit **before compiling or evaluating revised test code**.

## Source provenance

- `src/tools/engine_audit.cpp`: original single-note T=.20 and 1.40 s event
  quantization to 64 host frames; note-off ratio denominator/8ms window.
- `src/core/FactoryPresets.cpp` tone selector 9, rank0 `Funky 1`, rank1
  `Funky 2`: `filterEnv(...,.25,.85,.015,.3,.15)` and
  `ampEnv(...,.002,.4,.35,.12)` for both ranks.
- `src/core/Dsp.h` `FilterEnv::tickReconstruction`, `Adsr::tickReconstruction`:
  source-inferred single-RC curve and exact endpoint threshold.
- Yamaha common M schematic **M41 p41**, Yamaha IC guide IG00152 **PDF p33 /
  printed 2-3** and IG00159 **PDF p41 / printed 2-7**: separate timing/level
  selection and buffered capacitor path; **no exact time law** obtained from
  those documents. See `docs/ENVELOPE_TARGET_TRANSITION_REVIEW.md`.
- `docs/evidence/continuation-2026-10-31/cold-idle-soa/click-causality-after.csv`:
  unchanged actual/omitted scores and exact prefix at the original event.
