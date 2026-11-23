# A55 — original macOS pluginval lanes and distributed payload binding

## Outcome

**All eight original macOS pluginval 1.0.4 lanes are qualified within this review's scope:** four arm64 lanes and four x86_64 lanes, each at strictness 10 with 523 programs. There were **8 validation invocations and 0 validation retries**. The original producer steps completed successfully; exact source and original command-log binding establish normal validator exit 0, including protection against `SUCCESS` followed by a crash.

The exact binary source is `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`, native run **45246863788**, attempt **1**. Producer jobs are [arm64 137945805542](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788/job/137945805542) and [Intel 137945806008](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788/job/137945806008), both completed `success`. The saved run snapshots were still `in_progress`; no overall run-success claim is made.

`release_qualified` remains **false**. Overall release acceptance and the other five validator lanes remain the parent's responsibility. A55 performed no product execution, new validator run, CI rerun, repository source edit or external mutation.

## Exact observed lanes

“0” below is the validator's normal process status inferred from the authentic successful original step and its source/log-matched unsuppressed Bash pipeline. It is not a separate numeric exit-status file. The independent A45 parser retains its own unknown exit fields; this review supplies the external binding.

| Target | Lane | Actual seed | Strictness | Programs | Editor tests | Original step / process exit | Invocations / retries |
|---|---|---|---:|---:|---|---|---|
| macos-arm64 | vst3 | `0x368004e` | 10 | 523 | No | success / 0 | 1 / 0 |
| macos-arm64 | au | `0x535ed06` | 10 | 523 | No | success / 0 | 1 / 0 |
| macos-arm64 | au-gui | `0x5a3f346` | 10 | 523 | Yes | success / 0 | 1 / 0 |
| macos-arm64 | au-gui-seed | `0x2b4eef5` | 10 | 523 | Yes | success / 0 | 1 / 0 |
| macos-x86_64 | vst3 | `0x1d1bc71` | 10 | 523 | No | success / 0 | 1 / 0 |
| macos-x86_64 | au | `0x72f5473` | 10 | 523 | No | success / 0 | 1 / 0 |
| macos-x86_64 | au-gui | `0x454a186` | 10 | 523 | Yes | success / 0 | 1 / 0 |
| macos-x86_64 | au-gui-seed | `0x2b4eef5` | 10 | 523 | Yes | success / 0 | 1 / 0 |

Every lane contains exactly one validation start and one terminal `SUCCESS`, with no failure marker. Console and report copies represent one invocation. Every uploaded console's complete normalized text matches the original producer output exactly after removing only timestamps and terminal coloring. Both GUI lanes on each architecture started editor tests; both headless lanes passed `--skip-gui-tests`.

### Warnings and explicit skips retained

Each of the **six AU lanes** contains exactly this warning: `!!! WARNING: Current program is -1... Is this correct?` It remains a warning in the detailed observations; the original steps passed.

Each of the **two VST3 lanes** contains `INFO: Skipping vst3 validator as validator path hasn't been set`. This refers to the additional external VST3 validator integration. It does not erase the observed pluginval lane, and this review makes no separate Steinberg-validator claim. No AU lane has an explicit skip notice in its pluginval summary.

## Artifact and original-producer binding

All six supplied original ZIPs (validator, installation and native-test evidence per architecture) were checked for canonical unique names and supported entry types before member reads. No archive was extracted. The fresh parser invocation used only the two Mac validator archives and intentionally omitted `--require-complete`, which would require all four platforms.

| Validator artifact | API ID | Original ZIP bytes | Original ZIP SHA-256 |
|---|---:|---:|---|
| macos-arm64 | `14856188848` | 10,377 | `a14b0fedcb8eaef2af315bb596ae9eb84dd77df906933b9fd1691f68b40ec302` |
| macos-x86_64 | `14859338331` | 10,329 | `8affb980a4d12a67213228a19dbba242174d52f95c1b86c2c160767739f7b358` |

The fresh binding helper checks the original ZIP byte count and SHA-256 against each authentic API object, exact artifact name, repository, source, run, expiry, producer attempt and successful upload. It verifies the original finalized artifact ID/name/digest/size/download URL and their timestamps inside the matching upload step. All cumulative `Uploaded bytes` observations are checked in order, with the last value equal to the API count. Both install uploads have intermediate `8,388,608`-byte progress records; these are not mistaken for final sizes. All six API records are unexpired and expire at `2027-02-21T19:14:35Z`.

