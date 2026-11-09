# Experimental memory control adapter and loaded receiver routing

This is the next bounded increment after the
[prepared memory port map](memory-audio-integration.md), not activation in the
shipping engine. Legacy values, host/state schemas and current PANEL/memory
audio remain unchanged.

## Versioned semantic adapter, not a recovered pot taper

`semantic-linear-travel-v1` chooses a normalized nominal control voltage by
inverting the existing named source-model control laws, then **assumes** that
fraction is electrical travel from the reference end to the powered end.
It is an explicit engineering model, not a hardware measurement, the inverse of
the loaded pot/diode circuit, or the GUI slider's skew law. The electrical
network still determines the actual loaded voltages. Consequently this adapter
does not promise to reproduce the old digitally programmed sound.

| Physical row / role | Inferred nominal fraction before clipping |
| --- | --- |
| 1 PWM rate | `log(rate / 0.05) / log(400)` |
| 2 PWM depth, 6 noise, 11/12 filter targets, 16/17 gains, 20 sustain, 22 level, 23–26 touch depths | Stored normalized control |
| 3 pulse-width proxy | `(duty - 0.5) / 0.4` |
| 4/5 pulse/saw | Logical switch, not an analog voltage |
| 7/9 cutoff | `(referenceBus + log2(Hz / 62.5) - alignmentOffset) / 10`, using the source model's distinct HP/LP alignments |
| 8/10 resonance | `1 - rankResonance`, the existing inverse rank-voltage bridge, not the Legacy compound resonance mixer |
| 13/18 attack | `-log10(seconds) / 3` |
| 14/15/19/21 decay or release | `log10(10 / seconds) / 3` |

The identity table is `ProgrammerControlIdentity.h`, never channel enum order.
Feet, pan and filter bypasses have no programmer row. All represented inputs
must be finite and within their existing host ranges. Values outside the
nominal inverse's realizable range are clipped to travel endpoints with an
explicit per-row saturation mask and retained requested nominal volts. In
particular, the digital 20-second attack range exceeds the nominal source law's
one-second maximum; low cutoff targets can require negative nominal bus volts.
These limitations must not silently become changes to existing programs.

Sources and model assumptions: [parts-list evidence](memory-pot-source.md),
[factory model and receiver proposal](../FACTORY_TONE_INTEGRATION_PROPOSAL.md),
[rank-I AL graph](../PRESET_AL_RECEIVER_GRAPH.md),
[rank-II receiver identities](../RANK_II_RECEIVER_PORTS.md), and
[the inference policy](../ANALOG_FIDELITY_REQUIREMENT.md). Pot resistance is an
explicit compiler input; the adapter does not reinterpret a shared 10 kOhm
test configuration as proof of every PANEL pot's resistance.

## Loaded receivers and shared-rail feedback

The prepared map already contains the one bus shunt and all eight nominal
receiver loads. Apply receiver **injection currents**, not another Thevenin
load. For cutoff rows 7/9, the injection is `gc*(t*sum(touch+KC) + e*sum(EG))`;
for Q rows 8/10 it is `8/440000 * masterRES`; for level row 22 it is
`gLV*tLV*sum(touch+KB)`. The coefficients come from the nominal receiver graph.

Touch depths are actual loaded buses 23–26. Their voltages move with the three
shared rails, so evaluating all injections using frozen depths is inconsistent.
The depth rows themselves have no injected current in this boundary model.
Substituting their diagonal-plus-three-rail map into the injections reduces the
feedback problem to three coordinates: `(I - B) rail = rhs`. A fixed-size
pivoted solve produces one simultaneous frame. Require `||B||infinity < 0.5`
and a finite current-balance residual before publishing; failure preserves the
previous complete output. No iteration-to-convergence, allocations or locks
belong in this evaluator.

Readouts retain pin volts, effective cutoff CV, Q and normalized level separately.
Envelope state remains the existing `z` coordinate; it is read as `(5/3)*z`
model volts, never overwritten. All eight voices, including silent control
circuits, contribute. The master RES boundary remains a single ideal/stiff
source, so this increment introduces no finite cross-rank source impedance.
Explicit per-key KC/KB voltages are supported; pitch key-volts are not KC.
Other global modulation boundaries remain zero here. A future Engine route must
choose one owner per transport rather than applying an existing effective-domain
contribution a second time.

