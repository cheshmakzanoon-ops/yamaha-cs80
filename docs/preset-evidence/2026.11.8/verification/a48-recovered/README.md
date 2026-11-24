# A48 — Actual Apple Silicon qualification, recovered evidence

**Apple Silicon is platform-qualified for source `ef1c472b161c37c490945f7c469086508aae6aef` only. Cross-platform, release and public-download qualification remain false.** A later Windows correction or another source change requires its own native qualification; this receipt must remain bound to ef1c472.

This actual review was performed after the transient workspace reset. It freshly parsed the original final-source Actions artifacts and authenticated API records. It does not reconstruct lost local test results or reuse an earlier ARM pass. No product was built, installed or executed here; no workflow was retried and no ref was changed.

## Exact execution and artifacts

| Binding | Value |
| --- | --- |
| Source / tree | `ef1c472b161c37c490945f7c469086508aae6aef` / `f4e00753cde135a36172d9ea549223dfdce5cc82` |
| Library revision | `2026.11.8` |
| Original run / attempt | [45439302351](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45439302351) / 1 |
| Original ARM producer | [138605635263](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45439302351/job/138605635263), successful, completed 2026-11-24 17:24:07 UTC |
| Native platform | `macos-15`, real `arm64`; macOS 15.8.2, Xcode 16.4, AppleClang 17.0.0.17000013 |
| Original job log | 258,791 bytes; SHA-256 `7393725dd452ba05c957db205fb980cd7232b1feaf0be7fbaa3c403cc494ed64`; Git blob `09d5d5480d124f0d5cc22873a9fe3aebc5e13b09` |
| Native evidence artifact | 14947505348; 89,899 bytes; SHA-256 `eeeb8fd43c3fcf0422152b0953f53b47da4c3cb2f86000a6dc84e7247fe5839f` |
| Validator artifact | 14947445386; 12,771 bytes; SHA-256 `66cf6de35348b91592d138823afb71b99cac3233bcb9b9af073b31884b4942a9` |
| Install artifact | 14947300509; 15,004,302 bytes; SHA-256 `ed406c8cd6095ae8c7645604f55ea8d798a989b279da6ef27b14dc9c55442472` |

All three exact upload IDs, names, byte counts, digests and creation timestamps match the original successful upload steps and freshly authenticated artifact API collection. All expire at **2027-02-22 16:37:33 UTC**, subject to deletion/retention changes. The containing run was still in progress when reviewed; only the completed ARM job is accepted.

## Native result and skip review

The exact source-derived roster is **283 registered = 281 passed + 2 formal capability skips**, with zero failed, disabled, unknown or missing cases. All six additions since `.7` passed. Original serial CTest completed normally in **1,670.38 seconds**; all 283 ordered console names/results equal the bound JUnit and complete LastTest session.

The actual damper terminal is **231 checks, 0 failures**, including the 92 portable assertions added inside its existing CTest registration. Body articulation reports **284 checks, 0 failures**; voicing reports **284/284**. Tine velocity-colour changes are **+18.1/+19.3/+19.2 dB** at MIDI 48/60/84; Electric Piano I/II onset ratios are **−13.5/−13.6 dB**. The full library audit completes **301 curated + 222 original programs**, with reported phrase/stress peaks **−6.2/−1.5 dBFS**.

Native UI smoke passes in **112.527 seconds**, including its instrument-body identity/recall phase. The original normal-timer STRING fixture shows finite host peak **0.0834435672** and meter **−21.6 dBFS**; PANEL shows **0.980465114** and **−0.2 dBFS**. Source capture lasts **4.214416 seconds**. Meter tests pass. This current ARM result does not explain or erase the earlier Intel meter failure.

`cs80_filter_batch_cap_tests` explicitly reports unavailable/disabled AVX2; arm64 AppleClang cannot select the final source's GNU x86_64 backend. `cs80_stereo_pra_exact_tests` formally returns 77 with empty stdout; its source executes only the exact Linux/GNU14.2/ISA/FP specialization. Neither capability is claimed as passed.

