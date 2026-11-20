# Two pairs of exact crossing predicates on held oscillator spans

The three fold thresholds and the independent pulse threshold all test
`(level-u) * (level-end) <= 0` against the **same original endpoint**.
A preceding accepted root can shorten the limit for another root, not the
endpoint of these four comparisons. The already-qualified, masked, nearest
GCC 14.2/Linux x86-64 held route evaluates the four existing binary64 products
and comparisons using two SSE2 pairs. If all are false, the scalar loop
cannot find a root. Otherwise the original scalar root search still runs in
its original order. Nothing changes in the turn, shaper, integration, residual
or output calculations, oscillator state, model parameters, latency, or the
direct scalar and nonqualified paths. There is no new ISA requirement.

**Compared to `5499056a60ee5044275ba17eb7dddded4c96e90d`.** On the
[unchanged component fixture](../held-span-and-comparator/component-work.cpp),
4,096 held ticks of 16 mixed-gate oscillators used 2,903,566 → **2,760,525**
translated guest instructions (−4.93%), 725,048 → **712,075** guest reads
(−1.79%), and 376,399 → **371,974** guest writes (−1.18%). Duplicate windows
matched exactly and the independent 4,112-tick run checked the longer prefix.
All returned-pin checksums matched between arms and between native and guest
within each arm. The [raw rows](component-work.json) are **not elapsed CPU
time, physical memory traffic, or an end-to-end callback result**.

[Direct exactness](direct-results.json) separately passed 323,584 paired ticks,
261,095 held-path admissions and 1,941,534 checks at each of GCC 14.2 `-O3`,
`-O2`, `-O1`, and `-Og`. It compared pins, private endpoint, complete public
state and retained diagnostic event slots, MXCSR, x87 status/control and errno.
It spans nine valid profile variants and eight rates, clean and preseeded
flags, changed controls, idle/restore/copy and poisoned residuals. The
registered `cs80_wsc_held_endpoint_tests` adds 144 deliberately constructed
borrowed crossings, including near-coincident and identical thresholds and
unreachable ±DBL_MAX pulse thresholds with overflow flags. Ignoring the high
pair failed the existing registered test at 8 kHz, sample 9.

## Exploratory native component clock: no CPU-time conclusion

A separate same-process component study used a pinned guest logical CPU,
the calling thread's CPU clock, and fixed 32 round × five-run randomized orders.
Each ordinary run rendered 32,768 held samples on each of 16 nominal oscillators;
the deliberate work control requested 36,864 samples. The clocks surround
only the rendering loop and its sums/dispatch, not setup/final checks. Generic
GNU 14.2 `-O3 -DNDEBUG -std=gnu++20 -fPIC`; cloud AMD EPYC guest, logical CPU 7,
not an isolated older target. All 160 observations are retained in the
[raw CSV](component-cpu.csv): round, tag (0 stock, 1 identical stock, 2
candidate, 3 identical stock, 4 stock +12.5%), tick count per oscillator,
number of borrowed ticks, thread nanoseconds, and output checksum.

The [paired geometric ratios and 95% whole-round bootstrap intervals](component-cpu.json)
were 0.9684 [0.9070, 1.0344] candidate/stock; 0.9675 [0.9036, 1.0332] and
0.9624 [0.8985, 1.0307] for the two same-stock controls; 1.1174 [1.0491,
1.1876] for deliberately extra work. Both same-stock intervals missed
the required ±2.5% bracket; the extra-work interval missed [1.08, 1.17].
The candidate interval itself crossed 1. No native speed or CPU-budget claim
is warranted. No timing rerun was substituted; numerical and instruction
results above are independent.

## Reproduce

From a checkout including the stock Git revision, on qualified GCC 14.2 /
Linux x86-64 with Valgrind 3.24 and its headers:

```sh
R=$PWD
BASE=5499056a60ee5044275ba17eb7dddded4c96e90d
H=$R/docs/experiments/cumulative-cpu-2026-11-19/held-span-and-comparator
D=$R/docs/experiments/cumulative-cpu-2026-11-19/held-crossing-pairs
python3 "$H/replay-direct.py" "$R" /tmp/cross-direct --baseline "$BASE"
python3 "$H/replay-component.py" "$R" /tmp/cross-work --baseline "$BASE"
python3 "$D/summarize-clock.py" "$D/component-cpu.csv"
```

Use new output directories, optional `--optimization O1`, `O2` or `Og` for
the other direct configurations, and the documented Valgrind path options for
a relocated installation. For the optional clock protocol on a *new* machine,
copy `component-clock.cpp` into the new O3 direct output directory, compile
it there against `pstock.o pcandidate.o` with the above GNU flags and
`-DCS80_ENABLE_BORROWED_HELD=1`, run with `taskset -c <available-CPU>`,
save stdout, and apply `summarize-clock.py`. Do not treat failure of the
controls as a measurement of a slowdown or speedup.

## Isolated module confirmation

The [64-frame native VST3 host work receipt](vst3-work.json) compared the
same baseline to an oscillator-only **hybrid Release**: the private stock
core was rebuilt with the shipping Release commands and linked against
unchanged wrapper/JUCE inputs, producing a byte-identical baseline module.
Only the guarded oscillator object was replaced to make this isolated
candidate. The default GCC 14.2 x86-64 pair was not a physical CPU timer
or a clean build of other concurrent changes.

| 48 kHz, four genuine 64-frame callbacks | Guest instructions | Guest reads | Guest writes |
|---|---:|---:|---:|
| Fresh quiet | Unchanged | Unchanged | Unchanged |
| Eight equal held notes | −1.384% | −0.502% | −0.445% |
| Eight varied held notes | −1.377% | −0.501% | −0.445% |
| Varied wake | −0.976% | −0.365% | −0.303% |

All repeated prefixes and next-callback additivity controls matched.
The independent [27 native musical histories](vst3-native.json) matched
82,944 callbacks and 3,286,656 stereo frames per arm, including public
state, FP traces and latency. Separate [direct interruptions and linked
renderer continuations](vst3-traps.json) matched 17,019,552 bytes in the
22-case render fixture, including seven real traps per arm. Those checks do
not establish all possible states or a real-time physical speedup.
To repeat, the [archived host commands](../resumed-exact-hotpaths/harness/REPRODUCE.md)
take actual stock and candidate ELF paths; the interruption fixture is
`replay-traps.py` in the sibling `held-span-and-comparator` directory.
