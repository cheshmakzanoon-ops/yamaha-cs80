# IG00156 numerical phase correction

2026-10-31. Circuit-engine audio only. The M41 nodal network, Q mapping,
OTA law, physical source gains and cutoff-control calibration are unchanged.
This is numerical qualification against declared equations, not measurement
of a Yamaha instrument. The electrical-model qualifications in
[`IG00156_NODAL_MODEL.md`](IG00156_NODAL_MODEL.md) still apply, particularly
the newly re-examined [source/loading discrepancy](evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md).

## Why one prewarp was insufficient

The prior solver already prewarped all capacitors at the requested cutoff.
That pins one point of the bilinear frequency map but leaves curvature across
the complete audio band. Its registered full-grid complex-response gate was
not met at internal rates 88.2, 96 and 192 kHz. The existing test asserted only
the subgrids that already passed. All full-grid limits are now assertions,
without changing `ws1-voice.json` or the continuous oracle.

Simple pole matching alone also fails to specify the sampled input response:
zero-order hold changes phase, while first-order hold adds high-frequency
droop. The selected correction addresses both the homogeneous state equation
and its input term. It is a declared numerical approximation, not an exact
matched-z claim or a changed circuit law.

## Independent equation

Use the existing physical states `x = [vin, AI−BP, BP, LP]`. The unchanged
continuous network can be written

\[
\dot x = A x + B u + N(x),
\]

where `A,B` are the small-signal nodal matrix and input vector already in
`reference/IG00156Network.h`. `N` subtracts the linear integrator gm currents
and restores the declared saturated OTA currents. With `h=1/fs`,
`beta=h²/12`, define

\[
Q=I-\frac h2 A+\beta A^2,\qquad
P=I+\frac h2 A+\beta A^2.
\]

The discrete equation is

\[
Qx_n-\frac h2 N(x_n)=Px_{n-1}+\frac h2 N(x_{n-1})
 +\frac h2 B(u_n+u_{n-1})
 -\beta AB(u_n-u_{n-1})
 -\beta B(u'_n-u'_{n-1}).
\]

For the homogeneous linear system this is the Padé [2/2] approximation to
`exp(hA)`, with fourth-order phase accuracy. The nonlinear correction remains
implicit trapezoidal, so no fourth-order large-signal claim is made. The
endpoint derivative difference uses the causal cubic backward-difference
formula

\[
u'_n-u'_{n-1}\approx
\frac{11u_n-29u_{n-1}+27u_{n-2}-11u_{n-3}+2u_{n-4}}{6h}.
\]

There is no lookahead, output fade, added sample delay or output equalizer.
Changing cutoff/Q freezes the interval coefficients at their new values;
audio-rate modulation is separately subjected to the existing stability test.

## Reduced realtime solve and caches

`core/Ig00156PhaseCorrection.h` eliminates vin, AI−BP and LP through a cached
three-by-three inverse, leaving one scalar implicit equation in BP. The
inverse is the analytic inverse of the sparse linear block; it is rebuilt
only with cutoff, Q or clock changes. The nonlinear Jacobian uses the current
OTA derivatives. Prior effective slopes provide only the initial estimate,
and an already-converged estimate performs no Newton step. The residual
target is `1e-10`, tighter than the existing `1e-7` acceptance ceiling.
Previously evaluated OTA currents are reused only while their capacitor
nodes are unchanged; importing state recomputes them.

The original coefficient caches and decimated idle equation remain intact.
Audio histories exposed for state transfer are `z=x+(h/2)*dx/dt`; the corrected
audio solver itself advances the physical node voltages. The snapshot also
stores the four input samples required by reconstruction. Changing clocks
keeps the physical nodes and derivatives, then rebuilds the short input
history during the existing full-rate wake replay. Note assignment does not
clear capacitor voltages or oscillator phase.

The offline oracle in `reference/IG00156PhaseCorrection.h` solves the full
four-state matrix, without the realtime scalar elimination, slope cache or
fast-tanh approximation. The continuity oracle uses full four-state Newton
with `std::tanh` and up to 64 iterations. The strict short-gap comparison now
transfers the complete input history as well as all capacitor states.

## Gates and results

Prerequisite acceptance registration:
`da8e0233b703d31bab5f5670296744b1a3ee2755` (the published commit;
the earlier `85ccd9a` citation did not identify a published revision).
It precedes audio `4917708268d8eed177670f4d270dbc50fc08e831`; all existing
ws1 and ws3 limits are unchanged. The grid includes isolated HP, isolated LP and their
cascade, five cutoffs, five Q values, and all four internal rates.

| Internal rate | Worst normalized complex error, core | Worst normalized complex error, full band | Existing limits, core / full band |
| ---: | ---: | ---: | ---: |
| 88,200 Hz | 0.008897 | 0.048837 | 0.03 / 0.08 |
| 96,000 Hz | 0.008916 | 0.043923 | 0.03 / 0.08 |
| 192,000 Hz | 0.001730 | 0.001954 | 0.03 / 0.02 |
| 384,000 Hz | 0.00005090 | 0.00005825 | 0.03 / 0.02 |

