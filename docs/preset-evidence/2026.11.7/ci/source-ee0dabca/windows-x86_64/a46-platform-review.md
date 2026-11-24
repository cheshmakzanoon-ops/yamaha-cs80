# A46 actual Windows platform qualification — original attempt 1

## Outcome and immutable scope

**Windows x86_64 platform qualification passes for source `ee0dabcacb4cdff93f8803844f60348e0ceda6ed`, tree `17115913cbb29157bd00539485bcc5f38b57219c`, factory revision 2026.11.7, run 45397189410, attempt 1.** The original producer is 138455976256; the separate original validator consumer is 138483772246. All 17 prepared acceptance gates passed. Release and global qualification remain **false**.

The fresh [platform qualification receipt](platform-qualification.json) is 160850 bytes, SHA256 `a86ab93d18fb390359f60be8a50ed9cc09b34c9b70aab8922c23581fa455c048`. It composes seven original prepared checker executions, all exit 0, plus individual skip, native-detail, validator, and package-chain reviews. The new final composition checker also exited 0 on its first invocation; [its command receipt](a46-final-composition-first-command.json) retains exact argv, source hash, log hash and result hash.

This evidence belongs only to the two original Windows jobs above. The separate Intel failure and any later rerun retain their own original outcomes and attempts. Source-time docs-only changes on main do not identify the compiled source. No release-wide pass is inferred from this result.

## Original acquisition, commands and artifact identities

Root supplied authenticated repository-owner connector acquisition. The complete [retrieval receipt](retrieval-receipt.json), SHA256 `b914626a64b3dec3305a0228540f4976d908570741bc6b131dcd6236e8fd9938`, binds 16 raw API/log files and four archives, and preserves the initial producer receipt `5edb6a9a9c2531f0682f0f3ef9567bf02320b2d13d28721c5a00b6324728d868`. A46 independently rehashed them and matched the exact original source/run/attempt/job/step/upload records. Offline consistency checking does not itself authenticate arbitrary JSON.

| Role | Original artifact name | Artifact ID | ZIP bytes | ZIP SHA256 |
|---|---|---:|---:|---|
| native | cs80-windows-test-evidence | 14926075048 | 84062 | `5f1a1a5b5d3ff1a79640978d1d896d8a7736afe8ee4184a24b170ae329f0ee97` |
| vst3 | cs80-windows-vst3 | 14924634293 | 3205643 | `d9a8a318c409ed19c9a85deab7183f15bc3ea751c93bb52e9a292e008aa7d9b9` |
| install | cs80-windows-install-bundle | 14925251949 | 6409200 | `8f8f885c72910a0af02055036203a6a2e0aa96a9390adce7c1ec480243ba0feb` |
| validator | cs80-pluginval-windows | 14925398286 | 5082 | `28a102fce6fee3c7713c6ed96380d4b4c86c910593fbd0b10e7930facdd342e2` |

All four artifact IDs, names, sizes, digests, source/run/repository fields, expiration, original successful upload steps and upload times agree. The product artifact `cs80-windows-vst3` and text-report artifact `cs80-pluginval-windows` are distinct. Producer completion was 13:36:24 UTC; consumer completion was 13:46:17 UTC on 2026-11-24. The retained overall workflow snapshot was still in progress; it is not used as a global-success claim.

The original Test command was:

```powershell
ctest --test-dir build -C Release --output-on-failure --no-tests=error --output-junit "D:\a\yamaha-cs80\yamaha-cs80/build/ctest-results.xml"
```

Its successful original API step, exact direct unsuppressed command, and complete original 255-case terminal summary establish normal exit 0. The child numeric exit was not separately printed. [Original CI binding](original-ci-binding.json) retains all source command prologues and original required step outcomes, including clean checkout, configure, clean JUCE verification, build, test, product upload, package, installation upload and evidence upload.

## Complete native evidence and reviewed skips

The exact source-derived roster contains **255 CTest cases: 253 passed, two formal capability skips, zero failed, zero unknown and zero disabled**. JUnit and the complete numbered LastTest session agree. Internal assertions and nested Python cases do not inflate the CTest count.

| Formal CTest skip | Actual formal marker | Reviewed basis and limit |
|---|---|---|
| cs80_filter_batch_cap_tests | SKIP_RETURN_CODE=77 | Original output says AVX2 unavailable or disabled. Frozen non-GNU/MSVC configuration disables the AVX2 batch kernel; no SIMD qualification is claimed. |
| cs80_stereo_pra_exact_tests | SKIP_RETURN_CODE=77 | Original runtime output is empty. The frozen exact-reference test is guarded to Linux x86_64 GCC 14.2 and excludes MSVC; its fallback returns 77. No unprinted runtime message is invented. |

JUnit formal skip status remains authoritative even though LastTest labels these sections Passed. [Skip review](skip-review.json), SHA256 `2a52a2a1be1b05f442ca95c47679c11f68866c28ad27f2fc7825e6b0ae6fc760`, records the source guards and actual raw evidence.

