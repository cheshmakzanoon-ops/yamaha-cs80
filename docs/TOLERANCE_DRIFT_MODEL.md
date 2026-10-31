# Component tolerances and thermal drift (circuit engine)

Plan step 2.5, implemented by the integrator on 2026-10-31 from ws1-voice's
design notes: `src/core/ToleranceModel.h`, wired in `src/core/Engine.cpp`.
Gate: `cs80_tolerance_drift_tests` (`tests/ToleranceDriftTests.cpp`), limits
pre-registered in `tests/parity/tolerances-v2/ws1-voice.json`
(`tolerance_drift`, commit `22b6854`; two fixture corrections recorded before
evaluation in `df79243` and `71a449d`). Nothing here is a measurement of a
CS-80: the distributions are declared, the mapping laws follow the circuit
topology.

## Scope

| Policy | Tune / cutoff / Q / level / envelope trims | Drift |
| --- | --- | --- |
| Legacy | the historical uniform trims (tune and pan only), unchanged | the historical 0.031 Hz sine, unchanged |
| Circuit engine (Source Performance) | component draws below, scaled by VOICE DISPERSION | thermal 1/f (default) or the sine ("worn unit") |
| PURE CS-80 | none (VOICE DISPERSION masked to 0) | none (ANALOG DRIFT masked to 0) |

## Component draws

Per voice channel (8 cards x 2 ranks), fixed seeds independent of host rate,
oversampling and block size:

| Component | Tolerance (declared) | Maps to |
| --- | --- | --- |
| HP and LP integrator capacitors C1, C2 (1.5 nF) | 10 % | cutoff ~ 1/sqrt(C1 C2), Q ~ sqrt(C1/C2) |
| LC summing resistors Rf, Rin | 5 % | level ~ Rf/Rin |
| Filter EG timing R and 0.22 uF | 5 % / 10 % | filter EG times ~ R C |
| Amplitude EG timing R and 0.22 uF | 5 % / 10 % | amplitude EG times ~ R C |
| VCO alignment residual | sigma 1 cent, cut at 2 cents | tune |

Distributions: truncated normal, sigma = tolerance / 2, truncated at the
tolerance (a 5 % resistor never reads 5.1 % out). The CS-80 parts lists give
values, not tolerance classes; these are the common classes of the era's parts.
The Q orientation (damping on the BP integrator) is declared; the inverse ratio
is the alternative. The VCOs are aligned on the bench, so their component
spread is trimmed out and only a residual remains.

VOICE DISPERSION scales every ratio as `ratio^(dispersion / 0.35)`: its
default 0.35 is the declared distribution, 0 gives identical cards, 1 is about
2.9 times wider (an unaligned or aged unit). Pan dispersion keeps its existing
law.

## Thermal drift

Each channel's exponential converter wanders as 1/f noise: six first-order
sections with time constants 0.5, 2, 8, 32, 128 and 512 s and equal variance,
2 cents RMS in total at the ANALOG DRIFT default 0.25 (scaled linearly), plus a
warm-up offset (sigma 3 cents at the default) that decays with 120 s from
`prepare()`. The process starts stationary, as for a unit that has been on; the
warm-up models the remaining settling after power-on. It is advanced at
100 Hz from a sample counter, so the render does not depend on the host's block
partition, and held between steps (phase-continuous steps of a few tenths of a
cent at most; per-sample interpolation made the oscillator recompute its branch
nodes on every sample). DRIFT MODEL = 1 ("worn unit") restores the
historical +/- 22 cent 0.031 Hz sine, rendered exactly as Legacy renders it.
Filter drift is not modelled (open).

## Gate results (2026-10-31)

| Check | Limit | Result |
| --- | --- | --- |
| Draw sigma (4096 cards), vs 0.8796 x declared sigma | 10 % | within 2 % |
| Draw mean | 3 standard errors | pass |
| Truncation | at the tolerance | max 0.1000 (C), 0.0500 (R) |
| Mapping laws | 1e-6 | 5.9e-8 |
| Engine trims at 44.1/96 kHz (RankInfo cutoffs, Q, oscillator) | 1e-5 | 2.0e-7 |
| PURE CS-80 and dispersion 0 | identity | exact |
| Drift PSD slope, 0.003-0.3 Hz | -1.3 .. -0.7 | -1.10 |
| Drift RMS at the default | 1.6 .. 2.4 cents | 1.96 cents |
| Warm-up at 120 s | 1/e within 5 % | 0.36788 |
| Worn unit vs Legacy sine | 1e-4 cents | 0 |

Mutations caught: dropping one stage's cutoff ratio in the engine (2 failing
checks); equal drift time constants (slope -2.02).

## Intentional circuit-engine sound change

Default circuit-engine renders now have per-card cutoff (about 3 % sigma), Q,
level and envelope-time spread and a slow 2-cent drift instead of the
22-cent sine. Historical renders: commit `71a449d` and earlier.
