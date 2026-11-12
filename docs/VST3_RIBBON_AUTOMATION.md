# Candidate VST3 RIBBON automation

Append existing global P_ribbon at lane78, for79 lanes. Only the production
roster changes. Existing −1..1/default0/linear metadata, IDs, DSP, shared helper
and47 historical contracts remain unchanged. Historical SINE/NOISE and combined
D/R fixtures permit larger runtime rosters while retaining exact78 contracts
and complete prefix checks.

## Existing controller path

Engine.cpp2456 smooths the sum of external ribbon and P_ribbon using the existing
controller dezipper. Temporary compensation2488 and restoration2518 preserve
the event-set target between samples. Held voices2828 update the existing
`1 + clamp(displacement,-1,1)` ratio (Dsp.h150), used in oscillator frequency2883.
This parameter is global for sources13/0/11; no rank mask is bypassed. Filter key
CV excludes ribbon in the current source, but this PCM gate asserts no filter
invariant. Exact physical travel and pitch-transfer calibration remain unmeasured.

## Planned held-note scope

External ribbon starts reset0; ribbon CC adapter, MPE, portamento, initial bend
and pedal holds are disabled, with static sustainMode0. Physical displacement0
holds at0/128, then.2/−.15/.3/−.2/.1/−.1 at511/1023/1791/2559/3583/4095 via existing
normalization (physical0 is normalized.5). Interior ratios.8..1.3 avoid endpoint
zero-frequency/clamp cases. No CC-adapter, released-tail or SustainII claim follows.

Explicit PANEL settings: pulse1,saw0,sine0,noise0,PW.5/PWMdepth0,LPF4000Hz,
HPF25Hz,resonances0,filterIL/AL0,A.001/D.002,VCA A.001/D.002/S.7/R.5,
VCFlevel.78,VCAlevel.35,mix.5,neutral macros/dispersion/touch/LFO/modulation.
Unchanged default MIDI48 helper is used. Matched active holds require exact
pre128 PCM and positive held effect only in frames[128,3072). The queue continues
later, but this is not a separate release-latching or tail-response proof.
Scalar/five block partitions retain the original thresholds.

One global lane ×sources13/0/11 ×capacities8/16 (two/sixteen active notes)
×44.1/48/96kHz ×realtime/offline gives36 full positive cases. With36 frozen78
compatibility cases:684 renders in12 processes,108 actual candidate79/frozen78
no-queue/Legacy/PURE byte pairs. Six CMake entries use
`cs80_vst3_ribbon_automation_tests`, original host contract and new
`host-automation-ribbon-v1.json`,timeout180 seconds.

Verified runtime results are recorded below. No calibrated pitch, filter invariant, tail/SustainII,
hardware, GUI, CPU or whole-project completion claim is made.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 36 full cases and 36 frozen-78 compatibility cases, totaling 684 renders. All 108 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or historical full-suite executions.

All 36 pre-change prefixes through frame 127 were exact. The maximum measured partition error was 5.96046e-8, within the unchanged 2e-6 limit. All selected PANEL/factory/MEMORY sources had positive held-window [128,3072) effects, ranging 0.0276681–0.550055. This qualifies the global host ribbon parameter in the bounded held-note fixture, not release-latching, Sustain II behavior, the MIDI CC adapter, a filter invariant, physical resistance law or calibrated pitch response.

[Verification and retained evidence](experiments/ribbon-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 47 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Both changed historical guard sources are included in provenance. Final prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
