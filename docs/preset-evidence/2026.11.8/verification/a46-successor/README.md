# A46 successor: exact-source native and delivery verification

This is a separate verification preparation packet for source `1b5b3c70471e369481b1f813880a6bcc762a2f57`, tree `b235b67a801d33e9572076b8dfcfbb89e71fac29`, revision `2026.11.8`, native build **45457535944, attempt 1**. The supporting DSP run is **45457536046, attempt 1**. Repository: `cheshmakzanoon-ops/yamaha-cs80`, ID `1436742091`.

**No actual native artifact, install package, validator result, audition archive, or public download is accepted by this packet.** The earlier ef1 helpers and receipts remain unchanged. Their native results cannot qualify this new source.

## Source binding and preserved helpers

`prepare_successor.py` freshly derived `source-contract.json` from immutable Git objects. It rechecked the published .7 contract, its .4 roster inputs, the six .8 registrations, every full native name, and the source exporter request list. It also verified the five-commit parent chain from this source back to ef1 using hashed raw commit objects; this works even when a shallow checkout hides that chain from `merge-base`.

The source `src` tree, `CMakeLists.txt`, `cmake` tree, and native `.github/workflows/build.yml` objects are identical to ef1. The only changes outside documentation/evidence are the approved `SixteenVoiceReleaseTests.cpp` fixture ownership correction, supporting DSP workflow, and its two sanitizer scripts. Their exact approved blobs are required by the derivation. Documentation and retained evidence changes are recorded separately in the full changed-path list.

The six scripts under `helpers/` retain their published bytes. `helper-recovery.json` records their new-source Git paths, byte counts, SHA-256 and blob IDs. The native checker also remains byte-identical to the recovered checker. Only the fixed source, tree, and native run constants changed in `inspect_receipt.py`. `preparation-validation.json` confirms these identities, all 18 preserved predecessor files, the derived names, and rejection of the old source contract. These checks inspect preparation inputs; they do not execute product tests.

`run-api-at-preparation.json` is the authenticated exact-attempt API response checked for source, tree, repository, workflow, run and attempt. It was **queued**, with no conclusion, at observation. `compare-api-context.json` is an explicitly labelled selection of fields from the authenticated comparison response; it is not a complete API response or execution receipt.

## Exact required outputs

| Target | Native executions | Validator lanes | Install artifact name |
|---|---:|---:|---|
| Linux x86_64 | 619 | 3 | `cs80-linux-install-bundle` |
| Windows x86_64 | 261 | 2 | `cs80-windows-install-bundle` |
| macOS arm64 | 283 | 5 | `cs80-macos-15-install-bundle` |
| macOS x86_64 | 283 | 5 | `cs80-macos-15-intel-install-bundle` |
| Total | 1,446 | 15 | Four install artifacts |

The full native names and lane definitions are in `source-contract.json`; counts alone are insufficient. Every target must pass the six added .8 tests. Formal skips remain limited to the historical capability declarations `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`, subject to actual platform/source review. Nested unittest skips must be accounted for separately.

The audition artifact name is `cs80-curated-preset-audition-1b5b3c70471e369481b1f813880a6bcc762a2f57`. Its expected content is **256 WAVs, 95 selected programs, 60 folders, 1,487.2 seconds, and 321 regular files**, with a complete **523 × 204** inventory. All 238 earlier requests are preserved and 18 are appended. Canonical selection SHA-256 is `09e60ba3a5756d2403d56109413a37b51744d0809dfc2a9e7153e99f00627714`.

## Prepared interfaces

`commands-template.json` contains argument vectors for every target: native checks, install checks, payload matching, parser-member binding, and the five public GETs. Replace its angle-bracket placeholders with authentically acquired actual inputs. Its commands have not been executed. `native-inputs-template.json` is the source/run-pinned layout for the all-target parser.

Run from this packet directory and use fresh output paths:

