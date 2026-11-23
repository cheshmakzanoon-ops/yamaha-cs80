# A34 — artifact verifier readiness review

## Decision

The three helpers match final native source `bcc86aa39a33a0162254614e5502705f0113d701` byte for byte. The supported self-tests passed once each. They are usable as **layers of release evidence**, provided the parent supplies authentic artifact API records and separately qualifies the producing jobs, attempts, native tests and original validator exits. None is a complete release gate; no helper establishes acoustic equivalence to a real instrument.

No production files were changed. This review used source inspection and the two existing self-tests only. No actual final-run ZIP was accepted during this review, and no additional synthetic tests were invented.

## Existing self-tests

| Tool | Result | Scope |
| --- | --- | --- |
| `verify-install-artifact.py --self-test` | Exit 0; 4 positive target fixtures; 8 rejected mutations | Four advertised native target headers; each incorrect API digest and coherent wrong-CPU payload rejected. Inert header fixtures. |
| `verify-validator-payload.py --self-test` | Exit 0; 3 positive fixtures; 24 rejected mutations | Linux/Windows VST3 payload identity; resources, Linux link target, stale revision, known Windows compiler sidecar boundary and consumer identity/target rejection. |
| `verify-audition-zip.py` | No `--self-test` option | Not invoked with an unsupported flag. |

Logs and JSON are in `/workspace/scratch/dde4ad85ed5b/a34/`. `tool-source-identity.json` records source matching and helper hashes; `self-test-summary.json` records the invocations' exit codes.

## What each helper proves

### Installation artifact

Requires an authentic **single-artifact API object**, positive artifact ID, exact target-specific artifact name, `expired: false`, matching workflow run ID and full source SHA, positive size within 512 MiB, exact downloaded byte count, and matching API `sha256:` digest. It compares the local production packager and inner verifier bytes against the requested source commit before importing them.

The outer ZIP must contain exactly three distinct, regular members with exact expected names: the inner native archive, its sidecar manifest, and `SHA256SUMS`. The count plus set equality rejects duplicate outer entries. Metadata and inner sizes are bounded and streams are fully read.

The source-pinned inner checker verifies the sidecar checksum, inner archive checksum, exact source commit, expected format/platform/root name, complete payload inventory, actual member digests and sizes, and byte-identical embedded versus sidecar manifest. It rejects duplicate/escaping archive members, malformed paths, duplicate JSON keys, unsupported members, invalid links, dangling/cyclic links, and ambiguous Windows names. It bounds expanded data and metadata. This is substantially stronger than merely checking an archive filename.

Additional checks enforce revision `2026.11.4`, manifest run ID, JUCE commit `c352e2489000ffeb9e397051377d75eef2efe641` and version `8.0.11`, exact `SOURCE.txt`, and embedded JUCE clean/PASS/source provenance. Every advertised plugin and standalone binary receives bounded ELF64/PE32+/Mach-O target and role header checks. These are format sanity checks, not loader execution. Mac targets include VST3, AU and standalone.

### Audition artifact

Compares actual ZIP SHA256 with the supplied expected digest. It loads the capture roster from the requested Git commit, binds declared source/tree/repository/run/attempt and clean-capture flags, checks completed capture status, and checks source-commit text. It verifies exact file inventory and all SHA256SUMS entries, the 30-group source roster, 138 WAVs, 74 selected programs and 870 seconds. The inventory must contain 523 unique programs at indices 0–522, 200 unique parameter IDs, finite 200-value vectors, and the defined final ten body parameter IDs. Rendered parameter digests must agree with the inventory.

Every WAV is parsed from actual bytes: exact RIFF/chunk lengths, stereo 48 kHz 32-bit IEEE-float format, expected frame count, finite samples, no sample beyond full scale, and independently recomputed peak/RMS agreement with CSV metrics. All 19 body models must appear among active audition layers.

The first 190 parameter IDs and the 523 parameter vectors are **not independently reconstructed from source** by this helper. It proves their internal inventory/metrics consistency, supported by source-bound trusted capture provenance. It does not prove they equal a clean native build without that producer evidence. It accepts a syntactically valid capture binary hash; it does not compare it to a separately supplied executable. It does not reject every silent render or certify perceptual realism.

### Windows/Linux validator-input identity

Requires authentic individual API objects for both downloaded installation and validator-input ZIPs; both are bound to expected artifact names, positive IDs, current expiry state, run/source, actual size and API digest. It source-pins the production checkers, validates the installation archive inventory and revision/run/target, then compares the **complete VST3 tree**: every file's byte count and SHA256, resource, directory topology and Linux symbolic-link target. Extra, missing or changed selected-bundle content fails. Canonical paths and duplicate members are rejected; inferred parent directories accommodate Actions ZIP behavior. Permissions, timestamps and ownership are intentionally not compared.

Windows can contain only the exact top-level `CS-80 Reference.exp` and `.lib` pair outside the selected VST3 bundle. The pair receives bounded AMD64 COFF/import-archive format checks. If present, the helper requires the source workflow's reviewed upload/download/selection commands and an original consumer log showing the exact artifact ID, size and digest and exactly two validations of the selected VST3 directory. These checks do not establish the invocations exited successfully.

This helper supports only Windows/Linux x86_64. It does not establish Mac AU or VST3 validated/distributed identity.

## Mandatory parent checks before release use

