# IG00156 four-state matched integration — research prototype (2026-11-01)

**Status: research only; no production DSP change, no passed release gate.**
This note records a numerical candidate for the outstanding 2×/4×
small-signal mismatch in [IG00156_NODAL_MODEL.md](IG00156_NODAL_MODEL.md).
The current `Ig00156Stage` still uses its existing prewarped trapezoid;
the pre-registered full-grid verdict in `cs80_ig00156_nodal_tests` is
**NOT MET**. No resistor, capacitance, cutoff, Q law, input scale,
V1 audio or tolerance was changed for this experiment.

## Derivation and provenance

The reference [IG00156Network.h](../src/reference/IG00156Network.h) gives the
declared continuous four-state linear system at each fixed cutoff and Q:

\[
 \dot x=A x+B u,\quad
 x=[v_{\mathrm{input}},v_{\mathrm{FB}},v_{\mathrm{BP}},v_{\mathrm{LP}}]^\mathsf T.
\]

Its topology comes from the M41 filter network, Yamaha service-manual
pages 41–42 and the CS20M integrator comparison in the
[evidence ledger](IG00156_MODEL_EVIDENCE.md). The HP 75 kΩ and LP
150 kΩ source assumptions and the Q-current law remain **declared
inferences**, not measurements. The numerical experiment reuses the
reference network exactly; it does not select between those topology
alternatives or address the ADJ p11 setup-point miss.

For sampling interval \(T\), evaluate \(A_d=e^{AT}\), and reconstruct
the source during \(((n-1)T,nT]\) as the causal degree-three Lagrange
polynomial through the available samples \(u[n],u[n-1],u[n-2],u[n-3]\).
Write \(\xi=t/T-1\) and

\[
 L_j(\xi)=\prod_{k\ne j}\frac{\xi+k}{k-j},\quad
 B_j=\int_0^T e^{A(T-t)}B\,L_j(t/T-1)\,dt,
\]

then advance the *unchanged* continuous circuit's sampled state by

\[
 x[n]=A_d x[n-1]+\sum_{j=0}^3 B_j u[n-j].
\]

This is an exact expression for a cubic-interpolated source in the
**linear** state equation (the scratch implementation evaluates integrals
by 16-point Gauss–Legendre quadrature and matrix exponentials).
It is causal at sample \(n\), has no forced output fade and introduces
no nominal sample delay. It is not a hardware-response fit. Numerical
accuracy depends on interpolation bandwidth, quadrature conditioning,
and the nonlinear extension discussed below.

## Scratch-model findings

The ignored `build-metrics/prototype_nodal_matchedz.py` recomputes the
same HP/LP/cascade configurations, 5 Q settings and 5 cutoffs as
`tests/Ig00156NodalTests.cpp::smallSignalError` at 88.2, 96, 192 and
384 kHz. Its baseline prewarped-bilinear values reproduce the existing
C++ gate's published misses to three decimal places. Unlike the actual
registered gate, this **research sweep** uses a 750-point logarithmic
peak-search grid; its promising results cannot be cited as an official
test pass.

The table reports **core/full-band normalized complex-response error**
at the hardest cutoffs. Limits remain core **0.03**, full-band **0.08**
below 192 kHz, or **0.02** at and above 192 kHz.

| fs | cutoff | Existing prewarped trapezoid | Causal cubic + whole-state exponential |
| --- | ---: | ---: | ---: |
| 88.2 kHz | 4 kHz | 0.020 / 0.113 | 0.002 / 0.050 |
| 88.2 kHz | 12 kHz | 0.048 / 0.140 | 0.017 / 0.059 |
| 96 kHz | 4 kHz | 0.017 / 0.111 | 0.002 / 0.050 |
| 96 kHz | 12 kHz | 0.040 / 0.137 | 0.017 / 0.052 |
| 192 kHz | 4 kHz | 0.004 / 0.028 | < 0.001 / 0.004 |
| 192 kHz | 12 kHz | 0.035 / 0.035 | 0.004 / 0.004 |
| 384 kHz | 12 kHz | 0.009 / 0.009 | < 0.001 / < 0.001 |

In this scratch linear sweep, cubic interpolation satisfies the unchanged
limits for **all 5 cutoffs, 5 Q values and 3 configurations** at each
sample rate. Quadratic interpolation still misses the 88.2/96 kHz
12 kHz **core** limit (0.034). Conventional zero-order hold, first-order
hold, and the naive pole/zero matched-z variants did not meet the grid;
the raw matched-z numerators also exhibited numerical-conditioning
warnings, so those results are not reliable evidence against properly
conditioned matched-z designs.

## Production blockers and verification needed

1. **Nonlinear OTA law:** the actual stage uses
   \(g_m L\tanh(v/L)\) in both integrators, with an implicit Newton
   solve. A linear state transition alone would silently remove saturation.
   A candidate must retain the same nonlinear currents, ideally using a
   low-rank implicit exponential/collocation update, and re-validate at
   full signal levels against the existing 16× continuous nonlinear truth.
2. **CPU:** dense 4×4 multiply and up to four delayed source taps per stage
   may exceed the 8-voice CPU budget. Cache transition matrices when
   controls are unchanged, but measure three independent 48 kHz / 256-frame
   benchmark rounds before and after; avoid a permanent cost in idle ranks.
3. **State transfer and stability:** validate resets, free-running idle
   step changes, modulation sweeps, signed zeros, fast cutoff changes,
   aliasing of square/pulse sources, saturation and input levels beyond the
   linear model; the current trapezoid's four-history snapshot and
   re-reference method cannot be carried over without derivation.
4. **Truth-model-first:** introduce a separate pre-registered gate and
   mutation-check it before applying any DSP change. Then run unchanged
   legacy/V1 byte checks and, if intentional V2 audio changes, use the
   mandated separate audio-commit / V2-pin-commit / strict-V2 sequence.

This is a candidate mathematical discretization, not a resolved hardware
law or a completed handover item.
