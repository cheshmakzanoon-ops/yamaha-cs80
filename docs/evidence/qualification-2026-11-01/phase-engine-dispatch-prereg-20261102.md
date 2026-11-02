# Frozen design: true Engine-dispatched nonlinear phase-lane SIMD

2026-11-02 · **test-only** · original `src` Git tree
`7dd159d491012fd47b46c0aa651e86ea959ca289` unchanged

Existing standalone preregistration `71a0226` independently proved
**79,337,496** exact phase-solver bit comparisons with zero mismatches,
including mixed 1/2/3/4/7/8/15/16-width groups, varied rates, true
Newton iterations and source states under 4 IEEE rounding modes and x86
FTZ/DAZ combinations. It deliberately had **zero real SIMD source calls**.
This new registration addresses the missing **Engine-dispatch boundary**
*before* changing any production source. Its normative JSON is
`tests/cpu/phase-engine-dispatch.json`; it cannot be backfilled from a new
optimization.

## True dependency boundaries

`Engine::tickRanks` advances each voice and rank in order. Each rank
clocks the HP IG00156 stage and then feeds its computed HP output into the
LP IG00156 stage, after which its VCA/touch envelope, pan and stereo mix
are advanced. Processing a single rank's HP and LP simultaneously is
**illegal**: LP input depends on HP output. But the HP phases of distinct
voices/ranks are causally independent *within the current internal sample*;
so are their LP phases **once the corresponding HP results exist**.

An eligible fast path must preserve:

1. The original per-sample `tickGlobals` and prior-rank readouts, then
   oscillator/panel/shared-noise and global `audioRng` draws in the same
   `vi,ch` order, without reading prematurely updated rank info.
2. All HP stage coefficients, previous stage-rate references and source
   state, then independent HP `process` across actual active lanes.
3. Corresponding LP stage clocks with each lane's exact HP output as its
   input, then independent LP nonlinear `process` across actual active lanes.
4. The original `vi,ch` ordering of VCA/EG operations and every floating
   stereo summand addition, rank readout and voice retirement. No
   cancellation of physical idle or wake clocks.

The initial candidate may *only* batch warmed source/PURE eight-held
full-rate filters with all voices currently active and no idle-clock,
first-wake replay, cold circuit or tail coupling. Mixed active/inactive,
Legacy, V1/other render policies, unsupported ISA, rounding or unqualified
FTZ/DAZ take the unchanged scalar Engine path. This is an initial safety
*eligibility*, not a sound-model simplification or proof that a digital
schedule commutes without checking public state.

## Mandatory real work and negative controls

One actual Engine-dispatched width-4/8/16 nonlinear batch must execute
compiled vector Newton/tanh arithmetic, commit precisely active lane states
and count scalar remainder/fallbacks. Source-site counters belong in
**separately compiled test-only instrumentation**, not production audio
state. Mere vector loads, wrapper flags, compiler-generated scalar loops
or a backend that is never invoked by `Engine::tickRanks` are not accepted.
The independent original phase oracle, pristine eight-held `Engine` audio
and global/readout FNV64 checkpoints across callback partitions1/17/256,
touch idle/wake oracle, strict Legacy/tails/V1/current V2 byte captures,
and full original physical/clock CTests remain unrelaxed.

Future *actual-source* negative mutants must suppress a qualified fast
entry or fake its vector-work count, advance a masked lane, reverse HP/LP,
change one Newton exit step, alter one voice input/RNG draw, reorder a
floating mix summand, and skip a wake clock. Every mutant must compile
successfully and fail a named runtime output/state/work assertion with
ordinary exit1; a build failure, timeout or crash does not count.

This registration follows the rigorously rejected small experiments:
the isolated WSC shortcut gained only ~2% on one paired-median comparison,
while the exact drift-zero sine candidate **slowed 10/11 held presets**
(-12.91% median paired improvement) and worsened quiet idle. Both were
preserved separately and **must not enter PR6**. Original hard targets
held **0.15–0.20 RTF**, idle **≤0.02** are still **RED**. The nonlinear
phase process was ~29.2% of the instrumented held profile; idle needs
*separate* control/touch/output scheduling because phase is almost absent
there. Profiling share is not a promised RTF speedup.

No SIMD backend, production change or new CPU speedup is asserted by
this test-only document. Three exclusive original-source BEFORE and three
same-host AFTER runs of the unchanged complete 51-fixture benchmark are
mandatory **after** an actual engine-dispatched, independently tested
implementation. No feature/main merge, new pin or release before the
original absolute gates pass.
