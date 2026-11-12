# Initial pitch-bend amount automation

## Append-only candidate

This candidate appends existing `P_initBend` at lane37, for38 capability lanes.
The entire preceding37-lane roster, parameter IDs/order, range0–1, zero default
and linear conversion are preserved. The sole production change is the
capability list. DSP, existing smoothing, TouchCircuit, UI and shared renderer
remain unchanged, as do all30 old contracts. The historical master-sustain
gate accepts a larger runtime roster while retaining its exact contract37
and full original prefix assertions.

## Live amount, existing transient

The global route already publishes initial-bend amount to both engines. It
is explicitly in the existing smoothed-control list; modern internal samples
advance that smoothing before applying the amount to oscillator pitch.
Although Note On initializes a negative velocity transient, modern processing
then obtains `initialPitchTransient` from the TouchCircuit acquisition,
high-pass and clamp path. It is not the simple exponential decay used by the
legacy path. Each rank adds the product of smoothed amount and this transient
to its existing aggregate oscillator pitch.

The amount is therefore live, not latched only at note onset. This extension
neither changes that source model nor calibrates a hardware knob, capacitor,
pitch curve or complete MIDI-to-hardware touch mapping.

## Bounded witness

Initial amount .2 is held at samples0 and64, covering all existing note onsets
at17..32. The finite subsequent targets are .8@255, .3@767, .9@1535,
.15@2303, .65@3583 and .4@4095. A matched active-constant queue uses the same
initial value and publication path. The PCM prefix before64 must remain
exactly equal; a positive later waveform difference is an aggregate audible
path witness, not a measurement of transient shape or pitch accuracy.

The scalar reference and five alternative partitions retain the original
numerical threshold. Glide mode0 and portamento0 avoid introducing glide or
quantized-glissando behavior. Other variation is neutralized. There is no
additional inert-control claim: no fixed footswitch gate removes this term.

## Native qualification plan

`cs80_vst3_initial_bend_automation_tests` registers44.1/48/96kHz,
realtime/offline, using the standard CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE` and
separate compatibility-only mode. Sources13/0/11 and capacities8/16 give36
full cases. The unchanged4096-frame helper uses two notes at capacity8 and
sixteen at capacity16.

The plan is684 renders in12 processes:36 full cases with13 renders each
(six no-queue/policy observations, six scalar/partition renders and one matched
active-constant control), plus36 frozen37 compatibility cases with6 each.
The108 candidate/frozen37 no-queue and Legacy/PURE capture pairs require
actual byte comparisons, not just identity counts.

Registration or compilation alone is not runtime qualification. Broad, malformed-input, fuzz, negative-baseline, GUI/DAW,
AU ramps, CPU/benchmark, hardware, pitch-calibration and release-wide tests
remain excluded. No historical fixture rerun is implied.

## Verified local result

The candidate VST3 and supported host build completed with exit0. All12 direct
native runs passed:36 full cases and36 compatibility cases,684 renders total.
The108 actual candidate/frozen37 PCM pairs are byte-identical. Maximum observed
partition error was1.49012e-08 (unchanged threshold2e-6). Later positive effects
ranged from .0561151 to .625419; all36 pre64 stereo prefixes were exact.
No failed attempts or runtime retries occurred. There is no bypass-control claim.

[Retained verification](experiments/initial-bend-automation/verification.json)
records commands, full logs, source/binary identities and actual capture hashes.
SDK, compiler, Python and source identities were recorded before compilation
and checked after runtime; newly built host and candidate/frozen module identities
were recorded before runtime and checked afterward. Final documentation was
updated only after that provenance check. All30 old contracts, prior exporters,
shared renderer and production files other than the capability header remain
byte-identical to the base. The historical master-sustain guard change was reviewed
but that historical suite was not executed here. CMake registration is distinct
from these direct runs; no CTest execution is claimed. Raw PCM and executables
remain in the local ignored build directory, so the committed evidence is not
a complete replay bundle. These are aggregate waveform checks, not an isolated
TWS/pitch calibration or hardware timing qualification. Independent final evidence
review gates publication.
