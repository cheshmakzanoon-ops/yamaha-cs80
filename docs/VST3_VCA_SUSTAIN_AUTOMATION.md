# Candidate VST3 VCA envelope SUSTAIN automation

Append existing rank0/rank1 CP_vS at lanes56/57, for58 lanes. The sole production
change is the automation roster. Parameter IDs, range0..1, default.78, linear
conversion, DSP, source authority, shared renderer and39 historical contracts
remain unchanged. Historical VCF LEVEL checks retain the exact56 contract and
full prefix while allowing a larger runtime roster.

## Existing live-target semantics

Engine.cpp562 publishes S to active modern amplitude envelopes through
`setSustain`. This is not an ordinary smoothed rank gain. Dsp.h554 preserves
stored envelope state and stage: Attack stores S for Decay, Decay/Sustain use
the existing decay coefficient to approach S, and Release/Idle ignore S.
The precise ADSR recurrence remains unchanged. PANEL13 receives panel queues;
factory0/MEMORY11 keep their existing masked rank-control authority. These
engineering-inferred target laws are not hardware calibration.

## Bounded candidate gate

Each lane is isolated across sources13/0/11, capacities8/16 (two/sixteen active
notes),44.1/48/96kHz and realtime/offline. Unchanged helper notes start at MIDI48.
PANEL A=.001s,D=.002s,envTimeScale=1,dispersion=0,initial S=.5 provide an attack
and decay settling interval of about.003s, shorter than the minimum.005s from
the last initial onset32 to hold endpoint512 at96kHz. This source-derived
excitation argument is not an independent stage-timing accuracy assertion.

Hold S=.5 at0/512, then.2/.8/.3/.7/.15/.6 at767/1279/1919/2559/3071/4095,
using existing normalization. Explicit pulse1,saw0,sine0,noise0,PW.5,PWMdepth0,
VCAlevel.35,mix.5,neutral macros and zero touch/LFO/modulation amounts isolate
an ordinary sounding PANEL path. Matched active constant queues require exact
pre512 PCM and positive later PANEL effect; factory/MEMORY require exact
selected-source PCM masking. Scalar/five partitions retain existing tolerances.
The late curve after note-off is not a separate release-only invariant proof.

Planned scope:72 full cases (24 PANEL positive,48 masked),72 compatibility
cases,1,368 renders in12 processes;216 actual candidate58/frozen56 no-queue
and Legacy/PURE byte pairs. Six CMake registrations use
`cs80_vst3_vca_sustain_automation_tests`, original host contract and new
`host-automation-vca-sustain-v1.json`, timeout180 seconds. No runtime outcome
is claimed before qualification. No whole-voice, simultaneous-lane, calibrated
release/envelope, hardware, GUI or CPU qualification follows from this gate.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 72 full cases and 72 frozen-56 compatibility cases, totaling 1,368 renders. All 216 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or a historical full-suite execution.

All 72 pre-change prefixes were exact. The maximum measured partition error was zero (limit 2e-6). PANEL effect ranges were 0.00418879–0.0208264 for rank 0 and 0.00459032–0.0226942 for rank 1. All 48 selected factory/MEMORY cases remained exactly masked. This qualifies isolated live sustain-target updates through the existing envelope law in this bounded window, not an ordinary smoothed-gain law, envelope-stage timing accuracy, a release-only invariant, simultaneous-rank behavior or hardware equivalence.

[Verification and retained evidence](experiments/vca-sustain-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 39 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Final result prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
