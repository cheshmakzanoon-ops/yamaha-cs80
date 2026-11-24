# Native and validator observation binding — 2026.11.6

These are offline observation and identity tools. **They never qualify a platform or release.** The intended source is `fcc3c8e055223fe4f786ca162ff46076c33471c8`, native run `45366603800`, attempt 1. No actual native artifact is accepted by this preparation.

## Why a separate parser is required

The frozen `.4` parser has a hard-coded 13-lane roster; its JSON input can remap member paths but cannot add lanes. This separate copy adds `au-gui-seed-c76` on both Mac targets, with console `pluginval-au-seed-c76-regression.log`, report `pluginval-reports/cs80-au-seed-c76-regression`, fixed seed `0x3c99748`, and workflow step `Validate AU GUI regression seed 0x3c99748`. Its roster is **3 Linux + 2 Windows + 5 Apple Silicon + 5 Intel = 15 lanes**. Each console and report must also contain exactly one `Validation started` marker. The existing strictness, program count, terminal-success, GUI coverage, fixed-seed, Linux child-exit and preserved-retry observations remain.

The new parser SHA-256 is `f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846` (18,241 bytes). The binder pins these exact bytes, requires the observation's parser hash and byte count to match, and also verifies the unchanged historical parser from the requested Git source. The new analysis helper is **not a compiled-product input** and need not exist in the binary source commit. Do not modify it without explicitly updating/reviewing the binder pin.

## Parse the downloaded ZIPs

Copy [inputs-template.json](inputs-template.json) to the actual evidence directory and replace every placeholder archive path. Relative paths resolve against that copied JSON. Supply original artifact ZIPs, not unpacked directories; the binder requires archive identity. Keep API metadata, original job logs and step/attempt records separately. Run from the repository root:

```bash
python3 docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py \
  --inputs /absolute/evidence/native-inputs.json \
  --out /absolute/evidence/native-observations.json --require-complete
```

For a single-platform preliminary observation, include that platform in both lists and omit `--require-complete`. A zero exit means only the supplied observations are green. The parser deliberately leaves `release_qualified: false`, CTest process exits unknown and validator process exits unknown. Require all four targets for the final cross-platform report. Missing members or corrupt data produce exit 2; observed test/lane failures produce exit 1. Existing output paths are refused.

## Bind observations to the actual ZIP bytes and API objects

For each target, use the same actual ZIPs that were parsed and its **single artifact API objects**, not their list wrappers. Authenticate those receipts through GitHub independently. Example for Apple Silicon:

```bash
python3 docs/preset-evidence/2026.11.6/verification/a52/bind-parser-archives.py \
  --repo /workspace/scratch/b9c11dd92abc/yamaha-cs80 \
  --source fcc3c8e055223fe4f786ca162ff46076c33471c8 \
  --run-id 45366603800 --attempt 1 --target macos-arm64 \
  --observations /absolute/evidence/native-observations.json \
  --native-zip /absolute/downloads/cs80-macos-15-test-evidence.zip \
  --native-api /absolute/evidence/macos-arm64-native-api.json \
  --validator-zip /absolute/downloads/cs80-pluginval-macos-15.zip \
  --validator-api /absolute/evidence/macos-arm64-validator-api.json \
  --out /absolute/evidence/macos-arm64-parser-archive-binding.json
```

Repeat with `linux-x86_64`, `windows-x86_64` and `macos-x86_64`, using their exact inputs. The binder checks API route/name/source/run/hash/byte count, parser archive hash/byte count, and every parsed member hash/byte count—including JUnit, LastTest, JUCE provenance, version receipt, all console/report pairs, Linux child-exit logs and retained retry logs. The ZIPs are snapshotted once for hashing and inspection. Missing JUCE provenance is rejected here even though the parser's limited observation function tolerates its absence.

Successful identity binding deliberately leaves `native_results_accepted`, `original_attempt_binding_verified`, `platform_qualified` and `release_qualified` false. Consistent caller-provided JSON is not authenticated provenance. The `--attempt` number records context only; it does not establish the producing attempt.

## Acceptance gates still required

- Read authenticated run, original job and original step metadata for the exact source and attempt. Require completed successful producer build/test/package/upload and validator steps, with original timestamped commands and unsuppressed exit propagation. A `SUCCESS` line alone cannot prove normal process termination.
- Bind artifact IDs and actual ZIP sizes/digests to successful upload steps and their timestamps. API artifact metadata does not itself identify an attempt. Re-fetch expiry/current metadata at publication.
- Review actual registered CTest names and counts against the configured source/platform. The template lists all seven `.6` additions and the retained `.5` family gates. **No guessed registered totals are supplied.** Empty or disabled inventories must not qualify just because parsing succeeds. Require zero failed/unknown cases and explicitly review every formal skip and disabled case.
- Count nested Python skips separately from CTest skips; JUnit and LastTest copies and explicit unittest skip lines must not inflate counts. The parser uses LastTest terminal summaries for nested counts. Internal assertions are not additional CTest executions.
- Verify the pinned clean JUCE revision `c352e2489000ffeb9e397051377d75eef2efe641`, version `8.0.11`, and source/configure/build order. Merely having a provenance text file is insufficient.
- Require all fifteen lane observations green, actual original normal-zero exits, expected source-pinned headless/GUI commands, 523 programs and strictness 10. A failed randomized lane cannot be cleared by a passing fixed-seed lane. Inspect every retained retry and its eligibility; the parser does not establish original retry eligibility.
- Bind actual distributed binaries to validated input payloads using [A51's payload instructions](../a51/USAGE.md), including full macOS AU directory comparisons. These parser bindings do not establish binary equivalence.
- Verify installation bundle inventory/architecture/source and the actual audition archive separately; confirm the public download routes before claiming them tested.

## Synthetic verification

```bash
python3 docs/preset-evidence/2026.11.6/verification/a52/test-evidence.py
```

Seven methods cover all fifteen synthetic lanes; formal versus nested skips; missing/malformed/DTD/aggregate-disagreeing JUnit; duplicate/disagreeing LastTest; failure observations; absent/wrong fifth lanes and seed/strictness/program/GUI/start/success conditions; Linux child crash and retry gaps; exact ZIP/API/member/parser identity; missing provenance; CLI completeness/output preservation; and rejection of a modified parser. These are inert fixtures, not native execution.
