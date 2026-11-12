# Rank AFTER TOUCH amount automation

## Append-only candidate

Existing rank0 AFTER BRILLIANCE/LEVEL append at46/47, rank1 at48/49, for50
capability lanes. The previous46 entries,IDs,0–1 ranges,zero defaults and
linear conversion remain fixed. Only the production capability list changes.
DSP,smoothing,source masks,UI and shared renderer are unchanged, along with
all36 earlier contracts. The historical INITIAL TOUCH gate retains exact
contract46 and full prefix/order checks while accepting a larger runtime roster.

Existing rank-source authority permits live PANEL13 queues. Factory0 and
MEMORY11 remain selected-source PCM masking cases, not positive modulation
witnesses. This does not override programmer authority or qualify later
source switches.

## Supported pressure excitation and live amount

The unchanged helper's pressure option emits .7 polyPressure at128+key,
with matching note pitch/ID/channel after onsets17+key and before releases
3072+key. MPE,pressure curve and threshold are0. The positive held-key target
passes through existing controller smoothing and modern TouchCircuit's a
node recurrence with0.1s time constant. This is the modern pressure node,
not a newly assumed raw-pressure or calibrated hardware sensor law.

Existing smoothed AFTER BRILLIANCE multiplies that node in the cutoff term;
AFTER LEVEL multiplies it in LC gain. Amounts are live, not onset-latched.
Mix stays.5, PANEL VCA level.35 and all other initial/after touch amounts0.
The default MIDI48 cohort,velocity.75 and note renderer remain unchanged.

## Bounded comparisons

Initial amount.15 holds at0/256, after every pressure event at128..143.
Targets .6/.2/.7/.1/.5/.25 occur at511/1023/1791/2559/3583/4095. A matched
active-constant queue must preserve exact PCM before256. Later PANEL output
must differ positively; factory/MEMORY queued and matchedhold output must
stay exact. Scalar plus five alternative partitions retain original thresholds.
No threshold relaxation follows from source feasibility.

For each of48 PANEL cases, two extra renders disable pressure events and
compare queued against matched activeconstant amounts. With all other pressure
sources disabled, the reset pressure node remains0, so this is a justified
PCM-output inertness check. It does not assert internal-state equality or
behavior after future pressure arrives. It is not an extra factory/MEMORY gate.

`cs80_vst3_after_touch_automation_tests` registers44.1/48/96kHz,
realtime/offline, standard CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE` plus
separate compatibility-only mode. Four isolated lanes,sources13/0/11 and
capacities8/16 give144 full cases:48PANELpositive and96masked. Capacity8
uses two notes,capacity16 sixteen,over4096 frames.

The plan is2832 renders in12 processes:144 full cases with13 primary renders
(six no-queue/policy,six scalar/partition,one matchedhold),96 extra pressure-off
renders,and144 frozen46 compatibility cases with6 each. All432 actual
no-queue/Legacy/PURE capture pairs require byte comparisons. No historical
feature suite or helper regression rerun is included.

This is candidate scope,not passing evidence. Exact source/host/module hashes,
commands,errors/effects and comparisons require retained evidence and independent
review. Registration/build alone is not runtime evidence. Finite waveform
qualification does not calibrate touch acquisition or individual circuits.
Broad,malformed-input,fuzz,negative-baseline,GUI/DAW,AU ramps,CPU/benchmark,
hardware and release-wide qualification remain excluded.

## Verified local result

The candidate VST3 and supported host build completed with exit0. All12 direct
native runs passed:144 full cases and144 compatibility cases,2832 renders total.
All432 actual candidate50/frozen46 PCM pairs are byte-identical. Maximum partition
error was3.72529e-09 (unchanged threshold2e-6); all144 pre256 stereo prefixes
were exact. All96 factory/MEMORY whole-PCM mask comparisons were exact.
The48 PANEL pressure-OFF queued/hold comparisons were exact; these are additional
controls, not part of the432 baseline capture pairs. The48 PANEL pressure-ON
cases were positive: lane46 .000878016–.00431646, lane47 .0015175–.015816,
lane48 .000792487–.00381013 and lane49 .00203637–.0164303 (12 cases per lane).
No failed attempts or runtime retries occurred.

[Retained verification](experiments/after-touch-automation/verification.json)
records commands, full logs, source/binary identities and actual capture hashes.
SDK, compiler, Python and source identities were recorded before compilation
and checked after runtime; newly built host and candidate/frozen module identities
were recorded before runtime and checked afterward. Final documentation was
updated only after that provenance check. All36 old contracts, prior exporters,
shared renderer and production files other than the capability header remain
byte-identical to the base. The historical guard change was reviewed but that
historical suite was not executed here. CMake registration is distinct from
these direct runs; no CTest execution is claimed. Raw PCM and executables remain
in ignored local build directories, so the committed evidence is not a complete
replay bundle. Pressure-OFF checks establish output equality, not internal-state
or later-pressure equivalence; isolated amount checks are not simultaneous-rank
or calibrated hardware touch qualification. Independent final review gates publication.
