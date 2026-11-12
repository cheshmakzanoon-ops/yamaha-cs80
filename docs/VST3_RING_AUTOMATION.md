# Sample-offset ring speed and amount

## Scope and compatibility

The existing `P_ringSpeed` (`ring_speed`) and `P_ringAmount` (`ring_amount`)
controls append at lanes 28 and 29, after the unchanged 28-lane prefix. Modern
VST3 automation therefore covers 30 lanes. Ring depth, attack, decay and envelope
behavior are not added to this capability. No parameter, host ID, parameter
index, AU version hint, conversion law or DSP algorithm is introduced.

Ring speed retains its existing 0.1–500 Hz parameter range and skewed normalized
conversion. Amount retains its existing 0–1 range. Both are global parameters
before the rank-parameter range, so the existing automation exclusion for hidden
rank controls does not suppress them for factory or MEMORY selections. The
generic physical-value conversion and control-frame publication already deliver
them to the active engines.

Engine continues to read `global.ringSpeed` and `global.ringAmount`. Its existing
frequency law, envelope-depth contribution, phase clocking, amount blend and
policy-specific behavior are unchanged. In particular, PURE's effective speed
cap and the oscillator/envelope clock while amount is bypassed remain existing
DSP behavior, not new features or hardware-fidelity claims.

The only production change is the appended capability-list entries. Legacy and
PURE keep final-point-at-block-start timing; no-queue behavior is not intentionally
changed. AU and Standalone are not expanded. Fixed lane storage grows by two;
there is no new queue copy or performance qualification.

All 19 earlier numerical/capability JSON files remain unchanged. The new
`host-automation-ring-v1.json` declares the exact 30-lane capability. Historical
legacy-SUB individual and combined fixtures retain their exact 28-lane contract
and prefix/order assertions while allowing later lanes. The new ring fixture
owns the complete 30-lane assertion.

## Native verification gate

`cs80_vst3_ring_automation_tests` has six registrations: 44.1/48/96 kHz in
realtime/offline mode. It uses ordinary supported finite queues and the unchanged
`host-automation.json` numerical tolerance. Actual source selections, active-note
counts and voice capacities must be stated with the executed evidence rather
than inferred from a mode label.

Runtime results must identify exact source, host and module hashes and preserve
comparison with a frozen pre-feature module. Full candidate qualification and
compatibility-only observations must remain distinct. Implementation or
compilation alone is not a passing result. Earlier automation results retain
their own pinned scopes and do not qualify this extension automatically.

## Limits

This is a Linux float32 software-protocol capability, not a new ring circuit
model, measured Yamaha parity, native GUI/DAW qualification, AU scheduled ramps,
CPU-deadline result or allocation-performance result. Envelope/depth automation,
other parameters and untested configurations remain separate scopes. Broad,
malformed-input, fuzz and CPU fixtures are outside this gate.

## Verified bounded result

The final CMake-built plugin and three final CMake-built hosts were invoked
directly at 44.1/48/96 kHz in realtime and offline modes. All 36 matrix runs
exited zero. Full candidate checks covered 72 ring cases, 36 legacy SUB-speed
cases, and 72 combined legacy SUB speed/touch cases. These were direct native
host executions, not CTest executions.

The ring fixture isolates the two new lanes. Each has PANEL (13), factory (0)
and MEMORY (11) selections at eight/sixteen voice capacities. The unchanged
renderer supplies two/sixteen active keys respectively; eight-note saturation
is not claimed. Ring speed initially is 20 Hz, amount 0.5 and depth zero.
Finite normalized queues avoid an envelope-depth dependency. Maximum partition
error was zero for all three full candidate gates. Ring automation effects
ranged from 0.0106765 to 0.385975. All 36 speed-lane zero-amount controls had
exactly identical PCM output with and without automation. This establishes
output inertness only: the oscillator continues advancing, so internal-state
equality and later re-enable behavior are not claimed.

Each final host also completed six compatibility-only runs on the frozen
28-lane module. All 540 actual complete PCM capture pairs were byte-identical:
216 ring, 108 SUB speed, and 216 combined SUB. No-queue/null identity and
Legacy/PURE flattening passed. Historical isolated-speed and combined-SUB
behavioral assertions passed within those fixtures' existing scopes.

A separate single full 48 kHz realtime ring run against the frozen 28-lane
module exited **1**, with exactly twelve partition failures. This retained
negative control is not a passing full qualification. No runtime outcomes were
retried or discarded. All 19 earlier JSON contracts remain byte-identical.

Fresh configuration and the plugin/three-host build succeeded. Exact
commands, source/host/module hashes before and after execution, all logs, and
all 540 capture-pair hashes are retained in
`experiments/ring-automation/verification.json` and its referenced files. The
limits above still apply.
