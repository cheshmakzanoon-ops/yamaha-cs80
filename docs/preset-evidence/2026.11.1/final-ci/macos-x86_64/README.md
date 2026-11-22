# Assignment 53 — Intel Mac package verification

**PASS: first-attempt native CI and the actual downloaded Intel Mac archive.**
This is an Intel-specific result; it does not certify the other platforms or
release publication.

The binary source is `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`. GitHub Actions build
`45044040694`, attempt **1**, produced Intel Mac job `137274597912`, which
completed successfully at **2026-11-22T17:17:53Z**. All required configuration,
build, test, validation, packaging and upload steps succeeded.

## Native test and validator evidence

The current CTest summary reads `100% tests passed out of 243`. Its **243**
per-test log rows reconcile one-to-one with JUnit: **241 executed and passed**,
**2 explicitly skipped**, **0 failures**, **0 disabled cases**, and **0 other
not-run cases**. Skips are not counted as executed passes.

| Skipped test | Recorded qualification |
| --- | --- |
| `cs80_filter_batch_cap_tests` | Return code 77; LastTest.log says AVX2 is unavailable or disabled, so this case supplies no SIMD qualification. |
| `cs80_stereo_pra_exact_tests` | Return code 77; the source restricts this exact specialization to qualified Linux x86-64 GCC 14.2 arithmetic. Intel macOS does not enter it. |

| Validator lane | Strictness | Random seed | Result |
| --- | ---: | --- | --- |
| VST3 | 10 | `0x2b37ae3` | SUCCESS |
| AU | 10 | `0x3dfa008` | SUCCESS |
| AU GUI | 10 | `0x19211fd` | SUCCESS |

Each lane has one exact terminal SUCCESS marker in its separately downloaded
log. All three workflow steps use `set -euo pipefail`; their successful status
establishes zero validator/pipeline status. Numeric raw child exit codes are
not separately printed. AU validation uses the workflow's registered component.
The job log, JUnit, full CTest log, validator logs and API metadata are retained.

## Actual package

[Credential-free, artifact-ID-pinned download](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763644216.zip)

- Artifact: `14763644216`, **16,057,766 bytes**.
- Outer SHA-256: `325b42beb55ca861b65e9f8416ec286a4b6061946181210fdb41d041e7d3cf1f`; exact match to GitHub API metadata.
- Expiry recorded by GitHub: `2027-02-20T16:28:38Z`. This artifact
  link is separate from the later durable release assets.
- Full offline verification covers **37 inventory entries**, source/run identity,
  preset revision **2026.11.1**, and JUCE **8.0.11** at
  `c352e2489000ffeb9e397051377d75eef2efe641`.
- All three code payloads are native **x86-64 Mach-O64**: VST3 and AU are bundles;
  Standalone is an executable. The scan examines every inventoried regular file.
- All **43 repository links** in the three shipped guides point to the exact
  binary source. Both bundled relative file targets exist; this archive check
  does not independently certify every destination heading anchor.
- The outer ZIP remains in the shared artifact cache. The temporary inner TAR
  and binary inspection copies were removed; no product bundle was extracted.

## Reproduction and records

```sh
python3 build-w11-macos-intel/fetch-ci-evidence.py
python3 build-w11-macos-intel/check-ci-gates.py
python3 build-w11-macos-intel/verify-intel-download.py \
  --artifact-json build-w11-macos-intel/artifact-api.json \
  --run-json build-w11-macos-intel/run.json
```

`summary.json` is the compact result; `ci-proof.json` and `artifact-proof.json`
record the complete qualifications. `test-cases.json` lists all 243 individual
results. `ci-evidence-downloads.json` binds the downloaded logs/JUnit to their
own API artifact hashes. `SHA256SUMS` inventories this verification directory.

The initial preparation assumed legacy CTest summary wording. The original
checker is retained as `preparation-check-ci-gates.py`, and
`preparation-parser-adjustment.json` records the adaptation to the observed
modern wording plus full JUnit/per-row reconciliation. The legacy preparation
checker was not run against this native output; no native failure or rerun is
being relabeled as a parser correction. `pending-api-snapshot.json` preserves
the earlier in-progress state.

No production source, workflow, ref, tag or release was changed. No downloaded
payload was executed locally, and this assignment did not launch Standalone.
These results establish bounded native CI and archive consistency; they do not
establish signing, notarization, subjective sound quality, hardware matching
or compatibility with arbitrary DAWs.
