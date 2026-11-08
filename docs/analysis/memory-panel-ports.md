# Offline memory-panel loaded ports

`src/reference/MemoryPanelPorts.h` makes the existing reference circuit available
for nonlinear current-injection and local small-signal studies. It is not linked
to the audio engine and does not change a preset, sound, or quality setting.

## Contract

`Setup::injectedAmps[row - 1]` is positive current **into** physical output row
`row`. Rows 4 and 5 are digital: their injection must remain zero. Non-finite
currents and nonzero digital-row currents produce a non-converged result.
All existing receiver conductances and the 10 kΩ bus shunt remain connected.
Zero injection preserves the previous reference calculation.

`linearise(setup)` solves the nonlinear operating point, exports its analytic KCL
Jacobian, and solves `J dV = -dI` for the 24 analog injection ports. `Ports::rows`
provides their explicit one-based physical row mapping; `loadedOhms` is row-major,
with output-voltage row and input-current column. A caller must check `valid`.
Failures do not provide a usable matrix. The implementation deliberately uses a
full pivoted solve rather than assuming independent rows or reciprocal sources.
It allocates memory and is intended for offline analysis, not a real-time thread.

For sufficiently small current changes with all other settings unchanged:

```
V_new ≈ operatingPoint.busVolts + loadedOhms * deltaCurrent
```

Use only the analog rows in this expression. For larger excursions, solve the
nonlinear network again and relinearise. `valid` means the nominal nonlinear
solve converged and the derivative solve was finite and nonsingular at its pivot
threshold. It is not a condition-number guarantee or an error bound on an
arbitrary current excursion. Inputs otherwise follow the existing reference
network's valid electrical-configuration contract.

This matrix is **loaded transimpedance**, not open-circuit source impedance.
Do not insert it directly into `ToneControls.sources`: that interface expects
open-circuit voltage plus source impedance and separately applies loading.
Doing so would double-load the circuit. Ideal followers on rows 7/9 can make the
matrix nonsymmetric; shared rails couple different rows.

## Evidence and assumptions

The existing circuit source graph and receiver loads are described in
`src/core/MemoryPanels.h` and `docs/workstreams/ws6-library.md`. Pot resistance
(default 50 kΩ, studied alternatives 10/100 kΩ), linear taper, endpoint behavior,
and follower/driver/diode laws remain declared inferences, not hardware calibration.
The mapping from stored digital controls to physical pot travel is not established.
This API does not resolve those questions or wire the memory circuit into audio.

A prior private study used nine uniform operating points (10/50/100 kΩ and
positions 0/0.5/1), 1,296 nonlinear perturbation solves, and full analytic/finite-
difference Jacobians. Its 288 finite stress cases at 1/10/100/200 µA had worst
linear prediction errors approximately 4.48 µV / 0.281 mV / 52.1 mV / 299 mV.
Those are empirical sampled results, not continuous bounds or recovered hardware
current limits. The nonlinear endpoint response rules out a universal fixed
linear replacement. A three-rail low-rank factorization was algebraically checked
in that study but is not used here and has no claimed runtime speedup.

## Reproducible regression

Build and run `cs80_memory_panel_ports_tests` (or select that CTest name). It tests
both ranks, factory selectors 0/5/10 and memory/PANEL selectors 11/12/13, three pot
resistances, and unequal per-row/per-bank settings including endpoints. It compares
all 576 matrix entries per operating point against central nonlinear differences
at ±0.1 µA, verifies that requesting the derivative preserves bus-voltage bits,
checks a biased operating point, and rejects invalid injection/digital ports.
The tolerance includes finite-difference truncation and nonlinear solver error;
it is not a physical fidelity specification. Existing memory-panel tests remain
separate coverage of zero-excitation behavior and the compiled model.
