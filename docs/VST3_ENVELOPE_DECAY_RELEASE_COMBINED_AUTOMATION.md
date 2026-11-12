# Candidate combined DECAY/RELEASE automation qualification

Test-only integration of eight existing filter/VCA D/R lanes66..73 within the
unchanged78-lane roster. No production, parameter, helper, source mask or prior
46-contract change. This reuses the frozen78 native module and builds only the
new supported host for direct qualification; CMake registration is provided for
future use, not a claim of CTest or historical-suite execution.

## Mixed-source integration

Source pairs(13,0),(0,13),(13,11),(11,13), capacities8/16 (two/sixteen active
notes),44.1/48/96kHz and realtime/offline yield48 cases. Every case has one PANEL
rank and one masked factory/MEMORY rank. Both ranks receive the eight queues;
source authority is not bypassed. Shared helper/default MIDI48 notes are unchanged.

D starts.5s and holds0/512, then.2/.6/.3/.7/.25/.4 at
767/1279/1919/2559/3071/4095. R starts.5s and holds0/3100, then.3/.6/.4/.7 at
3327/3583/3839/4095. Both attacks.001s, filterIL.5/AL.3 and VCA S.3 establish
a settled Attack followed by still-active Decay during the held witness.
MinimumD.2 and held duration<.070 keep gaps above completion thresholds. Minimum
R.3 dominates keyRelease.18/masterSustain0; after all offs3072..3087, the short
tail remains in Release. Neutral macros/envTimeScale1 and dispersion0 preserve
these source-derived bounds. Existing state/anchor-preserving timing laws remain
unchanged, not independently calibrated by these PCM comparisons.

Explicit sounding settings: pulse1,saw0,sine0,noise0,PW.5/PWMdepth0,
LPF1500Hz/HPF25Hz,resonances0,VCFlevel.78,VCAlevel.35,mix.5,zero touch/LFO/
modulation amounts and no held pedal. Two matched-active comparisons are distinct:

- All eight queues versus all eight constant active holds: exact pre512 PCM and
  positive aggregate held effect in frames[512,3072).
- All eight queues versus identical D trajectories plus four constant active R
  holds: exact pre3100 PCM and positive release-group tail effect[3100,4096).

The second comparator controls D history and ongoing D values, isolating the
R group rather than confusing decay history with release changes. Neither
comparison attributes an effect to each individual lane or masked rank.

Each case uses six noqueue/Legacy/PURE compatibility renders, scalar plus five
partitions, and the two comparator renders:14×48=672 renders across six processes.
The144 retained capture identities are not new-versus-baseline byte comparisons.
No historical reruns or new native plugin build are implied. Original thresholds
remain unchanged. Six CMake entries use
`cs80_vst3_envelope_decay_release_combined_automation_tests`, original contract
and `host-automation-envelope-decay-release-combined-v1.json`,timeout180 seconds.

Verified runtime results are recorded below. No complete stage response, calibrated hardware,
individual-lane contribution, GUI, CPU or broader integration claim is made.

## Verified local result

Direct compilation of the supported host succeeded. All six direct runs passed: 48 mixed-source cases and 672 renders on the unchanged frozen 78-lane module. The 144 retained capture identities cover no-queue/Legacy/PURE outputs; they are not comparisons against a separate baseline. There was no CMake execution, production rebuild or CTest run.

Both comparator-prefix checks were exact in all 48 cases: through frame 511 against the eight-active-hold comparator, and through frame 3099 against identical decay curves with active-held releases. Maximum measured partition error was zero (limit 2e-6). Aggregate decay-group effect in [512,3072) ranged 0.000962647–0.00666386; release-group tail effect in [3100,4096) ranged 0.000276196–0.00243403. The latter comparator preserves identical decay automation and therefore does not confuse decay-induced release-start differences with the release-group change. These are group effects, not independent per-lane contribution measurements.

[Verification and retained evidence](experiments/envelope-decay-release-combined/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before direct host compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All production sources, shared renderer, prior exporters and 46 prior contracts matched the base. Final prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; committed logs, commands and hashes are not a complete replay bundle. No failed attempts or retries occurred. No hardware, complete-stage or calibrated-timing claim follows.
