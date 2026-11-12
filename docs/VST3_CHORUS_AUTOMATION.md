# Candidate sample-offset chorus speed and depth

## Minimal capability change

Existing `P_chorusSpeed` (`chorus_speed`) and `P_chorusDepth` (`chorus_depth`)
append at lanes 33 and 34 after the unchanged 33-lane prefix. The candidate
modern VST3 capability contains 35 lanes. The only production change is this
roster append: no IDs, parameter indices, defaults, DSP, routing or UI changes.
Speed retains 0.05–8 Hz with skew .4; depth retains 0–1. This source-level scope
is not a claim of verified or published qualification.

The existing generic global automation path converts normalized values and
publishes the control frame. `Engine.cpp` already reads both controls and passes
them to the source-policy `BbdChorus` every tick. Their global authority is not
suppressed by factory/MEMORY rank selection. The width macro can add to depth;
the fixture holds that macro at its neutral .5 value to avoid this confound.

Initial active qualification is **CHORUS only** (`chorusOn=1`, `chorusMode=0`).
On/off and chorus/tremolo switches remain block-rate and no tremolo qualification
is claimed. Legacy/PURE retain their existing final-point timing. AU,
Standalone and protected native faders are unchanged. Fixed lane storage grows
by two; no CPU or allocation-performance conclusion follows.

## BBD behavior and bounded witnesses

The existing BBD implementation integrates both oscillator phases from speed.
In active CHORUS mode, depth affects the clock modulation. Its delay, noise and
oscillators continue running while bypassed, but `!on` returns the exact input
pair. Therefore an off-state matched-active comparison can establish
**PCM-output equality only**, not equality of hidden oscillator/delay states.

The new supported fixture isolates speed and depth with ordinary finite curves,
active speed/depth settings and neutral macros. Per-sample reference renders
are compared with five alternative block partitions under the unchanged
numerical tolerance. A matched active-constant queue supplies the effect
control, avoiding no-queue/active-conversion initialization differences.
Bypass renders compare the queued lane against its matched active-constant
lane with on=0; they do not toggle the effect mid-window.

The 4,096-frame window is about 42.7 ms at 96 kHz. It permits a finite BBD
startup/transient witness but is **not** a full LFO cycle or steady-state
frequency/depth response qualification. No measured Yamaha calibration or
hardware-fidelity inference follows. Exact active-note counts, voice capacity,
source selection and modes belong in executed evidence.

## Contracts and native gate

All 24 previous numerical/capability contracts remain byte-identical. The new
`host-automation-chorus-v1.json` owns exact35 and the new lanes. Historical
RING times, retrigger and combined fixtures retain exact contract count33 and
their complete historical prefix/position checks. Only runtime roster size is
relaxed to >=33 so those gates remain meaningful after an append.

`cs80_vst3_chorus_automation_tests` has six registrations: 44.1/48/96 kHz,
realtime/offline, with the standard module/original-contract/new-contract/
output/rate/mode CLI. Only the supported renderer is compiled; old broad or
malformed-input bodies are excluded.

The bounded matrix covers isolated speed/depth with PANEL/factory/
MEMORY source choices and 8/16 voice capacities: 72 full cases plus 72 frozen33
compatibility-only cases, totaling 1,512 renders. No-queue and Legacy/PURE PCM
compatibility against the frozen module remains separate from matched-active
feature witnesses. Preserve all outcomes and exact source/host/module hashes;
compilation or registration alone is not passing evidence.

The verified local result below is bounded to this scope. Broad,
malformed-input, fuzz, GUI/DAW, hardware, AU scheduled ramps, CPU deadlines and
allocation benchmarks remain outside this gate.

## Verified local result

[Retained verification, build commands and logs](experiments/chorus-automation/verification.json)
record successful configure/build exit 0, targeting only `CS80Reference_VST3`
and `cs80_vst3_chorus_automation_tests`. Six full candidate and six frozen33
compatibility-only processes all exited 0. These were direct host runs, not
CTest execution or a run of the three historical fixtures.

All 72 full cases passed the unchanged 2e-6 partition threshold. Measured
maximum partition errors were **5.96046e-08 for speed** and **3.72529e-08 for
depth**, not zero. Speed effects versus matched active constant controls ranged
.012098–.163533; depth effects ranged .0206073–.245787. All 72 off-state queued
versus matched-hold controls were exactly PCM-output inert. Internal BBD
oscillators, delay and noise continue running; this does not establish state
identity or later re-enable behavior.

All 216 actual no-queue and Legacy/PURE capture pairs matched the frozen33
module byte-for-byte. This transient 4096-frame fixture uses static CHORUS
on=1/mode=0, speed 3 Hz, depth .7 and neutral .5 macros. It does not qualify
TREMOLO, a complete LFO cycle, steady-state behavior, arbitrary events or a
universal numerical bound. The 8-voice capacity uses two notes; the 16-voice
capacity uses 16.

SDK/compiler/Python and recorded source hashes were captured before compilation
and verified afterward, as were runtime host/module inputs. The final source
map includes the three historical fixtures whose capability guards were
updated. All 24 prior contracts, all prior scripts and production sources
other than the capability-list header match the base; the shared renderer is
unchanged. No failures or retries occurred. Raw PCM and binaries remain locally
in ignored build directories; committed evidence retains their identities and
logs, not a complete replay bundle. No negative baseline, broad, malformed,
fuzz, GUI, DAW, AU, hardware or CPU tests were run.