A separate **11 nested Python skips across eight passing CTest suites** were individually reviewed:

| CTest suite | Nested skips / tests | Source-backed capability scope |
|---|---:|---|
| packaging_notice | 1 / 7 | Native Linux amd64 Debian packaging fixture. |
| windows_installer_preflight | 1 / 12 | Windows symlink privilege fixture; native junction and ISCC preflight fixtures passed. |
| validator_diagnostic_selection | 1 / 9 | POSIX Bash replay-shell fixture. |
| preset_capture | 1 / 10 | POSIX file-size-limit write-failure injection. |
| render_cli | 3 / 7 | POSIX write-failure and bounded-buffer fixtures. |
| truth_render | 2 / 16 | POSIX file-size-limit control/input failure fixtures. |
| vca_reference | 1 / 12 | POSIX write-failure rollback fixture. |
| float_wav | 1 / 4 | Unavailable resource/signal failure-injection capability; ordinary checks precede this source guard. |

For nonverbose suites, exact skipped methods were mapped from observed unittest progress positions to frozen default sorted class/method order. Unprinted dynamic reasons are not asserted. These fixture results do not constitute a real installer execution.

All four .7 native additions passed: body noise rate, low choir spectrum, piano partial decay and string body formant. The repaired `cs80_parity_capture_tests` ran the unchanged **13-case** harness, including all six live-capture cases: **4.026 seconds inside the Python harness; 4.10695 seconds in CTest/JUnit**. No failures, errors or harness skips occurred. The prior c554 original stack-overflow failure remains a failure and is preserved byte-for-byte.

Native UI smoke passed in 115.648 seconds. The complete LastTest output contains the body host identity/recall start, its 4.797345-second completion, and terminal UI pass in order. JUnit UI stdout was truncated, so complete LastTest evidence is used. Library audit passed and reported 301 curated plus 222 original circuit programs; its numeric output supports the automated audit only, without a listening or fidelity claim. See [native detail review](a46-native-detail-review.json).

## Both original pluginval lanes

Official Windows pluginval **1.0.4**, release 188928656 / asset 210999198, was downloaded in one successful transfer attempt: 2408590 bytes, SHA256 `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab`. Its receipt and original fetch/version output agree.

Both original lanes used strictness 10, in-process validation, timeout 120000 ms and 523 programs. Each had one validation start, one clean terminal result, no warnings/failure markers, and **normal exit 0**. The randomized lane's actual seed was `0x3e82dcb`; the fixed regression seed was `0x5037a0c`. There were **two original invocations and zero validation retries**.

### Validate VST3 headless

