# A52 — final-source build and dependency provenance

## Phase 1 decision

**Frozen-source preparation is complete. Actual build-to-package provenance remains pending on all four native targets.** At this inspection, the fresh-source directory contains the initial authenticated run/jobs/source-commit metadata and its retrieval receipt, but no completed producer log, native test archive, install archive, or validator archive for this source. No native platform or package is accepted by this report. The initial API snapshots are historical observations, not a statement about the run's current progress.

The binary source remains **`ee0dabcacb4cdff93f8803844f60348e0ceda6ed`**, tree **`17115913cbb29157bd00539485bcc5f38b57219c`**, factory revision **`2026.11.7`**, run **`45397189410`**, attempt **`1`**. The full source commit and tree agree between the local Git objects and the retained authenticated commit API. The initial run and all four producer-job records name that same full source SHA, run, and attempt. I independently recomputed every file digest in the initial retrieval receipt. See [the source inspection](../verification/a52/phase1-source-inspection.json) and [the exact four-platform input matrix](../verification/a52/phase1-four-platform-input-matrix.json).

The workspace HEAD observed during this review was `39811916ef812b5c5cb5805cd532358ab684529f`; it is recorded only as the review workspace context. All build, dependency, packaging, and validator-download code inspected here was read with `git show` from the frozen binary source, not from that later documentation HEAD.

## Actual rows inspected

| Target | Producer job | Retained initial API observation | Actual producer log / native archive / install archive inspected | Actual chain decision |
| --- | ---: | --- | --- | --- |
| Linux x86_64 | `138455975798` | `pending`; runner label `ubuntu-24.04`; no steps in this snapshot | None / none / none | Pending |
| Windows x86_64 | `138455976256` | `in_progress`; `windows-2022`; checkout succeeded and configure was in progress in this snapshot | None / none / none | Pending |
| macOS arm64 | `138455976227` | `queued`; runner label `macos-15`; no steps in this snapshot | None / none / none | Pending |
| macOS x86_64 | `138455976307` | `queued`; runner label `macos-15-intel`; no steps in this snapshot | None / none / none | Pending |

The four API identities above were inspected. **Zero actual native/test archives and zero actual install archives were inspected.** No compiler version, actual OS version, actual payload architecture, native/package provenance equality, package JUCE license bytes, or fresh validator download receipt is claimed for any row. The runner labels in the table are scheduling metadata; the later raw execution logs and binary headers must supply the actual observations.

## Frozen-source findings

### Checkout, configuration, dependency verification, build, and package chain

The frozen [build workflow](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/ee0dabcacb4cdff93f8803844f60348e0ceda6ed/.github/workflows/build.yml) declares Linux and both Mac targets with Ninja, `CMAKE_BUILD_TYPE=Release`, `CS80_BUILD_PLUGIN=ON`, and `CS80_BUILD_TESTS=ON`. Windows declares Visual Studio 17 2022 with `-A x64`, those same plugin/test options, and a Release build. These are declared inputs until the original configure/build log and successful job steps bind them to execution. The input matrix retains the exact commands and expected artifact names per platform.

The frozen CMake configuration fetches JUCE from `https://github.com/juce-framework/JUCE.git` at **`c352e2489000ffeb9e397051377d75eef2efe641`**. The three native workflow definitions, covering four targets, explicitly invoke the source verifier for that SHA and **8.0.11** after successful configuration and before building. Local dependency overrides remain possible in general project configuration, so the separate CI check is material.

The frozen [JUCE verifier](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/ee0dabcacb4cdff93f8803844f60348e0ceda6ed/scripts/verify-ci-juce-source.cmake) reads `JUCE_SOURCE_DIR`, `FETCHCONTENT_SOURCE_DIR_JUCE`, and `CMAKE_HOME_DIRECTORY` from the actual configuration cache. It rejects an override, requires the resolved source to be `build/_deps/juce-src`, requires that directory to be the root of its own Git checkout, records HEAD/origin/description, and checks `git status --porcelain=v1 --untracked-files=all`. It verifies the exact HEAD and the JUCE project version before emitting `verification=PASS`. The report records the workflow SHA/run and configured/project paths. These are the fields to check in the fresh native evidence, not assumptions to fill from a historical report.

The frozen [packager](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/ee0dabcacb4cdff93f8803844f60348e0ceda6ed/scripts/package-ci-artifacts.py) reads repository HEAD and requires `git diff --quiet HEAD --` plus equality with `GITHUB_SHA`. This is a check of tracked product-source changes at package time. It is not a blanket assertion that the working directory has no generated/untracked files. The packager requires the earlier clean, passing JUCE report for the same full source SHA and exact JUCE version/pin, and requires its configured path to resolve to the fetched dependency directory. The original successful configure, JUCE verification, build, package, and upload sequence remains to be checked separately on every actual producer log/API. A recipe or helper self-test does not establish that sequence by itself.

