# Candidate VST3 SINE/NOISE level automation

Append rank0 SINE/NOISE levels at74/75 and rank1 at76/77, for78 lanes. Only the
production roster changes. Existing0..1/default0/linear metadata, IDs, DSP,
shared renderer, source masks and45 historical contracts remain unchanged.
Historical RELEASE checks permit a larger runtime roster while retaining their
exact74 contract and complete prefix. This addresses existing rank continuous
gain controls, not whole-project or hardware completion.

## Existing distinct audio paths

Engine.cpp37 includes sine/noise in rank smoothing;786/787 reads PANEL levels,
and selected source tones override them at831/832. Noise2918 multiplies the
existing shared circuit panel-noise sample by the rank level and.8 before
filters. The existing shared/random draw sequence remains unchanged. Sine3004
multiplies the oscillator sine and is added after filtered-path gain3042/3128.
PANEL13 receives panel queues; factory0/MEMORY11 retain selected-source masking.
No source-route or oscillator/noise generator change is introduced.

## Planned isolated qualification

Only the selected rank and selected SINE/NOISE control starts at.2; all other
sine/noise levels are0. Hold0/128 then.6/.3/.7/.15/.5/.25 at
511/1023/1791/2559/3583/4095 using existing normalization. Explicit saw/pulse0,
PW.5/PWMdepth0,LPF4000Hz/HPF25Hz,resonances0,filterIL0/AL0/A.001/D.002,
VCA A.001/D.002/S.7/R.5,VCFlevel.78,VCAlevel.35,mix.5 and neutral macros/
dispersion/touch/LFO/modulation amounts provide the fixed sounding path.
The unchanged default MIDI48 helper and deterministic reset support PCM
comparisons; these are not spectral or statistical noise-quality measurements.

Matched active holds require exact pre128 PCM, positive PANEL effect in held
frames[128,3072), and full PCM masking equality for sources0/11. Scalar/five
partitions retain original thresholds. No true-zero filter-output or sine-only
inertness claim is made; circuit offsets/transients need not vanish.

Four isolated lanes ×sources13/0/11 ×capacities8/16 (two/sixteen active notes)
×44.1/48/96kHz ×realtime/offline gives144 full cases:48 positive and96 masked.
With144 frozen74 compatibility cases this is2736 renders in12 processes and432
actual candidate78/frozen74 no-queue/Legacy/PURE byte pairs. Six CMake entries
use `cs80_vst3_sine_noise_automation_tests`, original host contract and new
`host-automation-sine-noise-v1.json`,timeout180 seconds.

Verified runtime results are recorded below. No calibrated gain, spectral fidelity, simultaneous
lanes, whole-voice, hardware, GUI or CPU qualification is claimed.

## Verified local result

The scoped official VST3 and supported-host build completed successfully. All 12 direct processes passed: 144 full cases and 144 frozen-74 compatibility cases, totaling 2,736 renders. All 432 actual no-queue/Legacy/PURE PCM capture pairs matched the frozen module exactly. These were direct fixture runs, not CTest or a historical full-suite execution.

All 144 pre-change prefixes through frame 127 were exact. The maximum measured partition error was 3.72529e-9, within the unchanged 2e-6 limit. PANEL effect ranges within the held window [128,3072) were 0.00193781–0.0221469 for rank 0 sine, 0.00219601–0.031873 for rank 0 noise, 0.00509418–0.0182727 for rank 1 sine, and 0.00219601–0.031873 for rank 1 noise. All 96 selected factory/MEMORY cases remained exactly masked over the full PCM output. This qualifies isolated live selected-rank level updates in the bounded fixture, not calibrated gain, spectral fidelity, random-process quality, path purity, simultaneous-rank behavior or hardware equivalence.

[Verification and retained evidence](experiments/sine-noise-automation/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All 45 prior contracts, prior exporters, shared renderer and production sources other than the capability roster header matched the base. Final result prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; the committed evidence contains their hashes and full logs, not a complete replay bundle. There were no failed attempts or retries in this qualification.
