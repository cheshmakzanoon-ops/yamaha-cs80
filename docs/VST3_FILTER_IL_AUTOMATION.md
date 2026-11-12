# Candidate VST3 filter EG IL automation

Append rank0/rank1 CP_fIl at lanes58/59, for60 lanes. The sole production change
is the roster; metadata range0..1/default.5/linear, IDs, DSP, shared renderer,
source masks and40 historical contracts remain unchanged. The historical VCA
SUSTAIN fixture permits a larger runtime roster but retains its exact58 contract
and complete prefix checks.

## Existing stage-selected target

Engine.cpp563 publishes both filter target ports to modern circuits. IL maps
through `filterInitialOffset` to the existing negative provisional octave range.
Dsp.h811 stores IL without replacing precise state or restarting a stage.
Release approaches IL with the existing release coefficient; selected completed
release Idle also follows IL, but this gate does not qualify that Idle behavior.
This is not ordinary rank smoothing. The attack target AL and grounded held
target remain unchanged. Engine.cpp2943–2953 applies envelope influence to LP
and provisional half-strength HP cutoff paths; no calibrated hardware law is
claimed. Existing non-PANEL rank masking remains intact.

## Planned bounded gate

Two isolated lanes, sources13/0/11, capacities8/16 (two/sixteen active notes),
44.1/48/96kHz and realtime/offline produce72 full cases:24 PANEL positive and48
factory/MEMORY masked. Shared helper/default MIDI48 notes remain unchanged;
all note-offs occur at3072..3087. IL starts.2 and holds0/3100, then takes
.6/.3/.7/.15 at3327/3583/3839/4095 via existing normalization.

Explicit PANEL filter A.001s,D.002s,AL.3,R.18s, keyRelease.18s, masterSustain0
establish a settled held filter before note-off and a moving release target.
VCA A.001s,D.002s,S.7,R.5s keeps the finite tail audible. Neutral macros give
envTimeScale1; dispersion0 avoids time-ratio changes. Pulse1,saw0,sine0,noise0,
PW.5,PWMdepth0,LPF1500Hz,HPF25Hz,resonances0,VCFlevel.78,VCAlevel.35,mix.5,
and zero touch/LFO/modulation amounts provide a bounded sounding path.

Matched active constant queues require exact PCM before3100 and positive later
PANEL tail effect; source0/11 require exact selected-source PCM masking. Scalar
and five partitions retain existing tolerances. The1368-render/12-process plan
also includes72 frozen58 compatibility cases and216 actual no-queue/Legacy/PURE
candidate60 versus frozen58 byte pairs. Six CMake registrations use
`cs80_vst3_filter_il_automation_tests`, original contract and new
`host-automation-filter-il-v1.json`, timeout180 seconds.

Verified runtime results are recorded below. This gate does not independently establish
stage timing, completed Idle, full release response, simultaneous two-lane
behavior, hardware fidelity, GUI or CPU qualification.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 72 full cases and 72 frozen-58 compatibility cases, totaling 1,368 renders. All 216 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or a historical full-suite execution.

All 72 pre-change prefixes through frame 3099 were exact. The maximum measured partition error was zero (limit 2e-6). PANEL later-tail effect ranges were 0.000830489–0.00814152 for rank 0 and 0.00152308–0.00845705 for rank 1. All 48 selected factory/MEMORY cases remained exactly masked. This qualifies isolated live IL changes during the bounded release tail, not completed-release/Idle behavior, the full release trajectory, calibrated timing, simultaneous-rank behavior or hardware equivalence.

[Verification and retained evidence](experiments/filter-il-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 40 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Final result prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
