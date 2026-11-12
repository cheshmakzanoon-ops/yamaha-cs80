# Portamento-time host automation

## Append-only candidate capability

This candidate appends existing `P_portamento` at lane 35, bringing the roster
to 36 lanes without moving the preceding 35 entries. The sole production
change is the capability list in `HostAutomationBridge.h`. Existing parameter
ID, 0–5 second range, zero default, skew .5 conversion, DSP, glide law, UI and
shared host renderer remain unchanged. All 27 earlier contracts are preserved;
`host-automation-portamento-v1.json` describes the new capability. Historical
CHORUS, TREMOLO and RING/chorus gates accept a larger runtime roster while
retaining their exact contract35 and full original prefix/order assertions.

## Existing glide and gate semantics

The global route already publishes portamento time to both engines. In
`Engine.cpp`, a gated positive time initializes a new voice's glide pitch from
`lastPitch`, then updates the existing exponential recurrence using current
time each tick. This is live time-coefficient control, not a new pitch law.
The existing held-key-node behavior is preserved.

The fixture holds smooth `glideMode=0`. Reset `lastPitch=60` and the existing
first note48 provide an initial pitch gap; subsequent supported notes retain
existing behavior. This is explicitly a reset-pitch/startup transient witness,
not a general legato-performance or quantized-glissando qualification.
Initial time is .12 seconds; finite targets .03/.2/.06/.25/.04/.15 seconds at
255/767/1535/2303/3583/4095 remain strictly above the existing off threshold.
No zero crossing or gate/mode-switch automation is introduced.

Queued effects compare with a matched active-constant initial queue, not a
no-queue initialization proxy. A scalar reference and five alternative block
partitions use the unchanged original threshold. Global source authority is
checked across PANEL/factory/MEMORY choices.

A separate control fixes `glideAssign=1`, `footswitchPlugged=1`,
`footswitchPressed=0`, and `sustainAssign=0`. This open assigned switch disables
glide: onset pitch is the target and running glide snaps to target. Queued time
versus matched active-constant time is therefore tested for PCM-output
inertness only. No internal-state or later re-enable equivalence is claimed.
The active witness uses an unassigned glide switch so glide remains enabled.

## Bounded native qualification plan

`cs80_vst3_portamento_automation_tests` registers six configurations:
44.1/48/96 kHz, realtime/offline. The CLI is
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE`, with a
separate compatibility-only mode. Sources13/0/11 and capacities8/16 give
36 full cases. The existing 4,096-frame note sequence has two active notes
for capacity8 and sixteen for capacity16.

The agreed plan is 756 renders in 12 native processes: 36 full cases with
15 renders each and 36 frozen35 compatibility cases with six each. Each full
case contains six no-queue/policy observations, six scalar/partition renders,
one matched active-constant effect control and two gate-off controls.
The 108 candidate/frozen35 no-queue and Legacy/PURE capture pairs must be
compared as actual bytes, not merely counted as identities.

This candidate scope has the retained local results below. Build/runtime
commands, exact source/host/module hashes, observed errors, effect witnesses,
control outcomes and compatibility comparisons require retained evidence and
independent review. Registration or compilation alone is not runtime evidence.
Prior [35-lane CHORUS evidence](VST3_CHORUS_AUTOMATION.md) retains its own inputs
and scope. Broad, malformed-input, fuzz, negative-baseline, GUI/DAW, AU ramps,
CPU/benchmark, hardware, release, threshold-transition and general legato
qualification are excluded. No historical fixture rerun is implied.

## Verified local result

[Retained verification, commands and logs](experiments/portamento-automation/verification.json)
record successful configure/build exit 0 for `CS80Reference_VST3` and
`cs80_vst3_portamento_automation_tests` only. All six candidate full processes
and six frozen35 compatibility-only processes exited 0. These were direct
host runs, not CTest or executions of the historical fixtures.

All 36 full cases passed the unchanged 2e-6 partition threshold. The measured
maximum partition error was **4.47035e-08**, not zero. Matched-active effects
ranged .0566545–.556828; all 36 assigned-open footswitch control comparisons
were exactly PCM-output inert. All 108 actual no-queue/Legacy/PURE capture
pairs matched the frozen35 module byte-for-byte.

This finite fixture uses static smooth mode0, initial .12 s and strictly
positive time targets. Its first note48 starts from reset lastPitch60; this
qualifies the declared reset/onset transient, not broad musical legato behavior.
Sources13/0/11 and8/16 capacities are tested at44.1/48/96 kHz in realtime and
offline modes. Capacity8 uses two held notes, capacity16 uses16. Gate-off
means assign1/plugged1/pressed0 with sustainAssign0; no later gate switching
or internal-state equivalence is claimed.

SDK/compiler/Python and recorded source identities were captured before
compilation and verified after runtime. Host/module hashes were captured before
runtime and verified afterward. All27 previous contracts, prior scripts and
production sources except the capability header match the base; the shared
renderer is unchanged. No failures or retries occurred. Raw PCM and binaries
remain locally in ignored build directories; the committed hashes/logs are not
a complete replay bundle. No negative baseline, broad, malformed, fuzz, GUI,
DAW, AU, hardware or CPU tests were run.
