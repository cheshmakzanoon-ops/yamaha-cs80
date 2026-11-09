# Memory electrical audio integration: bounded first increment

## Required separation

The shipping programmer stores channel values in Hz, seconds and normalized
control units. The electrical model consumes one-based physical output rows
and wiper fractions. `ProgrammerControlIdentity.h` establishes row identity,
not a value conversion. A per-role conversion may be an explicitly justified
engineering inference; hardware measurement is not required by this project's
fidelity policy. It must not be invented implicitly by copying storage order
or treating GUI skew as a recovered electrical taper.

The parts-list evidence supports 10 kOhm B-coded memory sliders, not a universal
PANEL resistance, measured taper, endpoint orientation or diode/driver law.
The historical 50 kOhm configuration remains a comparison fixture.
See the source scan identification and page references in
[memory-pot-source.md](memory-pot-source.md), the loaded-port boundary in
[memory-panel-ports.md](memory-panel-ports.md), and the inference policy in
[ANALOG_FIDELITY_REQUIREMENT.md](../ANALOG_FIDELITY_REQUIREMENT.md).

The full reference network also exports **loaded** port derivatives. They
cannot be installed in `ToneControls.sources`, which expects unloaded source
equivalents and applies receiver loads itself. Independent per-row Thevenin
ports additionally lose shared-rail coupling and follower asymmetry.

## First increment: fixed-size loaded port map

Prepare a memory/PANEL operating point outside the audio callback using the
existing full nonlinear reference, including inactive branches. Eliminate the
internal wiper nodes from its analytic Jacobian. Retain three rail coordinates
and the 24 analog bus coordinates. The bus-only diagonal and three rail
coordinates give a diagonal-plus-rank-three map:

```
deltaRail = currentToRail * deltaCurrent
bus = operatingPoint + localOhms * deltaCurrent + railToBus * deltaRail
```

This preserves off-diagonal coupling and the generally nonsymmetric response
of the ideal followers. Only fixed arrays and bounded scalar loops are needed
to evaluate the prepared map. The offline compiler is not real-time safe; a
future controller must publish complete immutable prepared maps rather than
calling that compiler in `processBlock`. Factory selectors are excluded from
this increment because their selected emitter is a different live coordinate.

Acceptance limits were recorded in
`tests/parity/tolerances-v2/memory-port-map.json` before the factorization was
evaluated. They compare two calculations of the **same local derivative**.
Matching the dense oracle under a large current vector is only an algebraic
check; it does not bound error against the nonlinear circuit at that excursion.

## Shipping-audio integration sequence

1. Qualify the fixed-size map against all 576 full-reference derivatives at
   memory/PANEL operating points, with source-supported 10 kOhm, historical
   50 kOhm and 100 kOhm sensitivity settings, endpoints, unequal banks and biased currents.
2. Specify a versioned, per-role digital-value-to-wiper adapter. Preserve stored
   host values/IDs and Legacy arithmetic. Do not normalize Hz or seconds directly
   and call the result physical travel. Declare the chosen orientation and taper,
   source basis and alternatives. Initially scope audible changes to selected
   memories rather than silently revoice all PANEL programs.
3. Connect **loaded** buses to typed receiver readouts without reapplying the
   loads. Cutoff, Q and level sources must account for all eight receiver drives
   at the same interval; EG/touch/RES current injection must have a single owner.
   Keep envelope state coordinates and voice continuity unchanged on selection.
4. Qualify nonlinear excursion and relinearization/fallback policy across real
   performance trajectories. A valid derivative is not a global linear model.
   Bound worst-case publication and evaluation cost, not just average cost.
5. Integrate complete control frames atomically, preserve inactive banks and
   session/selector behavior, then run compatibility, automation, sample-rate,
   onset/release, host and audible-change evidence on the final source.

This first increment is deliberately not connected to `Engine` or the plugin.
It does not claim the memory electrical model is already driving shipping audio.
The following experimental [semantic adapter and loaded receiver increment](memory-control-routing.md)
implements a named value/travel hypothesis and simultaneous local-linear
receiver feedback; it also remains outside shipping audio.

## Implementation and verification

`reference/MemoryPanelPortCompiler.h` prepares
`experimental/MemoryPanelPortMap.h`. The compiler accepts only memory/PANEL
selectors 11/12/13 and rejects malformed parameters or an unsupported graph
layout without replacing the caller's last valid map. The evaluator validates
current inputs and nonfinite arithmetic before publishing a complete result;
digital rows cannot receive injected current. A zero delta copies the nonlinear
operating-point voltage bits exactly. Runtime storage is fixed (1,888 bytes on
the tested 64-bit GCC build, with a 2,048-byte compile-time size ceiling).

The registered `cs80_memory_panel_port_map_tests` checks 90 points: both ranks,
three memory/PANEL selectors, 10/50/100 kOhm sensitivity settings, endpoints,
midpoints, unequal banks and a current-biased point. It checks all 51,840 matrix
entries against the existing dense oracle and evaluates five simultaneous
current-vector magnitudes against dense multiplication. Initial GCC results:
66,171 checks passed; largest derivative difference 1.44e-10 ohms and largest
linear-prediction difference 1.75e-14 V. These are substantially within the
preregistered limits, which were not changed after evaluation. ASan + UBSan with
library assertions also passed (LeakSanitizer disabled in the sandbox).

Twenty thousand varying map evaluations allocate no ordinary C++ memory.
Inspection confirms fixed arrays and bounded loops, with no lock, Newton solve,
I/O or container resizing in the evaluator. The printed microbenchmark is
informational shared-machine timing, not a worst-case deadline or instrument CPU
qualification. Preparation intentionally calls the allocating reference solver.

On GNU/Clang, `cs80_memory_port_map_negative_controls` recompiles temporary copies
of the actual test with two faults. Removing shared-rail terms fails 44,264
checks. Forcing symmetric derivatives fails 5,736 checks. The original header
passes; no repository file is mutated. The existing memory-panel, loaded-port
and programmer-identity CTests also pass independently.

Reproduce the registered tests with `CS80_BUILD_PLUGIN=OFF`, then build
`cs80_memory_panel_port_map_tests`, `cs80_memory_panel_ports_tests`,
`cs80_memory_panel_tests` and `cs80_programmer_control_identity_tests`, and run
`ctest --output-on-failure -R 'cs80_memory_panel|cs80_memory_port_map|cs80_programmer_control_identity'`.
All new runtime code remains unreferenced by shipping targets, so this increment
does not change Legacy arithmetic, parameter IDs, saved state or audio behavior.
