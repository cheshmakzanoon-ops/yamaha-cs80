# Reuse equal source cutoff drives within one sample

Factory tone cutoff evaluation normally computes eight per-voice readouts
per rank. When all eight envelope, velocity and pressure vectors have
identical double representations, the engine can compute the shared
inputs and one pin/CV result per side and publish the same result for all
eight voices. The original eight-term sums still use the original order
and the public scalar evaluator is unchanged.

This is a **per-call** specialization, not a cache across samples.
There is no change to oscillators, envelope progression, touch state,
filter ticking, analog tail state, or audio/smoothing settings. Comparing
input representations does not equate a negative zero or a subnormal with
positive zero. Unequal inputs, non-qualified arithmetic settings and
unmasked floating-point exceptions take the original path. Failed frames
never replace the completed readout. A varied-velocity release can remain
entirely on the scalar path even when the audible output is silent.

## Evidence and limits

- Factory nominal-model suite: 374,219 checks passed, including new exact
  readout comparisons for equal controls, one differing voice, and
  transactional rejection of mixed or uniform NaN input.
- Independent component matrix: 418,176 paired checks of return value,
  output bits, x86 FP state and errno on the equivalent arithmetic. The
  generic build admitted 52,272. Explicit off and nonqualified FMA,
  x87 and AVX512 compile controls admitted none.
- The exact release candidate ELF
  `0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2`
  matched the preceding main build across 65,024 actual native VST3 callbacks
  and 4,161,536 stereo frames, including fresh quiet, held, release, and wake.
  Callback FP/status and 58 public component snapshots matched byte-for-byte.

These checks do not imply a native CPU percentage, DAW behavior on all
machines, or a result for every preset. Quiet must not be equated with idle
physical circuits or a license to skip the callback.

In a separate translated work check, the exact release candidate executed
**9.855% fewer guest instructions** over four fresh quiet native-interface
callbacks and **1.385% fewer** over four short held callbacks with eight
equal-velocity notes. Equal-work stock controls and the independent positive
control (four plus fifth equals five) matched all instruction/read/write
counts. These are hardware-model guest counts, not native CPU time or a
claim about long held notes or varied-velocity release.
