# Bass Stack and Funky tone-tab event zoom — 2026-11-02

**Exploratory diagnostic PASS; original click-score failures stay FAIL.**
This is an independent, read-only seven-scenario continuation study of the
original `cs80_engine_audit` phrase, not a new physical sound law or acceptance
gate. No production source, original event, score, normalization window,
threshold, reporter, or CMake test registration has changed.

## Fixed source and independent replay

Source `src/` Git tree: `7dd159d491012fd47b46c0aa651e86ea959ca289`.
The original 9-row click-causality audit established the 44.1/48/96 kHz
Funky tone-tab note-off and separate *original physical `PANEL`* Bass Stack
retrigger targets. The physical `PANEL` coordinate **must not** be silently
replaced by the electrical-programmer coordinate (which passes separately).

`BassFunkyEventZoom.cpp` is an archived standalone, test-only probe. It
includes the *unchanged* audit translation unit to reuse its exact 64-frame
event rounding, factory/hardware-tone frames, events, rendering and score
reference. It independently creates two engines, compares every pre-event
PCM sample bit-for-bit, and dispatches the target event to only one engine.
For note-on into an inactive free-running circuit it first completes the
separate reference engine's documented pending idle clock using the existing
`EngineEventContinuityProbe` friend interface; **stale idle state is never
mislabelled a MIDI reset**. It then compares the assigned rank's 72 exposed
oscillator, antialias-residual, nodal-filter, EG-charge, VCA-coupling and
phase fields with the independently caught-up reference *without advancing
audio time*. For Funky note-off it compares all 8 voices × 2 ranks, including
the still-audible selected circuit, immediately before/after the gate.

Separately, it renders complete identical baselines and one-event omissions
to `event + 30 ms`. It enforces zero pre-event PCM bit differences, computes
the **original unchanged** 8 ms post-event maximum-sample-jump scores and
reference, and measures event-only derivatives in five nonoverlapping
post-event windows: 0–1, 1–2, 2–4, 4–6, 6–8 ms. The one-event omissions are
one Bass note-on at 6.00 s, one Bass retrigger note-on at 6.25 s, or one
Funky note-off at 1.40 s. Every other earlier event is identical.

Reproduce after configuring/building `cs80_core` in the frozen source tree:

```bash
g++ -std=c++20 -O3 -DNDEBUG -Isrc -Itests \
  docs/evidence/BassFunkyEventZoom.cpp build-core/libcs80_core.a \
  -o build-core/bass-funky-event-zoom
./build-core/bass-funky-event-zoom \
  --out docs/evidence/bass-funky-event-zoom-20261102.txt
```

## Exact zero-time state findings

| Original event | Host rate | Selected source voice | Independent idle-wake field changes | MIDI audio-state field bit mismatches |
| --- | ---: | ---: | ---: | ---: |
| Bass first strike | 48 kHz | 5, inactive | 42 | 0 / 144 |
| Bass retrigger | 48 kHz | 6, inactive | 34 | 0 / 144 |
| Bass first strike | 96 kHz | 5, inactive | 41 | 0 / 144 |
| Bass retrigger | 96 kHz | 6, inactive | 28 | 0 / 144 |
| Funky single note-off | 44.1 kHz | Held voice released | Not applicable | 0 / 1,152 |
| Funky single note-off | 48 kHz | Held voice released | Not applicable | 0 / 1,152 |
| Funky single note-off | 96 kHz | Held voice released | Not applicable | 0 / 1,152 |

In total **0 / 4,032 exposed rank audio-state fields** changed bits *at the event
instant*, after legitimate independent idle catch-up where required. All
two-engine prefixes were bit-identical. The `Engine::startVoice` source branch
retains free-running oscillator phase, filter integrator histories and VCA
coupling; it restarts the envelope gate without clearing retained charge.
The existing `cs80_click_event_continuation_tests` independently assert
Funky release-stage transitions and Bass wake/reference identity and passed.
These observations exclude only the tested instantaneous state resets, not
unexposed/global control changes or hardware-model error.

