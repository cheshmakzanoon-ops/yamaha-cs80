# Output sample-peak telemetry

The output indicator is a monitoring aid, **not a limiter, true-peak meter or
loudness measurement**. `OVER-RANGE` means a finite output sample exceeded absolute
value 1.0 (0 dBFS); it does not prove that a floating-point host bus hard-clipped.
`NON-FINITE` is a separate NaN/Inf warning. Neither event changes the signal.

## Measurement boundary

The processor reads the completed host output buffer at the end of each
nonempty audio callback, after the engine's decimator and plugin mono/stereo
mapping. Only actual output channels count. This observes mono cancellation or
mono-sum overflow as delivered, rather than reporting internal stereo values.
Input-only/unused channels and MIDI do not participate. Zero-frame callbacks do
not erase a pending peak. Prepare/release clear transient telemetry; it is not
a parameter and is not serialized with a preset or host session.

Magnitude classification/max uses 32-bit IEEE754 integer bit ordering. It does
not perform arithmetic on NaN/Inf, and preserves exact finite sample magnitudes,
including subnormals while the audio thread uses FTZ/DAZ. Compile-time assertions
require 32-bit IEC559 floats and always-lock-free 32-bit atomics on the target.

## Bounded communication contract

`OutputMeter` has one audio producer and one UI consumer. The producer publishes
the maximum finite sample magnitude and separate over-range/nonfinite event bits.
The consumer calls `consumeOutputMeter()`; finite peaks/events accumulate while
the editor is not polling. All atomic operations are relaxed because no separate
object storage is being published through them.

The operation count below is bounded at the C++ level, not a machine-level
wait-free or worst-case-execution-time proof: a lock-free target can still use
hardware LL/SC retries internally. Real-time CPU qualification remains separate.

For each positive block peak, publication performs at most:

1. One load of the pending magnitude bits.
2. One **strong** compare/exchange with `max(observed, incoming)`.
3. On failure, one store of the incoming peak.

The CAS runs even when the incoming peak is smaller than the observed value: a
relaxed load could otherwise observe a historical larger value after a UI clear
and incorrectly skip the new smaller observation. This is a formal robustness
case, not a claim of a reproduced native processor failure.

There is no retry loop. With a single producer, the only competing modification
is the consumer's exchange to zero. A failed strong CAS is not spurious and
cannot conceal another producer's newer/larger value; the fallback store either
reaches that consumer or remains for the next one. Over-range/nonfinite bits use
one bounded `fetch_or` when needed. Measurement uses a local accumulator, with
no audio allocations, locks, parameter writes or output writes.

Peak and event bits are **independent observations**, not one coherent or
sample-timestamped packet. A publication overlapping a poll can put its peak and
flags in adjacent polls. UI overload indication therefore also considers
`samplePeak > 1`. Opening the editor consumes/discards once to clear stale history;
a publication already in flight may appear in the next poll. Publications after
that clear has completed remain observable. Lifecycle `reset()` is only used
where host prepare/release excludes concurrent audio publication.

The existing 8 Hz message-thread timer owns visual decay and short warning holds,
using UI wall time rather than audio samples. The display is read-only, uses text
as well as color, and does not create a new parameter or alter saved sound. The
numeric sample peak releases at 12 dB/s with a -120 dBFS display floor;
`OVER-RANGE` holds for one second and `NON-FINITE` for two seconds. The two-line
`SAMPLE PEAK` display is in the upper-right header beside PANIC.

## Tests and evidence limits

- `OutputMeterTests.cpp`: finite/absolute max, exact full-scale boundary, multiple
  blocks/polls, NaN/Inf separation, finite extremes/subnormals, resets and no
  sample mutation.
- `OutputMeterAuditTests.cpp`: independent deterministic atomic interleavings,
  stale-observation robustness, bounded operation counts, native producer/
  consumer stress and FTZ/DAZ magnitude preservation. This is not a universal
  weak-memory proof or full host-thread qualification.
- `OutputMeterHostTests.h`: an unmetered core oracle with actual APVTS controls
  compares ordinary host output bit-for-bit in all three rendering policies,
  mono/stereo and oversized/zero-frame blocks. It verifies telemetry against an
  independent final-buffer scan, excludes NaNs in unused channels, preserves
  MIDI/parameters, and opens an editor mid-phrase without changing later audio.
- `OutputMeterDisplayTests.cpp`: message-thread decay/hold behavior and distinct
  warning state. Native rendering/layout/accessibility checks remain part of
  the regular UI smoke, not a substitute for the DSP oracle.

Synthetic NaN/Inf values are injected only into the isolated telemetry primitive,
never into the production DSP. Header syntax/core tests do not by themselves
establish native plugin/GUI runtime, host overload behavior or real-time CPU
safety on every system.

Final local primitive execution: **20 checks, zero failures**. The
[independent audit](output-meter/README.md) records **6,348 checks, zero failures**
in native, AddressSanitizer/UndefinedBehaviorSanitizer and ThreadSanitizer runs,
including the bounded validating-CAS stale-observation regression and FTZ/DAZ
subnormal preservation. These are the isolated helper's checks, not native DAW
or whole-plugin sanitizer qualification. ASan leak detection was explicitly
disabled for the environment's ptrace limitation; no leak-check pass is claimed.
The aggregate **38 core CTest suites**
also passed; the host helper and combined editor smoke compile syntactically,
while native plugin/GUI runtime for this change remains a separate CI gate.
