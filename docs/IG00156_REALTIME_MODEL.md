# IG00156 circuit-engine filter (first realtime stage)

Phase 1.1 of [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md). It applies to the circuit
engine (Source Performance and Isolation policies) only, through
`Svf::processCircuit` in `src/core/Dsp.h`. The legacy policy keeps `Svf::process` byte-for-byte.

This is a **first** realtime step toward the recovered IG00156 graph
([IG00156_MODEL_EVIDENCE.md](IG00156_MODEL_EVIDENCE.md),
`src/reference/IG00156Linear.h`). It is not the complete nodal model.

## What is modelled

| Element | Class | Implementation |
| --- | --- | --- |
| Two-integrator HP/BP/LP core, serial HP→LP stages | source topology | unchanged state-variable structure, trapezoidal integrators |
| 4.7 MΩ feedforward paths into the integrator summing nodes | source-derived | `w = (gm + 1/Rff)/C` adds 1/(2π·4.7 MΩ·1.5 nF) = **22.575 Hz** to each stage's cutoff (`kIg00156FeedforwardHz`). The documented `125·Kv·2^V` law is treated as the gm part. This is an ideal-model contribution, not a measured minimum, and it replaces the arbitrary 18 Hz clamp as the low limit. |
| Transconductor saturation inside the loop | **inferred** | each integrator is driven through `L·tanh(u/L)`, with L = `kIg00156OtaLevel` = 2.5 engine units |
| Input clip | removed | the legacy `softClip(1.1x)` on the filter input is not used |
| Cutoff law, Q law | unchanged | `nominalFilterCutoff`, and `provisionalFilterQ` or the source-tone Q maps |

Solver: for each sample, the implicit trapezoidal equations are reduced to a single
unknown, the HP node. It is solved by Newton iteration, starting from the linear
solution that uses each OTA's effective gain from the previous sample. The tolerance
is 1e-7, with at most five evaluations. `fastTanh` is a clamped 7/6 Lambert
continued fraction. Its error is below 1e-6 for |x| < 3 and below 1e-4 everywhere.

## Why the level is 2.5

A full oscillator mix reaches about ±1.8 at the HP node. At L = 2.5 that mix is only
mildly nonlinear (tanh 0.72 ≈ 0.62), while resonant peaks compress strongly. At L = 1
the integrators slew-limit on ordinary signals. A slew-limited high-pass then passes
much more low end, which made some programs 6–9 dB louder. The level is a declared
engineering choice. The sources give no OTA bias current or input attenuation for
this IC.

## Results

- Alias suite, 48 kHz, source policy: the worst saw non-harmonic energy improves from
  −33.8 to −51.1 dB, and the worst pulse from −39.3 to −53.5 dB. That is close to the
  PolyBLEP oscillator's own alias floor, so the −70 dB target needs the Phase 1.2
  oscillator.
- Level: some low-cutoff string and brass tabs are up to 5–6 dB louder, because the
  feedforward floor raises very low LP cutoffs. The worst tab peak is −0.8 dBFS. The
  extreme dense velocity-127 PANEL chord in the UI self-test peaks at 1.017 (+0.15 dB).
  The driver holds at full scale, and the coupling network adds the remainder.
- CPU, on a scratch benchmark with 8 held voices at 48 kHz: the circuit engine rises from
  0.23 to 0.37 RTF. The planned engine restructure is expected to recover part of this.

## Tests

`tests/Ig00156FilterTests.cpp` checks five things:
- Small signals match a linear TPT reference within 0.2 % over a rate × cutoff × Q grid.
- Resonant gain equals Q at a low level and compresses at a high level. This is the
  test that distinguishes in-loop saturation from an input clip; it was
  mutation-checked by making the OTA effectively linear.
- The filter stays stable and bounded on hard noise at 88.2–384 kHz, from 18 Hz to
  0.45·fs, with Q from 0.35 to 9.
- The feedforward constant equals its derivation.
- `fastTanh` stays within its error bounds and is monotonic and odd.

## Still open

- The 1.2 MΩ/180 pF feedback branch (the third state in `DynamicModel`).
- The Q block's VQ→conductance law.
- The input and ground networks that differ between HP and LP.
- The 1 µF interstage coupling.
- A gm-versus-control-current law.
- Finite amplifier effects.

These need the explicit hypotheses listed in the model evidence before they can be
integrated.
