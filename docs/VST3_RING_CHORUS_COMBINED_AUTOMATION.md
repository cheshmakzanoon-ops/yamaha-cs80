# Combined RING and chorus-control qualification

## Existing capability, simultaneous controls

This test-only candidate combines the existing five [RING controls](VST3_RING_COMBINED_AUTOMATION.md)
with [chorus speed/depth](VST3_CHORUS_AUTOMATION.md), in static CHORUS mode=0
and [TREMOLO mode=1](VST3_TREMOLO_AUTOMATION.md). Chorus stays on. The existing
35-lane roster, parameter metadata, production DSP, UI and shared renderer are
unchanged. All 26 earlier contracts remain byte-identical; the new
`host-automation-ring-chorus-combined-v1.json` is a qualification contract,
not another capability append.

RING uses lanes 28–32 and chorus speed/depth lanes 33/34. The existing global
route publishes their converted controls to both engines regardless of rank
source. RING processing precedes the downstream BBD effect, with existing
intervening processing preserved. Nothing here establishes equivalence to a
hardware signal chain or introduces a new DSP equation.

## Timing and comparisons

The unchanged supported renderer supplies two note cycles: first onsets at
17+key, releases at 1024+key, second onsets at 2048+key, and releases at
3584+key, within 4,096 frames. RING attack/decay remain trigger-latched, not
live envelope reshaping. Their initial queue values are held through sample64,
covering both engines' first onsets; later targets precede the second cohort
and remain held through its final onset. Controls are published before MIDI
at the same sample under the existing wrapper semantics.

All seven finite queues run together, with neutral macros and a fixed effect
mode per case. A scalar reference and five alternative block partitions use
the original numerical threshold. The effect witness compares against seven
matched active-constant queues with identical initial values and paths. It
is an **aggregate** effect, not evidence of each lane's individual contribution.
No-queue and policy captures are separate observations, not substitutes for
that matched-active control.

The scope is a finite startup/transient window. It is not a complete LFO-cycle,
steady-state, mode-switch or per-lane attribution qualification. There is no
optional double-bypass control in this matrix.

## Bounded matrix and evidence requirements

`cs80_vst3_ring_chorus_combined_automation_tests` registers six configurations:
44.1/48/96 kHz, each realtime/offline. It uses the standard CLI:
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE`.
The frozen 35-lane module is reused; only the supported host fixture needs
compilation. No production build or historical helper rerun is implied.

The plan is **96 cases and 1,248 renders**. Four ordered rank-source pairs
(13,0), (0,13), (13,11), (11,13), two voice capacities (8/16), and two static
effect modes run in each configuration. The 8-capacity case uses two notes
per cycle; the 16-capacity case uses 16. Each case has six compatibility
observations, six scalar/partition renders and one seven-active-constant
control: 13 renders. The 288 retained local capture identities are **not
baseline comparisons**; no baseline replay is part of this gate.

This is candidate scope, not a passing result. Source/module/host hashes,
commands, actual partition errors, aggregate effects and complete outcomes
must be retained and independently reviewed before calling it verified.
Registration or successful compilation alone is not runtime evidence.
Prior checkpoints retain their own frozen source identities and scope.
Broad, malformed-input, fuzz, negative-baseline, native GUI/DAW, AU scheduled
ramps, hardware, CPU deadlines and allocation benchmarks are excluded.

## Verified local result

[Retained verification, commands and logs](experiments/ring-chorus-combined-automation/verification.json)
record successful direct official-SDK host compilation and six native processes,
all exit 0. The frozen 35-lane module was reused without a production build.
No CMake configure/build or CTest execution occurred.

All 96 cases passed the unchanged 2e-6 partition threshold: 48 static CHORUS
and 48 static TREMOLO cases across the four ordered rank-source pairs, two
capacities, three rates and two process modes. Maximum partition errors were
**5.96046e-08 for CHORUS** and **1.49012e-08 for TREMOLO**, not zero. Seven-lane
effects versus matched active constant controls ranged .0548363–.507416 and
.0554125–.507465, respectively. These are aggregate effects, not proof of each
lane's individual contribution or a universal numerical bound.

The unchanged two-note-cycle helper and finite 4096-frame window qualify only
this startup/transient interaction. Mode is static within each case; no mode
switch, full LFO cycle, steady-state or extra bypass control was tested.
Capture names include CHORUS/TREMOLO to keep identities distinct. The 288
retained local capture hashes are **not baseline comparisons**; no baseline
replay was performed.

SDK/compiler/Python and recorded source identities were captured before
compilation and verified after runtime. Host/module identities were captured
before runtime and verified after runtime. All
production sources, prior scripts, the shared helper and 26 previous contracts
match the base. No failures or retries occurred. Raw PCM and executable files
remain in ignored local build directories; committed evidence is not a
complete replay bundle. No broad, malformed, fuzz, negative-baseline, GUI,
DAW, AU, CPU or hardware tests were run.
