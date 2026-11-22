# 2026.11.1 artifact verification preparation — V3

**Preparation complete; no actual candidate artifacts have been downloaded or verified.**

The current candidate is `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, Build run `45044040694`, preset sound-data revision `2026.11.1`. The packaging contract was reviewed at `be460b0b93bdcb8dce5eb8925b18b72706b25e19`; its package/verifier source bytes are identical at the candidate. Run `45042166237` for the earlier source was superseded and cancelled by documented main-branch concurrency; it is not candidate qualification evidence.

The original V2 driver and completed `d0c3c23` / `45029235535` four-package report are preserved unchanged. V3 is a separate scratch file, `verify-downloaded-artifacts-v3.py`. The earlier report continues to describe only the earlier source and its recorded integrity scope.

## Reviewed source contract

The production helper is `scripts/package-ci-artifacts.py` (there is no `scripts/package.py` at the reviewed source). The relevant changes are:

- The canonical `curatedLibraryRevision` accepts `YYYY.MM` or `YYYY.MM.PATCH`. A patch must be a positive integer without a leading zero. The source header declares `2026.11.1`.
- `PRESET_GUIDE.md`, `INSTALL_AND_QUICKSTART.md`, and `DISTRIBUTION_STATUS.md` are emitted through the source helper's `packaged_markdown()` function.
- The inner archive filename, directory layout, three outer artifact files, schema, payload formats, license/notice paths, and binary-header helper are unchanged. The offline inner verifier is byte-identical to the earlier version.

| Contract file | Reviewed SHA-256 |
| --- | --- |
| `scripts/package-ci-artifacts.py` | `f2df2edfd4a55a759ad5c0bad72fa05a5f3880d323412e1211ede1d15bdaa8cf` |
| `scripts/verify-ci-bundle.py` | `65b02309dfd8ff3b3f05585df91e6eb48a6a116563327c63d24d0b516d0a63aa` |

V3 refuses a different package/verifier script hash until that contract is reviewed. It obtains every reference file through `git show <expected-commit>:<path>`, snapshots the scripts, three source guides and preset header in scratch, and replays the reviewed source helper against that snapshot. A newer or modified working checkout is neither used as reference content nor changed.

## Exact document transformation

For the supported inline Markdown syntax, links to one of the three bundled guides are mapped to its archive filename. Other relative repository targets become `https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/<expected-full-commit>/<repository-path>`, retaining their query and fragment. The source function decodes path escapes, resolves the source-relative path, checks that path, and quotes the resulting URL path. External/schemed URLs, network locations, root-relative paths and fragment-only links are left unchanged. Fenced examples are preserved by the source function's fence handling.

The function reads text with Python universal-newline handling, then encodes UTF-8 bytes. **The expected guides therefore have LF newlines on every platform.** V3 requires exact equality to this source-bound output, including on Windows. It does not carry forward V2's exception for raw Windows checkout CRLF copies and does not normalize downloaded archive bytes.

Reports distinguish `source_sha256`, `expected_packaged_sha256`, `archived_sha256`, `exact_source_bytes`, and `exact_source_bound_packaged_bytes`. Raw source-byte equality can correctly be false because link destinations changed. V3 does not label that condition as mere newline equality.

`SOURCE.txt` remains generated, exact UTF-8/LF text. Its commit, source URL, revision and online guide URL must all contain the requested source/revision. Manifest source/run/revision and architecture checks remain mandatory; V3 requires `--expected-run-id`.

### Candidate expected guide bytes

These are source-derived expectations, not hashes of downloaded artifacts:

| Archive document | Expected bytes | Expected packaged SHA-256 |
| --- | ---: | --- |
| `PRESET_GUIDE.md` | 17,675 | `7b9755466e7936a4a510f1b12d167efce30bb595b0b4664c552b323abb19f3e5` |
| `INSTALL_AND_QUICKSTART.md` | 15,885 | `6f0e1e52c064caee9350935474669fe59ab3105d2a44330a9efaa61f0ba0d37b` |
| `DISTRIBUTION_STATUS.md` | 3,707 | `9a99b3b7c2d83a968b9fdde2eb88d5211d5169c6ecdeaf51710a047460d9d448` |

Expected files and readable source-to-bundle diffs are retained under `expected-candidate-guides/`. The audit found 28 distinct pinned relative-link targets counted per document (4 installation, 23 preset, 1 distribution); each resolves to a blob in the candidate Git tree. It found no other absolute GitHub `blob`/`tree` refs in these expected documents. This checks local source targets, not network availability or fragment/heading correctness.

## Inventory and target mapping

The wrapper still selects exactly the expected inner archive, matching `.manifest.json`, and `SHA256SUMS` from each safely enumerated outer ZIP. It delegates all inner inventory, hashes, modes, path/link safety and embedded-manifest checks to the unchanged repository verifier. Actual entry counts will be taken from real manifests; synthetic counts are not candidate evidence.

| Target | Actions install artifact | Product formats |
| --- | --- | --- |
| `linux-x86_64` | `cs80-linux-install-bundle` | VST3, Standalone |
| `macos-arm64` | `cs80-macos-15-install-bundle` | VST3, AU, Standalone |
| `macos-x86_64` | `cs80-macos-15-intel-install-bundle` | VST3, AU, Standalone |
| `windows-x86_64` | `cs80-windows-install-bundle` | VST3, Standalone |

Candidate inner names end in `-6d3cc220edef.tar.gz`, or `-6d3cc220edef.zip` for Windows. Source archives remain named by source commit, not by the sound-data revision. CPU and executable/plugin roles are checked through the reviewed packager helper using temporary non-executable copies; payloads are never loaded or run.

## Focused checks performed

`check-v3-contract.py` passed eight checks, recorded in `fixture-checks.json`:

1. An independently spelled expected Markdown result from CRLF source, including local guide mapping, exact commit pin, query/fragment/title, external link, anchor and fenced example.
2. Canonical `2026.11.1` revision acceptance and `2026.11.01` rejection.
3. A Linux tar bundle containing transformed LF guides.
4. A Windows ZIP bundle containing transformed LF guides.
5. Rejection of the old Windows CRLF guide tolerance.
6. Rejection of guide URLs pinned to the superseded source.
7. Rejection of an untransformed source guide.
8. Rejection of `2026.11` in a patch-candidate manifest.

The negative archive cases first passed the unchanged inner verifier, then failed the added source-contract check. All binary inputs were the existing packager's inert header-only fixtures, with synthetic run ID `0`. Temporary fixtures were removed. No native build, CI rerun, real package download, repository change or publisher action was performed.

## Transform risks and limits

- The source function intentionally supports a limited inline-link syntax. Reference-style links, HTML links, escaped/nested constructs, angle-bracket targets, and whitespace/parentheses in link destinations are outside its general handling. V3 proves exact conformity to that reviewed function, not full Markdown semantic correctness.
- Fenced code is explicitly protected; inline-code and indented-code examples are not separately parsed. A matching inline-link sequence there can be rewritten. The actual candidate diff was reviewed and did not reveal that problem.
- Existing absolute repository URLs are left unchanged, including any movable branch reference that future source documents might contain. The current candidate scan found no noncandidate absolute GitHub `blob`/`tree` refs, but this is not a promise about future documents.
- Replaying the same reviewed transformation proves that artifact bytes correspond to source and the declared transformation. It is not an independent proof that every link renders, that its fragment exists, or that its online target is accessible.
- An artifact integrity PASS does not establish CI success, pluginval/runtime success, release signing/notarization, publisher gate approval, audio quality or DAW qualification. Native and publisher evidence must be checked separately for the exact candidate source/run.

## Invocation when actual candidate metadata and ZIPs exist

Use real paths and API digests from the candidate's artifact metadata. The command below is a template and has not been run on candidate artifacts:

```bash
python -B /workspace/scratch/ffe08111d1ce/review-2026.11.1-artifact-integrity/verify-downloaded-artifacts-v3.py \
  --source-root /workspace/scratch/ffe08111d1ce/yamaha-cs80 \
  --expected-commit 6d3cc220edef5a4a2c7cd968ea8a786c7c83198c \
  --expected-run-id 45044040694 \
  --expected-preset-revision 2026.11.1 \
  --require-all \
  --artifact linux-x86_64 "/actual/path/linux-outer.zip" \
  --outer-sha256 linux-x86_64 "ACTUAL_API_SHA256" \
  --artifact macos-arm64 "/actual/path/macos-arm64-outer.zip" \
  --outer-sha256 macos-arm64 "ACTUAL_API_SHA256" \
  --artifact macos-x86_64 "/actual/path/macos-intel-outer.zip" \
  --outer-sha256 macos-x86_64 "ACTUAL_API_SHA256" \
  --artifact windows-x86_64 "/actual/path/windows-outer.zip" \
  --outer-sha256 windows-x86_64 "ACTUAL_API_SHA256" \
  --out-dir /workspace/scratch/ffe08111d1ce/review-2026.11.1-artifact-integrity/actual-run-45044040694-all
```

For incremental arrival, omit `--require-all` and supply only the downloaded target(s). The report will keep `all_four_targets_verified` false. Use a fresh output directory each time and preserve the independently obtained download metadata alongside the report.