1. **Audition API binding is external.** Its CLI accepts no API JSON, ID, expected name, byte count or expiry state. Read the authentic API object for the exact artifact, require `cs80-curated-preset-audition-bcc86aa39a33a0162254614e5502705f0113d701`, the selected run/head SHA, nonexpired state, and exact local byte count/digest. Record its ID and API metadata alongside acceptance. Pass the API digest without its `sha256:` prefix. Require the matching capture and upload steps to have succeeded in the intended attempt: the workflow deliberately uploads a failure artifact when capture fails, so upload success alone is insufficient.
2. **All three require producer and attempt binding.** Installation and validator tools bind run ID/head SHA, not run attempt or artifact creation/upload step. Use run/job/upload/consumer logs and artifact metadata from the same run attempt, and require original normal zero exits for the complete 13-lane validation contract. Artifact name, claimed manifest and an `accepted` JSON do not replace those outcomes.
3. **Audition ZIP entry metadata needs a supplemental check on actual bytes.** The audition helper filters out directory entries before its count, duplicate and safety checks, and does not reject symlink/special-file attributes on otherwise expected file names. Thus its success cannot be described as a complete safe-extraction or all-member-structure review. For actual release acceptance, inspect all `infolist()` entries, reject unsafe/noncanonical paths and duplicate canonical names, and require supported regular-file/directory attributes; the expected source producer makes ordinary files. Do not extract an unreviewed archive. The helper never extracts, so this gap is not a filesystem-write vulnerability in the helper itself.
4. **Use the installation checker as well as the payload checker.** Payload equality alone does not run the installation helper's JUCE/SOURCE.txt/native-header checks. For Mac, separately inspect the source workflow's AU before/after tree-equality and executable-hash evidence, VST3 validated-path evidence, packaging provenance and successful original VST3/AU exits. There is no Mac payload mode to invoke here.
5. **Use fresh output paths and require exit 0.** A failed invocation does not remove a preexisting successful JSON file at `--out`. Do not accept a report merely because that filename exists. Check report source/run/digest/ID against the current input after a successful invocation.

These are required scope completions rather than reasons to edit production code immediately. No blocking flaw was found in installation inventory or Windows/Linux selected-bundle comparison for properly source-bound, authentic inputs. The audition metadata/entry checks above are blocking if the helper alone is intended to authorize publication.

## Minimum files and commands

Run from repository root with final source present in Git. Use a fresh scratch output directory. The placeholders below must be assigned from the final successful run; this reviewer did not choose a run ID or attempt.

```bash
CS80_SOURCE=bcc86aa39a33a0162254614e5502705f0113d701
CS80_VERIFY=docs/preset-evidence/2026.11.4/ci/verification-tools
CS80_INPUT=/absolute/path/to/downloaded-artifacts
CS80_ACCEPT=/absolute/path/to/fresh-acceptance-output
CS80_RUN_ID=<actual-final-run-id>
CS80_ATTEMPT=<actual-successful-attempt>
```

Installation minimum: one original downloaded outer Actions ZIP; one authentic single-artifact API JSON; Git source commit; source-identical `scripts/verify-ci-bundle.py` and `scripts/package-ci-artifacts.py`. Invoke once for each target. Example:

```bash
python3 "$CS80_VERIFY/verify-install-artifact.py" \
  --archive "$CS80_INPUT/linux-install.zip" \
  --artifact-api "$CS80_INPUT/linux-install-api.json" \
  --source "$CS80_SOURCE" --run-id "$CS80_RUN_ID" \
  --platform linux --architecture x86_64 \
  --out "$CS80_ACCEPT/linux-install.json"
```

Target/name mapping:

| Platform | Architecture | Required API artifact name |
| --- | --- | --- |
| linux | x86_64 | `cs80-linux-install-bundle` |
| windows | x86_64 | `cs80-windows-install-bundle` |
| macos | arm64 | `cs80-macos-15-install-bundle` |
| macos | x86_64 | `cs80-macos-15-intel-install-bundle` |

Audition minimum: original downloaded ZIP; authentic API metadata independently checked as above; Git object containing final `scripts/capture-factory-auditions.py`; source tree; run/attempt; bare API SHA256 digest.

```bash
python3 "$CS80_VERIFY/verify-audition-zip.py" \
  --archive "$CS80_INPUT/curated-auditions.zip" \
  --source "$CS80_SOURCE" --run-id "$CS80_RUN_ID" \
  --attempt "$CS80_ATTEMPT" \
  --expected-zip-sha256 <64-hex-digest-from-authentic-artifact-api> \
  --out "$CS80_ACCEPT/auditions.json"
```

Windows/Linux identity minimum: installation ZIP/API JSON; validator-input ZIP/API JSON; source-identical production checkers and Git source; original Windows pluginval consumer job log when the known sidecars occur. Repeat for Linux with `linux` names and no consumer-log argument.

```bash
python3 "$CS80_VERIFY/verify-validator-payload.py" \
  --install-archive "$CS80_INPUT/windows-install.zip" \
  --install-api "$CS80_INPUT/windows-install-api.json" \
  --validator-archive "$CS80_INPUT/windows-vst3.zip" \
  --validator-api "$CS80_INPUT/windows-vst3-api.json" \
  --consumer-log "$CS80_INPUT/pluginval-windows-job.log" \
  --source "$CS80_SOURCE" --run-id "$CS80_RUN_ID" --platform windows \
  --out "$CS80_ACCEPT/windows-vst3-identity.json"
```

Full original job logs, job/run API metadata, macOS identity proof files and native test evidence are additionally required for the parent qualification. The API JSON files above are artifact objects, not the wrapper containing an `artifacts` list. Never store expiring signed download URLs in committed evidence. Retain stable API IDs, names, run/source/attempt, sizes, digests, producer and validator outcomes instead.
