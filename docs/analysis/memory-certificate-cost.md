# Offline verified-log cost experiment

## Preregistered candidate and gates

This follows the [sparse memory report](memory-sparse-update.md), whose measured
certificate cost still rejects audio activation. No Engine, state format,
physical model coefficient, requested node bound or existing tolerance file
will change in this experiment.

Try exact binary range reduction before the verified logarithm series: after
`x = 2^k*m`, `1 <= m < 2`, replace `m` by `m/2` and increment k when `m > 1.5`.
This operation is exact. Now `0.75 < m <= 1.5`, so the atanh coordinate
`z = (m-1)/(m+1)` satisfies `abs(z) <= 0.2`. Twelve terms have a remaining
absolute tail no greater than `2*abs(z)^25/(25*(1-z*z))`, approximately 2.8e-19
at the endpoint. Retain outward arithmetic and the remainder, with correct
sign handling for negative z. Do not assume a libm error bound or drop the tail.

Acceptance requires the existing exact-rational/high-precision primitive checks,
strict-environment tests, full 2,016-case/94-node sparse gate, failure/alias
checks, mutation controls and sanitizer/compiler profiles. Any accepted result
must retain the exact same conditional numerical meaning. Report availability
changes at tight widths rather than hiding safe rejections. Independently
review the signed-series proof and source before accepting the implementation.

Measure before/after using the same updated probe with **128 corresponding
evaluations per scenario**, including dense cold, with full output
materialization. Protocol clarification: the three solve scenarios traverse
identical input indices 0..127; certificate-only and coefficient preparation
repeat their identical fixed state/source workloads 128 times. Use the original
header as an include overlay for the baseline so only the candidate
implementation differs; repeat in reversed version order. Preserve
all trials and failure counts. Shared-machine trial means are not WCET, actual
CPU utilization or whole-instrument qualification. Keep the existing activation
blocker unless a separately qualified complete scheduling path meets its gates.

## Result: retain the offline improvement, keep the activation blocker

The centered candidate passes the unchanged numerical contract and lowers
measured warm/certificate cost in all three fixtures in both version orders.
It is still **not fast enough for the proposed audio schedule**. No Engine,
plugin, host-state or coefficient-model code is changed.

### Mathematical and numerical checks

For the exact reduced mantissa, `z` lies in `(-1/7,1/5]`. The code encloses every
signed partial-sum operation, takes the largest absolute endpoint of the
remaining odd power as the tail numerator bound, and uses a sign-specific tail
only when the whole z interval proves that sign. Otherwise it adds both signs.
The old nonnegative odd-power clamp is removed; the z-squared lower clamp is
retained because the true square is nonnegative. The maximum exact analytic
remainder is **2.7962026666666665e-19**. The original exact-rational `log(2)`
constant proof is unchanged. A largest-finite input may now have k=1024, which
is represented exactly and safely in the exponent contribution.

Independent source review and a separately written exact-Fraction oracle cover
457 reduced-mantissa/public-log cases, including negative reduced z, threshold
neighbors, subnormals and k=1024. Both excluded mantissa neighbors are rejected.
That oracle is retained in `tests/test_memory_centered_log_rational.py`; it does
not use Decimal or libm as its logarithm truth.

Four GCC 14.2 profiles (`-O0`, `-O2`, `-O3 -march=native`, and an
ASan/UBSan/float-cast-overflow/assertion build) each pass **5,478 independent
vectors and 56 boundary/environment checks**. Thirty explicit threshold-neighbor
vectors extend the prior test set. Additional 1e-15 A availability probes now
produce **860 accepted enclosures and 4 safe rejections**, versus 853/11 in the
baseline. Every accepted interval meets its requested width and oracle
containment; rejection still leaves output unchanged. This is not a promise to
resolve arbitrarily small requested widths.

The unchanged full-network gate passes **2,016 cases and 533,518 checks**,
including all 94 nodes and failure preservation. Maximum dense node difference
remains **1.56923 nV**, maximum accepted bound **6.01681 nV**, maximum cold/warm
steps **28/6**, and observed allocations **zero**. The same full gate passes
ASan/UBSan/float-cast-overflow with library assertions. LeakSanitizer remains
disabled for the sandbox; no leak qualification is claimed.

All four primitive faults remain detected (2,256 / 1,512 / 1,684 / 2,928 failed
vectors for removed outward rounding / trusted hint / inflated derivative /
wrong log(2), respectively), as do both full-network source mutations (650 and
9 failures). Fast-math is rejected. Four registered focused CTests pass,
including the new exact-rational test. Records and file identities are retained
in [the study manifest](memory-certificate-cost-study/manifest.json).

### Paired timing method and results

