# Candidate VST3 filter EG AL automation

Append existing rank0/rank1 CP_fAl at lanes60/61, for62 lanes. Only the production
roster changes; IDs, range0..1/default.55/linear conversion, DSP, source masks,
shared renderer and41 historical contracts remain unchanged. Historical IL
runtime checks allow a larger roster while retaining exact60 contract and prefix.

## Attack-selected live target

Engine.cpp563 publishes filter target ports. Dsp.h811 changes AL during Attack
by setting attackFrom to the current precise value and restarting preciseTime,
while preserving capacitor state and stage. This is not ordinary rank smoothing.
AL maps through the existing positive provisional octave scale. Other stages
store AL rather than treating the update as a fresh trigger. PANEL13 receives
queues; factory0/MEMORY11 retain the existing selected-source panel mask.

## Planned bounded gate

PANEL filter A=.5s,IL=.5 (−1.5 provisional octaves),initial AL=.2 (+.6) establishes
a target above the default cold0 or IL start. Hold AL at0/128, then increase
monotonically to.3/.4/.5/.6/.7/.8 at511/1023/1791/2559/3071/4095.
Neutral macros give envTimeScale1 and dispersion0 removes time-ratio variation.
First changes occur before fixed-target attack completion; the longest held
interval is under.070s at44.1kHz, below.5s. Increasing targets avoid a downward
crossing, and each changed target reanchors from current state. This is a
source-derived bounded excitation argument, not measured stage-timing accuracy.

Other explicit PANEL settings: filter D.2/R.18, keyRelease.18/masterSustain0;
VCA A.001/D.002/S.7/R.5; pulse1,saw0,sine0,noise0,PW.5,PWMdepth0;
LPF1500Hz/HPF25Hz,resonances0,VCFlevel.78,VCAlevel.35,mix.5 and zero touch/LFO/
modulation amounts. Unchanged default MIDI48 helper onsets17+key and offs3072+key
are used. Positive PANEL effects against matched active holds are measured only
in frames[128,3072); pre128 PCM is exact. No claim of AL selection after note-off
is made. Factory/MEMORY cases require full selected-source PCM equality.

Two isolated lanes ×sources13/0/11 ×capacities8/16 (two/sixteen active notes)
×44.1/48/96kHz ×realtime/offline gives72 full cases:24 positive and48 masked.
With72 frozen60 compatibility cases this is1368 renders in12 processes and216
actual candidate62/frozen60 no-queue/Legacy/PURE byte pairs. Scalar/five block
partitions retain original tolerances. Six CMake registrations use
`cs80_vst3_filter_al_automation_tests`, original host contract and new
`host-automation-filter-al-v1.json`, timeout180 seconds.

Verified runtime results are recorded below. No full attack, calibrated contour law,
hardware, simultaneous-lane, GUI or CPU qualification is claimed.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 72 full cases and 72 frozen-60 compatibility cases, totaling 1,368 renders. All 216 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or a historical full-suite execution.

All 72 pre-change prefixes through frame 127 were exact. The maximum measured partition error was zero (limit 2e-6). PANEL effect ranges within the held window [128,3072) were 0.000119767–0.00438854 for rank 0 and 0.000369615–0.00325018 for rank 1. All 48 selected factory/MEMORY cases remained exactly masked over the full PCM output. This qualifies isolated live AL updates in the bounded held window, not post-note-off AL selection, full-stage behavior, calibrated timing, simultaneous-rank behavior or hardware equivalence.

[Verification and retained evidence](experiments/filter-al-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 41 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Final result prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