## Preregistered gates and remaining work

`tests/parity/tolerances-v2/memory-control-routing.json` records acceptance
limits before implementation is evaluated. Compare the bounded feedback route
against an independent dense 24-bus solution using the full loaded Jacobian,
and check the receiver equations without reapplying loads. Check semantic
round trips only within the explicitly realizable domain, endpoint orientation,
rank/selector identity, selected switches, invalid-input retention and no
allocation in repeated evaluation. Fault injections must detect frozen touch
depths and duplicate loading.

These are algebra/model-consistency gates, not nonlinear excursion limits or
hardware fidelity. Before shipping, qualify the reference-linearization range,
relinearization/fallback behavior, whole-instrument cost, immutable frame
publication and all source-selection/host continuity. No loaded bus is to be
stored in `ToneControls.sources`, whose contract remains unloaded.
The model has the original eight receiver circuits per rank. A future 16-voice
extension must explicitly choose its shared-source/load topology rather than
silently treating this eight-load map as a sixteen-load network.

## Implemented boundary and reproducible results

`experimental/MemoryControlAdapter.h` performs the named semantic conversion.
`reference/MemoryControlFrameCompiler.h` prepares all three rank-local banks
outside the callback and retains their saturation masks, selected logical
switches, electrical assumptions, frame/adapter/source versions and source hash.
The compiler intentionally allocates through the existing nonlinear reference.

`experimental/LoadedMemoryReceivers.h` evaluates one simultaneous bounded
local-linear frame. It produces distinct electrical buses, cutoff pins/effective
CV, Q pins/Q, level pins/gains, and typed oscillator/envelope controls. It does
not manufacture unloaded `SourceEquivalent` objects, mutate envelope state or
apply a second receiver load. Failed preparation and failed readout evaluation
both preserve the previous complete result.

Initial GCC verification passed 48,376 checks: 36 operating points and 216 full
receiver frames, both ranks, all three supported selectors, 10/50 kOhm settings,
unequal banks, and maps prepared with nonzero cutoff and touch-depth currents.
An independent 24-bus Gaussian solve uses the dense full-reference Jacobian and
separately derived receiver resistor coefficients. Largest readout comparison
difference was 7.994e-15; the largest current residual was 2.711e-20 A and the
largest feedback infinity norm was 3.284e-5. No preregistered limit was changed.
The semantic grid has 414 realizable round trips and 90 explicitly clipped
targets; it does not treat those 90 cases as successful exact inversions.

Tests also check all 26 row identities, excluded controls, every bank's retained
limits, rank-specific touch identities, exact zero-drive operating-point bits,
voice-order independence, invalid input/version and ill-conditioning rejection,
and complete-result retention. Twenty thousand varying receiver evaluations
allocate no ordinary C++ memory. Inspection confirms bounded arrays/loops and a
fixed three-coordinate solve with no locks, I/O or convergence loop. Printed
local timing is not instrument CPU or worst-case deadline qualification.

ASan + UBSan with library assertions passed (LeakSanitizer disabled in the
sandbox). Seven focused CTests, including the existing memory/port/identity
tests, passed. The temporary-source negative-control test passes its baseline,
detects frozen touch feedback with 150 failures, and detects duplicate loading
with 18,390 failures; compile errors and crashes are not counted as detection.

With `CS80_BUILD_PLUGIN=OFF`, build `cs80_memory_control_routing_tests` plus the
four targets listed in [the first increment](memory-audio-integration.md), then
run `ctest --output-on-failure -R 'cs80_memory_|cs80_programmer_control_identity'`.
The new source modules have no Engine/plugin consumers. Their checks establish
only the experimental boundary above, not final host or audible integration.

The subsequent [nonlinear range and scheduling study](memory-nonlinear-qualification.md)
finds explicit approximation-budget violations and rejects simply activating
this fixed map. A tiny affine receiver residual does not certify the nonlinear
source. The study specifies the next residual-certificate/sparse-update step.
