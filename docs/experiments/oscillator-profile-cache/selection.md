# Candidate narrowing before timing

The broad aggregate cache and the same formulas as flat fields passed generic
GCC but failed the unchanged `-O3 -DNDEBUG -march=native` bitwise corpus:
318,101 failures / 328,508 checks. The first continuation difference at nominal
profile / 8 kHz was inputBaselineVolts `0x1.ef7c066325cp-9` versus
`0x1.ef7c06632a8p-9`. Identical-header A/B passes. Disabling contraction also
makes the broad cache pass, but that is diagnostic evidence, not permission to
change the qualification compiler flags. The exact contraction/inlining
instruction responsible has not been attributed.

Each of six isolated groups passed normal native GCC: maximum bound, triangle
gain, reset rate, half/spacing, fold levels, and shaper slopes. Combining the
three geometry groups failed like the broad cache. Combining only the three
quotient-related values passes generic and normal native GCC: 328,508 checks /
161,787 compared ticks, with no failures.

The selected candidate therefore caches only maximum frequency, triangle gain,
and reset rate. Event search, half/spacing, fold levels and shaper slopes retain
the original expressions. The rejected broad/flat/geometry headers and their
logs are archived under `rejected/`, outside shipping includes. The original
correctness and timing acceptance gates remain unchanged. No timing result was
used to choose this smaller subset.

## Final decision

The quotient-only candidate was selected for measurement, not accepted for
shipping. Both GCC profiles subsequently failed the preregistered active-cost
gate. See [the final negative result](../../analysis/oscillator-profile-cache.md).
The exact candidate and harness now live under `rejected/quotients/`; shipping
source, registered tests and CMake were restored to `35f3431`.
