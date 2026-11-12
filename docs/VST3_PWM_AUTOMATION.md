# Rank PWM speed/depth automation

## Append-only candidate

Existing rank0 PWM SPEED/DEPTH append at50/51 and rank1 at52/53, for54 lanes.
The previous50 entries and parameter metadata remain fixed: speed.05–20Hz,
default.45Hz,skew.35; depth0–1,default0,linear conversion. Production changes
only the capability list. DSP,smoothing,source masks,UI and shared renderer
are unchanged,as are all37 old contracts. The historical AFTER TOUCH gate
retains exact contract50/full prefix while allowing a larger runtime roster.

## Live oscillator-control path

Both rank controls already have smoothing. Speed advances per-rank PWMphase
continuously; its sine is multiplied by depth*.35 and added to base pulsewidth
before the existing.05–.95 clamp. Modern WSC consumes the resulting inferred
pulse threshold. Neither amount nor speed is note-on latched. This does not
introduce or calibrate a new oscillator,threshold or hardware control law.

Existing source authority permits positive live queue witnesses only for
PANEL13. Factory0 and MEMORY11 are selected-source PCM masking checks,not
positive modulation cases. The source mask is not bypassed.

## Explicit excitation and bounded inputs

PANEL uses pulseON,sawOFF,sine/noise0,basePW.5,VCFlevel.78,VCAlevel.35,mix.5.
Other pulsewidth LFO/modulation paths are0. Initialspeed3Hz anddepth.6 hold
at0/128. Speed targets1/7/2/6/1.5/4Hz occur at511/1023/1791/2559/3583/4095;
depth targets.2/.8/.3/.7/.15/.5 use the same offsets. Physical speeds pass
through the existing skewed normalization,not a presumed linear mapping.
Depth<=.8 with PW.5 and no other PWM contribution keeps the nominal width
within.22–.78,away from the clamp. Each case automates one lane.

A matched active-constant comparator must yield exact PCM before128; later
PANEL PCM must differ positively while factory/MEMORY PCM stays exact.
Scalar and five alternative partitions retain original thresholds. Actual
positive effects remain required; source feasibility alone is not a pass.

Each of24 PANEL speed cases also compares queued speed with matchedhold
at selected-rank depth0. Phase still advances differently internally,but its
pulsewidth contribution is removed. This is PCM-output inertness only,not
phase-state or later-depth-change equivalence. It is not a depth-case control.

## Native plan and limits

`cs80_vst3_pwm_automation_tests` registers44.1/48/96kHz,realtime/offline,
standard CLI `MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE`
plus separate compatibility-only mode. Four isolated lanes,sources13/0/11 and
capacities8/16 give144 full cases:48positive and96masked. Unchanged default48
notes use two voices at capacity8 or16 at capacity16 over4096 frames.

The plan is2784 renders in12 processes:144full×13 (six no-queue/policy,six
scalar/partition,one matchedhold),48extra depth-zero control renders,and
144frozen50compatibility×6. All432 actual no-queue/Legacy/PURE pairs require
byte comparisons. No historical full-suite or helper rerun is included.

This is candidate scope,not verified results. Exact source/host/module hashes,
commands,errors/effects and comparisons require evidence and independentreview.
Registration/build alone is not runtime evidence. The finite startup window
does not qualify a full PWM/LFO cycle,steady-state response,alias performance
or hardware equivalence. Broad,malformed-input,fuzz,negative-baseline,GUI/DAW,
AU ramps,CPU/benchmark and release-wide qualification remain excluded.

## Verified local result

The candidate VST3 and supported host build completed with exit0. All12 direct
native runs passed:144 full cases and144 compatibility cases,2784 renders total.
All432 actual candidate54/frozen50 PCM pairs are byte-identical. Maximum partition
error was0 (unchanged threshold2e-6); all144 pre128 stereo prefixes were exact.
All96 factory/MEMORY whole-PCM mask comparisons were exact. The24 PANEL speed
at selected-rank depth0 comparisons were exact, separate from baseline pairs.
The48 PANEL cases were positive: lane50 .0195108–.054517, lane51 .0197524–.069304,
lane52 .0189907–.0516189 and lane53 .0199042–.0662175 (12 cases per lane).
No failed attempts or runtime retries occurred.

[Retained verification](experiments/pwm-automation/verification.json) records
commands, full logs, source/binary identities and actual capture hashes. SDK,
compiler, Python and source identities were recorded before compilation and
checked after runtime; new host and candidate/frozen modules were recorded before
runtime and checked afterward. Final documentation was updated only after that
provenance check. All37 old contracts, prior exporters, shared renderer and
production files other than the capability header remain byte-identical to the
base. The historical guard was reviewed but that historical suite was not run.
CMake registration is distinct from direct runs; no CTest execution is claimed.
Raw PCM and executables remain in ignored local build directories, so committed
evidence is not a complete replay bundle. Depth0 establishes PCM equality only,
not phase-state or later re-enable equivalence. No full-cycle or calibrated
hardware PWM claim follows. Independent final review gates publication.