Production-to-independent-matrix equivalence is at most `1.91e-7` of output
peak, below the unchanged `2e-6` limit. Direct realtime lock-in measurements
across the top octave pass; the broader Q=0.5/5 sweep has worst error
0.007440. The unchanged large-signal 16×-oversampled reference gives a worst
relative RMS error approximately 0.0017 versus the 0.03 limit. Hard ±8 noise,
cascade growth, audio-rate modulation, DC rejection and resonance compression
all retain their original tests and pass. Nodal test: **122 checks, zero
failures**. The additional phase-correction matrix/direct test has 681 checks.

Free-running continuity: **73,918 checks, zero failures**, worst wake residual
**−43.08 dB** against −40 dB, with strict −90 dB short-gap replay also passing.
The coefficient-cache continuation test remains exact (maximum output
difference 0); its worst accepted residual is below `1e-10`.

Mutation qualification, temporary copies only:

- Set the production `beta` to zero: **13/681 failures**, worst direct error
  **0.436140**. This removes the phase correction while retaining the same
  source equations and registered thresholds.
- Omit only the input-derivative term: **4/681 failures**, worst direct error
  **0.057575**. Merely correcting the homogeneous poles is insufficient.

The service fixture still reports **HP 1.692 Vpp, outside 3.3–5.3 Vpp**, and
**LP 1.648 Vpp, inside 1.2–3.2 Vpp**. It uses the unchanged continuous model,
so numerical integration improvements do not repair its source/topology
uncertainty. No gain was fitted. CPU comparison and compatibility captures
belong to the integration evidence; numerical accuracy alone is not a CPU or
hardware-fidelity certification.

### Supplemental 44.1 kHz / 4× coverage, 2026-11-01

Published preregistration `2f18afc3f76c62be3e3682d113134598d3d1f8b6`
adds the previously missing 176,400 Hz internal rate with core/full-band
limits 0.03/0.02; every original grid point and limit is retained. The
permanent phase test now checks all five unique internal rates needed for
44.1/48/96 kHz at 2× and 4×. It passes 853 checks. At 176.4 kHz, the
core/full-band errors are 0.00264237663/0.00296699871, independent-matrix
equivalence is 1.52966926e-7, and direct realtime error is 0.000235758641.
A production-header mutation removing phase correction only at 176.4 kHz
fails three checks while the original rates remain unchanged. No production
audio changes. [Fresh controls, mutation output, published chronology and
native-source reread](evidence/nodal-review-20261101/README.md) retain the
unresolved 1.692 Vpp HP setup prediction.


## Coefficient setup refactor

A subsequent CPU refactor preserves the corrected integrator and its `1e-10`
Newton residual target. The audio path no longer builds the separate idle
trapezoid's elimination coefficients on every cutoff/Q update. An audio/idle
clock transition still forces the required method's full setup. Fixed network
conductances and constant nodal rows are cached once at construction. The
sparse `A^2` product is expanded with each nonzero term in its original
summation order. Once the current residual passes, the solver returns without
calculating an unused Jacobian; when another step is needed it uses the same
current OTA derivatives as before.

A frozen copy of the preceding kernel was compared against this refactor at
88.2/96/192/384 kHz, for both stage networks, over 40,000 moving-cutoff/Q samples
per network/rate. The input alternates a two-tone signal and uniform hard noise
in ±8. All 960,000 HP/BP/LP output floats match bit for bit (peak difference 0)
in the local Release build. This checks arithmetic preservation; the independent
matrix, nonlinear, stability and wake gates remain the accuracy authority.
The fixed source constants are immutable because the stage's network has no
runtime setter. No signal-dependent Jacobian is reused across nonlinear states.

The qualified before/after benchmark snapshots are `cs80-cold-filter-benchmark`
and `cs80-phase-cache-benchmark`; the integration evidence supplies the required
three timing rounds. CPU benefit is not inferred from operation counts alone.


The refactored build passes all four focused CTest suites: the original full
nodal grid, independent phase matrix/direct sweep, forced-rebuild coefficient
cache continuation and free-running wake tests. The wake suite now has 73,965
checks because its obsolete SUSTAIN-II reset expectation was strengthened into
exact all-circuit retention plus explicit Legacy reset checks. Its numerical
wake limits and grid are unchanged. Results are in
`evidence/continuation-2026-10-31/filter-coefficient-cache-qualification.txt`.

## Rejected coefficient column-layout investigation

The prospective column-layout optimization did not qualify against the frozen
scalar continuation contract. Its ordinary fixed/moving-control and coordinate
suites were bit-identical, but exceptional-state and directed-rounding suites
failed while the unchanged positive control passed every suite. The candidate
was restored; it contributes no production audio change or measured CPU gain.
The source patch, hashes, complete failure totals, representative diagnostics and
actual-source comparator mutation controls are retained in
[`column-layout-rejected`](evidence/continuation-2026-10-31/column-layout-rejected/README.md).
The full-grid accuracy qualification above and the unresolved ADJ p11 HP
amplitude discrepancy are unchanged.
