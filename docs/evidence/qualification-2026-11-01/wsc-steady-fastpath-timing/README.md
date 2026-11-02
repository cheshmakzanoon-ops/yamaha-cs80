# WSC cached-interior scheduler experiment — exact-state and same-host CPU result

**Result: safe numerical experiment, insufficient CPU gain; do not promote.**
The frozen original-loop WSC oracle passed with zero mismatches, all local core tests
passed, and the legacy, V1 and strict V2 audio captures were byte-identical.
The three-before/three-after whole-engine comparison produced a **−1.98% median
matched-fixture change** in held RTF (lower is better), with five of eleven pairs
unchanged or slower to the displayed precision. Variability precludes attributing
a reliable engine-wide improvement of that size to the shortcut. Neither hard
target is achieved: eight-voice held RTF must be **0.15–0.20**, and quiet-idle
RTF must be **≤0.02**. This optimization does not run in the quiet-idle path.

## Fixed code and test identity

| Role | Immutable identifier |
| --- | --- |
| Original production source/three-before | `7c864f266a939aa2a42422440dd4c1c0cabe3814` |
| Original `src` tree, also test-only prereg source | `7dd159d491012fd47b46c0aa651e86ea959ca289` |
| Preregistered independent reference and zero-bit threshold | `5809a52805983716a4e8f566af044ad0437ea66b` |
| **Local only** WSC production candidate/three-after | `28334e31c2eaa1fd156003377369b2c98d3433c8` |
| Candidate `src` tree | `b9c6a0d26ad0228eafe3d0e2dc319964f68e85f8` |

Production diff: one file, `src/core/ReconstructedOscillator.h`, 20 added lines,
one removed loop header. An unchanged control/cached branch with positive
finite charge and a strictly interior physical/branch boundary advances exactly
one sample with the original `advance`, remaining countdown and endpoint
expressions. Reset, control, event, invalid, boundary and unknown-cache cases
use the original loop. No new circuit law, approximate kernel or altered audio
epilogue is involved. The original software reference is test-only and frozen.

The source was compiled `Release` with GCC 13.3.0 (C++20, `-O3`) on the same
9-vCPU KVM Intel Xeon Platinum 8573C host, Linux `6.18.57`. Both sets used
the identical `cs80_source_factory_benchmark` full baseline suite at host
48,000 Hz, 256 frames, 1,024 callbacks per warmed-held phase, 1-second warmup,
seed `803010`, 11 source two-rank preset pairs with eight held notes, legacy
anchors, selector movement, quiet/noisy idle and release tails. These are core
callback timings with no JUCE, DAW or driver; their metadata explicitly marks
them **not** hardware equivalence or real-time qualification. Root coordinated
exclusive local CPU timing windows with other agents for both three-round sets;
the virtual host still exhibits visible scheduling variance and long-tail
outliers. The before/after sessions are serial, not randomized interleavings.

The exact commands were repeated with `N=1,2,3` in their respective unchanged
source worktrees:

```sh
# Before: checkout 7c864f2; all three full rounds predate the production edit.
./build-core/cs80_source_factory_benchmark \
  --out artifacts/cpu-baseline-wsc-20261101-rN \
  --source-commit 7c864f266a939aa2a42422440dd4c1c0cabe3814 \
  --callbacks 1024 --rate 48000 --block 256

# After: checkout 28334e3; same host, compiler, build mode and suite.
./build-core/cs80_source_factory_benchmark \
  --out artifacts/cpu-wsc-after-rN \
  --source-commit 28334e31c2eaa1fd156003377369b2c98d3433c8 \
  --callbacks 1024 --rate 48000 --block 256
```

Every raw `results.csv`, `metadata.json`, and `completed.json` file for all six
rounds is preserved in `before-r1/` through `before-r3/` and `after-r1/`
through `after-r3/`: **51 rows and 27 cases per round, no excluded row or
outlier**. The manifest is in the benchmark's metadata; each run is marked
complete and has source fingerprint `a95212770767733c`. All 51 fixture keys
have exactly the same `checksum`, `peak` and `rms` textual values in every
before and after run; this is additional evidence, not a substitute for the
separate binary audio compatibility captures.

