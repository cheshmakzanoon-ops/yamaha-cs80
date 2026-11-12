# LOW keyboard-level automation

## Append-only candidate capability

Existing `P_kbdLevelLow` is appended at lane38, giving39 lanes while retaining
the entire previous38-lane roster. Its parameter ID, physical -1–1 range,
zero default and linear normalized conversion remain unchanged. Production
changes only the capability list; DSP, smoothing and UI are unchanged.
The shared test renderer gains an opt-in note-base argument, default48;
historical default event emissions are retained and checked separately. All31 earlier contracts remain intact. The historical initial-bend
gate accepts a larger runtime roster while retaining exact contract38 and
full prefix/order checks.

## Existing live contour path

The global lever is an existing smoothed field. Changes to smoothed contour
levers refresh the held per-voice KBC nodes, not just future key onsets.
All eight nodes load the shared buses in each engine. Factory ranks use the
existing own-node plus shared-bus LC receiver map; PANEL/MEMORY use the
existing direct KB-to-LC gain. This level contribution joins the existing
VCA control sum and clamping behavior.

This is not MIDI velocity automation or a new contour circuit equation.
Shared loading means no individual-key invariance is assumed. A waveform
difference does not independently calibrate the keyboard contour, each
receiver branch or hardware output.

## Bounded witness and plan

Physical0 (normalized .5) is held at samples0 and64. Subsequent physical
targets .4/-.3/.25/-.4/.3/-.2 occur at255/767/1535/2303/3583/4095 and use the
existing physical-to-normalized helper. Negative physical travel is not sent
as a negative host normalized value. HIGH level remains0 and other variation
is neutralized. A matched active-constant initial queue must produce identical
PCM before64 and a positive later aggregate difference. Scalar and five
alternative partition renders retain the original numerical threshold.
No unsupported inert-control or complete-keyboard-response claim is added.

`cs80_vst3_kbd_level_low_automation_tests` registers44.1/48/96kHz,
realtime/offline, with standard CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE` and
separate compatibility-only mode. Sources13/0/11 and capacities8/16 give36
full cases over4096 frames. This fixture opts into MIDI24/25 at capacity8
and MIDI24..39 at capacity16, all within the physical keyboard24..84.
They map to physical keys0..15, all sensitive to LOW. Historical helper
callers keep default MIDI48 onward.

The corrected feature matrix is684 renders in12 processes:36 full cases with13 renders each
(six no-queue/policy observations, six scalar/partition renders, one matched
active-constant comparator), plus36 frozen38 compatibility cases with6 each.
The108 actual no-queue/Legacy/PURE candidate-baseline capture pairs require
byte equality, not merely matching counts of identities.

This document describes candidate scope, not passing results. Exact source,
module/host hashes, commands, errors/effects and comparisons require retained
evidence and independent review. Compilation or registration is not runtime
evidence. Broad, malformed-input, fuzz, negative-baseline, GUI/DAW, AU ramps,
CPU/benchmark, hardware, full keyboard contour and release-wide qualification
remain excluded. The historical initial-bend host is run only in the
compatibility-only default-helper check described below, not its full suite.


## Retained first failure and corrected key cohort

The first candidate configuration failed all six positive-effect checks with
zero effect; execution stopped and that failure is retained, not counted as
passing evidence. The default MIDI48..63 cohort was insensitive to LOW with
HIGH fixed0: physical24 is the G2 reference endpoint,25..36 use G3 reference,
and37..39 use G4 HIGH-only interpolation. The earlier feasibility assessment
missed this key-decoder dependence. No threshold or production law is relaxed.

The corrected MIDI24..39 cohort excites LOW for every active note. In
`KbcContour.h` and the existing reference equation, physical0 uses `r-L`;
physical1..12 use `(r-L)*(1-m*1000/(33000+VR1))`, positive for taps0..11;
physical13..15 use `(2/3)*(r-L)*(1-m/11)`, positive for taps0..2.
The cold unused MIDI60 node stays neutral. Factory shared-bus loading sums
same-sign LOW contributions; it does not cancel this excitation. These are
source-model facts, not calibrated hardware keyboard measurements.

The correction appends `noteBase=48` to the supported renderer APIs. Only this
fixture opts into24; note-on/off and optional pressure identities use the
selected base consistently, including the two-cycle helper path. The literal
historical default note branch remains in place.

An additional432 renders in12 compatibility-only processes compare the rebuilt
historical initial-bend host with its frozen predecessor on the SAME frozen38
module, checking108 actual default-cohort capture pairs. Together with the
corrected feature matrix this is1116 renders,24 processes and216 actual pairs.
The new39 candidate module is not used as that historical frozen input.
This bounded default-helper check does not claim every historical suite ran.
Corrected runtime results are recorded below.

## Verified corrected local result

The corrected build completed with exit0. All24 corrected direct native processes
passed, totaling1116 renders:36 full LOW cases,36 lower-cohort compatibility cases
and72 default-helper compatibility cases. The108 candidate39/frozen38 lower-cohort
PCM pairs and108 rebuilt/frozen historical-host default-cohort pairs are exact.
LOW maximum partition error was0 (unchanged threshold2e-6); positive effects
ranged from .00382685 to .0285558 and all36 pre64 prefixes were exact.
These passing results do not erase the first attempt: its six positive-effect
failures and original source/host/module identities are retained separately.
No threshold or production DSP change was made to obtain the corrected result.

[Retained verification](experiments/kbd-level-low-automation/verification.json)
contains commands, complete logs, actual capture hashes and provenance. Before-build
SDK/compiler/Python/code identities were checked after all24 processes. Runtime
host/module identities were checked before and after their runs. A documentation-only
scope correction was reported after build start; its exact old/new hashes and timing
uncertainty are explicitly recorded, not retroactively rewritten into the before
snapshot. All executable inputs were unchanged during the matrix. Final result prose
was added after runtime. All31 previous contracts, prior exporters and production
files other than the capability header remain byte-identical to the base. The helper
change is the reviewed optional noteBase48, with explicit24 only in the LOW fixture.

No CTest, broad suite, isolated-key or hardware calibration is claimed. The historical
host comparison is compatibility-only, not its full feature gate. Raw PCM and native
binaries, including the failed-attempt binaries, stay in ignored local build directories;
the retained textual evidence is not a complete replay bundle. Final independent
binary/evidence review gates publication.
