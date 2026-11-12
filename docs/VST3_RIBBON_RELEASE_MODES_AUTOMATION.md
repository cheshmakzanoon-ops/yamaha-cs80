# Candidate RIBBON release-mode qualification

Test-only static Sustain I/II qualification of existing global RIBBON lane78
within the unchanged79-lane roster. Production, parameter metadata, source
routing, helper and48 prior contracts remain unchanged. The frozen79 module is
reused; only the supported test host is compiled for direct qualification.

## Source and controlled history

Engine.cpp2828 updates a voice ribbon ratio when held, pedal-held, or in
sustainMode1 with sustainGate enabled. Mode0 therefore holds its released ratio;
mode1 follows subsequent ribbon motion when the gate is enabled. Controller
smoothing remains unchanged. The ribbon also reaches the modern matrix source,
so all16 matrix amounts are explicitly0. No alternative route is introduced.

Both compared instances use identical default MIDI48 note history with
onsets17+key and offs3072+key, no held pedal, sustainAssign0 (enabled gate),
masterSustain0,keyRelease.5 and rank filter/VCA R.5. The global release floor
supports audible factory/MEMORY tails too. Ribbon starts0 and holds0/3100,
then.2/−.15/.3/−.2 at3327/3583/3839/4095, after all note-offs through3087.
The reference is a matched active constant0 queue within the SAME static mode.
There is no cross-mode PCM comparison, avoiding unrelated mode-dependent
held-node history as a confound.

Other settings retain the prior held-ribbon sounding path: CCadapteroff,MPE0,
portamento/initBend0,pulse1,saw0,sine/noise0,PW.5/PWMdepth0,LPF4000Hz/HPF25Hz,
resonances0,filterIL/AL0,A.001/D.002,VCA A.001/D.002/S.7,VCFlevel.78,
VCAlevel.35,mix.5,neutral macros/dispersion/touch/LFO/modulation.

## Planned bounded checks

- Both modes: exact PCM before3100 versus their within-mode active holds.
- Sustain I: whole-capture PCM equality after ribbon changes. This is not
  internal controller-state equivalence or a later-note claim.
- Sustain II: positive tail effect in frames[3100,4096), not calibrated pitch
  tracking or complete release behavior.

Static modes0/1 ×sources13/0/11 ×capacities8/16 (two/sixteen notes)
×44.1/48/96kHz ×realtime/offline gives72 cases. Six compatibility renders,
scalar/five partitions and one active comparator per case yield936 renders
in six processes. The216 capture identities are NOT baseline byte comparisons.
No historical rerun or new plugin build is implied. Six CMake entries use
`cs80_vst3_ribbon_release_modes_automation_tests`, original contract and
`host-automation-ribbon-release-modes-v1.json`,timeout180 seconds.

Verified runtime results are recorded below. No live mode-switch, filter invariant, internal
state, complete release, hardware, GUI, CPU or broader integration claim is made.

## Verified local result

Direct compilation of the supported host succeeded. All six direct runs passed: 72 cases and 936 renders on the unchanged frozen 79-lane module. The 216 retained capture identities cover no-queue/Legacy/PURE outputs; they are not comparisons against a separate baseline. There was no CMake execution, production rebuild or CTest run.

All 72 prefixes through frame 3099 were exact against the corresponding within-mode active-zero-hold comparator. Maximum measured partition error was zero (limit 2e-6). All 36 static Sustain I cases had exact whole-PCM equality; all 36 static Sustain II cases had positive tail [3100,4096) effects, ranging 0.0122444–0.460777. The comparisons are within each static mode only: no cross-mode PCM equality, mode-switch behavior, internal-state equality or calibrated pitch response is claimed.

[Verification and retained evidence](experiments/ribbon-release-modes/verification.json) records all outcomes and identities. SDK/compiler/Python/source hashes were captured before direct host compilation and checked after runtime; final host/module identities were captured before and after runtime. All recorded execution inputs were unchanged. All production sources, shared renderer, prior exporters and 48 prior contracts matched the base. Final prose was added after these checks. Raw PCM and native binaries remain in the ignored local workspace; committed logs, commands and hashes are not a complete replay bundle. No failed attempts or retries occurred. No hardware or filter-invariant claim follows.
