# One-sample certificates for the factory Q and level receivers

On the qualified Linux x86-64 GCC 14.2 SSE2 build, the engine can publish the
played voices’ shared Q and level readout without sanitizing all sixteen
drives again or copying a full staged result. Both rank cutoff transactions
must have **succeeded on this sample without clipping any drive**. Neither
a retained mask alone, an inactive rank nor a failed cutoff qualifies. All
three drive values originate as floats; promoting one cannot create a double
subnormal. The eligible values, including signed zeros, are therefore exactly
what the scalar sanitation would return. The receiver still independently
checks both rank boundaries and resonance before writing any field. Ordered
touch sums, each individual level, every envelope and all audio clocks remain
in place. No data from an earlier sample supplies the proof. Uniform drives
still use the existing uniform receiver path; non-nearest/unmasked controls,
uncertified drives and other builds keep their existing paths.

**Isolated deterministic work, not native CPU time.** Against `5499056`,
four 48 kHz/64-frame standalone core renders had 10.565% fewer translated
instructions with one held note, 3.697% fewer with eight equal-velocity notes,
3.674% fewer with eight varied notes and 2.505% fewer in the actively routed
factory chord. The fresh factory quiet case used **0.305% more instructions**;
Legacy was unchanged. See [all nine cases and controls](evidence.json).
Each pair compared byte outputs within its own execution runtime. Counts are
Callgrind 3.24 work over the render only, not a plug-in callback or a DAW CPU
meter. Do not add these percentages to other studies.

- The [direct public-scalar differential](check.cpp) checked 65,536 calls,
  including 20,312 independent boundary/master rejects: return, output
  including unchanged failure destinations, MXCSR, x87 state and errno.
- The [Engine dispatch differential](handoff.cpp) compared 20,480 handoffs,
  including stale zero/nonzero masks, failed/malformed cutoffs, either rank
  disabled, invalid shared loads, signed zeros, NaNs, saturation and float
  subnormals. All four rounding modes and four DAZ/FTZ settings were covered;
  29,839,360 bytes of published observations matched.
- The [nine-scene render probe](../engine-lazy-records/probe.cpp) gave
  5,845,680 matching observation bytes natively and again under Callgrind.
  Both arms of quiet, one-held and varied-held passed duplicate and exact
  four-plus-one work controls. The [22-case trap probe](../wsc-terminal-staging/render-traps.cpp)
  delivered seven actual SIGFPE per arm; 17,019,552 bytes including partial
  state and the masked continuation matched. Six [longer histories](../synchronized-thermal-bank/trace.cpp)
  also matched, including release/wake, rate and policy changes.
  These probes observe selected state, not every possible execution.

For the direct checker, from repository root:
```sh
g++ -std=c++20 -O3 -DNDEBUG -fPIC -Isrc docs/experiments/certified-shared-receivers/check.cpp -o /tmp/certified-readout
/tmp/certified-readout
```
For the handoff probe, build `core/Engine.cpp`, `core/FactoryPresets.cpp`
and `core/Ig00156Batch.cpp` under `src` as separate normal Release/PIC objects
(the last with `-ffp-contract=off`). Compile only `handoff.cpp` with the same
flags plus `-fno-access-control`, link the four objects, and pass one fresh
output filename. Compare files byte for byte between arms. The other probe
sources and [core build instructions](../engine-lazy-records/README.md)
describe the same recipe. No test-only access or timing enters shipping DSP.