The same probe is built twice: baseline uses the unchanged historical header
through an include overlay; candidate uses the centered header. The three solve
scenarios now traverse **identical input indices 0..127**, fixing the earlier
report's nonidentical dense/sparse sampling. Certificate-only instead repeats
`inputs[0]` and the fixed valid base state; coefficient preparation repeats the
same fixed source. Those two scenarios each perform 128 repetitions of their
identical fixed workload, not a 128-input sweep. Both versions materialize the
same complete output sets. Each fixture/scenario has seven trial means per version. Execute
baseline then candidate, then repeat in reverse version order. The 420 raw
scenario/trial rows all have zero rejected evaluations.

Times below are median microseconds per rank update. The first pair is the
baseline-then-candidate run; the second pair is the candidate-then-baseline run,
shown with baseline first for comparison.

| Fixture | Warm baseline → centered, first order | Warm baseline → centered, reverse order | Certificate-only baseline → centered, first order | Certificate-only baseline → centered, reverse order |
| --- | ---: | ---: | ---: | ---: |
| 50 kOhm, unequal banks | 387.0 → 269.8 | 388.0 → 262.0 | 302.4 → 191.6 | 315.9 → 189.4 |
| 100 kOhm, full travel | 186.3 → 140.5 | 190.3 → 140.3 | 131.2 → 79.9 | 132.3 → 80.7 |
| 100 kOhm, unequal banks | 491.0 → 304.4 | 482.9 → 352.3 | 422.4 → 226.2 | 417.0 → 264.0 |

Across these paired fixture/order medians, warm cost is **24.6–38.0% lower**
and certificate-only cost **36.6–46.5% lower**. Cold certified cost is lower by
6.6–20.4%. These are observations on this shared machine, not universal speedup
or performance guarantees. The mathematically unchanged dense path varies from
an apparent 6.7% slowdown to a 5.8% speedup between builds; its raw rows remain
visible rather than being omitted. Fine differences and cold-start percentage
estimates therefore deserve particular caution.

Raw data: [baseline first](memory-certificate-cost-study/baseline-forward.csv),
[candidate second](memory-certificate-cost-study/centered-forward.csv),
[candidate first](memory-certificate-cost-study/centered-reverse.csv), and
[baseline second](memory-certificate-cost-study/baseline-reverse.csv).
These retain trial ranges, not just medians. Dense cold and sparse warm remain
different algorithms/start conditions; pairing their inputs does not turn them
into equal-work scenarios. Only the same scenario across baseline/candidate is
used for the percentage comparisons above. Fixed coefficient preparation,
source readout, scheduling/publication and the rest of the instrument remain
separate costs.

Even the best warm median, **140.3 us for one rank**, exceeds the proposed
8-sample/96-kHz update period of 83.3 us. Two ranks at that rate project roughly
**3.37–8.46 seconds of serial measured work per second of audio**, before the
receiver readout and instrument. This is a wall-time workload projection, not
measured CPU utilization or a WCET bound. The isolated certificate's best
median near 80 us does not establish feasibility: it omits the nonlinear update,
second rank, readout, event spikes and whole-instrument load.

The next performance work still needs to reduce or safely amortize verified
residual work, with explicit domain/event invalidation and the complete
conditional certificate intact. A faster scalar kernel alone does not qualify
causal scheduling, asynchronous publication, fallback behavior or 16-voice
loading. **Keep this path offline.**

### Reproduction and source identity

Candidate kernel: local `42c8486`, header SHA-256
`8aab3590117357c8bccf6a613b1bc4896775a1fd83f57513adf6066985736cb1`.
The test-only follow-up `9c44823` retains the independent rational oracle and
registers it; benchmark header/probe bytes are unchanged. These local commit
IDs are provenance, not claims about a release head. The content-hash manifest
is portable across integration cherry-picks.

The historical [baseline header](memory-certificate-cost-study/baseline-MemoryVerifiedDiode.h)
is retained only to reproduce the overlay benchmark, not as a second shipping
implementation. Build the two variants with the same
`-std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off` flags:

```sh
mkdir -p /tmp/memory-baseline/experimental
cp docs/analysis/memory-certificate-cost-study/baseline-MemoryVerifiedDiode.h \
   /tmp/memory-baseline/experimental/MemoryVerifiedDiode.h
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off \
    -I /tmp/memory-baseline -I src tests/MemorySparseProbe.cpp -o /tmp/memory-baseline-probe
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off \
    -I src tests/MemorySparseProbe.cpp -o /tmp/memory-centered-probe
/tmp/memory-baseline-probe /tmp/baseline-forward.csv
/tmp/memory-centered-probe /tmp/centered-forward.csv
/tmp/memory-centered-probe /tmp/centered-reverse.csv
/tmp/memory-baseline-probe /tmp/baseline-reverse.csv
```

Run `tests/run_memory_verified_diode_checks.sh [build-directory]` for all
primitive profiles, faults and the exact-rational oracle. CMake's
`cs80_memory_sparse_update_tests` and `cs80_memory_sparse_negative_controls`
retain the full network/failed-publication checks.
