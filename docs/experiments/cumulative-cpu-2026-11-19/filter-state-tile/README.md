# Four-lane filter state transfers

This changes memory layout transfers inside the existing runtime-dispatched
GCC AVX2 filter backend, not its solver, admission, reported latency, or scalar
fallback. A 4×4 transpose moves four neighboring coefficients/history values
per lane and commits matching state across four distinct physical stages.
Compile-time layout checks and bytewise copies avoid unaligned or out-of-object
typed access. The optional AVX2 path still requires masked SSE exceptions and
round-to-nearest.

## Measured work and exactness

A matched local Debian x86-64 GCC 14.2 Release/PIC VST3 pair used one recompilation
of the changed backend object with the same command and JUCE/other object inputs.
The [whole callback receipt](work.json) keeps all four fixed 48 kHz / 64-frame
workloads, native and translated equality, repeated four-callback controls and
the independent fifth-callback identity. All 40 real VST3 loads completed.

| Fixed scene | Guest instructions | Guest reads | Guest writes |
| --- | ---: | ---: | ---: |
| Fresh quiet | unchanged | unchanged | unchanged |
| Eight equal held notes | −0.667% | −1.588% | −3.906% |
| Eight varied held notes | −0.662% | −1.582% | −3.908% |
| Varied wake | −0.487% | −1.177% | −2.781% |

A separate native host compared four fresh instances across ten fixed histories,
800 callbacks and 36,140 stereo frames per instance. Captured audio, public
state, floating point observations, parameter/output events, latency and tail
matched within the tested platform. Standard realtime at 48 kHz still reported
67 samples. Component tests covered lane permutations and placements, including
unaligned vectors. The in-tree batch, batch-engine and scheduling suites passed.
An explicitly scalar-only backend object stayed byte-identical.

No callback clocks were collected. Guest counts are not CPU time, DAW latency,
or confirmation of a physical low-end CPU target.
