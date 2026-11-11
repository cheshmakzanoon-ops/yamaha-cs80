# Reuse validated MTS frequencies within a poll

This follows the channel-aware master implementation at `3ffbe7fb`. The adapter
already reused a pitch conversion when consecutive channels supplied exactly the
same safe Hz within one note. It now also reuses that frequency's validated range
admission. A cache entry exists only after finite bounded Hz produced finite
pitch; invalid, filtered fallback values never enter it. The cache is local to
one note in one poll, not persistent across blocks.

Every external frequency and filter query is retained, including the capability
primer. Every channel's own mapping flag is read. Complete-bank staging,
observed-disconnect rejection, global MPE/PURE behavior, and repeated publication
are unchanged. In particular, this does not skip unchanged master polling or
prevent the next master poll from replacing a local SysEx update.

## Predeclared mock-backend cost gate

Five alternating baseline/candidate process pairs ran the existing precomputed
backend fixture pinned to CPU 8. Each process produced 54 rows, with nine
alternating global-API/bank-API pairs per mode and 1,000 polls per row. There are
45 bank measurements per mode per implementation. The gate required at least
5% lower common-frequency median cost and at most 3% regression in either other
mode. Both binaries used GCC 14.2, C++20, `-O3 -DNDEBUG` without native ISA flags.

| Bank mode | Baseline ns/poll | Candidate ns/poll | Candidate / baseline |
| --- | ---: | ---: | ---: |
| Global / MPE | 1,416.964 | 1,437.223 | 1.0142975 |
| Conventional, common frequencies | 10,455.477 | 8,856.421 | 0.8470604 |
| Conventional, distinct frequencies | 20,987.730 | 21,407.048 | 1.0199792 |

The declared aggregate-median gate passes. One individual global-mode process
pair had a 1.065 ratio; the gate does not claim every repeat stayed within 3%.
Common-frequency improvement appeared in all five process pairs (ratios
0.833–0.853). This is approximately 15.3% lower common-table adapter
cost in this mock fixture, not a whole-instrument speedup. The distinct-table path
was approximately 2.0% slower in this run. The extra channel polling remains more
expensive than the old global-only feature. These timings omit real external
library/service calls, engine publication, contention and audio rendering. They
establish neither live-service deadline safety nor the project CPU target.

## Exactness and regression scope

The expanded adapter fixture compares output float bits against direct conversion
for mixed repeated/distinct frequencies under all four standard rounding modes,
equal accepted minimum/maximum Hz, and valid–filtered-invalid–same-valid sequences.
It retains malformed values, mapping independence, observed disconnect and
capability-transition cases. Both old and new implementations must pass the same
expanded contract; this is an optimization, not a behavior correction.

The expanded old and new adapter fixtures both passed **141,469 checks / 0
failures**. The final Release/Linux focused suite passed **14/14 in 64.99
seconds**, including core tuning, MTS fixtures, six actual-module VST3 rate/mode
cases and the existing callback-allocation fixture. This is not a full-project
qualification, and the allocation fixture does not model a live master.
An independent post-rebuild repeat passed **14/14 in 65.25 seconds**. The earlier
62.80-second run was preliminary: review identified stale sibling executables.
All selected targets were then rebuilt (11 build steps) before these final runs.

No persistent floating-point environment cache, polling throttle, approximate
conversion, changed tuning semantics or upstream vendor modification is added.
Live-master concurrency and cross-platform performance remain unqualified.
