# Candidate VST3 envelope RELEASE TIME automation

Append rank0 filter/VCA release times at70/71 and rank1 at72/73, for74 lanes.
Only the production roster changes. Existing .001..20s range, skew.4, defaults,
IDs, DSP, helper, source masks and44 historical contracts remain unchanged.
Historical DECAY checks permit a larger runtime roster while retaining exact70
contract and complete prefix.

## Existing live time and effective floors

Engine.cpp213 refreshes release times using the maximum of rank release,
keyRelease and gated masterSustain*10, followed by existing scale/tolerance.
Note-off uses the same rule. Dsp.h `setTimes` changes coefficients without
replacing precise state, stage, elapsed time or anchors. PANEL13 accepts queues;
factory0/MEMORY11 preserve selected-source panel masking. No max-floor bypass or
ordinary rank gain smoothing is introduced.

## Planned release-window gate

Selected R starts.5s and holds0/3100, then takes physical .3/.6/.4/.7 seconds at
3327/3583/3839/4095 via existing skew conversion. Nonselected R=.5s, keyRelease
=.18s,masterSustain=0,neutral macros/envTimeScale1,dispersion0 and no held pedal
ensure selected minimum.3s dominates the effective floor. Both attacks.001s
and decays.002s,filterIL.5/AL.3 and VCA S.7 settle the held envelope before all
note-offs3072..3087. The filter releases from near0 toward−1.5, while VCA releases
from.7 toward0. Tail duration below.024s at44.1kHz and minimumR.3 give residual
gap factor greater than exp(-ln(1000)*.024/.3), approximately.575. Both envelopes
remain above completion thresholds and the nonselected.5s release retains an
audible path. This is bounded source reasoning, not calibrated release timing.

Other explicit settings: pulse1,saw0,sine0,noise0,PW.5/PWMdepth0,LPF1500Hz,
HPF25Hz,resonances0,VCFlevel.78,VCAlevel.35,mix.5,sustainMode0,neutral macros,
zero touch/LFO/modulation amounts. Default MIDI48 helper is unchanged.
Matched active holds require exact pre3100 PCM, positive PANEL effect in tail
frames[3100,4096), and full PCM masking equality for sources0/11. Scalar/five
partitions retain existing thresholds.

Four isolated lanes ×sources13/0/11 ×capacities8/16 (two/sixteen active notes)
×44.1/48/96kHz ×realtime/offline gives144 full cases:48 positive and96 masked.
With144 frozen70 compatibility cases this is2736 renders in12 processes and432
actual candidate74/frozen70 no-queue/Legacy/PURE byte pairs. Six CMake entries
use `cs80_vst3_envelope_release_automation_tests`, original host contract and
new `host-automation-envelope-release-v1.json`, timeout180 seconds.

Verified runtime results are recorded below. No complete release, calibrated hardware response,
simultaneous-lane, whole-voice, GUI or CPU qualification is claimed.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 144 full cases and 144 frozen-70 compatibility cases, totaling 2,736 renders. All 432 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or a historical full-suite execution.

All 144 pre-change prefixes through frame 3099 were exact. The maximum measured partition error was zero (limit 2e-6). PANEL effect ranges within the release-tail window [3100,4096) were 0.000112576–0.00106421 for rank 0 filter release, 0.000151519–0.00175437 for rank 0 VCA release, 0.000162158–0.00127732 for rank 1 filter release, and 0.000222927–0.00226979 for rank 1 VCA release. All 96 selected factory/MEMORY cases remained exactly masked over the full PCM output. This qualifies isolated live release-time updates above the configured floors in the bounded tail, not complete-release/Idle behavior, calibrated timing, state-reset equivalence, simultaneous-rank behavior or hardware equivalence.

[Verification and retained evidence](experiments/envelope-release-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 44 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Final result prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
