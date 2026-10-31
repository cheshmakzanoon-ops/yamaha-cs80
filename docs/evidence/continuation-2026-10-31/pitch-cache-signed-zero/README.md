# MIDI pitch memo: signed-zero reference

Registration extension: `81b8c79e5cf226c9de6bbb0a2991b2c4a76c309b`, published
before this qualification. The existing exact zero-error limit is unchanged.

`Dsp.h::midiToHz` multiplies the reference by a positive power-of-two factor
throughout the registered finite pitch domain. A negative-zero reference thus
produces negative zero. Numeric equality in the memo key previously treated
positive and negative zero as the same reference and reused the wrong sign.

The fix retains both existing floating-point equality conditions and adds a
reference sign-bit comparison. NaN pitch/reference values still force a miss.
All 174,978 original checks remain, with 24,584 added reference sign-transition
and repeat checks across the original pitch grid.

| Header used in isolated fixture | Checks | Failures |
| --- | ---: | ---: |
| Original numeric-only key | 199,562 | 9,219 |
| Sign-sensitive key | 199,562 | 0 |
| Mutation restoring numeric-only key | 199,562 | 9,219 |

The normal Engine clamps the tuning parameter to 415–465 Hz, and Pure uses
440 Hz (`Params.h` and `Engine.cpp`). Zero-reference behavior is part
of the registered transform API contract; this repair does not change an
accepted Engine tuning value or the original converter expression. Root still
runs the full compatibility captures and whole-engine benchmark sequence.

The compile/run logs, exact mutation diff and source hashes are retained here.
All fixture executables and mutated headers were built in temporary directories;
the shared root build directories were untouched.
