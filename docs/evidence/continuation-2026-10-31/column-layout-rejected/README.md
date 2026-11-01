# Rejected IG00156 coefficient column-layout candidate

The candidate was rejected. The production `Ig00156PhaseCorrection.h` remains
byte-identical to the frozen scalar header. This investigation changes tests and
evidence only; it makes no audio, CPU-target, work-reduction or hardware-fidelity
claim. No circuit V2 re-pin is appropriate for a rejected source patch.

## Contract and provenance

The evaluated source basis is `d06648c386486cf2630de6e41195ddd5d4360ff0`.
The exact numerical contract was published in
`c56726fd8ad12b78edf745b319a9ddc5a6696f2f` before this implementation. Its
column-layout work contract and implementation clarification were also published
before evaluation:

- [`tests/cpu/exact-scalar-kernels.json`](../../../../tests/cpu/exact-scalar-kernels.json)
- [`tests/cpu/phase-column-layout-work.json`](../../../../tests/cpu/phase-column-layout-work.json)
- [`tests/cpu/phase-column-layout-implementation.json`](../../../../tests/cpu/phase-column-layout-implementation.json)

The executable comparator was published separately in
`08607ebd915a6dcd5cbfdf2d8d53286c8c227102`. It compares the production stage
and direct phase kernel with independent, hash-sealed copies of the complete
scalar math dependencies. Zero-bit equality includes physical nodes, companion
histories, input histories, nonlinear slopes and accepted currents, residuals,
Newton corrections, state transitions, nonfinite payloads, floating-point
control state and every supported rounding/denormal combination. The tolerances
and frozen oracle were not changed to accommodate the candidate.

`candidate.patch` contains the entire rejected source change. It stores the
right and inverse matrices by column through compile-time address accessors,
adds three positive-zero padding cells to the inverse storage, and retains the
original arithmetic expressions and accumulation order. Reversing only these
address/storage substitutions and the padding initialization recovers the frozen
phase header byte for byte. Source identity is useful review evidence but does
not guarantee identical compiled floating-point behavior.

`source-audit.json` records an independent reversal of the archived patch and
the exact resulting source hash. `result.json` records the compiler, source
hashes and numerical outcomes.
`numeric-commands.json` contains the executed commands and stdout/stderr hashes;
`baseline-source-sha256.json`, `candidate-source-sha256.json` and
`frozen-oracle-manifest.json` identify every numerical input. The test build used
GCC 13.3.0 on x86_64 with `-std=c++20 -O3 -DNDEBUG`.

## Fresh baseline and candidate results

Each count is a scalar bit/state assertion; failing assertions are not distinct
samples. Every suite was run against both independently built versions.

| Suite | Assertions per version | Baseline failures | Candidate failures |
| --- | ---: | ---: | ---: |
| Fixed controls and component grid | 576,843,520 | 0 | 0 |
| Moving cutoff/Q | 49,807,360 | 0 | 0 |
| Copy/reset/restore, continuation and exceptional input | 73,759,498 | 0 | 113,616 |
| Individual input/state coordinates | 21,405,696 | 0 | 0 |
| Rounding/denormal modes and transitions | 59,498,228 | 0 | 3,360,312 |

The first recorded state mismatch is a NaN-sign propagation difference: the
candidate returns LP bits `0xffc12345`, while the baseline returns
`0x7fc12345`. The first recorded floating-point-mode mismatch is finite: under
`FE_DOWNWARD`, at 88.2 kHz, LP node bits differ by one ULP
(`0xbe80ef21dcf95b6a` versus `0xbe80ef21dcf95b69`). The logs retain the first
24 failures from each failing suite and the full assertion/failure totals.

The change in generated arithmetic caused by the storage layout is the working
explanation for these differences; this report does not claim a separately
proven compiler-instruction diagnosis. The observed failures alone invalidate
adoption under the pre-registered contract. Passing ordinary nearest-rounding
finite traces is insufficient. The complete baseline passes, so the candidate
failures cannot be dismissed as a pre-existing comparator failure.

The candidate was restored before any integrated performance trial. Neither the
scalar-layout fallback nor the proposed column-work observers are qualified by
this investigation. No speed measurement was made or inferred from the rejected
layout.

## Comparator mutation qualification

The final fresh run built five representative mutations of actual production
source, with the frozen oracle untouched. Each mutant compiled successfully and
exited normally with code 1 through the named numerical assertion; crashes,
compile errors and timeouts are not counted as kills.

| Actual-source mutation | Suite | Named rejecting observable | Failing assertions |
| --- | --- | --- | ---: |
| Reuse coefficients after the first key | Moving | Physical stage node | 16,805,306 |
| Move one right-matrix coefficient by one ULP | Row coordinates | Physical stage node | 113,504 |
| Drop causal input history during restore | State | Exported input history | 10,038,200 |
| Stop Newton at `1e-7` instead of `1e-10` | Moving | Physical stage node | 16,338,622 |
| Reverse a four-term row accumulation | Row coordinates | Physical stage node | 3,121,973 |

Both the fresh positive control and an independently rebuilt restored production
copy pass all five complete suites: **781,314,302 assertions, zero failures**
per control. No acceptance threshold or frozen-header byte was modified. This is
representative qualification of the filter comparator, not a claim that every
prospective column-work or oscillator mutation has been implemented.

`comparator-mutations/` preserves every final command, source-hash manifest,
compile log, numerical log and completion result. `diagnostic-attempts/` also
retains the two preliminary runner-diagnostic failures: both mutants already
failed correctly, but the runner initially searched for a different field than
those included in the first 24 printed errors. The diagnostic labels were
corrected to the actual physical-node observables; the comparator and its rules
were never altered.

## Reproduction

Build and run the production exact comparator through CMake:

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --target cs80_exact_scalar_filter_tests -j2
ctest --test-dir build-core --output-on-failure -R '^cs80_exact_scalar_filter_'
```

To repeat the actual-source mutation qualification in fresh temporary builds:

```sh
python3 tests/cpu/qualify-exact-filter.py --out /tmp/cs80-exact-filter-evidence
```

The output directory must not already exist. The runner copies production
headers and mutates only those copies; the immutable oracle and working source
remain untouched. The archived candidate can be applied in a separate disposable
checkout at the recorded basis for reproducing its rejection; it must not be
mistaken for an accepted implementation.

## Physical-model status

This rejected CPU experiment makes no change to the independently qualified
phase correction or its original full-grid tolerances. The inherited phase
correction and coefficient-cache evidence remain documented in
[`docs/IG00156_PHASE_CORRECTION.md`](../../../IG00156_PHASE_CORRECTION.md).
The ADJ p11 HP setup fixture still predicts approximately **1.692 Vpp**, below
its **3.3–5.3 Vpp** service interval. LP predicts **1.648 Vpp**, within its
**1.2–3.2 Vpp** interval. These are model predictions against the manual setup,
not hardware measurements. The HP source/setup ambiguity remains open; neither
the phase correction nor this rejected layout was fitted to that amplitude.
