# Candidate VST3 envelope DECAY TIME automation

Append rank0 filter/VCA decay times at66/67 and rank1 at68/69, for70 lanes.
Only the production roster changes. Existing .001..20s range, skew.4, defaults,
IDs, DSP, helper, source masks and43 historical contracts remain unchanged.
Historical ATTACK checks allow a larger runtime roster while retaining its
exact66 contract and full prefix.

## Existing live coefficient and stage scope

Engine.cpp213 refreshes envelope times; both Dsp.h `setTimes` implementations
assign a/d/r without replacing precise state, stage, elapsed time or anchors.
This is neither ordinary rank gain smoothing nor target reanchoring. Selected
PANEL13 accepts panel queues; factory0/MEMORY11 keep existing masking.

Both attacks are.001s, selected decay starts.5s and nonselected decay is.2s.
Filter IL.5/AL.3 gives initial Decay gap.9 toward0; VCA S.3 gives gap.7 from1.
Neutral macros give envTimeScale1 and dispersion0 removes time-ratio changes.
Last onset32 plus the short attack ends before512 even at96kHz. Selected decay
stays at least.2s throughout normalized interpolation. Held duration below.070s
means remaining gap factor exceeds exp(-ln(1000)*.070/.2), approximately.089.
Filter/VCA gaps thus remain above.080/.062, far above completion thresholds
.0008/.0005. This source-derived window is not independent stage-time accuracy.

## Planned bounded gate

Hold selected D=.5s at0/512, then physical .2/.6/.3/.7/.25/.4 seconds at
767/1279/1919/2559/3071/4095 using existing skew normalization. Other explicit
settings: filter R.18,VCA R.5,keyRelease.18/masterSustain0,pulse1,saw0,sine0,
noise0,PW.5/PWMdepth0,LPF1500Hz/HPF25Hz,resonances0,VCFlevel.78,VCAlevel.35,
mix.5 and zero touch/LFO/modulation amounts. Default MIDI48 helper is unchanged.

Matched active holds require exact pre512 PCM and positive PANEL effect only
in held frames[512,3072); masked sources require full PCM equality. There is no
post-noteoff decay-selection claim. Scalar/five partitions retain thresholds.
Four isolated lanes ×sources13/0/11 ×capacities8/16 (two/sixteen active notes)
×44.1/48/96kHz ×realtime/offline gives144 full cases:48 positive and96 masked.
With144 frozen66 compatibility cases this is2736 renders in12 processes and432
actual candidate70/frozen66 no-queue/Legacy/PURE byte pairs.

Six CMake registrations use `cs80_vst3_envelope_decay_automation_tests`, original
host contract and new `host-automation-envelope-decay-v1.json`,timeout180 seconds.
Verified runtime results are recorded below. No complete decay response, hardware calibration,
simultaneous-lane, whole-voice, GUI or CPU qualification is claimed.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 144 full cases and 144 frozen-66 compatibility cases, totaling 2,736 renders. All 432 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or a historical full-suite execution.

All 144 pre-change prefixes through frame 511 were exact. The maximum measured partition error was zero (limit 2e-6). PANEL effect ranges within the held Decay window [512,3072) were 0.000343025–0.00137963 for rank 0 filter decay, 0.000703451–0.00626182 for rank 0 VCA decay, 0.000385639–0.00143132 for rank 1 filter decay, and 0.00104121–0.00493812 for rank 1 VCA decay. All 96 selected factory/MEMORY cases remained exactly masked over the full PCM output. This qualifies isolated live decay-time updates in the bounded held window, not complete-stage behavior, calibrated timing, state-reset equivalence, simultaneous-rank behavior or hardware equivalence.

[Verification and retained evidence](experiments/envelope-decay-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 43 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Final result prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
