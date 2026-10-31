# Programmer audio integration: registered design

2026-10-31. Acceptance limits are registered in
`tests/parity/tolerances-v2/programmer-integration.json` before evaluating the
integration. The electrical model and its alternatives were already registered
in ws6; this step connects that model to the circuit-engine rank chain.

The plugin's APVTS parameters are still the only mutable authority. A
`ControlFrame` may carry explicit electrical positions for its selected
memory/PANEL bank. Legacy ignores them. Existing frames without positions and
`Engine::setParams` preserve their physical-value API. Plugin, audit and
benchmark source-policy frames opt into the electrical coordinate explicitly.
No parameter IDs or saved-memory format are changed.

## Declared coordinate, not a fitted calibration

Each row maps by channel enum and the already declared nominal receiver law.
Rows 1/2/3 are PWM rate/depth/PW, 4/5 pulse/saw, 6 noise, 7/8 HP cutoff/Q,
9/10 LP cutoff/Q, 11/12 filter IL/AL, 13/14/15 filter A/D/R, 16/17 filter/sine
level, 18/19/20/21 amplitude A/D/S/R, 22 overall level, 23/24 initial
brilliance/level, 25/26 aftertouch brilliance/level. The source evidence and
receiver-law qualifications are in `SourceFactoryTones.h`, `MemoryPanels.h`,
`PRESET_BA_CUTOFF_BRANCHES.md`, `PRESET_AL_RECEIVER_GRAPH.md` and
`RANK_II_RECEIVER_PORTS.md` (BA27/28, MBK63, PN1, M41 and R39).

The map uses inverse nominal voltage laws for seconds and resonance; PWM
rate is logarithmic. Cutoff uses the existing key-voltage reference and
frozen affine service-alignment coordinate. Linear level/depth controls use
their declared fraction. Fractions are saturated to the physical pot ends.
The mapping is a **declared interface inference**: alternatives are actual
measured shaft/wiper coordinates or a different mechanical taper. It is not
selected to preserve a preset waveform or force a service voltage to pass.

The selected 50 kΩ linear pots, 1 Ω endpoints and ideal cutoff followers remain
the pre-existing declared electrical inference. Alternatives 10 kΩ/100 kΩ are
retained in the model documentation. Loaded bus voltages can therefore change
timing, level and resonance relative to nominal parameter labels. Those
changes must be audited and reported, not removed through per-program gains.

## Source boundary

Compile the shared rail and every selected bus at its original operating
point. A per-row Thevenin equivalent includes the pot, diode differential
resistance, assembled 10 kΩ shunt and shared-rail differential impedance, with
the receiver conductance removed exactly once. It feeds the existing cutoff,
Q, LC, EG and waveform adapters. The equivalent is a local linearization;
off-diagonal dynamic rail coupling and large-injection diode changes remain
declared limitations, as in the refined factory boundary.

Unchanged positions reuse the compiled network. Selection and all positions
are validated and compiled before `setControlFrame` publishes either rank.
The four memories use existing rank/bank ownership: rank I M1/M3 and rank II
M2/M4. Memory/PANEL hardware is available under Pure CS-80.


## Explicit parameter coordinate

Let `x` be the electrical fraction, `u` the normalized parameter value,
`t` its seconds value, `f` its Hz value, and `clip` saturation to `[0,1]`.
The assignments use channel enum names, never numeric parameter offsets.

| Source row | Channel enum | Fraction before clipping |
| ---: | --- | --- |
| 1 | `CP_pwmSpeed` | `log(f / 0.05) / log(400)` |
| 2 | `CP_pwmDepth` | `u` |
| 3 | `CP_pw` | `(u - 0.5) / 0.4` |
| 4, 5 | `CP_pulse`, `CP_saw` | Boolean switch state |
| 6 | `CP_noise` | `u` |
| 7 | `CP_hpfCutoff` | `(4.0 + log2(f / 62.5) - 4.05640725) / 10` |
| 8 | `CP_hpfRes` | `1 - u` |
| 9 | `CP_lpfCutoff` | `(3.3 + log2(f / 62.5) - 4.07460544) / 10` |
| 10 | `CP_lpfRes` | `1 - u` |
| 11, 12 | `CP_fIl`, `CP_fAl` | `u` |
| 13 | `CP_fA` | `-log10(t) / 3` |
| 14, 15 | `CP_fD`, `CP_fR` | `(1 - log10(t)) / 3` |
| 16, 17 | `CP_vcfLevel`, `CP_sine` | `u` |
| 18 | `CP_vA` | `-log10(t) / 3` |
| 19 | `CP_vD` | `(1 - log10(t)) / 3` |
| 20 | `CP_vS` | `u` |
| 21 | `CP_vR` | `(1 - log10(t)) / 3` |
| 22 | `CP_vLevel` | `u` |
| 23, 24 | `CP_initBrill`, `CP_initLevel` | `u` |
| 25, 26 | `CP_afterBrill`, `CP_afterLevel` | `u` |

