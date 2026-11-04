# VCA tail modulation qualification, 2026-11-01

Audio: `83b7f7670f000a95c6b44f96ffb0df3894755d14`.
Separate V2 pin: `14ef653ed60be67ff98656966f9181c5e9b6b4fc`.
Third STATUS qualification: `af943c27b9c4012cf128a0e0063bdfcdcb3edf33`.

The [report](../../vca-tail-modulation-20261101.md) explains the source
mechanism, scope, preregistrations and unchanged original click requirements.
This bundle records software tests, not hardware measurements.

## Outcomes

- Baseline engine witness: 618 checks, 92 failures.
- Current engine witness: 618 checks, zero failures.
- Direct mono/held-gate ownership: 6 checks, zero failures.
- Existing VCA primitive and idle/panic/reset suites: 59 + 361 checks, zero failures.
- Fourteen source mutations rejected; unmodified/restored controls green.
- Non-strict against the preceding pin and strict against the new pin both exit 0;
  all four byte comparisons equal.
- Three before and three after benchmark rounds completed, 27 cases / 51 rows
  each. They are contended local diagnostics, not a performance qualification.

## Files and capture identity

`before-extended.log` is the final old-engine witness; `before.log` preserves
the earlier 576-check stage before the additional boundaries were registered.
`after.log`, `scheduling.log`, and `positive-summary.json` cover the current
624 assertions. `vca-primitive.log` and `vca-idle.log` preserve the existing
420 assertions. `mutations/qualification.json` lists every result, source
hash, selected driver and completion; patches/build/run logs are retained
beside it. `production-change.patch` contains the two-file source delta.

`compatibility.json` identifies both gate invocations. The strict log and
`strict-receipt.json` retain the published pins and independent saved-file
comparisons. The temporary local workflow pin was restored afterward.
The production source, new test drivers, contract, and mutation-runner blobs
match the published audio commit exactly. The published CMake also supplies
`ig00156-176400.json` to the unrelated nodal phase test; integration runs that
stronger invocation. The new tail-test target definitions match.
The unused untracked header from abandoned preregistration `739b7c4` is not
compiled by any target or included in any qualification commit or manifest.

The three before timing captures are the preceding correction's after rounds:
[round 1](../vca-idle-handoff/after-benchmark-1/results.csv),
[round 2](../vca-idle-handoff/after-benchmark-2/results.csv), and
[round 3](../vca-idle-handoff/after-benchmark-3/results.csv).
Their metadata/completion records and
[environment](../vca-idle-handoff/environment.json) remain in that immutable
bundle. This folder holds all three new after captures and their metadata.
The caller-local source SHAs map to canonical audio SHAs in `manifest.json`.
The baseline benchmark covers Legacy/Pure and does not activate modern pan.

## Reproduction from the repository root

```bash
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --target \
  cs80_vca_tail_modulation_tests cs80_modern_tail_scheduling_tests
ctest --test-dir build-core --output-on-failure \
  -R 'cs80_(vca_tail_modulation|modern_tail_scheduling)_tests'
python3 tests/click/run-vca-tail-modulation-mutations.py \
  --source . --evidence /tmp/new-vca-tail-mutation-evidence
scripts/check-compat-gates.sh --strict-v2
```

Full core/plugin CTest and the final all-rate engine audit are separate
integration artifacts. This focused bundle does not substitute for them or
claim completion of the original raw phrase-score targets.
