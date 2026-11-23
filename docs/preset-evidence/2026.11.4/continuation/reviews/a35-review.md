# Agent 35 — native qualification checklist

Read-only source review at `bcc86aa39a33a0162254614e5502705f0113d701` (confirmed local HEAD). Reviewed `.github/workflows/build.yml`, `scripts/package-ci-artifacts.py`, `scripts/run-pluginval-linux.sh`, `.github/workflows/release.yml`, `.github/workflows/ci-evidence.yml`, the publisher trigger, and `docs/CI_DISTRIBUTION_STATUS.md`. This document records required evidence, **not observed CI success**. No API polling or production edits were performed.

## Four native builds; six expanded build-workflow jobs

| Build job | Runner / advertised payload | Required ordinary success-path steps |
| --- | --- | --- |
| `linux` | Ubuntu 24.04 / x86-64; VST3 + Standalone | Checkout; install dependencies; configure Release with plugin/tests ON; verify clean configured JUCE 8.0.11 at `c352e2489000ffeb9e397051377d75eef2efe641`; build; complete serial CTest with `--no-tests=error` and JUnit; capture curated auditions; upload audition; upload validator-input VST3; render demo and execute UI snapshot `--self-test`; upload renders; packaging synthetic self-test; package actual products; upload install bundle; upload test/JUCE evidence. |
| `windows` | Windows 2022 / x64; VST3 + Standalone | Checkout; configure VS 2022 `-A x64` with plugin/tests ON; verify identical JUCE pin; build Release; complete serial CTest with `--no-tests=error` and JUnit; upload validator-input VST3; packaging synthetic self-test; package actual products; upload install bundle; upload test/JUCE evidence. Configure has `continue-on-error: true` only for diagnostics: its failed outcome triggers a separate failing step and blocks acceptance. |
| `macos (macos-15)` | Native Apple Silicon / arm64; VST3 + AU + Standalone | Checkout; record `uname`, macOS and Xcode; configure Release with plugin/tests ON; verify identical JUCE pin; build; complete serial CTest with `--no-tests=error` and JUnit; download pluginval; ad hoc sign/register exact AU and establish before-validation binding; four validator lanes below; establish after-validation binding; packaging synthetic self-test; package actual products; upload install bundle; upload validator reports and test/JUCE evidence. |
| `macos (macos-15-intel)` | Native Intel Mac / x86-64; VST3 + AU + Standalone | Same Mac steps, executed independently on Intel. |

The two further workflow jobs are `pluginval` (depends on successful Linux build) and `pluginval-windows` (depends on successful Windows build). **All six expanded jobs must complete successfully** for this build workflow to qualify. An uploaded package may precede its dependent validator, so package existence alone is insufficient. Required artifacts with `if-no-files-found: error` and evidence uploads with `warn` differ: a green warning-only upload does not establish that the expected evidence exists.

## Exactly 13 required pluginval 1.0.4 lanes

Every lane uses strictness **10**, `--validate-in-process`, and a per-test timeout of **120000 ms**. “Randomized” means no explicit fixed seed in the workflow; record the actual seed from the original log.

| # | Platform / format | GUI | Seed | Original log |
| ---: | --- | --- | --- | --- |
| 1 | Linux VST3 | skipped | randomized | `pluginval-headless.log` |
| 2 | Linux VST3 | Xvfb | randomized | `pluginval-gui.log` |
| 3 | Linux VST3 | Xvfb | `0x2da72db` | `pluginval-gui-seed-regression.log` |
| 4 | Windows VST3 | skipped | randomized | `pluginval-windows.log` |
| 5 | Windows VST3 | skipped | `0x5037a0c` | `pluginval-windows-seed-regression.log` |
| 6 | macOS arm64 VST3 | skipped | randomized | `pluginval-vst3.log` |
| 7 | macOS arm64 AU | skipped | randomized | `pluginval-au.log` |
| 8 | macOS arm64 AU | enabled | randomized | `pluginval-au-gui.log` |
| 9 | macOS arm64 AU | enabled | `0x2b4eef5` | `pluginval-au-seed-regression.log` |
| 10 | macOS Intel VST3 | skipped | randomized | `pluginval-vst3.log` |
| 11 | macOS Intel AU | skipped | randomized | `pluginval-au.log` |
| 12 | macOS Intel AU | enabled | randomized | `pluginval-au-gui.log` |
| 13 | macOS Intel AU | enabled | `0x2b4eef5` | `pluginval-au-seed-regression.log` |

Acceptance requires successful original process termination **and** an exact standalone `SUCCESS` line. `SUCCESS` followed by signal/crash is failure. Mac `pipefail` and Windows `$LASTEXITCODE` enforce this. A fixed-seed gate is an additional required invocation, never a replacement for randomized validation.

Linux's wrapper allows at most three complete attempts **only** for exit 139 after a completed test phase with neither `SUCCESS` nor `FAILED`. Each discarded attempt must survive as `*-attemptN.log`; eventual passing attempt must exit 0 with `SUCCESS` and no `FAILED`. Report actual retry counts and preserve failures. This condition does not prove crash ownership or completion of the whole suite. The gdb/fault-thread diagnostic executions cannot replace a failed gate, even if they pass. The fault-thread shim build/self-test is an ordinary job step with no `continue-on-error`; failure there also makes the workflow unsuccessful. Linux/Windows seeded gates can run after a failed randomized gate, but cannot reverse its failure.

## AU validation-to-package binding, each Mac separately