### JUCE provenance and license connection

The packager embeds the original report bytes as `evidence/juce-provenance.txt`. For each actual package, those bytes must equal `ci-juce-provenance.txt` from that platform's native test-evidence ZIP. The packaged `SOURCE.txt`, manifests, revision, run ID, and archive/member checksums must also identify the frozen source and target. There is no package report to compare yet.

For the JUCE license, the relevant read path is explicit: the packager sets `source` from the verified report's `configured_source`, checks its resolved location against `build/_deps/juce-src`, then reads `source / "LICENSE.md"` directly into `licenses/JUCE-LICENSE.md`. Actual acceptance must connect that read to the **original successful clean dependency check and unchanged source-pinned packager execution**, with intervening commands reviewed. A separate local JUCE checkout or a matching license from an old package cannot replace that connection. The package inventory and actual archived license bytes will be hashed when available.

This is a bounded execution-provenance argument. The packager consumes the earlier verifier report; it does not itself rerun the dependency's complete Git status check after compilation. No fresh post-build dependency cleanliness measurement is invented here.

### Validator download provenance

The constants below were extracted from the two download scripts at the frozen binary source, rather than copied from old CI receipts.

| Platform | Version | Official release ID | Official asset ID | Expected archive bytes | Expected archive SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| Both Macs | `1.0.4` | `188928656` | `210999197` | `9202795` | `3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f` |
| Windows | `1.0.4` | `188928656` | `210999198` | `2408590` | `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab` |

Those scripts require the archive size and hash to match before publishing the download, and produce `pluginval-download.json`. The actual per-platform receipt, its successful original fetch step, and its validator execution linkage remain unobserved. The Linux workflow uses a **version-pinned `v1.0.4/pluginval_Linux.zip` URL** and records the tool's version output. It does **not** supply an independently pinned Linux archive digest receipt. That difference is retained explicitly; no Mac/Windows digest policy is imputed to Linux.

### Architecture, toolchain, reproducibility, and signatures

The source-pinned packager checks native runner/target selection and inspects payload headers, but no actual header or package architecture was inspected in this phase. Later verification must record the compiler ID/version and generator from the original configure/build log, actual OS/runner-image observations where printed, and the Mac `uname -m`, `sw_vers`, and `xcodebuild -version` output. Any SDK or exact compiler/linker flags not present must stay unobserved.

The native evidence upload includes `LastTest.log`, `ctest-results.xml`, and `ci-juce-provenance.txt`. It does not upload the full `CMakeCache.txt` or compilation-command database. An immutable application/dependency source pin and hash-bound artifacts do not prove a fully reproducible build, pin every runner/toolchain/runtime package, or establish flags that are absent from the retained output.

The Mac workflow explicitly applies **ad-hoc AU signing**, copies that AU into the registrar location, and compares the source/copy before and after validation before packaging. The actual before/after hashes and logs are still required. This establishes no publisher Developer ID identity or Apple notarization. The package manifest's own signature scope is `not inspected; no release signing/notarization performed by this helper`. Archive hashes and successful CI are also not publisher signatures or publication authorization.

## Source continuity and pending completion

I checked A40's consumer-context full source/tree/run/attempt pins, independently recomputed its three helper file hashes, and directly compared the old/new Git trees for `.github`, `cmake`, `scripts`, `src/core`, `src/plugin`, `src/vendor`, `assets`, and `installer`. Those trees are unchanged. A40's source continuity is useful context for earlier audio reviews; it does not turn a `c5549f7d…` binary or receipt into an `ee0dabca…` one. No old artifact is accepted here. The expected suite totals `613 / 255 / 277 / 277` and 15 validator lanes are contextual requirements handled by the native/validator reviewers, not qualifications issued by A52.

The next pass requires the actual authenticated completed-job APIs/logs and artifact APIs/ZIPs under `ci/source-ee0dabca/<target>` and `/tmp/cs80-ci-1cee5a/ee0dabca`. I will check raw evidence behind A46/A47/A48's platform reports, then replace the pending rows with precisely scoped observations. Publication remains blocked on completion of those actual evidence gates; this phase found no additional frozen-source contract defect.

## Reproduction and retained output

Run from the repository root:

```sh
python docs/preset-evidence/2026.11.7/verification/a52/phase1-source-audit.py
```

The script only reads frozen Git objects and retained metadata, then writes A52's two preparation JSON files. It does not fetch, rebuild, package, alter source, or accept unavailable artifacts. Its [successful execution log](../verification/a52/phase1-source-audit.log), [frozen-source/readset inspection](../verification/a52/phase1-source-inspection.json), and [four-target input/check matrix](../verification/a52/phase1-four-platform-input-matrix.json) retain this phase's exact scope.