## Frozen 8 ms scores and derivative timing

| Coordinate / event | Rate | Original score | Omitted event | Peak delay after MIDI | Event-only score, first 1 ms | Event-only maximum score |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Bass physical PANEL, first strike | 48 kHz | 1.123291 | 0.027242 | 7.604 ms | 0.0000488 | 1.130243 |
| Bass physical PANEL, retrigger | 48 kHz | **1.440924** | 0.031435 | 7.792 ms | 0.0000442 | 1.437373 |
| Bass physical PANEL, first strike | 96 kHz | 1.106081 | 0.069099 | 6.896 ms | 0.0175634 | 1.093113 |
| Bass physical PANEL, retrigger | 96 kHz | **1.365311** | 0.073043 | 7.813 ms | 0.0187373 | 1.358449 |
| Funky 1 + Funky 2 tabs, note-off | 44.1 kHz | **1.626889** | 1.626888 | 1.224 ms | 0.00000142 | 0.293804 |
| Funky 1 + Funky 2 tabs, note-off | 48 kHz | **1.291310** | 1.380503 | 1.583 ms | 0.0000231 | 0.290841 |
| Funky 1 + Funky 2 tabs, note-off | 96 kHz | **1.220958** | 1.238904 | 0.875 ms | 0.0179454 | 0.290468 |

All scores remain defined by the original unmodified denominator; no
event-only attribution metric becomes a substitute acceptance limit.

The **Bass note-on response is genuinely event-dependent**, but even the
first normal strike exceeds the same score target at 48 and 96 kHz. The
retrigger peak is roughly 7.8 ms after the block-aligned event; its first
millisecond contribution is tiny relative to the later 6–8 ms filtered
waveform slope. The separately archived rank-resolved investigation
`docs/evidence/continuation-2026-10-31/bass-continuity-3226011/README.md`
showed this later maximum follows the rank-I saw/filter input slope through
the 63-sample FIR delay, with VCA control, output capacitor and pedestal
terms much smaller. This new independent replay agrees; it does not claim
that the modeled oscillator/filter attack matches a measured original unit.

The **Funky note-off is not necessary for the over-limit worst step**
under this controlled experiment: in *all three rates*, the event-omission
render still exceeds 1; at 48 and 96 kHz omission is actually *worse*.
The event contribution at the baseline worst sample is only
`9.19e-7`, `0.007997`, and `0.014698` (fractions) respectively. Changes
later in the 8 ms window are compatible with a physically meaningful release
envelope changing amplitude; zero-time exposed audio state stays continuous.

No source-grounded immediate reset or pedestal defect was established.
Neither the electrical programmer's distinct Bass result nor the separate
HP-noise/NI topology investigation supplies an authorized causal fix here.
**All original >1 scores remain FAIL and the physical sound-law inference
remains OPEN.** Do not add a fade, relax a gate, or alter score thresholds.

## Evidence integrity

- Probe: `docs/evidence/BassFunkyEventZoom.cpp`, SHA-256
  `f38b8fc1f8ad4a6b7ceafbf877f2039229fe60db616b1c7286792c9b3ce022dc`.
- Complete 57-line raw per-event/per-window output:
  `docs/evidence/bass-funky-event-zoom-20261102.txt`, SHA-256
  `a8d4c9c6b0c0309e135a37419d8804003d5430075622215f7378ccdcb1fccd5e`.
- Original archived full-audit `phrase.csv` SHA-256
  `2330ba8ead63ad1f3322bd5764a9c112b5cec3ea015696b9e720f36668444914`;
  its 61-row counterfactual output SHA-256 remains
  `9a91a0827a441ce1b34a29715c2fde38246d81a98458fba6c9c59f04852259a5`.
- Four original continuity/click CTests passed together: 4/4. This
  independent exploratory probe is **not registered as a new acceptance
  CTest** and no production/qualification code was modified.
