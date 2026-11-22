# Assignment 52 — Apple Silicon download verification

## Status

**PASS for the exact Apple Silicon archive and its native CI job.** Source
`6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, build run `45044040694`, first
attempt, job `137274597823` (`macOS / macos-15 / VST3 + AU + Standalone`). The
job completed successfully at `2026-11-22T16:57:46Z`. Other platforms and the
overall run were still pending when these records were fetched; this report
does not certify their final outcome or release publication.

| Check | Observed result |
| --- | --- |
| Artifact | `14763761497`, `cs80-macos-15-install-bundle` |
| Credential-free public GET | 14,900,540 bytes, exact API size and SHA-256 match |
| Outer ZIP SHA-256 | `662f66e20a8658374b3d4d30102b698234e01bb99e3753a49206a04f99b201a7` |
| Inner TAR SHA-256 | `ad0212a12ff0623f2d60d77c2dc7fa0c574c47b5beeb99cf037d938a2cae184f` |
| Complete inner inventory | 37 entries verified |
| Native code images | Three arm64 Mach-O images: VST3, AU and Standalone; expected image roles and executable modes |
| Library and source | Revision `2026.11.1`, exact commit/run, clean pinned JUCE `8.0.11` / `c352e2489000ffeb9e397051377d75eef2efe641` |
| Guide links | 43 repository links pinned to the binary source; both relative destinations exist in the bundle |
| CTest | 243 listed: **241 passed, 2 skipped, 0 failed, 0 disabled**; no other unclassified not-run cases |
| pluginval | VST3, AU and AU GUI all pass at strictness 10 on attempt 1 |

The two skipped tests both return the configured skip code 77. The batch-cap
test logs that AVX2 is unavailable or disabled, so it does not qualify SIMD on
this runner. The stereo-PRA exact test prints no skip reason; its pinned source
restricts that specialization to a qualified Linux x86-64 GCC 14.2 arithmetic
configuration and returns 77 outside it. JUnit marks these same two cases
`notrun` with `SKIP_RETURN_CODE=77`; they must not be counted twice or described
as passed. All 243 log rows are reconciled with JUnit in `test-cases.json`.
`skip-source.json` records the exact source guards and file hashes.

`artifact-proof.json`, `ci-proof.json` and `ci-evidence-downloads.json` contain
the detailed results. Small CTest and pluginval evidence artifacts were also
downloaded and matched to their API digests before their text members were read.
The complete job log retains the exact decoded response bytes, including its
UTF-8 BOM. Earlier `initial-job.json` and `last-observed-*` files remain explicitly
historical snapshots taken before completion.

## Method

The observed API metadata is saved as `run.json`, `job.json` and
`artifact.json`. The downloaded outer artifact is kept in the shared
`build-coordination/artifact-cache` directory, addressed by immutable artifact
ID. The command is:

```sh
python3 build-w11-macos-arm/verify-arm-download.py \
  --artifact-json build-w11-macos-arm/artifact.json \
  --run-json build-w11-macos-arm/run.json
```

The verifier requires the exact source, main push, repository, build workflow,
run ID and first attempt. It compares a credential-free public GET's complete
outer ZIP bytes with the artifact API's size and SHA-256. Trusted repository
verification code then checks the full inner inventory, checksums, safe archive
paths and modes, revision `2026.11.1`, pinned JUCE `8.0.11` and source commit,
workflow identity, and expected VST3/AU/Standalone products.

An additional direct inspection scans every regular archive member for Mach-O
headers and requires exactly the three expected native arm64 images. It checks
64-bit little-endian CPU and loadable image types, executable modes and bundle
plist executable names. `SOURCE.txt`, JUCE evidence, all repository links in the
three packaged guides, and relative guide destinations are checked directly.
Temporary copies of the inner archive and header-verification binaries are
removed afterwards. No downloaded product is loaded or executed.

The native job's successful CTest step and three strictness-10 pluginval steps
(VST3, AU and AU with GUI) are separately established from the exact API
job metadata and logs by `check-ci-gates.py`. A `SUCCESS` line alone is not sufficient: the workflow
uses `set -euo pipefail`, requires the validator pipeline and final marker check
to succeed, and completes each required step successfully on attempt 1. The artifact
does not by itself certify those gates or the remaining platforms. Numeric raw
child exits are not separately printed; zero validator status follows from the
successful strict pipeline steps, rather than being invented as a logged field.

## Qualification

This is archive, architecture and bounded CI evidence. It does not establish
developer signing, notarization, all-DAW compatibility, human listening quality
or physical-CS-80/acoustic-instrument identity. No production source, publication,
workflow retry or remote branch is changed by this assignment.