```powershell
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path pluginval-reports | Out-Null
& .\pluginval\pluginval.exe `
  --strictness-level 10 `
  --validate-in-process `
  --skip-gui-tests `
  --timeout-ms 120000 `
  --output-dir pluginval-reports `
  --output-filename cs80-windows `
  "vst3\CS-80 Reference.vst3" 2>&1 | Tee-Object -FilePath pluginval-windows.log
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (-not (Select-String -Path pluginval-windows.log -Pattern '^SUCCESS$' -Quiet)) {
  Write-Output "::error title=pluginval Windows::validator exited without printing SUCCESS"
  exit 1
}
```

### Validate state regression seed 0x5037a0c

```powershell
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path pluginval-reports | Out-Null
& .\pluginval\pluginval.exe `
  --random-seed 0x5037a0c `
  --strictness-level 10 `
  --validate-in-process `
  --skip-gui-tests `
  --timeout-ms 120000 `
  --output-dir pluginval-reports `
  --output-filename cs80-windows-seed-regression `
  "vst3\CS-80 Reference.vst3" 2>&1 | Tee-Object -FilePath pluginval-windows-seed-regression.log
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (-not (Select-String -Path pluginval-windows-seed-regression.log -Pattern '^SUCCESS$' -Quiet)) {
  Write-Output "::error title=pluginval Windows seeded regression::validator exited without printing SUCCESS"
  exit 1
}
```

Each complete console equals its original timestamp-unwrapped runtime; the corresponding report equals the console apart from the selected-path header. Normal exits follow successful original API steps plus the exact immediate LASTEXITCODE propagation shown above. SUCCESS text alone is insufficient, and direct numeric child-exit logging is not claimed.

Each lane explicitly reports that the separate VST3 SDK validator path was not set. This informational skip was reviewed; an independent Steinberg SDK validator run is not claimed. Both pluginval lanes are headless; native UI smoke is separately checked. [Validator review](a46-validator-review.json) keeps these distinctions explicit.

## Consumer download and complete installed payload

Independently of compiler-sidecar presence, the original Download Windows VST3 step binds product artifact **14924634293**, **3205643 bytes**, expected and calculated SHA256 `d9a8a318c409ed19c9a85deab7183f15bc3ea751c93bb52e9a292e008aa7d9b9`, successful completion and extraction to `D:\a\yamaha-cs80\yamaha-cs80\vst3`. Both original invocations select its exact `CS-80 Reference.vst3` subdirectory.

The unchanged A41 checker accepted the complete installation artifact: exactly three outer members; inner archive `cs80-reference-windows-x86_64-ee0dabcacb4c.zip`, 6403884 bytes, SHA256 `e8afc12ce60a000b7f62d5478a098b0e6baaf6a5a99484339f34c6eb3f1a04ac`; 20 inventory entries comprising 16 files and four directories; no Windows symlinks. Both VST3 and standalone have correct x64 PE32+ headers and DLL/non-DLL roles. The original package command printed the same inner archive hash; [package producer proof](a46-package-producer-proof.json) binds that output.

The unchanged A36 checker compared every member of the installed VST3 with the selected consumed product bundle: **six entries, two files, four directories, 7394935 file bytes**, tree SHA256 `155eff8cb5857fc6c513990755a5657efd11ca6e941952dd1c5a66458741b1e5`. The VST3 binary SHA256 is `7b99ca04decb3a325a019eca5c4e7daa52300e2a2aa3c678b216736c8ae161c5`; moduleinfo.json is also byte-identical. Only the exact regular top-level `CS-80 Reference.exp` AMD64 COFF export object and `CS-80 Reference.lib` COFF import archive are excluded from the selected bundle comparison. Their formats, raw hashes, source upload contract and original consumer paths are bound; no selected bundle member is exempted. See [payload binding](payload-binding.json).

## Source documentation, notices and JUCE

All 13 required metadata files are regular, nonempty and mode 0644. All three packaged guides equal the exact frozen source's `packaged_markdown` output. Their historical/pending text is preserved.

All seven repository notices use one permitted mode: **complete LF-to-CRLF checkout transformation**. An isolated clean checkout of the frozen source with controlled Git configuration demonstrates the exact bytes. No mixed mode, partial transformation, BOM, textual change or arbitrary newline normalization is accepted. Actual CI Git EOL settings were not printed; the result is the exact byte equivalence plus original clean checkout/package sequence. [Docs and notices](docs-and-notices.json) retains each raw source, checkout and actual hash.

| Packaged repository notice | Actual bytes | Actual SHA256 |
|---|---:|---|
| LICENSE | 35184 | `6f1e622c82a380075843bb084a7ec3b1f1d12a4a02526d75e78b0924a860aa75` |
| licenses/BarlowCondensed-OFL.txt | 4470 | `b4cc842153e834885f3f9385ce26de4e1213d3ffbe473a6c0519e5522a8e3608` |
| licenses/BarlowCondensed-SOURCE.txt | 1010 | `3ab63fda5832592118033ccd14e61aae95b20166145f618bb3e08e943609496e` |
| licenses/MTS-ESP-LICENSE.txt | 666 | `e57ad8fe52e75038366594539d7656107f493e5fdd68b48063e368b3dfb06c47` |
| licenses/libxml2-Copyright.txt | 1338 | `f06bad318a967a790be27cc90235573a2548a2125f6c54b6129d1c802ed201a8` |
| licenses/libxml2-SOURCE.txt | 682 | `0afbd154bd05bb26fd9d6fc2db195da5b7347b5cdde10dd39d320a6c3cfbbe2c` |
| licenses/libxml2-ADDITIONAL-NOTICES.txt | 1444 | `773ec3c805a88806812048182c3b0fa90b4bc18012ea4dc131e01ba3729bd337` |

The packaged JUCE license is separately retained as **5038 bytes**, SHA256 `ca3a08368c9ed3195253f2df63ef08db1b0ea6be99388bb130c921b7c3e54616`. The seven-notice exception does not extend to this license. Acceptance uses raw inventory equality plus the original clean JUCE **8.0.11 / c352e2489000ffeb9e397051377d75eef2efe641** configure/build/package chain. Native and packaged JUCE provenance are identical, 585 bytes, SHA256 `d32b6f7ed4ec57b0bf102f03563fd13653a3921adf1577d1dcb1df427baa44fa`. Independent dependency-checkout license-byte comparison remains false.

## Preservation and limits

All 20 phase-one retained files and all 13 protected historical inputs remain unchanged, including the A30 checker/accepted receipt, A32 transformation proof, c554 original failure/API/log and old verification helpers. Raw root APIs, logs and archive inputs were never edited. The seven subordinate checker receipts retain their original bounded false qualification flags; the new final receipt composes the missing external gates without relabelling those older results.

This qualification establishes the scoped automated Windows platform and package result. It does not establish release/global completion, other-platform or rerun results, all fifteen external matrix lanes, public download GET, publication, signing, notarization, real-machine installation, standalone launch, DAW use, human listening, hardware/reference audition or acoustic fidelity. Those remain separate work for root. No production code, build file, workflow, test, old helper, shared guide, git index or ref was modified by A46.
