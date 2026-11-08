# Linear delay-reader wrap safety

## Failure and scope

`DelayLine::read()` used float arithmetic to wrap a negative read position by
adding the buffer capacity. A sufficiently small negative position can round to
exactly that capacity. Converting it to an index then reads one float past the
allocated buffer.

This is reachable with ordinary finite controls. The classic FX-slot chorus at
88,200 Hz internal rate (44,100 Hz host at 2x), rate 0.62 and depth 0.62, reaches
the boundary within 262,358 samples, about 2.975 seconds. One failing read has:

- Buffer capacity: 7,058 samples.
- Write position: 1,212.
- Requested delay: 1,212.000244140625 samples.
- Negative read position: -0.000244140625.
- Rounded position after adding capacity: 7,058, which is outside the buffer.

A standalone AddressSanitizer run reproduced the heap-buffer-overflow at this
read. This finding is independent of plugin startup or shutdown; it does not
establish the cause of any separately reported plugin-validation failure.

## Correction and compatibility

After the existing float wrap, a position at or beyond the capacity is normalized
to sample zero. All formerly valid positions retain exactly the same arithmetic
and interpolation. The delay-time law, effect parameters and buffers are unchanged.
Changing all position calculations to double was deliberately avoided because it
would change ordinary legacy interpolation samples.

The correction removes undefined reads, so no compatibility claim is made for
the former out-of-bounds output itself. This is a software memory-safety fix,
not a change to the hardware-model evidence or calibration.

## Regression coverage

`cs80_delay_line_tests` is a JUCE-free test registered in CTest. It covers:

- Rounded wrap and reset at eight internal rates from 16 kHz to 1.536 MHz.
- The exact production chorus delay above.
- Three complete buffer traversals at four rates, comparing every valid old
  interpolation with its original arithmetic by floating-point bit pattern.
- Low/high delay clamps, integer/fractional reads and wrapped reads.
- 300,000 samples through the real classic Chorus at 88.2 and 96 kHz, checking
  finite, bounded, non-silent output.

The new regression fails on the preceding reader under AddressSanitizer and
passes all 36 checks after the correction with AddressSanitizer and
UndefinedBehaviorSanitizer. LeakSanitizer could not run in the execution
environment; leak detection was disabled for that focused run. No leak check or
plugin/GUI validation is claimed by this test.

Standalone reproduction of the fixed regression:

```sh
c++ -std=c++20 -O1 -g -fno-omit-frame-pointer \
  -fsanitize=address,undefined -Isrc tests/DelayLineTests.cpp -o /tmp/cs80-delay-tests
ASAN_OPTIONS=detect_leaks=0 /tmp/cs80-delay-tests
```
