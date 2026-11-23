# A51 — Apple Silicon installation and native-test qualification

**Accepted for this review’s scope:** the actual Apple Silicon installation archive and original native CTest suite for source `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`. The overall release still requires the other platforms, validator-payload identity, auditions and supporting gates.

[Original producer job](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788/job/137945805542), run **45246863788**, **attempt 1**, completed **successfully**. Its exact checkout, native platform, JUCE source, build, Test, packaging and uploads are bound by authentic API metadata and the original 231,693-character job log. Artifact creation times fall within their successful upload steps; IDs, byte counts and SHA256 digests match those original uploads and actual downloaded files.

## Actual installation archive

| Field | Verified value |
|---|---|
| API artifact | `14855214289` — `cs80-macos-15-install-bundle` |
| Outer ZIP | 14,949,170 bytes |
| Outer SHA256 | `a185194b05d68f74c08bdb98f084f826f86f251e842890fe6e07183284dddcc8` |
| Inner archive | `cs80-reference-macos-arm64-078c0981ffa1.tar.gz` |
| Library revision | **2026.11.4** |
| Payload inventory | **39 entries**, complete checksums/source/run/manifest verification |
| Native formats | **VST3, AU, Standalone**, all verified **arm64 Mach-O** headers |
| JUCE | **8.0.11**, commit `c352e2489000ffeb9e397051377d75eef2efe641`, clean/PASS provenance |
| API expiry | **2027-02-21 19:14:35 UTC** |

The existing source-pinned `verify-install-artifact.py` completed with exit **0**. Exact `SOURCE.txt`, embedded and sidecar manifests, inner checksums and every advertised native executable passed. This helper inspects bytes; it does not execute or independently certify a native host, publisher signature or Apple notarization.

The distributed AU executable SHA256 is `25de9ef4917a9a2716277b0f95440897a6bb37667fe7052baf06ac959282358b`; the separate Mac validator reviewer must compare it with the recorded before/after validation identity.

## Original native test execution

Actual test-evidence artifact **14855209144** is **66,236 bytes**, SHA256 `2a353ffb9da02532d2dfd5ddf7c2da6115716aad5f3117cfeaf4ea017588e895`. Its ZIP contains exactly `Testing/Temporary/LastTest.log`, `ctest-results.xml` and `ci-juce-provenance.txt`.

The A45 parser reconciled actual JUnit records against the complete LastTest log and exited **0**. A second direct XML count independently agrees:

- **251 registered CTest executions**;
- **249 passed, zero failed, two formally skipped**; zero disabled/unknown;
- **five nested Python unittest skips**, counted separately;
- original `Test` step started **19:24:36 UTC** and completed successfully **19:45:37 UTC**; JUnit duration **1260.0 seconds**.

The formal skips are `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`, each `SKIP_RETURN_CODE=77`. The five nested skips belong to package binary identity (1), packaging notices (1), Windows installer preflight (2) and render CLI (1). Assertions are not added to these CTest totals.

Both new tine/bowed regressions, direct body and engine integration tests, the complete-library audit, panel golden executable and UI smoke passed. The Mac golden execution is not presented as a strict Linux pixel-threshold pass.

## Shared validator transport

The actual validator artifact **14856188848**, **10,377 bytes**, SHA256 `a14b0fedcb8eaef2af315bb596ae9eb84dd77df906933b9fd1691f68b40ec302`, is saved as `release-078c098/input/macos-arm64-pluginval.zip` with its single-artifact API JSON. Its download bytes and original upload are bound in `a51/pluginval-download-binding.json`. Validator lane analysis and AU before/after equality remain separate; this transport record does not replace them.

## Evidence and limits

Primary machine summary: `a51-summary.json` (duplicate under `a51/summary.json`). Strict artifact report and native observations are in `release-078c098/verified/`; authentic APIs and original job log are under `a51/`. All three original ZIPs and individual API records are under `release-078c098/input/` with `macos-arm64` prefixes.

Artifact acquisition used the GitHub connector. No unauthenticated download-route claim is made. No source, workflow, test, or verifier was modified; no CI rerun, local product execution or new plug-in test was performed. Native success does not prove exact acoustic realism.
