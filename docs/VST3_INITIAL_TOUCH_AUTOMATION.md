# Rank INITIAL TOUCH amount automation

## Append-only candidate

Four existing rank controls are appended after the unchanged42-lane prefix:
rank0 INITIAL BRILLIANCE/LEVEL at42/43 and rank1 at44/45, for46 lanes.
IDs, physical0–1 ranges, zero defaults and linear conversion remain unchanged.
Only the production capability list changes. DSP,smoothing,source routing,UI
and shared renderer are unchanged, as are all35 old contracts. Historical
BRILLIANCE/combined-contour gates retain exact contract42/full prefix checks
while allowing a larger runtime roster.

## Live amounts and source authority

The rank amounts are existing smoothed controls. Modern `touchVelocity` is
TouchCircuit's acquired/held b node from the supported note strike, not an
assumed constant velocity or a newly calibrated sensor law. INITIAL BRILLIANCE
multiplies b in the existing cutoff contribution; INITIAL LEVEL multiplies b
in the existing LC sum. Amounts remain live after onset, not note-latched.

The generic rank automation guard masks every non-PANEL source except the
separately supported pan control. Thus PANEL13 has positive live-amount
witnesses; factory0 and MEMORY11 have **selected-source PCM masking** checks.
Those checks do not claim actual factory/MEMORY amount modulation, nor does
this extension override stored source authority or change later source switching.
Factory touch behavior retains its existing compiled source boundary.

## Bounded inputs and comparisons

Each case automates one of the four lanes. Mix stays.5 so both ranks contribute;
PANEL VCA level is.35 to leave LC headroom. Other initial/after touch amounts
are0. The helper is unchanged: default MIDI48 onward, velocity.75, no pressure
extension, two notes at capacity8 or sixteen at capacity16,4096 frames.

Initial amount.15 holds at0/128, then targets.6/.2/.7/.1/.5/.25 occur at
511/1023/1791/2559/3583/4095. A matched active-constant queue uses identical
initial paths. Prefix PCM before128 must be exact. Later PANEL PCM must show
a positive aggregate effect; factory/MEMORY queued and matchedhold PCM must
remain exact. Source analysis does not guarantee a measured threshold: any
failure is retained, not removed by relaxing the witness. Scalar plus five
alternative partitions retain the original numerical threshold.

`cs80_vst3_initial_touch_automation_tests` registers44.1/48/96kHz,
realtime/offline, with standard CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE` and
separate compatibility-only mode. Four isolated lanes, sources13/0/11 and
capacities8/16 give144 full cases:48PANEL positive and96 masked-source cases.

The plan is2736 renders in12 processes:144 full cases with13 renders each
(six no-queue/policy observations,six scalar/partition renders,one matchedhold),
plus144 frozen42 compatibility cases with6 each. All432 actual no-queue/
Legacy/PURE capture pairs require byte comparisons. No historical feature
suite or helper regression rerun is implied.

Registration/build alone is not runtime evidence. This finite waveform
scope does not calibrate touch acquisition,individual circuits,hardware or
complete source switching. Broad,malformed-input,fuzz,negative-baseline,GUI/DAW,
AU ramps,CPU/benchmark and release-wide qualification remain excluded.

## Verified local result

The candidate VST3 and supported host build completed with exit0. All12 direct
native runs passed:144 full cases and144 compatibility cases,2736 renders total.
All432 actual candidate46/frozen42 PCM pairs are byte-identical. Maximum partition
error was0 (unchanged threshold2e-6); all144 pre128 stereo prefixes were exact.
All96 factory/MEMORY whole-PCM mask comparisons were exact. The48 PANEL cases
were positive: lane42 effects .00620968–.0168474, lane43 .0106065–.0535502,
lane44 .00599171–.0143468 and lane45 .014335–.0573346 (12 cases per lane).
No failed attempts or runtime retries occurred.

[Retained verification](experiments/initial-touch-automation/verification.json)
records commands, full logs, source/binary identities and actual capture hashes.
SDK, compiler, Python and source identities were recorded before compilation
and checked after runtime; newly built host and candidate/frozen module identities
were recorded before runtime and checked afterward. Final documentation was
updated only after that provenance check. All35 old contracts, prior exporters,
shared renderer and production files other than the capability header remain
byte-identical to the base. Historical guard changes were reviewed but those
historical suites were not executed here. CMake registration is distinct from
these direct runs; no CTest execution is claimed. Raw PCM and executables remain
in ignored local build directories, so the committed evidence is not a complete
replay bundle. These isolated amount checks do not qualify simultaneous-rank
interactions or calibrated hardware touch response. Independent final review
gates publication.
