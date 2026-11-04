# Independent nodal filter review and 44.1 kHz / 4× coverage

2026-11-01. Production reviewed at
`8a85faab1a125cc14425d9184f7022f6746e40a6`, against original main
`db24a47170610f81236e8829ef5536af69dfc94d`. This change extends a numerical
test and corrects documentation; it changes no production DSP, circuit value,
parameter, continuous oracle, original acceptance limit or service-voltage band.
All observations below are software results or inspection of published source
documents, not hardware measurements.

## Acceptance chronology and implementation

The published original full-grid contract is
`da8e0233b703d31bab5f5670296744b1a3ee2755`, before the phase-correction audio
commit `4917708268d8eed177670f4d270dbc50fc08e831`. The previous `85ccd9a`
reference in `IG00156_PHASE_CORRECTION.md` did not identify a published
revision and is corrected. The separate pin commit
`9246dbb2584eb2d87d744b7a52b43661bc7f15ce` sets `SOURCE_AUDIO_V2` to that
full audio SHA; qualification follows in
`24c3a7018e5b18d2e5a6db9da7fbd11db4d8abf8`.

The original `tests/parity/tolerances-v2/ws1-voice.json` is byte-identical to
original main: both Git blobs are
`54e995c974dee6d869022a95042bb02a749b6e1c`, SHA-256
`d19de5bea8f7203e76674abdb271be678526688add4807105f9dcd2098d4177c`.
Its original four internal rates remain in the permanent test unchanged.

The numerical correction is Padé [2/2] on the linear nodal matrix, with the
existing nonlinear OTA currents kept in an implicit trapezoidal residual.
It also corrects the input term using a causal cubic derivative difference.
The independent reference solves the complete four-state matrix; production
eliminates three states and solves one scalar residual. The reference does
not call the production reduction or its cached slopes. This is a numerical
approximation to the declared circuit equations, not an exact matched-z or
fourth-order large-signal claim. The later coefficient and lazy-history
optimizations retain the independent accuracy tests.

## Missing-rate qualification

The original grid covered 88.2, 96, 192 and 384 kHz internally. It omitted
44.1 kHz audio at 4×. Before evaluating that point, the supplemental contract
was published as
`2f18afc3f76c62be3e3682d113134598d3d1f8b6` in
`tests/parity/tolerances-v2/ig00156-176400.json`. It requires the same five
cutoffs, five Q settings and HP/LP/cascade configurations, with equivalence
≤2e-6, core-band error ≤0.03 and full-band error ≤0.02. Existing rates retain
their existing limits. CMake passes both contract paths explicitly.

| Host rate | 2× internal rate | 4× internal rate |
| ---: | ---: | ---: |
| 44,100 Hz | 88,200 Hz | 176,400 Hz, added here |
| 48,000 Hz | 96,000 Hz | 192,000 Hz |
| 96,000 Hz | 192,000 Hz | 384,000 Hz |

The expanded permanent phase test passes **853 checks, zero failures** on
GCC 13.3.0, C++20, `-O3 -DNDEBUG`. The original four-rate results reproduce
their existing values exactly in the printed output.

| Internal rate | Worst core-band error | Worst full-band error | Core / full-band limits |
| ---: | ---: | ---: | ---: |
| 88,200 Hz | 0.00889716555 | 0.0488373171 | 0.03 / 0.08 |
| 96,000 Hz | 0.00891568599 | 0.0439232280 | 0.03 / 0.08 |
| 176,400 Hz | 0.00264237663 | 0.00296699871 | 0.03 / 0.02 |
| 192,000 Hz | 0.00173014801 | 0.00195379230 | 0.03 / 0.02 |
| 384,000 Hz | 0.0000509026489 | 0.0000582535180 | 0.03 / 0.02 |

At the new rate, production-to-independent-matrix equivalence is
**1.52966926e-7**, and direct realtime lock-in error is **0.000235758641**.
The complete positive output is [extended-phase.log](extended-phase.log).

