# Sample-offset compatibility-mode SUB speed

## Scope and naming

The existing `P_subSpeed` (`sub_speed`) control appends at lane 27 after the
unchanged 27-lane prefix, bringing modern VST3 automation to 28 lanes. It is the
0.05–30 Hz speed used when original SUB mode is off, not `P_subOriginalSpeed`
(the separate original-panel control). No parameter, host ID, parameter index,
AU version hint, conversion law or oscillator is added.

“Compatibility-mode SUB” names the oscillator selection. It does not mean the
Legacy render policy: sample-offset timing applies only under the existing
modern policy. Legacy and PURE retain final-point-at-block-start behavior for
this control just as they do for the earlier automation lanes.

## Existing routing and preserved behavior

`P_subSpeed` is a global parameter before the rank-parameter range, so the
existing rank-source exclusion does not suppress it for factory or MEMORY
selections. The generic automation path performs its existing normalized-to-
physical conversion and publishes the existing control frame. Engine already
reads that value into `global.subSpeed`; its compatibility-mode branch retains
its established pressure multiplier, modulation-rate multiplier and smoothing.
The original-mode branches continue to use `global.subOriginalSpeed` instead.

Production changes are confined to the appended capability-list entry. Mode
selection, waveform selection, pressure routing, source ownership and DSP
response laws are unchanged. No-queue behavior and the separate AU/Standalone
paths are not intentionally changed. No new queue copy is introduced, and no
CPU-deadline or allocation-performance claim follows from one more fixed lane.

All earlier numerical/capability JSON files remain byte-identical. The new
`host-automation-sub-legacy-speed-v1.json` declares the exact 28-lane capability.
The individual and combined touch gates retain their exact historical contract
count of 27 and each lane/order assertion, but allow the production roster to
extend beyond that preserved prefix. The new speed gate owns the complete
28-lane assertion.

## Native verification gate

`cs80_vst3_sub_legacy_speed_automation_tests` is registered for 44.1/48/96 kHz
in realtime/offline mode. The fixture selects a static compatibility-mode sine
and eight/sixteen voice capacities, with two/sixteen active keys respectively.
Its ordinary supported finite queues are measured against a one-sample reference
with the unchanged `host-automation.json` numerical tolerance. Compatibility observations must be compared against the
frozen pre-feature module, rather than inferred from successful compilation.

Runtime results must identify exact source, host and module hashes and clearly
distinguish full candidate qualification from compatibility-only observations.
Implementation alone does not establish a passing result. Historical SUB depth,
touch, combined and static-waveform results remain separate pinned checkpoints.

## Limits

This is a Linux float32 VST3 protocol capability, not measured hardware parity,
discrete mode/waveform automation, GUI/DAW qualification, AU scheduled ramps or
a performance target. Only the supported inputs and configurations actually
recorded by the fixture are qualified. Broad, malformed-input, fuzz and CPU
fixtures are outside this gate.

## Verified bounded result

The final CMake-built plugin and three CMake-built hosts were run directly for
44.1/48/96 kHz in realtime and offline modes. These were direct native-host
executions, not CTest executions. All 36 matrix invocations exited zero:

- New speed gate: 36 full candidate cases across six configurations.
- Individual touch regression: 108 full candidate cases across six configurations.
- Combined touch regression: 72 full candidate cases across six configurations.
- Each final host also ran six compatibility-only configurations on the same
  frozen 27-lane module. All 648 complete PCM capture pairs were byte-identical
  (108 speed, 324 individual touch, 216 combined touch). These captures cover
  no-queue and Legacy/PURE behavior, not baseline sample-offset qualification.

Maximum observed partition error was zero in all three full candidate gates.
The new speed automation witness ranged from 0.043749 to 0.635032; its static
original-oscillator-mode inactive control had exactly zero difference in all
36 cases. The historical touch assertions, including their pressure witnesses
and no-pressure inertness, also passed. Combined witnesses remain aggregate,
not independent lane isolation.

A separate, single full 48 kHz realtime speed-gate run against the frozen
27-lane module exited **1**, with exactly six partition failures and observed
errors of 0.0633988–0.554831. This retained negative control demonstrates that
successful compilation or a positive audible-effect witness alone does not
establish sample-offset support. It is not a passing qualification result.

Every runtime outcome was retained without retries. All 18 earlier JSON
contracts were verified byte-identical to the base revision. The initial build
attempt failed because configuration preceded a concurrent CMake target edit;
no tests ran in that attempt. Explicit reconfiguration and the final 98-step
build succeeded. Both attempts' logs and exact commands are retained alongside
source/module/host hashes, all runtime logs, and per-capture hashes in
`experiments/sub-legacy-speed-automation/verification.json` and its referenced
evidence files. This result does not expand the limits stated above.
