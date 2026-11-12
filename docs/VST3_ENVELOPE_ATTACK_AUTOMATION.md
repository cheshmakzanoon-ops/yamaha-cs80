# Candidate VST3 envelope ATTACK TIME automation

Append rank0 filter/VCA attack times at62/63 and rank1 at64/65, for66 lanes.
Only the production roster changes. Existing .001..20s range, skew.4, defaults,
IDs, DSP, helper, source masks and42 historical contracts remain unchanged.
Historical AL checks allow a larger runtime roster while retaining the exact62
contract and full prefix.

## Live time, not target reanchoring

Engine.cpp213 `refreshEnvelopeTimes` updates active VCA and all filter circuits.
Both Dsp.h `setTimes` implementations only assign a/d/r: precise state, stage,
elapsed time and attack anchor remain unchanged. This differs from changed AL
reanchoring and is not ordinary rank gain smoothing. Fixed-target modern attack
recurrences use the current attack coefficient. PANEL13 accepts queues; selected
factory0/MEMORY11 retain existing panel-control masking.

## Planned stage-bounded gate

Each selected attack time starts.5s, holds0/128, then takes physical seconds
.2/.6/.3/.7/.25/.4 at511/1023/1791/2559/3071/4095 through existing skew conversion.
The nonselected envelope attack is.001s. Filter IL.5/AL.3/D.2/R.18 and VCA
D.002/S.7/R.5 keep targets fixed. Neutral macros give envTimeScale1; dispersion0
removes time-ratio variation. Effective selected A remains at least.2s even
between normalized knots. The longest held interval is below.070s at44.1kHz,
so accumulated integral dt/A is below.35, short of the unit comparator threshold.
Thus the selected envelope remains in Attack during the bounded witness window.
This source argument is not an independent calibrated attack-timing measurement.

Other explicit settings: pulse1,saw0,sine0,noise0,PW.5,PWMdepth0,LPF1500Hz,
HPF25Hz,resonances0,VCFlevel.78,VCAlevel.35,mix.5,keyRelease.18,masterSustain0,
zero touch/LFO/modulation amounts. Default MIDI48 helper remains unchanged.
Matched active constant queues require exact pre128 PCM, positive PANEL effect
only in frames[128,3072), and full selected-source PCM equality for masked cases.
No post-noteoff attack-selection claim is made. Scalar/five partitions retain
existing thresholds.

Four isolated lanes ×sources13/0/11 ×capacities8/16 (two/sixteen active notes)
×44.1/48/96kHz ×realtime/offline gives144 full cases:48 positive and96 masked.
With144 frozen62 compatibility cases:2736 renders in12 processes,432 actual
candidate66/frozen62 no-queue/Legacy/PURE byte pairs. Six CMake registrations
use `cs80_vst3_envelope_attack_automation_tests`, original host contract and new
`host-automation-envelope-attack-v1.json`, timeout180 seconds.

Verified runtime results are recorded below. No complete attack, calibrated hardware response,
simultaneous-lane, whole-voice, GUI or CPU qualification is claimed.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 144 full cases and 144 frozen-62 compatibility cases, totaling 2,736 renders. All 432 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or a historical full-suite execution.

All 144 pre-change prefixes through frame 127 were exact. The maximum measured partition error was zero (limit 2e-6). PANEL effect ranges within the held window [128,3072) were 0.000208229–0.00499755 for rank 0 filter attack, 0.000407298–0.00494455 for rank 0 VCA attack, 0.000436984–0.00340163 for rank 1 filter attack, and 0.000568775–0.00380111 for rank 1 VCA attack. All 96 selected factory/MEMORY cases remained exactly masked over the full PCM output. This qualifies isolated live attack-time updates in the bounded held window, not full-stage behavior, calibrated timing, state-reset equivalence, simultaneous-rank behavior or hardware equivalence.

[Verification and retained evidence](experiments/envelope-attack-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 42 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Final result prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
