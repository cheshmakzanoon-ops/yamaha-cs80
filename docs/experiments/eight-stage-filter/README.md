# Eight-double IG00156 solver: direct research evidence

**Research only. No Engine integration, native-plugin acceptance, CPU saving,
promotion or timing is established by this package.** Integration/native evidence,
if added separately, belongs in its own subdirectory and must not be attributed to
these direct tests.

## What is different

This prototype widens the existing four-double solver to eight doubles. It retains
the equations, tanh polynomial, convergence masks, five-iteration cap, state writes
and optional recovery policy. It is not the older static EVEX-256 experiment,
which only exposed more registers to width-four arithmetic and did not execute its
specialized object. The bounded historical search found no equivalent executed
native eight-stage rejection; it was not an exhaustive history audit.

The applied patch adds `Ig00156Eight.h/.cpp` and one inert private friend declaration.
No persistent fields or original solver arithmetic change. The reference is **two
actual original four-stage blocks**, in order. Each reference rejection uses the
unchanged caller scalar fallback. Eight-stage rejection is checked for untouched
state and sentinel outputs *before* those two original quartet paths are used.

## Observed direct results

| Build | Cohorts | Real eight-wide successes | Eight-wide declines | Stock quartet fallback decisions |
|---|---:|---:|---:|---:|
| Normal | 12,288 | 12,276 | 12 | 12 |
| Recovery enabled | 12,288 | 12,191 | 97 | 98 |

Each row compares 24,576 original reference quartet calls, 1,474,560 output/memory
fields, every byte of each complete 296-byte Stage, aggregate SSE/x87 status and
errno. Actual ZMM add/multiply/divide instructions were assembled and executed;
six ZMM divide sites and no FMA instructions were found. Static kernel sizes were
4,886 bytes for eight and 2,895 for four. **These are not performance measurements.**

Coverage includes eight rates from 8 kHz through 1.536 MHz, HP/LP networks, varied
cutoff/Q, independently driven lanes, an all-zero quartet beside a driven quartet,
small alternating signals, signed-zero/float-denormal inputs and subnormal restored
histories/slopes. Different convergence behavior is exercised by construction;
exact iteration counters were not recorded.

Controls for these passing runs were x87 CW `0x037f` and initial MXCSR `0x1f80`,
`0x9fc0`, `0x1fc0`, or `0x9f80`, covering all four FTZ/DAZ combinations with nearest
rounding and masked SSE exceptions. `errno` was initialized to `EDOM` separately
before the reference pair and candidate call. See `observed-results.json` for the
exact result lines, initial harness admission stop and qualification limits.

## Dispatch and remaining obligations

The setup-only selector requires AVX2, AVX512F, AVX512VL and AVX512DQ through the
GCC CPU/OS feature runtime. The observed host reported all four, and specialized
execution then succeeded. The per-call guard requires x87 CW `0x037f`, nearest SSE
rounding and all SSE exception masks, without changing FTZ/DAZ. All eight stage
pointers must be nonnull and distinct; the existing state/coefficient bounds remain.

The direct API supplies a **backend** strict-build guard. Any integration must also
retain the caller-local `CS80_IG00156_EXACT_BATCH_BUILD` gate, so an unsafe/FMA caller
cannot accidentally invoke a separately strict backend. False results must return
to the caller's original ordered quartet preparation/fallback behavior, not force
vector execution or reuse already-mutated stages. This package establishes no
future-rank reordering, group eligibility, clock or setter-no-op proof.

No explicit unsupported-control, actual-trap, negative-compiler or unavailable-ISA
execution matrix was run in this bound. CPU migration, other compiler/runtime
families, concurrent state observers and broader adversarial restored states remain
unqualified. Recovery was tested as a separately defined build, not silently
substituted for normal shipping behavior.

## Portable reproduction on a matching toolchain

Prerequisites: Linux x86-64, GCC 14.2/C++20, Python3, Bash and Git. Positive execution
requires CPU and OS support for the four ISA features above. The program exits77
before specialized work when selection is unavailable; that is **not a pass**.
No external library/core archive or full plugin build is required.

From this directory, explicitly run:

```sh
bash reproduce.sh /path/to/source-checkout /path/to/new-empty-work-directory
```

The script verifies the six transitive stock source pins, copies only those files
into a new private work directory, checks/applies `prototype.patch`, and verifies
the candidate pins. It never modifies the supplied checkout. It compiles original
four and friend-header four objects and requires complete object identity, then
runs the direct test with and without the recovery define.

All TUs use `-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic
-ffp-contract=off`. The specialized function target is
`avx2,avx512f,avx512vl,avx512dq,no-fma`; no global native/AVX512/FMA/unsafe flags are
used. There was no explicit `-no-pie` or vectorization-disable option. Header-only
scalar fallback is compiled under the same strict caller contract.

`direct-test.cpp` and `dispatch-probe.cpp` are byte-for-byte copies of the executed
sources. Hashes pin the prototype, original solver and relevant dependencies.
The export/reproduction script itself was checked only offline (syntax, patch and
hashes); it was not compiled or run during packaging. Historical object hashes are
receipts, not an expectation of identical object metadata on another toolchain.
No raw binaries or private absolute paths are included.
