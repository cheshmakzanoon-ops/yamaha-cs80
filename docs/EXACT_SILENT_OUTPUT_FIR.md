# Exact silent output FIR windows

Both reconstruction filters keep accepting every internal sample and
advancing the same mirrored history/cursor. The Engine-owned decimator
tracks the length and sign of a contiguous, bit-exact signed-zero suffix.
Only a suffix covering the **entire** active filter history can yield a
shortcut. It does not use output level, a noise gate, denormal flushing,
voice activity or an arbitrary silence threshold. Every oscillator, filter,
envelope, modulation, delay and release step still runs.

A uniform positive-zero window produces positive zero. A uniform
negative-zero window may also produce positive zero: the stored center
coefficient must have nonnegative sign and a stored negative side
coefficient must provide a positive-zero term. If the coefficient proof
is absent (including the default unprepared all-zero filter), use the
scalar summation and its original sign. Mixed signs, any nonzero bit
pattern including a subnormal, dirty histories, directed rounding and
unmasked exceptions also retain the original path.

The fast path is scoped to strict generic GNU/Linux x86-64 SSE math.
One translation unit owns the wrapper; the class layout does not
depend on per-translation-unit flags. Unsupported builds do not keep a
per-sample run counter. The ordinary public decimator and its original
scalar implementation are unchanged.

## Verification limits

The oversampling checks compare exact outputs for 2×, 4× and 8×,
including unprepared signed zeros, positive and negative windows, an
impulse with its complete tail, an opposite-sign zero, a subnormal,
copy/reset and alternate rounding. The test covers behavior, not
the absence of all possible cross-platform regressions.

Earlier separate prototypes used a window scan and showed a sparse-input
overhead in an isolated leaf test. This implementation uses an O(1) signed
suffix counter so a dirty/sparse window does not add a scan before the
scalar fallback. Active callback bookkeeping is still nonzero.

## Linux Release observations

The final oversampling executable passed **11,706 checks**. The exact
combined release VST3 (SHA-256
`74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895`)
matched the preceding cutoff build over **65,024 native callbacks,
4,161,536 stereo frames, and 76 public component-state snapshots**.
Six additional automation cells at 44.1/48/96 kHz in realtime/offline
modes all passed on both builds and matched 216 captured files.

In a separate four-callback guest work check using the real Linux VST3
interface, the same final candidate executed **13.71% fewer instructions**
on fresh quiet and **13.43% fewer** on a quiet window after an actual note
release. The short held fixture executed **0.037% more** guest instructions.
Equal-work stock control and independently observed additional-work control
were exact in all scenes. These counts are not native CPU time or a DAW
improvement. Stock native and translated executions differed in very small
denormal values before the settled window, so the strict *cross-runtime*
release comparison did not pass. Native stock/candidate and translated
stock/candidate each matched in their own runtimes. The quiet-window
translated count result remains a diagnostic, not a transfer to native CPU.