## Whole-engine timing, all raw observations retained

Values are realtime factors (RTF), so 0.60 is 60% of the one-core callback
budget. “Held median” means the median of the eleven **individual** warmed-held
source-pair median RTFs for that round. “Worst p99” is the largest per-pair p99
RTF in that round, *including* the 63.384869 baseline outlier. “Misses” are
counted callbacks over the 5,333.333 µs deadline among 11×1,024 held callbacks.

| Round | Source held median | Source held pair min–max | Worst held p99 | Held deadline misses / 11,264 | Legacy held median | Source quiet-idle median / p99 | Moving source held median |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Before 1 | 0.870989 | 0.671121–1.207045 | 2.371858 | 3,784 | 0.312110 | 0.131382 / 0.343693 | 1.157620 |
| Before 2 | 0.827203 | 0.682458–1.026670 | **63.384869** | 2,511 | 0.310595 | 0.063492 / 0.257549 | 0.988649 |
| Before 3 | 0.793311 | 0.656408–0.971989 | 2.077146 | 1,392 | 0.300198 | 0.061296 / 0.236311 | 0.964218 |
| After 1 | 0.767956 | 0.636229–0.997724 | 2.937347 | 1,860 | 0.312603 | 0.059903 / 0.160436 | 0.979043 |
| After 2 | 0.758176 | 0.622172–1.073733 | 2.700633 | 1,837 | 0.304918 | 0.059350 / 0.211214 | 0.954903 |
| After 3 | 0.844195 | 0.649962–1.070819 | **4.791063** | 2,780 | 0.320716 | 0.059405 / 0.236089 | 1.190005 |

Across the 33 warmed-held source pairs in each group, the untrimmed worst
p99 was **63.384869 before** and **4.791063 after**; the large drop cannot be
claimed as a scheduler improvement because it is dominated by one extreme
baseline observation on a virtual host. Total held deadline misses were
**7,687 / 33,792 before** versus **6,477 / 33,792 after**, still thousands of
real-time failures. The unmodified legacy-anchor medians overlap closely
(0.300198–0.312110 before, 0.304918–0.320716 after); this detects no gross
host speed-scale change but does not eliminate within-run virtualization noise.

The primary paired comparison uses each preset pair's median over its **three
complete before rounds** versus its median over **three complete after rounds**;
no fixture is dropped or weighted differently:

| Source pair | Before RTF | After RTF | Relative change |
| --- | ---: | ---: | ---: |
| String 1 / String 2 | 0.671121 | 0.673272 | +0.320% |
| String 3 / String 4 | 0.839579 | 0.749347 | −10.747% |
| Brass 1 / Brass 2 | 0.888044 | 0.774725 | −12.760% |
| Flute / Brass 3 | 0.808648 | 0.742981 | −8.121% |
| E. Piano / Bass | 0.870989 | 0.852144 | −2.164% |
| Clavichord 1 / Clavichord 2 | 0.832324 | 0.815830 | −1.982% |
| Harpsichord 1 / Harpsichord 2 | 0.770350 | 0.636229 | −17.410% |
| Organ 1 / Organ 2 | 0.757408 | 0.760392 | +0.394% |
| Guitar 1 / Guitar 2 | 0.793847 | 0.851708 | +7.289% |
| Funky 1 / Funky 2 | 0.818558 | 0.821833 | +0.400% |
| Funky 3 / Funky 4 | 0.971989 | 0.997724 | +2.648% |
| **Median of 11 matched relative changes** | | | **−1.982%** |

Five of eleven pairs move upward; the apparent small aggregate effect is
not a robust grounds for promotion. After source quiet-idle medians stay
0.059350–0.059903 (5.94–5.99% of realtime), still about **3×** the ≤2% target.
The earlier before idle 0.131382 is retained, not trimmed or interpreted as an
oscillator benefit. Post-change held pairs still require about 0.62–1.07 RTF
per round, well above 0.20. No shipped real-time or host qualification is
inferred.