The targeted mutation changes only the temporary production header from
`beta=h*h/12` to `beta=fs==176400.0f?0.0:h*h/12`. The continuous reference,
permanent test, thresholds and repository production files remain untouched.
It fails **3 of 853 checks**, normally returning exit **1**: the additional
matrix-equivalence check and the HP/LP 12 kHz, Q=5 direct checks. Their direct
errors become **0.108769661** and **0.0968438881**, both above 0.03. No failure
is reported at an original rate; original matrix-equivalence and response
summaries remain unchanged. See [the complete failing output](extended-176400-no-beta.log)
and [commands, exit codes and executable/header hashes](extended-mutation.json).

Before extending the grid, fresh focused runs also confirmed:

| Existing check or mutation | Fresh result |
| --- | --- |
| Original nodal grid, nonlinear/stability/anchor tests | 122 checks, zero failures |
| Original phase matrix and direct tests | 681 checks, zero failures |
| Forced coefficient-rebuild continuation | Maximum output difference 0; maximum accepted residual 9.99977600724e-11 |
| Remove phase correction at every rate | 13/681 failures; worst direct error 0.436139554; exit 1 |
| Remove only the input-derivative correction | 4/681 failures; worst direct error 0.0575747627; exit 1 |

These results validate the current numerical mechanism and show why correcting
the homogeneous poles alone is insufficient. They are not a CPU claim or a
replacement for the integrated core/plugin/compatibility CI. Original positive
outputs, original mutation commands and exact source/output identities are
retained beside this file. The two longer original mutation logs are compressed
without editing. One accidentally duplicated nodal invocation was stopped after
confirming the original was still running; only the original's completed exit-0
result is used.

## Independent native-source reread and HP setup boundary

The original GitHub Actions logs were fetched again from
[M41 job 124604536319](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536319)
and [ADJ job 124604536342](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536342).
Their contiguous base64 chunks were decoded and the resulting images visually
reinspected. SHA-256 values exactly match the existing provenance:

| Source | Inspected crop | PNG SHA-256 |
| --- | --- | --- |
| M41, printed p41, KEC-4223-65 | `m-filter-detail`, 2070×770, 420,870 bytes | `5dfe074bc3868f22fe662cbe55d718dbb9479db0d05f36a4089a8aae445cbcc7` |
| ADJ, printed p11 / PDF p13 | `cs80adjust_filter-page13-full-native`, 1391×1800, 138,147 bytes | `e784350d6a6ca086e40fc68444a6ac3433f026b0811ef4830eb6c89b5ce3af35` |

M41 visibly connects **CP7 terminal 53 through 150 kΩ to IC11 AI1**. The
neighbouring **33 kΩ** branch separately ends at ground. The 150 kΩ part is
not a second shunt. This independently confirms the dated correction in
[NATIVE_FILTER_EVIDENCE.md](../continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md).
The current numerical model already uses that series topology.

ADJ printed p11 specifies PANEL, 2′, pulse ON, centered brilliance and maximum
master resonance. For HP, RESH/LPF/RESL are HIGH and FH is +4.0 V; hold C1,
probe CP3, adjust VR7 for maximum amplitude, then VR8 for **4.3 ±1 Vpp**.
For LP, HPF/RESH are LOW, RESL HIGH and FL +3.3 V; maximize with VR9, then set
VR10 for **2.2 ±1 Vpp**. These instructions include an alignment operation;
they do not specify only a fixed isolated-chip gain. The ADJ image is a full
page rendered by the workflow at its recorded scale, not an extracted native
embedded scan.

The unchanged service fixture freshly reports **HP 1.692 Vpp**, outside
**3.3–5.3 Vpp**, and **LP 1.648 Vpp**, inside **1.2–3.2 Vpp**. Neither the
Padé correction nor the test extension changes those continuous-model results.
The HP 75 kΩ Thevenin source and half-pulse mixer drive remain declared
simplifications. The alternative is an explicit switched-source/loading graph,
including inactive-FET impedance, together with the documented peak-alignment
operation. The available pixels do not justify selecting an alternative by
closeness to 4.3 Vpp. **The HP setup discrepancy remains open; no component,
Q law, adapter gain or acceptance band was fitted.**
