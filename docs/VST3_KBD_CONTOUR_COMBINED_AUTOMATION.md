# Paired LEVEL + BRILLIANCE contour qualification

## Existing capability, interacting paths

This test-only candidate combines two existing controls at a time: LOW LEVEL
and BRILLIANCE (lanes38/40), or HIGH LEVEL and BRILLIANCE (39/41). It preserves
the full42-lane roster, production, metadata, DSP, smoothing, UI, shared
renderer and all34 earlier contracts. It does not add a parameter or qualify
all four levers acting simultaneously.

LEVEL's smoothed KBC2 nodes enter LC gain; BRILLIANCE's smoothed KBC1 nodes
enter distinct HP/LP cutoff controls. Factory own/shared loading and
PANEL/MEMORY direct maps remain unchanged. The gate measures their combined
rendered response, not each lane's independent contribution or calibrated
circuit interaction.

## Responsive cohorts and trajectories

LOW uses existing helper `noteBase=24`: notes24/25 at capacity8,24..39 at
capacity16. HIGH uses62: notes62/63 or62..77. These are the source-proven
responsive cohorts documented in [LOW level](VST3_KBD_LEVEL_LOW_AUTOMATION.md),
[HIGH level](VST3_KBD_LEVEL_HIGH_AUTOMATION.md) and
[BRILLIANCE](VST3_KBD_BRILLIANCE_AUTOMATION.md). Every active note has nonzero
same-sign sensitivity to its tested side; opposite-side levers remain0.
Shared loading means no individual-key invariance is assumed.

Both queues hold physical0 (normalized .5) at samples0/64. LEVEL follows
.4/-.3/.25/-.4/.3/-.2 at255/767/1535/2303/3583/4095. BRILLIANCE follows the
distinct curve .3/-.25/.4/-.2/.35/-.3 at383/895/1663/2431/3711/4095.
Signed values use the existing normalization helper.

A matched **two-active-constant** comparator has identical initial values
and publication paths. PCM before64 must match exactly; a positive later
aggregate effect is required without attributing it to each lane. Scalar and
five alternative partitions retain original thresholds. Source excitation is
not a guarantee that rendered output clears the threshold; any failure must
be retained. No unsupported inert control is added.

## Bounded native plan

`cs80_vst3_kbd_contour_combined_automation_tests` registers44.1/48/96kHz,
realtime/offline, standard CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE`.
Two side-pairs, sources13/0/11 and capacities8/16 give72 cases over4096 frames.
The plan is936 renders in six direct processes,13 per case: six no-queue/policy
observations,scalar plus five partitions,and one matched two-activeconstant.
The216 retained capture identities are **not baseline comparisons**.

The frozen42 module is reused and only the supported host is directly compiled.
CMake registrations support future use but do not mean a full CMake build or
CTest run occurred. No production build, helper change or historical rerun
is included. Exact host/module/source hashes,commands,errors/effects and all
outcomes require retained evidence and independent review before verification.
This is candidate scope, not a passing result.

This finite startup/transient gate does not establish whole-keyboard response,
all-four simultaneous operation,individual circuit contribution,hardware
calibration,broad integration or release qualification. Malformed-input,fuzz,
negative-baseline,GUI/DAW,AU ramps and CPU/benchmark tests remain excluded.

## Verified local result

The direct supported-host compilation completed with exit0. All six direct native
runs passed:72 full cases and936 renders. There are216 retained capture identities,
not baseline comparisons. Both sides had maximum partition error0 (unchanged
threshold2e-6). LOW aggregate effects ranged .0165838–.0650504; HIGH ranged
.00197661–.0545489, with36 cases each. All72 pre64 stereo prefixes were exact.
No failed attempts or runtime retries occurred.

[Retained verification](experiments/kbd-contour-combined-automation/verification.json)
records commands, full logs, source/binary identities and actual capture hashes.
SDK, compiler, Python and source identities were recorded before compilation
and checked after runtime; the new host and reused frozen42 module were hashed
before runtime and checked afterward. Final result documentation was updated
only after that provenance check. Production, shared helper, prior exporters and
all34 old contracts remain byte-identical to the base. No production rebuild,
CMake configuration or CTest execution occurred. Raw PCM and executable files
remain in ignored local build directories, so the committed evidence is not a
complete replay bundle. Effects are aggregate two-control witnesses, not per-lane
attribution or simultaneous four-lever/hardware qualification. Independent final
binary/evidence review gates publication.
