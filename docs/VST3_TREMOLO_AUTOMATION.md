# Static TREMOLO-mode speed/depth qualification

## Existing capability, different static mode

This test-only candidate qualifies the existing 35-lane
[chorus-control capability](VST3_CHORUS_AUTOMATION.md) with `chorusOn=1` and
`chorusMode=1` held fixed. Speed/depth remain lanes 33/34. There is no new roster,
parameter, production, DSP, UI or shared-renderer change. All 25 old contracts
remain byte-identical; `host-automation-tremolo-v1.json` is a qualification
contract, not another capability append.

The prior active CHORUS checkpoint did not qualify active TREMOLO behavior.
This gate does not automate the switches or exercise transitions between modes.
It preserves existing physical conversions and global source authority, with
neutral macros including width=.5 so depth is not remapped by that macro.

## Source-supported witnesses

In the existing `BbdChorus`, TREMOLO keeps clock modulation at zero, so the BBD
clock remains nominal. Speed advances the two oscillator phases; depth enters
the signed left/right gain modulation. Isolated speed tests therefore hold
nonzero depth, while isolated depth tests hold finite nonzero speed. Their
controls use matching active constant queues and initial values, not no-queue
initialization as a proxy.

The scalar reference and five block partitions use the original numerical
threshold. The isolated effect witnesses are finite startup/transient
observations. The 4,096-frame window is not full-LFO-cycle or steady-state
response coverage, nor a hardware/alias qualification.

Two source-supported controls have deliberately limited meaning:

- Off-state queued versus matched active-constant renders compare PCM only;
  delay/noise/oscillators continue running internally.
- In speed cases, depth is held at zero. TREMOLO then has nominal clock and
  phase-independent gains, so queued speed versus matched active-constant
  speed must be PCM-inert. Oscillator phases may still differ. No analogous
  zero-speed depth control is assumed or introduced.

Neither equality claims internal-state equivalence or behavior after a later
switch/depth change. No extra control beyond the approved matrix is implied.

## Bounded native matrix and evidence

`cs80_vst3_tremolo_automation_tests` registers six configurations:
44.1/48/96 kHz, realtime/offline, using the standard module/original-contract/
new-contract/output/rate/mode CLI. Its exact35/prefix assertions refer to the
existing capability. Only the supported finite-input host renderer is compiled.
The frozen35 module is reused; no new plugin build or historical helper rerun
is required because production and helper bytes are unchanged.

The agreed plan is 1,152 renders across 72 isolated speed/depth cases covering
PANEL/factory/MEMORY choices and 8/16 voice capacities. Separate no-queue/policy
observations do not substitute for matched-active feature controls. Source,
host/module hashes, commands, actual outcomes and capacity/active-note scope
must be retained before calling the gate verified. Registration or a successful
host compile is not runtime evidence.

This document describes a candidate scope only. Prior
[CHORUS evidence](VST3_CHORUS_AUTOMATION.md) keeps its own frozen inputs and
mode-specific limits. Broad, malformed-input, fuzz, GUI/DAW, AU scheduled ramps,
CPU deadlines, allocation benchmarks, hardware and switch-transition
qualification remain excluded.

## Verified local result

[Retained verification, commands and logs](experiments/tremolo-automation/verification.json)
record successful direct official-SDK host compilation and six native processes,
all exit 0. The frozen 35-lane module was reused; no production build, CMake
configure/build or CTest execution occurred.

All 72 full cases passed the unchanged 2e-6 partition threshold. Maximum
partition errors were **1.49012e-08 for speed** and **7.45058e-09 for depth**,
not zero. Matched-active speed effects ranged .000825892–.00899467 and depth
effects .00230088–.0358306. All 72 bypass controls were exactly PCM-output
inert. All 36 speed-only depth-zero controls were also exact; the depth cases
do not run that control. Internal phase evolution remains unconstrained.

The matrix covers sources 13/0/11, 8/16 voice capacities, 44.1/48/96 kHz and
realtime/offline modes. It is a static TREMOLO mode=1 startup/transient test
over 4096 frames, not a complete LFO cycle, steady-state or mode-switch test.
The 8-capacity fixture uses two notes and the 16-capacity fixture 16.

SDK/compiler/Python and recorded source identities were captured before
compilation and verified after runtime; host/module identities also remained
unchanged. All production sources, prior scripts, the shared renderer and
25 previous contracts match the base. No failures or retries occurred.
There are 216 retained local capture hashes, **not 216 baseline comparisons**:
no baseline replay was performed. Raw captures and the executable remain
locally in ignored build directories, so committed evidence is not a complete
replay bundle. No broad, malformed, fuzz, negative-baseline, GUI, DAW, AU,
hardware or CPU tests were run.