## Numerical, mutation and compatibility gates

* Independent frozen `tests/reference/WscLoopOriginal.h` oracle, rebuilt after
  restoring both temporary source mutants: **4 rates (88.2, 96, 192, 384 kHz),
  56,576 ticks, 12,038 events, 45,311 no-event ticks, 19,630 branch searches,
  44,294,052 checked fields, 0 bit mismatches**. This includes full event
  arrays, continuation state, residuals, flags, idle, restore and copy.
* Both preregistered witness mutants (`CS80_WSC_ORACLE_MUTATE_STATE`,
  `CS80_WSC_ORACLE_MUTATE_EVENT`) compile then fail their one-bit assertions,
  not their build. Actual-candidate eligibility-guard removal separately fails
  with 1,290,043 bit mismatches (events 12,019 vs 12,038); branch-countdown
  `dt * 0.5` mutation separately fails with 1,103,506 bit mismatches. Both
  temporary edits were restored before the final correct compile and commit.
* Full local `ctest --test-dir build-core --output-on-failure --parallel 2`:
  **107/107 PASS**, including oscillator phase, alias, idle, click, source
  integration, and the registered frozen oracle. No test threshold was relaxed.
* Exact original `scripts/check-compat-gates.sh --strict-v2` procedure was
  attempted. The helper halted because one newly linked *pinned legacy probe*
  unexpectedly lost its executable bit (0644) between its default and tails
  captures; this was **not** an audio mismatch. After restoring only that
  generated binary's executable bit, all of the same original compile,
  independent render and `cmp -s` operations were run manually: original
  **legacy default and tails** pinned `d56911a7c5f6953cd244457226b126cc567a538b`,
  immutable **V1 circuit bytes** pinned `22d6a32c48c85f9b813506edcd856f73f4293c2d`,
  and **strict V2 full-engine audio** pinned `83b7f7670f000a95c6b44f96ffb0df3894755d14`:
  all byte-identical (`cmp -s` exit 0). No source pins, reference truth or
  thresholds changed. The completed *manual equivalent* is what passed; the
  helper itself did not reach its final exit-zero status in this environment.

## Raw CSV SHA-256 provenance

| File | SHA-256 |
| --- | --- |
| `before-r1/results.csv` | `8107008d718f1ebf19f5444e04e6b11aa93cc544c9631bb47b2e7c32ca867e44` |
| `before-r2/results.csv` | `91be623d38c05a03de7d437b008d462cccc6b305b5895420acefac3c18efcb6d` |
| `before-r3/results.csv` | `f18b22feef4a899acbf9d024a4f4746d261c71cc163aaf50bc7c95fe4cc080ef` |
| `after-r1/results.csv` | `e2b9254027e4e9a1eda9480239e440a54ca7ade46b3af6e9aa98d51afb8306be` |
| `after-r2/results.csv` | `a18c88dd1efb7e6cc97168a3688a9413b8e7c8d921d0f5d700733a6fa4c28af3` |
| `after-r3/results.csv` | `63675ef5f0192b34c5dc8398d213362f11f9ebf3c7d51f070662f65cc45cabfb` |

**Decision:** retain the candidate and all evidence only on isolated CPU
development history for review; **do not merge, publish production source,
mark CPU green, or revise circuit qualification pins**. The next experiment
needs a larger profiled bottleneck: first measure actual warmed-held
`Ig00156PhaseCorrection` coefficient-revision rates before assuming expensive
`set()` recalculation survives the one-second warmup; then profile its
per-sample 4×4 RHS/Newton/`fastTanh` solver and use an independent zero-bit
oracle for any structural acceleration. Separately instrument source quiet-idle
`tickGlobals`, touch/modulation and FX continuation; WSC tick is skipped in
quiet-idle and cannot solve the 2% bound. Preserve the legacy/V1/V2 full-byte
oracles and original residual/alias/click limits in either path.
