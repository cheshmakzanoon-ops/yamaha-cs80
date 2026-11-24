# A29 — frozen .7 native content contract

This is a source-specific **native content check**, with the existing parameterized A52 parser and archive binder reused unchanged. It does not qualify a platform or release. The fixed context is revision `2026.11.7`, source `c5549f7d095a5c5f49a5ad22ec0ccc819245d201`, tree `469244af34233cfa145537266efd29024b66abba`, run `45385716988`, attempt `1`.

## Derive the complete expected names

From the repository root:

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a29/verify-native-contract.py \
  --repo . --describe --out /absolute/evidence/contract-new.json
```

The checked-in [contract.json](contract.json) is this derivation, not an execution result. Its complete name sets come from the four retained `.4` native observations in the frozen source, plus the exact 22 common CMake additions through `.6`, plus the four common `.7` additions. It pins all three CMake snapshots and the current workflow/UI identity inputs. The result is Linux **613**, Windows **255**, Apple Silicon **277**, Intel Mac **277**. Equal totals alone are insufficient: replacing a missing required test with a different name is rejected.

## Parse and bind actual evidence

Copy [native-inputs-template.json](native-inputs-template.json) into the actual evidence directory and fill all original ZIP paths. Fill each native entry's `api` and `skip_review` paths as well. Relative paths resolve against that manifest. The parser tolerates these two extra fields; A29 requires them. Use the **single complete artifact object extracted from the authenticated run artifact collection** for each `api`, not a list wrapper. A direct single-artifact GET is not supported by the current GitHub fetch wrapper. Preserve the original collection, acquisition context, producer log and upload record separately.

For the final cross-platform observation, use the unchanged `.6` parser:

```bash
python3 -B docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py \
  --inputs /absolute/evidence/native-inputs.json \
  --out /absolute/evidence/native-observations.json --require-complete
```

Run the unchanged A52 binder separately for each target, with `--source c5549f7d095a5c5f49a5ad22ec0ccc819245d201 --run-id 45385716988 --attempt 1`, the actual `--native-zip/--native-api`, `--validator-zip/--validator-api`, and those observations. See the [A52 usage](../../../2026.11.6/verification/a52/USAGE.md) for its exact invocation. It binds both archives and every parsed member; its attempt argument records context and does not prove the original producing attempt.

## Review skips and enforce the native contract

Use [skip-review-template.json](skip-review-template.json) for each target. Copy `formal_skips` and the complete `nested_python_skips` object from actual parser output. Inspect their source conditions and original LastTest output, explain each exemption in `review_note` or additional review fields, and only then set `reviewed: true`. Preserve nonverbose-output limitations; do not invent a runtime skip message that was not retained. Formal CTest skips and nested unittest skips are separate counts. The only formal capability cases permitted by this contract are `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`; a new `.7` test may not be skipped.

```bash
python3 -B docs/preset-evidence/2026.11.7/verification/a29/verify-native-contract.py \
  --repo . --inputs /absolute/evidence/native-inputs.json \
  --out /absolute/evidence/native-content-contract.json
```

Omitting `--target` requires exactly all four native targets. For an explicit preliminary single-target check, provide a one-target manifest and `--target macos-arm64` (or the relevant target). For a single-target A52 parse, supply that target in both native and validator lists and omit `--require-complete`. Every output path must be new.

A29 reparses the original native ZIP with the pinned A52 parser, reuses the pinned binder's ZIP/API/member checks, compares the complete source-derived name set, rejects failed/unknown/disabled results, checks LastTest full-session framing and numbering, requires exact reviewed skip observations, checks JUCE/source text, and requires the completed `instrument body host identity and recall` phase and terminal UI success. It rejects expired retention metadata. It does **not** authenticate caller-provided API JSON, establish original attempt/process exits, or accept validators/packages/auditions/distribution.

## Retained actual Apple Silicon exercise

[macos-arm64-native-contract.json](macos-arm64-native-contract.json) verifies actual artifact `14918856035`: 277 registrations, 275 passes, two reviewed formal skips, five separately reviewed nested Python skips, no failures/unknowns/disabled, and all four new tests present and passed. [macos-arm64-observations.json](macos-arm64-observations.json) also observes five green validator lanes; [macos-arm64-parser-archive-binding.json](macos-arm64-parser-archive-binding.json) binds both archive/member identities. These are preliminary content/identity results; all platform/release qualification flags remain false. The actual input manifest points to root-acquired downloads and retained metadata, and can be adapted to a fresh local download directory without changing source identity.

## Verification

The retained [contract-tests.log](contract-tests.log) records ten synthetic test methods, including a complete fixture and rejection of same-count name substitution, missing targets, failed/unknown/disabled results, a skipped new test, missing LastTest completion, a JUnit skip hiding LastTest failure, missing UI body phase, unreviewed/drifting formal or nested skips, source/run/hash/expiry mismatch, wrong JUCE source, and output replacement. The retained [a52-reuse-tests.log](a52-reuse-tests.log) records all seven unchanged A52 test methods passing. These fixtures execute no product binary.

The helper/test hashes and source hashes are in [helper-hashes.json](helper-hashes.json). The broader acceptance contract and remaining independent gates are in [the A29 review](../../reviews/a29.md).
