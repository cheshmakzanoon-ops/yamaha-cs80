# Avoid repeated envelope dispatch in the admitted source renderer

The source-mode renderer can invoke the existing RC reconstruction step
directly when the build and audio thread meet the guarded arithmetic
contract. The public envelope entry still dispatches legacy and
reconstructed behavior as before. Each envelope shares one canonical RC
equation between the original and direct call. Time advancement, stage
thresholds, envelope gates, level updates, reset behavior and offline
quality remain unchanged. The oscillator/filter physics are not replaced.

The targeted Linux x86-64 strict GNU/SSE2 path checks both floating-point
units, including unmasked traps and x87 precision/rounding. Other builds
or thread settings retain the original call. The split is justified by a
counterexample: a first unguarded version changed exception-time partial
records even though ordinary audio agreed. The guarded public Adsr and
FilterEnv tick functions preserve machine instructions and resolved
relocation semantics on the tested default and batch-disabled builds.

## Whole plug-in diagnostic
The real VST3 stock is the shared-receiver release from
`4b442e17e2ba901b590fc5638618aaaf17ac07fa`, default ELF
`0f848ae0c8eb7629262395d457ff31662d3d8806b498848a6a6ee868b5ed56d1`.
The exact candidate default ELF is
`6b5aa28e3cb31381b21068790e549a09eb7b5a2fe7052f65015da375e10e92d3`;
it matches the coherent rebuild of all 21 relevant header consumers.

| Four 48 kHz/64-frame callbacks | Default guest instruction change | With AVX2 filter batch disabled |
| --- | ---: | ---: |
| Fresh quiet | −2.676% | −2.881% |
| Eight equal-velocity held notes | −1.357% | −1.355% |
| Eight varied-velocity held notes | −1.349% | −1.347% |
| Varied wake | −0.949% | −0.964% |

Same-module repeats and independently counted extra callbacks matched in
each tested build. These are whole-plugin translated work counts, not CPU
time on physical hardware. The batch-disabled diagnostic is a separate
build, not a weak-CPU simulation.

## Exactness
The actual loaded builds matched in 8,505 directed cases, including 1,284
real floating-point exceptions and continuations, per profile. Seven
extended 48 kHz plugin histories and eight 44.1–192 kHz realtime/offline
scenes preserved the tested audio, public state and FP observations.
Focused rendering and envelope checks ran on both build profiles. The
ordinary product and admitted fallback checks are independent of the
cloud native clock qualification, which had no candidate.

The independent full 48 kHz native VST3 protocol completed on both
arms with byte-identical test reports. Focused envelope regression builds
each passed 22,741 checks. GCC O1, O2 and Og actual-module tests each also
matched all 8,505 directed cases, including the same 1,284 interruptions.

[Resolved machine operands](resolved-symbols.json) distinguish code
layout from meaning. In the default oscillator tick, four object-file
PC-relative references moved with the helper section. They still target
the identical entry and implementation. The stimulus and complete
oscillator/body bytes agree; numeric evidence remains scoped to the
tested builds and fixtures.

[Final build sealing](sealing.json) preserves the exact previously tested
Release objects in both profiles. Unoptimized and size-optimized builds
compile the original dispatcher; those rebuilt Engine objects were
byte-identical to stock. Clang 19 O0/O3 and fast-math controls also
retained the stock Engine objects. The public regression's deliberately
wrong 1.25× clock control failed 22,687 assertions.