The exact-source workflow (`.github/workflows/build.yml`, Git blob `70576cb10b78100b6aa98d67b37f00d0361c2a79`) matches its saved bytes. Each original validator command block matches that source exactly. Each invokes pluginval directly once, uses `set -euo pipefail`, pipes output through `tee`, and then requires `grep -qx 'SUCCESS'`. No Mac lane has `continue-on-error`, a retry loop or error suppression. A nonzero exit or signal after `SUCCESS` would fail the pipeline and original step. Successful step metadata therefore supplies the normal-exit evidence missing from the text parser alone.

Both pluginval download records verify version **1.0.4**, official asset **210999197**, **9,202,795 bytes**, SHA-256 `3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f`, consistent with the exact-source download script and successful original fetch steps. ARM used two **download transfer attempts**: an API HTTP 403, then a successful canonical release-URL transfer. Intel used one successful transfer. These are not validator retries.

## Distributed payload identity

A55 freshly hashed the actual AU and VST3 executable bytes inside each original distributed installation tarball, after checking all 40 inner entries for canonical unique names and supported regular-file/directory types. These digests agree with the existing installation verifier. The inventory contains 39 manifest-declared entries plus the embedded manifest.

| Target | Executable | Actual bytes | Actual SHA-256 |
|---|---|---:|---|
| macos-arm64 | AU | 14,293,088 | `25de9ef4917a9a2716277b0f95440897a6bb37667fe7052baf06ac959282358b` |
| macos-arm64 | VST3 | 14,570,784 | `d6b2e411a7f7de7aeb8af3f641a6368ff6d1439af28208810beda01a2cf683ac` |
| macos-x86_64 | AU | 15,233,024 | `2d820e1147b5d6c0ec4372b1e9f107c7d8c19ab0935c850acf62937c0db61876` |
| macos-x86_64 | VST3 | 15,533,376 | `b5803183278b81745e476de113ec45b35932daf2a6a1db93563ec8adbf0c13bf` |

For **AU**, the before and after validator artifact members each contain two digest lines: the exact build-source executable and exact registered `$HOME/Library/Audio/Plug-Ins/Components` executable. All four digests per architecture equal the freshly hashed distributed executable. The hash-file text also matches the original producer log. Original successful registration and packaging steps execute unsuppressed `diff -qr` over the entire two AU bundles before and after validation. This checks bundle resources as well as the main executable. Registration ad-hoc signing happens before the initial copy and digest; no release-signing or notarization assessment is implied.

For **VST3**, the original validator's `Started validating` path is the exact `build/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3` bundle. The exact-source packager collects from that same build root and adds those files directly to the tarball. The ordered workflow has no intervening rebuild or VST3 mutation; its packaging self-test uses a separate temporary fixture directory. The original successful packaging step's inner tarball path and SHA-256 match the distributed inner archive. This is source/workflow/producer binding to the distributed bytes; no invented VST3 pre/post digest artifact is claimed.

The failure-only `AU registration diagnostics` and `Report pluginval failure` steps were skipped in both jobs. `auval -a` is only a registration listing in that diagnostic path; it is not standalone AU validation and is not counted as another lane.

## Retained evidence and limits

- `a55-review.json`: compact structured result, lane table and evidence hashes.
- `a55/macos-validator-binding.json`: complete original job/step/source/upload/digest/payload binding, warnings and skips.
- `a55/validator-observations.json`: fresh A45 parser output; its `release_qualified: false` is preserved.
- `a55/parser-inputs.json`, `a55/archive-preflight.json`, and `a55/verify_binding.py`: exact scoped inputs, ZIP inventory checks and read-only binding helper.
- `a55/build-source-078c098.yml`, `a55/package-ci-artifacts-source-078c098.py`, and `a55/download-pluginval-macos-source-078c098.py`: immutable source snapshots matched to the commit.

This review qualifies eight original macOS pluginval lanes and their distributed payload binding. It does not claim all-platform release acceptance, exact acoustic realism, signing/notarization, standalone auval or separate external VST3 validation, or DAW coverage. No native-suite counts were recomputed or reclassified by A55; A51/A54 own those observations.
