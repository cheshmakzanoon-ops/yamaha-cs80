# Candidate sample-offset RING depth

## Minimal capability change

Existing `P_ringDepth` (`ring_depth`) appends at lane 30, after the unchanged
30-lane roster. The candidate modern VST3 capability therefore contains 31
lanes. No parameter ID/index, physical range, conversion law, DSP algorithm or
UI changes. This is a candidate scope; verified local results below do not
imply publication or release qualification.

Depth retains its existing 0–1 range. It is a global parameter before the rank
parameter range. `PluginProcessorAutomation.cpp` already converts its normalized
value, updates the cached physical value and publishes the control frame without
the rank-source exclusion. `Engine.cpp` already reads `global.ringDepth` and
uses it with ring speed and the ticking ring AD envelope in the existing
policy-specific frequency law. No extra routing change is required.

The only production diff is one capability-list entry. Attack and decay remain
block-rate; envelope generation, retrigger/reset behavior, frequency clamp and
phase clock are unchanged. Legacy and PURE retain their final-point-at-block-
start policy. No-queue PCM compatibility requires comparison with the frozen
pre-feature 30-lane module. AU, Standalone, protected faders and native GUI are
not expanded. Fixed lane storage grows by one; no performance claim follows.

## Contracts and historical gates

All 20 previous numerical/capability JSON files remain byte-identical. New
`host-automation-ring-depth-v1.json` declares exactly 31 lanes and the new lane.
The older ring fixture retains its exact contract count of 30, full 28-lane
prefix and exact speed/amount lanes 28/29. Only its available runtime roster
check becomes >=30 so it can verify that historical prefix after an append.
The new depth fixture owns the full exact31 assertion and old30 prefix check.

## Bounded native qualification plan

`cs80_vst3_ring_depth_automation_tests` has six candidate registrations:
44.1/48/96 kHz, realtime/offline. It compiles only the supported-input renderer,
not historical malformed-input or broad test bodies. The original
`host-automation.json` numerical thresholds remain unchanged.

The separate fixture covers isolated depth and combined speed/amount/depth
queues with PANEL/factory/MEMORY source choices and explicit voice capacities.
Static finite attack/decay must keep the ring envelope active during the depth
witness at every rate; ring amount must be nonzero and the carrier must not
saturate its frequency clamp. Queue-value changes alone are not an audible
effect witness. Per-sample references and alternative block partitions must
agree under the original thresholds. Isolated depth cases establish a separate
depth effect; the combined witness is aggregate and does not isolate depth
from simultaneous speed/amount changes. An amount-zero depth control is only
**PCM-output inertness**, not internal-state equivalence: AD/phase keep clocking.

Exact source, module and host hashes, active notes, source selections, runtime
modes and all outcomes belong in retained executed evidence. No-queue and
Legacy/PURE PCM captures must match the frozen30 module independently of full
candidate assertions. Implementation, compilation, registration or an earlier
ring-speed/amount result is not new qualification.

This is software-protocol timing, not Yamaha hardware fidelity, a new ring
model, DAW/native-GUI validation, AU scheduled ramps, CPU deadlines or allocation
qualification. Broad, malformed-input, fuzz and CPU fixtures remain outside scope.

## Verified local result

[Retained commands, logs and verification](experiments/ring-depth-automation/verification.json)
record one configure and build, both exit 0, targeting only
`CS80Reference_VST3` and `cs80_vst3_ring_depth_automation_tests`. Six direct full
candidate runs and six frozen30 compatibility-only runs each exited 0. CTest
was not executed; the historical ring host was not built or run.

The 72 full cases cover depth-only and combined queues, sources 13/0/11 and
8/16 voice capacities at 44.1/48/96 kHz in realtime/offline modes. Every scalar
versus 7/64/256/1024/4096 partition comparison had max error 0. Isolated depth
PCM effects ranged .0261518–.447292; combined aggregate effects ranged
.0651161–.335922. All 36 depth-only amount-zero controls were exactly
output-inert. These numbers describe the retained finite fixtures, not a
universal error bound.

Static ring attack/decay were .01/.45 s, initial speed 20 Hz, amount .5 and
depth .4. The unchanged renderer sends note-on at sample 17+key and note-off
at 3072+key; the 8-voice capacity uses two held notes, the 16-voice capacity
uses 16. No retrigger, attack/decay automation or broader event coverage is
implied. The combined witness does not prove per-lane contribution.

All 216 actual no-queue and Legacy/PURE PCM capture pairs matched the frozen
30-lane module exactly. All 20 previous contracts, previous scripts and
production source other than the capability-list header matched the base.
Exact source, final host and module hashes are retained. Raw PCM and binaries
remain in the local ignored build directories for inspection; the committed
evidence contains their hashes and logs, not a complete replay bundle.
No failures were retried, and no negative-baseline, broad, malformed, fuzz,
GUI, DAW, hardware, AU or CPU tests were run.