- Before validation, `Register built AU` ad hoc signs the **build-tree packaging source**, copies it to `$HOME/Library/Audio/Plug-Ins/Components/CS-80 Reference.component`, requires full `diff -qr` equality, writes both executable SHA-256s to `au-payload-before-validation.sha256`, and restarts the registrar.
- AU validation targets the registered copy; VST3 validation targets the build-tree VST3.
- Immediately before packaging, another full `diff -qr` must pass and both AU executable hashes are recorded in `au-payload-after-validation.sha256`.
- Inspect both successful step logs and both hash files. Require source/copy agreement before and after; compare the before/after hashes and match the distributed AU executable to them. For complete payload binding use the successful full-tree diffs plus the actual package member manifest. Missing before/after evidence is not cured by a green upload step.
- `auval -a` appears only in failure diagnostics as a registration listing. There is **no independent `auval` validation gate**; do not claim one.

## Actual artifact acceptance checklist

- [ ] Freeze exact source SHA, run ID, run attempt, workflow path, head repository, branch/event, all six job IDs/attempts/conclusions, and original producer step results. Canceled/superseded runs remain incomplete; do not combine their successful fragments into one supposedly qualified run.
- [ ] Collect all four JUnit files and complete LastTest logs plus JUCE provenance. Count registered/pass/fail/formal skip per platform, and nested Python subtest skips separately. Investigate unexpected missing, disabled, skipped or failed tests. **Do not reuse 2026.11.3 counts for 2026.11.4.**
- [ ] Account for all 13 original lanes, their exact flags/seeds, process exits, result lines, attempt counts and preserved failures. Match platform validator versions/download provenance to 1.0.4. The Linux exit-propagation fixtures and diagnostic helper tests must succeed.
- [ ] Obtain `cs80-linux-install-bundle`, `cs80-windows-install-bundle`, `cs80-macos-15-install-bundle`, and `cs80-macos-15-intel-install-bundle`; bind their API IDs, sizes, SHA-256 digests, source/run and upload producer evidence to the actual downloaded ZIP bytes. Record expiry.
- [ ] Verify inner archive checksum/manifest and every member's data/hash, path, type and applicable mode/link properties. Windows inner archive is ZIP; Linux/Mac inner archives are tar.gz. The outer Actions download is ZIP on all targets.
- [ ] Require `SOURCE.txt`/manifest full source `bcc86aa39a33a0162254614e5502705f0113d701`, correct run ID, revision `2026.11.4`, exact target architecture/formats, and clean pinned JUCE provenance. Packaging records source and run ID but **not run attempt**, so bind attempts from producer/API evidence. Inspect bundled guides/notices as source snapshots, not later live repository text.
- [ ] Inspect actual PE/ELF/Mach-O headers and image kinds. Runner labels or archive names alone do not prove architecture. These are separate native arm64/x86-64 Mac packages, not the universal release installer.
- [ ] Compare complete distributed Linux and Windows VST3 bundles with the separate `cs80-linux-vst3` / `cs80-windows-vst3` artifacts consumed by their validator jobs. For Mac VST3 use original build-tree validation and package provenance; for Mac AU complete the binding above.
- [ ] Independently inspect exact audition artifact and full program inventory using the dedicated audit; do not infer audition acceptance from capture/upload step success. No newly added preset claim is warranted by this workflow review.
- [ ] Publish links pinned to accepted artifact IDs with accurate expiry and source. Download acquisition through an authenticated connector does not establish independently tested unauthenticated public access.

## Publication gates versus supporting results

`build.yml` is the native qualification path. Missing/failed/incomplete required build, test, validator, package, or payload-binding evidence blocks describing the new links as qualified. There is **no final automatic publication job in this workflow**.

`DSP numerical measurements`, `Filter batch qualification`, `Hardware panel screenshots`, and `Upstream validator comparison` are independent workflows, not `needs` dependencies of these six build jobs. Keep their exact sources and outcomes separate. Relevant substantive failures still need explanation/resolution for the change; do not advertise “all CI green” while one is failed. Diagnostic/research workflow nonreproduction is not evidence that a historical crash was fixed. `Publish CI evidence` merely republishes GitHub run conclusions as commit statuses; its success cannot override a failed underlying run.

`Publish preset refinement 2026.11.1` is intentionally pinned to source `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c` / run `45044040694`; its skip on this newer build is expected and not a failed new-source publication attempt. `release.yml` builds fresh universal Mac/installable packages and has its own optional signing/notarization paths. Those freshly rebuilt bytes do **not** automatically inherit validation of these native CI archives.

No repository branch-protection configuration was inspected; do not invent or claim server-side merge gates from YAML alone.

## Accurate scope of the eventual claim

These native CI bundles receive **no publisher release signing or Apple notarization** through `build.yml`. The AU is deliberately ad hoc signed before validation/packaging; other Mac tooling may attach ad hoc signatures. The packager's manifest explicitly says signature status was not inspected and that it performs no release signing/notarization. Thus “no publisher release signing/notarization” is more precise than saying every byte is wholly unsigned.

Coverage is Windows headless VST3, Linux headless and GUI VST3, both Mac architectures' headless VST3 and headless/GUI AU, plus the recorded native tests/UI checks. It is **not all DAWs, all operating-system versions, every install path, universal Mac binary validation, or proof of exact acoustic realism**. The historical Linux post-`SUCCESS` SIGSEGV and earlier native/test failures retain their original failed status. A later passing exact-source run establishes that run's observed result only.
