# Static GLISSANDO-mode portamento qualification

## Existing capability, different static mode

This test-only candidate qualifies existing [portamento automation](VST3_PORTAMENTO_AUTOMATION.md)
in static `glideMode=2`. The 36-lane roster and `P_portamento` lane35 remain
unchanged. Production, parameter metadata, glide law, UI and the shared
supported renderer are unchanged. All 28 earlier contracts remain byte-identical;
`host-automation-glissando-v1.json` adds qualification, not a capability lane.

## What is rounded, and what is not

The existing Engine maintains a continuous exponential glide trajectory.
It adds the resulting glide offset to the other oscillator pitch terms, then
rounds that aggregate pitch when mode2 and the glide gate are enabled.
Ribbon ratio, feet ratio and held-key ratio multiply frequency after this
rounding. Filter key CV uses unrounded glide pitch plus master tune.

Consequently this gate does **not** establish that the whole signal, filter
CV or final oscillator frequency is quantized to semitones. Its positive
witness is a difference in rendered PCM against matched active-constant time,
not a measured pitch staircase, pitch accuracy or hardware-fidelity result.

## Bounded controls and matrix

The existing reset lastPitch60 and first note48 create a pitch gap without
changing helper events. Initial time .12 seconds and targets
.03/.2/.06/.25/.04/.15 at samples255/767/1535/2303/3583/4095 remain positive
and away from the existing off threshold. Mode2 is fixed throughout each case;
there are no mode transitions, zero-time crossings or general legato claims.
The active fixture has an unassigned glide switch. Its scalar reference and
five alternative partitions use the original numerical threshold, and its
waveform-effect comparator uses a matched active-constant initial queue.

The gate-off control fixes glideAssign1, footswitchPlugged1,
footswitchPressed0 and sustainAssign0. With the glide gate disabled, time
is inert in the glide recurrence and mode2 rounding is disabled too. Queued
versus matched active-constant time is compared for PCM equality only;
internal state or later re-enable equivalence is not claimed.

`cs80_vst3_glissando_automation_tests` registers six configurations:
44.1/48/96 kHz in realtime/offline modes, with standard CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE`.
Sources13/0/11 and capacities8/16 give 36 cases. The 4,096-frame renderer
uses two notes for capacity8 and sixteen for capacity16.

The plan is **540 renders in six native processes**: each case contains six
no-queue/policy observations, six scalar/partition renders, one matched
active-constant control and two gate-off renders. The frozen36 module is
reused, with only the supported host fixture compiled. There are 108 local
capture identities, **not baseline comparisons**; no baseline replay or
historical helper rerun is part of this unchanged-production qualification.

This is candidate scope, not a passing result. Exact source/host/module hashes,
commands, errors, effects and control outcomes must be retained and reviewed.
Registration or compilation alone is not runtime evidence. Prior smooth-mode
evidence keeps its own frozen inputs and limits. Broad, malformed-input, fuzz,
negative-baseline, GUI/DAW, AU ramps, CPU/benchmarks, hardware, release,
threshold-transition and full pitch-staircase qualification remain excluded.

## Verified local result

[Retained verification, commands and logs](experiments/glissando-automation/verification.json)
record successful direct official-SDK host compilation and six native processes,
all exit 0. All 36 full cases passed the unchanged 2e-6 partition threshold;
the measured maximum was **1.49012e-08**, not zero. Effects versus matched
active holds ranged .0426078–.468947. All 36 assigned-open gate controls were
exactly PCM-output inert. No failures or retries occurred.

This static mode2, finite positive-time fixture covers sources13/0/11,
capacities8/16, rates44.1/48/96 kHz and realtime/offline processing. The
aggregate PCM effect does not isolate oscillator step timing, prove staircase
accuracy or imply that continuous glide/key-CV paths or post-rounding frequency
factors are quantized. Reset pitch60→firstnote48 remains part of this bounded
onset transient, not general legato qualification.

The frozen36 module was reused without a production build; no CMake configure/
build or CTest execution occurred. All production sources, prior scripts, the
shared helper and 28 previous contracts match the base. SDK/compiler/Python
and recorded source identities were captured before compilation and verified
after runtime. Host/module identities were captured before runtime and verified
afterward. The 108 retained local capture hashes are identities, **not baseline
comparisons**; no baseline replay was performed. Raw PCM and executable files
remain locally in ignored build directories, so committed evidence is not a
complete replay bundle. No broad, malformed, fuzz, negative, GUI, DAW, AU, CPU
or hardware tests were run.