The cutoff constants are the existing frozen service alignment (`alignment`
in `SourceFactoryTones.h`), not newly fitted values. The reference pitch is
`125 * kReferenceKeyVolts = 62.5 Hz`. The interface spans beyond some hardware
pot ranges; the explicit clipping is a declared endpoint limitation. It does
not change the older physical-unit `setParams` interface.

## Qualification

The preregistration commit is `d9b8e2e`. `cs80_programmer_integration_tests`
checks both ranks, all three programmer selectors, every analog row, four
nonuniform position banks, and three uniform banks for differential current
injection. It also covers both source policies at 44.1, 48 and 96 kHz,
transactional invalid-frame rejection, compilation reuse, Legacy isolation,
and unselected programmer isolation while a factory tab is active.

Initial result: **840 checks, zero failures**. Worst bus voltage error is
**0.000183986 V** against the unchanged **0.002 V** ws6 limit. Source-equivalent
reconstruction error is **4.44e-16 V** against **1e-9 V**. Worst diagonal
incremental impedance error is **0.000062273 relative** (0.00623%) against
**0.01 relative** (1%). The pre-existing `cs80_memory_panel_tests` also passes.
No tolerance or electrical parameter was changed after evaluation.

These are model-to-model numerical comparisons. They do not establish the
hardware's pot taper, pot resistance or source rail impedance. No instrument
measurements are claimed.


### Differential source derivation

For row `i`, let `a_i` be its unloaded pot divider fraction, `r_i` the local
pot/follower output resistance plus diode series and differential resistance,
and `G_i` the sum of the assembled shunt and receiver conductances. With `Rout`
the selected rail driver's resistance, the shared rail's incremental resistance is

```
Rrail = Rout / (1 + Rout * (sum(1 / (Rpot + 2 * Rend))
                        + sum_unbuffered(a_i^2 * G_i / (1 + r_i * G_i))))
```

An unbuffered row's loaded diagonal bus impedance is

```
Rloaded_i = r_i / (1 + r_i * G_i)
          + a_i^2 * Rrail / (1 + r_i * G_i)^2
```

A buffered row has only the first term because its bus current does not flow
through the pot rail. Removing the receiver conductance `Gr_i` gives
`Rsource_i = Rloaded_i / (1 - Gr_i * Rloaded_i)` and
`Esource_i = Vbus_i * (1 + Gr_i * Rsource_i)`. The assembled 10 kΩ remains
inside this source. This is an operating-point Jacobian reduction of the
registered network, not an additional gain calibration. The independent test
perturbs the original full network with positive and negative current instead
of reusing these expressions.

### Mutation qualification

Each mutation is applied only to a temporary copy, with the original limits
and the full 840-check test unchanged. Every mutant exits with failure:

| Deliberate defect | Failed checks |
| --- | ---: |
| Reverse the resonance-voltage polarity | 9 |
| Disable the programmer route before rank settings/readouts | 61 |
| Leave receiver loading inside the exported source voltage | 1 |
| Force the exported source impedance to zero | 2 |
| Recompile an unchanged bank | 24 |
| Publish parameters before rejecting invalid positions | 3 |
| Permit programmer positions to change Legacy rendering | 1 |

Results are recorded in `programmer-integration.txt` and
`programmer-mutations.txt`. The unchanged note-off capacitor-continuity test
now obtains its Bass Stack PANEL frame through the production programmer
adapter; the physical-capacitor continuity limit remains exactly zero.

Full phrase, causal-click, benchmark and compatibility results are supplied
by the integration run. A passed electrical routing gate is not a claim that
the remaining phrase click target or HP service-voltage discrepancy is closed.
