# A58 — final actual audition verification

## Decision

**Accepted within the actual audition archive scope. No blocker remains in this scope.** Both the supplemental checker and the unchanged source-pinned frozen verifier exited `0`. This replaces the earlier pending checkpoint; the actual downloaded ZIP and all audio payloads have now been checked.

Source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`. Source tree: `d84661c6c465c53ff41361e8aba0cba5e4a53397`. Original native run: `45246863788`, attempt `1`. Producer: Linux job `137945804596`, completed successfully at `2026-11-23T22:52:41Z`. Verification completed at `2026-11-23T22:56:01.723738+00:00`.

## Authentic artifact

| Field | Observed value |
|---|---|
| Artifact ID | `14870917591` |
| Name | `cs80-curated-preset-audition-078c0981ffa1b5472342e0fb5ab8702bc3bd2889` |
| Actual ZIP bytes | `300067470` |
| ZIP SHA-256 | `ae8cd5c31e03f0db317852f49a9169ad7ff455e2f385e8a510e8da4569f500ad` |
| API expired | `false` |
| Created | `2026-11-23T22:50:53Z` |
| Expires | `2027-02-21T19:14:35Z` |

The ZIP came from the GitHub artifact download connector and a bounded temporary-file stream. Its byte count and SHA-256 exactly match the authentic artifact object from the workflow-run artifact collection. No signed download URL is retained. The explicit attempt-1 jobs collection binds the producer to the same source, run, and attempt.

The original completed producer log matches the artifact name, ID, SHA-256, run and final size. Its **36 cumulative upload-progress lines** are monotonic and finish at `300067470` bytes. Original checkout, configure, JUCE source verification, build, Test, capture and upload steps all succeeded in order. Capture ran `22:48:21–22:50:42 UTC`; upload ran `22:50:42–22:50:54 UTC`; artifact creation falls within that upload interval. The full job log is an unchanged copy supplied by A53, SHA-256 `a9251e66b8effa0936f9dbeb01f228ee604330117d53992a34e8c1de4bba13a6`.

## Actual archive and audio results

| Check | Observed result |
|---|---|
| ZIP entries | **173**, all ordinary regular files, UNIX mode `100644` |
| Explicit directory entries | **0** |
| Checksums | **172**, all matched; `SHA256SUMS` does not hash itself |
| Metrics CSVs / audition groups | **30** |
| WAVs / distinct auditioned programs | **138 / 74** |
| Audio duration | **870 seconds (14:30)** |
| Stereo frames / scalar samples | **41,760,000 / 83,520,000** |
| Raw audio payload | **334,080,000 bytes** |
| Expanded archive | **334,882,144 bytes** |
| Audio format | **48 kHz, stereo, 32-bit IEEE float** |
| Nonfinite / above-full-scale samples | **0 / 0** |
| Highest actual peak | **-13.673645330 dBFS** |
| Active body coverage | **IDs 1–19**, all 19 models |

The maximum peak occurs in `pressure-full/295-Pressure-Pad.wav`. The zero-peak files observed were: `pressure-zero/444-Pressure-Lead.wav`. Pressure Lead at pressure zero is intentionally silent in the source contract; the verifier does not infer perceptual quality from silence or amplitude.

All `ZipInfo` entries were inspected before reading members. Checks rejected noncanonical or unsafe paths, duplicate canonical/case-folded names, symlink/special-file attributes, conflicting file/directory topology, unsupported compression/encryption and excessive sizes. No extraction occurred. Actual file names exactly match the source-derived roster, with no extra directories or files.

The observed provenance is schema **3**, capture status **complete**. Inventory is schema **1**, revision **2026.11.4**. Actual manifest shapes are recorded separately. Source/tree/repository/run/attempt and clean-before/after flags match; all 30 capture commands, the inventory command and README exactly match the source producer. The source-pinned WAV writer exports the original float bits without export normalization or additional clipping; the verifier independently checked every sample, RIFF/fact/data framing, duration, peak/RMS-to-CSV agreement, settings, parameter digests, checksum and active body coverage.

## Delivered inventory versus retained evidence

The delivered inventory is **byte-for-byte identical** to the retained continuation inventory, SHA-256:

`0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611`

Compared all **523** ordered programs and all **200** parameter IDs. Every index, host index, name, category, description and tag list matches. All **104,600** parameter values also match independently at IEEE-754 binary32 precision. Ordered binary32-vector SHA-256: `1343270d888e4efa21eed9e3613c195feae58e322010b4ace4d02fd4a5ab9ec8`.

The retained export label remains `e32728b31d97fc356cac291cb98935fdcd38f803`. No historical inventory was replaced. Direct Git comparison confirms that `src/core`, `src/plugin`, `src/tools/preset_capture.cpp` and `scripts/capture-factory-auditions.py` are unchanged from that export source to the release source; the delivered-byte comparison independently completes this linkage.

## Verifier and capture identities

| Component | SHA-256 |
|---|---|
| Frozen verifier, unchanged | `75be3d29b566c9a88f9d301853ae8c60585aa604b3fc450ba23de1a6eaeebf62` |
| A58 supplement | `1dd142aa85bae7cbeddce32bde85e06b6e9a6a9aded7f30e759f2ee41d40aaf4` |
| Source audition producer | `99150f4e685db3d3f0dde95ef2bc003a0b26d36499a76c957464af13ed80d9c7` |
| Captured executable, as recorded by source-bound producer | `af2aa823a467b7f4997ebc1faf2280820665e06b2e1d016b8d903100740c54bd` |

The supplemental ZIP metadata checks also passed one ordinary-file/directory fixture and rejected seven targeted unsafe metadata cases using in-memory ZIPs. No native product executed during verification.

## Evidence to retain

The exact small-file list, sizes and hashes are in `a58/retention-manifest.json`. Primary records are:

- `a58-review.json` — compact final observed result and evidence map.
- `a58/actual-001/acceptance.json` — complete supplemental acceptance.
- `a58/actual-001/frozen-helper-actual-audition.json` — unchanged verifier result and all 138 per-WAV records.
- `a58/actual-001/producer-api-binding.json` — original producer/API/upload binding and all cumulative progress values.
- `a58/actual-001/all-zip-entries.json` — every actual entry and its metadata.
- `a58/actual-001/delivered-retained-inventory-comparison.json` — all identity/tag/vector comparison results.
- `a58/verify_audition_supplement.py` and `a58/actual-001/frozen-helper-invocation.json` — exact verification code and invocation/exit record.
- `release-078c098/input/audition-artifact-api.json`, `audition-native-job-api.json`, `audition-native-run-api.json`, and `audition-native-job.log` — authentic small API/log inputs.

The **300,067,470-byte ZIP remains at `release-078c098/input/audition.zip` only** and must not be copied into the repository. The parent will retain the exact small report bytes.

## Claimed limits

This is representative single-note archive and numerical evidence for **74 auditioned programs**, with **all 523 programs inventoried**. It makes no human-listening or acoustic-equivalence claim, does not claim all 523 programs were auditioned, and does not qualify chords, live controller transitions, whole-host behavior or the complete release. Overall native-validator and publication gates remain with the parent. No source/workflow changes, CI rerun, archive extraction or native product execution occurred.