```bash
python inspect_receipt.py inspect --archive "$CS80_ZIP" \
  --artifact-api "$CS80_API" --artifact-id "$CS80_ARTIFACT_ID" \
  --name "$CS80_ARTIFACT_NAME" --out "$CS80_NEW_RECEIPT"

python check_native.py --target "$CS80_TARGET" --archive "$CS80_NATIVE_ZIP" \
  --artifact-api "$CS80_NATIVE_API" --artifact-id "$CS80_NATIVE_ARTIFACT_ID" \
  --out "$CS80_NEW_NATIVE_REPORT"
```

The inspector binds complete outer ZIP bytes to exact API size/SHA, artifact name/ID, source, run, repository and expiry. It preflights ZIP paths and size bounds, rejects ambiguous or nonregular members, and reads all members to EOF for CRC and SHA-256. It does not extract or execute products. The native checker parses a private copy of those same bytes, binds parsed inputs to the archive, and compares JUCE/workflow provenance, full test names, JUnit results and numbered LastTest framing. Exit zero reports conforming observed content; it does not prove original execution or upload identity.

The install helper requires explicit source, revision, run, attempt, platform and architecture. It loads package checks from that exact source and checks SOURCE.txt, manifests, complete inventory/checksums, revision, JUCE and native ELF/PE/Mach-O architecture and image roles. Compare packaged guides/notices with the exact source too. Legacy helper prose mentioning 13 lanes is superseded by this contract's 15 lanes.

For Linux and Windows payload matching, provide the **VST3 input ZIP**, distinct from the validator report ZIP. Windows also needs its original validator job log to bind known compiler sidecars. Each Mac needs its install and validator report ZIPs. Whole-AU original unsuppressed `diff -qr` before and after validation, matching executable hashes, signing/copy order, identical VST3 path, and absence of intervening mutation/rebuild remain mandatory.

## Actual evidence still required

Retain original job/step APIs and complete logs for four producers and two validator jobs. Bind upload IDs, digests, timestamps and original attempt identity: artifact API metadata alone does not normally identify the producing attempt. Preserve failures and superseded attempts; diagnostic replays cannot replace original required lanes.

Each original native Test step must finish successfully with an unsuppressed, normal CTest exit zero and complete JUnit/LastTest evidence. Require exact names, zero failed/unknown/disabled cases, and explicit formal/nested skip review. Cancellation or successful-looking output followed by a crash does not satisfy this gate.

All 15 original pluginval lanes require version 1.0.4, strictness 10, 523 programs, one validation start and terminal SUCCESS, matching console/report seeds, required editor coverage and normal zero exit. Linux has headless, GUI and GUI seed `0x2da72db`; Windows has headless and seed `0x5037a0c`; each Mac has VST3, AU headless, AU GUI, and separate AU GUI seeds `0x2b4eef5` and `0x3c99748`. Review all warnings, skips and eligible retained retries.

A53 supplied a freshly rebuilt independent capture/inventory and reviewed audition verifier for this source. The interface remains `--expected-inventory PATH --independent-capture-receipt PATH`. The core contract deliberately leaves those input records unset; the separate `audition-inventory-binding.json` records their actual bytes and provenance without changing that frozen contract. The new inventory is 710,475 bytes with SHA-256 `399d4c1e8fc1d638f11a22dccb9643a77026acb40ea7fe2a96c66b940938b621`; its new-source capture receipt has SHA-256 `6e2929072752477ef8a3321065dc568a323e13b85bd30d2e93ca5d8e8057dcd0`. A46 checked A53's binding logic, source identities, and successful original configure/clean/build return records against their logs. A53 retains two earlier failed local configure attempts separately. This is a local independent reference, not an Actions qualification. Actual audition acceptance must still check all WAVs, settings, frames, format, finite samples, headroom, metrics, full checksums, capture provenance and inventory equality. Parameter or archive checks do not establish acoustic realism or DAW compatibility.

After actual content and native gates pass, use `inspect_receipt.py public-get` separately for all five advertised artifact-ID routes. It performs a full unauthenticated non-range HTTPS GET and requires HTTP 200 plus exact complete API size/SHA. Redirect query strings are omitted from receipts. Keep the exact five successful URLs, same-run Artifacts fallback and earliest expiry. No public GET was performed during preparation, and immutable blob creation moves no repository ref.