The **five nested Python skips** remain separate: one Windows-junction fixture; one explicit Linux Debian-notice fixture; two Windows installer fixtures; and one bounded-buffer resource-limit fixture. The last method is inferred from the exact nonverbose pattern and final-source alphabetical unittest order. Its exact dynamic skip reason is absent; that absence is retained, while the separate uncapped long-render write-failure case passed.

## Validators and packaged payload

| Original lane | Seed | Result |
| --- | --- | --- |
| VST3 headless | `0x7a5732f` | Normal exit 0 |
| AU headless | `0x7a798eb` | Normal exit 0 |
| AU GUI | `0x2daf6af` | Normal exit 0 |
| AU GUI regression | `0x2b4eef5` | Normal exit 0 |
| AU GUI regression | `0x3c99748` | Normal exit 0 |

All five use pinned official pluginval **1.0.4**, strictness **10**, and **523 programs**, with exactly one validation start and terminal SUCCESS per console/report, correct editor coverage and **zero validation retries**. All 13 executed shell command groups exactly match the final workflow. Complete timestamp-stripped original console output equals each archived console; each report equals that console's body. Successful original unsuppressed `set -euo pipefail` pipelines and terminal-success assertions establish the normal exits; each AU also records `auval` exit 0.

Four AU warnings, `Current program is -1... Is this correct?`, remain retained. The optional external Steinberg validator was unconfigured and skipped. Two downloader transport attempts—API 403 followed by successful pinned public-asset retrieval—are distinct from validation invocations/retries.

Both original whole-AU `diff -qr` comparisons passed. The packaging source was signed before copying and registrar reset. All four before/after executable hashes equal the packaged AU SHA-256 **`5635d860e62800841fb5141ed7061b62e71a8f3ddca631dc2829ea9e8def85c8`**. Complete packaged AU inventory has no symlinks. These comparisons establish ordinary-file membership/content equality, not ACL, xattr, ownership or signature validity.

The validated VST3 and packaged VST3 are bound by the identical original build-tree path and fully reviewed intervening command order, with no rebuild/mutation. No independent VST3 pre/post tree hash is invented. All three actual binaries are thin arm64 Mach-O with correct bundle/executable roles and `0755` modes. The complete install inventory has **39 entries: 13 directories and 26 files**; all **13 required metadata files**, **10 exact final-source guide/notice comparisons**, and identical native/packaged JUCE provenance pass. JUCE is clean pinned **8.0.11 / `c352e2489000ffeb9e397051377d75eef2efe641`**.

## Reproduction and retained boundaries

`qualification/platform-qualification.json` is the final ARM receipt. `qualification/original-execution-binding.json` retains command/upload/exit/payload proofs; `skip-review.json` and `native-reviewed-excerpts.json` retain the actual review. Generic observation, native-contract, install, archive-binding and document receipts remain unchanged and keep their narrower qualification flags.

Run `qualify-arm.py` with explicit `--repo`, `--a46` (recovered generic helpers/roster), `--inputs-dir` (this packet), `--artifacts` (the three original artifact-ID ZIPs/APIs and original ARM log), and a **new** `--out-dir`. It verifies fixed identities and reviewed receipt pins. Two local refusal checks reject an existing accepted destination and a modified observation omitting the damper case; accepted receipts remain unchanged.

The initial generic Mac helper invocation encountered a shallow-path `parents[5]` default error before reading inputs. Its unchanged bytes succeeded from `helpers/generic/`; the original local analysis error is retained in `content-command-receipts.json`. It is not a native failure or validator retry.

Public full-GET verification is root's separate gate. After this ARM receipt was frozen, root supplied `public-install-root-receipt.json`: the complete unauthenticated GET of install artifact 14947300509 reached HTTP 200/EOF and matched all 15,004,302 bytes and the API SHA-256. That independent delivery receipt is retained separately; it does not change the platform receipt's public-delivery flag or establish the other advertised routes. This acceptance does not establish other platforms, aggregate release/audition completion, publisher signing/notarization, a separate Standalone launch, broad DAW compatibility, listening quality or physical-instrument equivalence.
