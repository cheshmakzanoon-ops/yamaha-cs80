# Candidate sample-offset RING attack and decay

## Capability and preserved latch semantics

Existing `P_ringAttack` (`ring_attack`) and `P_ringDecay` (`ring_decay`) append
at lanes 31 and 32 after the unchanged 31-lane prefix. The candidate modern
VST3 roster contains 33 lanes. The only production change is those two list
entries. IDs, parameter indices, defaults, 0.001–10 s ranges and skew .4
normalized conversions remain unchanged; no UI, wrapper, DSP or latch edit.
This is candidate source scope, not a published or verified result.

These are **trigger-latched controls**, not live envelope-shape modulation.
`Engine.cpp` updates global ring times from the control frame, but uses them
only in `ringAdsr.noteOn` when `ringKeyDown` was false. A later note while a
ring key is already down does not re-latch them. The existing last-held/pedal-
held release path clears `ringKeyDown`; a subsequent qualifying onset uses the
then-current global values. This describes existing source semantics, not a new
retrigger mechanism or an independently qualified next-trigger scenario.

The existing wrapper attaches supported parameter queues before processing.
`PluginProcessor.cpp::advanceTo` publishes automation at the event sample
before dispatching MIDI; generic normalized-to-physical conversion and global
control-frame routing already include these controls. A qualifying note at
sample 17 therefore latches its sample-17 converted value. Global controls are
not suppressed by factory/MEMORY rank selection.

Changing times after the latch still updates stored/global parameters, but
does not alter the running AD trajectory. Do not call these controls globally
ignored or claim live ADSR reshaping. Existing Legacy/PURE final-point-at-block-
start timing remains; no-queue compatibility requires a frozen31 comparison.
AU, Standalone and protected native faders remain unchanged. Fixed lane storage
grows by two, without a CPU or allocation-performance qualification.

## Contracts and bounded native gate

All 21 earlier numerical/capability contracts remain byte-identical. New
`host-automation-ring-times-v1.json` declares exactly 33 lanes. The historical
depth fixture keeps exact contract count31, the complete old30 prefix and depth
lane30; only available runtime roster size changes from ==31 to >=31. The new
times fixture must verify exact33 and the complete old31 prefix.

The six `cs80_vst3_ring_times_automation_tests` registrations cover
44.1/48/96 kHz and realtime/offline using only supported finite-input renderer
code. Attack-only and decay-only cases require nonzero ring depth/amount and
active envelope conditions, per-sample reference/partition witnesses, and
source/voice settings recorded with the executed evidence. Original numerical
tolerances remain unchanged.

A same-offset witness can compare a target published at note sample17 with the
same target held since16. Converted values must use the existing skewed
parameter mapping, not a linear-seconds assumption. In 16-note mode the initial
onsets span17–32 and include a separate engine's first trigger. A late-only
queue must explicitly hold the initial value through **all** those triggers
(e.g. through64) before changing. A first future queue point alone is not a
late-only hold: normalized interpolation starts at implicit sample -1.

Late-only PCM inertness is scoped to a window with no later fresh ring trigger.
It is not state equivalence, nor proof that future triggers ignore updates.
No-queue and Legacy/PURE captures must compare against a frozen31 module;
compatibility-only runs and full candidate assertions remain distinct.

Exact source/host/module hashes, native modes, source choices, active-note counts,
voice capacities and all outcomes belong in retained runtime evidence. No build,
configuration or earlier depth result automatically qualifies this append. Verified local results are recorded below. Broad, malformed-input,
fuzz, GUI/DAW, hardware and CPU/benchmark scopes remain excluded.

## Verified local result and disclosed oracle correction

[Retained verification, commands and logs](experiments/ring-times-automation/verification.json)
include both attempts. The initial candidate VST3/host configure and build
succeeded. The first 44.1 kHz realtime configuration then returned exit 1 with
six decay late-control exact-equality failures (largest difference
3.72529e-07). Its source, commands, logs and binary identities are retained;
the remaining initial configurations were not started.

The original late-changing queue was compared against no queue. A normalized
zero-frame flush and active normalized conversion/publication can leave
different physical onset values; the exact numerical cause was not
instrumented. After independent review, the oracle was corrected to compare
late-changing automation against a constant active queue with the same initial
normalized value and partition. No DSP, tolerance or no-queue compatibility
assertion changed. The corrected host target was requested; its dependency step relinked the
candidate VST3, whose SHA remained unchanged. The build exited 0. This was a disclosed reviewed correction and
new attempt, not a failure-free initial run.

The corrected six full candidate runs and six frozen31 compatibility-only runs
all exited 0: 72 full cases and 216 actual no-queue/Legacy/PURE PCM pairs, all
byte-identical to the frozen module. Every scalar-versus-partition, same-offset
17-versus-early16, and late-versus-constant-active comparison had error 0.
Isolated attack effects ranged .0467184–.312214; isolated decay effects ranged
.0575063–.491292. These are finite-fixture results, not universal error bounds.

The matrix covers sources 13/0/11, 8/16 voice capacities and 44.1/48/96 kHz in
realtime/offline modes. Attack changes .08→.003 s; decay changes .45→.02 s
with attack fixed .003 s. Ring depth .6, amount .5 and speed 20 Hz keep the
modulation active. The 8-capacity fixture holds two notes and the 16-capacity
fixture 16; onsets are 17+key and releases 3072+key. Late queues explicitly
hold their initial value at samples 0 and 64, covering both engines' initial
onsets. No new onset follows in the 4096-sample window. Stored-time behavior at
a subsequent fresh trigger remains source reasoning, not runtime qualification.

All 21 previous contracts, prior scripts and production sources apart from the
capability header match the base. Only the candidate VST3 and new supported
host were built; no CTest, historical runtime gate, negative baseline, broad,
malformed, fuzz, GUI, DAW, AU, hardware or CPU test was run. Exact final source,
host, module and capture hashes are retained. Raw PCM and both host binaries
remain locally in ignored build directories; committed evidence is not a
complete replay bundle.
