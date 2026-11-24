# A49 — Actual Intel macOS qualification for 2026.11.8

## Decision and exact scope

**Intel macOS VST3, AU and Standalone installation payloads are platform-qualified for source `ef1c472b161c37c490945f7c469086508aae6aef`, tree `f4e00753cde135a36172d9ea549223dfdce5cc82`.** The accepted original execution is [build 45439302351, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45439302351), [Intel job 138605635495](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45439302351/job/138605635495), completed successfully at **2026-11-24 17:43:48 UTC** on `macos-15-intel`. Its original platform output is `x86_64`; configure identifies **AppleClang 17.0.0.17000013**.

The final receipt is `qualification/platform-qualification.json`. The full source/run/attempt/job/command/upload/process/payload join is `qualification/original-execution-binding.json`. **Cross-platform qualification, aggregate release acceptance and public delivery remain false in this packet.** The authenticated aggregate run snapshot was still `in_progress`; that is preserved rather than replaced with an inferred whole-run success.

This follow-up used fresh authenticated owner-connector API/log acquisition and full exact ZIP bytes after the previous transient workspace was lost. It did not recreate earlier local measurements, run downloaded products, change product source, poll/retry jobs, or change repository refs. New remote objects are immutable evidence blobs only. The old preparation is not treated as execution evidence.

## Exact artifacts and native payloads

Each ZIP matched its authenticated API name, ID, source/run/repository, complete byte count and SHA-256. The original successful upload step independently repeats the same artifact ID, ZIP byte count and digest at a timestamp inside that step, establishing **producing attempt 1**. All three API expiry fields are **2027-02-22T16:37:33Z**, subject to normal deletion/retention changes. These authenticated downloads do not establish unauthenticated public delivery.

| Role | Artifact ID | Exact name | ZIP bytes | SHA-256 |
|---|---:|---|---:|---|
| Native evidence | 14947404390 | `cs80-macos-15-intel-test-evidence` | 89,932 | `5d4dc86a704d09d4921e84061ed5e6ae90315700565c1a19b1c35fcceffffbe8` |
| Validator evidence | 14946709999 | `cs80-pluginval-macos-15-intel` | 12,719 | `41b9577b65b5c093361fdf190732d224fc579c1467df8213f6a00852cb30562c` |
| Install bundle | 14946874694 | `cs80-macos-15-intel-install-bundle` | 16,286,380 | `a8784577d2c04ea41df7dc2dd4f819338566953cb22469315e865ec446888015` |

The inner installation archive is `cs80-reference-macos-x86_64-ef1c472b161c.tar.gz`: **16,277,621 bytes**, SHA-256 `1219dee5b3a6db9b1c7d8166361d75cdbb0c171afee6bd3cef679c9a922b413b`.

All three actual executables are thin little-endian 64-bit Mach-O, CPU **`0x01000007`**, subtype **3**, with mode **0755**. AU and VST3 have Mach-O bundle role **8**; Standalone has executable role **2**.

| Format | Executable bytes | SHA-256 |
|---|---:|---|
| AU | 15,277,632 | `6d3a9d94d4b30257155ba7403f6a8178394e758a62bc1c73b5ebd57fdecf77cb` |
| VST3 | 15,582,128 | `3f249eae318e82bf26036ca1aee612783d0025aa221eb868ba7c96791269d017` |
| Standalone | 14,989,824 | `7e49454a74ed0a759b0ff76b94d0856f99f90de89a898dcd6c4e13576c030ee8` |

`install-acceptance.json`, `archive-payload-binding.json` and `docs-and-notices.json` retain the full archive/member/header/source checks. The installation has **39 inventory entries: 13 directories and 26 regular files, no symlinks**. All **13 required metadata files** and **10 exact source comparisons** pass, including source-transformed guide links, notices, `SOURCE.txt`, revision `2026.11.8` and packaged/native JUCE provenance. JUCE is clean **8.0.11**, commit `c352e2489000ffeb9e397051377d75eef2efe641`.

## Complete native execution

The original unsuppressed serial CTest process completed normally with exit **0**, in **2,584.11 seconds**. All **283 ordered original console results** agree with the bound JUnit and complete, numbered LastTest session. The exact final-source roster is accepted; no test is missing, extra, duplicated, failed, disabled, unknown or timed out.

**Accounting: 283 registered = 281 passed + 2 formal capability skips.** Five nested Python unittest skips are counted separately. All six additions relative to the preserved `.7` roster pass: damper, electric pickup, piano hammer contact, piano treble contact, Crystal string stability and preset repeatability.

The original native output includes:

- **Instrument damper: 231 checks, 0 failures**, including the committed and invoked `damperBoundaryRetimingAndReuse` regression. Its 92 added assertions do not create a new CTest registration.
- **Body articulation integration: 284 checks, 0 failures**.
- **Preset voicing: 284/284 checks passed**. Tine velocity-colour differences are **+18.1 / +19.3 / +19.2 dB** at MIDI **48 / 60 / 84**. Electric Piano I/II onset overtones/root are **−13.5 / −13.6 dB**, retaining their actual modest margin rather than rounding to an unspecified pass.
- **Library audit: 301 curated + 222 original programs**. Original audit output reports phrase peak **−6.2 dBFS**, stress peak **−1.5 dBFS**, and loudest-window RMS **−29.9…−18.0 dBFS**. These are this native audit's measurements, not listening or hardware-realism evidence.

The two formal return-77 cases are `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`. The first explicitly reports no SIMD qualification; native AppleClang is excluded from the source's GNU-only backend. This does not assert that the Intel CPU lacks AVX2. The second has empty stdout and is correctly excluded by its Linux/GNU 14.2/ISA/FP specialization predicate. Neither skipped specialization is counted as passed.

The five nested skips are binary identity **1** (Windows junction), packaging notices **1** (Linux Debian fixture), Windows installer preflight **2** (native junction and ISCC), and render CLI **1**. The render skip maps, through the exact original dot/skip pattern and frozen unittest method order, to bounded-buffer resource-limit coverage. Its exact dynamic skip reason is absent from the original nonverbose output and remains unspecified. The separate uncapped long-render write-failure test passed. Full source predicates, actual output and method mapping are retained in `qualification/skip-review.json`.

## Actual UI smoke and historical boundary

The complete native `cs80_ui_smoke` passed in **147.856 seconds**, with exactly one original `UI SMOKE TEST PASSED`, a LastTest pass, JUnit pass and complete normal CTest termination. The instrument-body host identity/recall phase completed; all output-meter unit/display/audit tests also passed. The real source-performance capture phase took **4.883518 seconds**.

| Original real-audio fixture | Finite | Actual host sample peak | Normal GUI-timer meter |
|---|---:|---:|---|
| STRING 3 complete / STRING 2 inferred | 1 | 0.0834435672 | −21.6 dBFS |
| PANEL headroom | 1 | 0.980465114 | −2.5 dBFS |

The original source assertion still requires real rendered audio to appear through the normal GUI timer. Its success was not substituted with injected telemetry, direct timer invocation, a synthetic meter result, screenshot-only review, a selected diagnostic run or a changed timeout. This final-source passing observation **does not establish a fix, flakiness or root cause for the older `.7` Intel meter failure**. The earlier failed and later foundation passing histories remain separate and unchanged; neither is used to accept this executable.

## Five original validator lanes and normal exits

Every original lane uses the verified official pluginval **1.0.4** macOS asset, strictness **10**, and exposes **523 programs**. Complete original job output is byte-identical to its archived console; each report equals that console's report body. Each contains one validation start, the expected editor coverage, one terminal SUCCESS, and no failure marker. The source commands match the exact timestamped original commands with `set -euo pipefail`, in-process validation, the unsuppressed `tee` pipeline and terminal SUCCESS check. Successful original step metadata plus that complete command/output chain establishes **normal exit 0** for all five. All four AU lanes also retain original `auval exited with code: 0`.

| Lane | GUI editor coverage | Actual seed | Original exit |
|---|---|---|---:|
| VST3 headless | No | `0x2e130f5` | 0 |
| AU headless | No | `0x51f28da` | 0 |
| AU GUI | Yes | `0x2cf9cfa` | 0 |
| AU GUI fixed seed | Yes | `0x2b4eef5` | 0 |
| AU GUI fixed seed C76 | Yes | `0x3c99748` | 0 |

There are **five original validator invocations and zero validation retries**. The downloader took **one transport attempt**, a separate count. The four AU current-program `−1` warnings remain visible. The optional external Steinberg validator was unconfigured; this does not change the passing required pluginval VST3 lane or create a claim that the optional validator executed.

## Validated-to-packaged identity

The original AU command ad-hoc signs the packaging source, copies it into the component registrar location, then completes an unsuppressed whole-directory `diff -qr` before validation. After all five lanes, a second complete-directory comparison passes before packaging. Both before/after hash files match original log output; all four source/installed executable hashes equal the actual packaged AU SHA-256 above. Complete packaged AU, VST3 and Standalone inventories are retained. The ordinary-file comparisons establish matching content and membership; executable-only hash files are not relabelled as whole-tree digests. ACLs, xattrs, owner identity and signature validity are not inferred from `diff -qr`.

The original VST3 validation path is the exact build-tree bundle subsequently read by the final-source packager. Every intervening original command is accounted for, with no VST3 rebuild, mutation, replacement or retry. That ordered-path evidence establishes packaged-to-validated VST3 identity; no independent VST3 before/after tree hash is claimed. Packaging's **46 fixture/rejection self-checks** use inert products in a temporary directory and do not mutate the actual build payload.

## Reproduction, preservation and remaining limits

Run the recorded generic commands in `content-command-receipts.json`, then:

```bash
python qualify-intel.py \
  --repo /tmp/cs80-614c8ce056b7 \
  --a46 /tmp/cs80-614c8ce056b7-notes/a46-recovered \
  --artifacts /mnt/data/cs80-614c8ce056b7-a49 \
  --out-dir NEW_OUTPUT_DIRECTORY
```

`reviewed-input-pins.json` freezes the actual receipts. `qualifier-derivation.json` and `intel-qualifier-review.patch` document the reviewed Intel successor of A48's immutable original-execution checker. Generic helpers remain unchanged. The native log is **263,197 UTF-8 bytes**, SHA-256 `89fc20bca65f21c543c240fde390e65a6d25ff6b9354b8ffbe01a5f4fe24ec46`, preserved as Git blob **`e0400e4f53657fe61efa90cc6fde29703388167b`**. `immutable-blob-packet.json` maps retained reports, code, authentic API/log inputs and compact primary archives to their immutable Git objects without changing refs.

No product was executed on this inspection host. Native processor/UI evidence and payload/header checks do not establish a separate end-user Standalone launch, publisher signing/notarization, listening approval, acoustic equivalence or broad DAW compatibility. Other native targets, the actual audition archive and all advertised routes' full unauthenticated GET/size/hash checks remain separate root-owned gates.

### Subsequent root-owned evidence and source boundary

Root subsequently supplied `public-14946874694.json`, recording a complete unauthenticated public GET of this exact Intel installation ZIP: HTTP 200, EOF, 16,286,380 bytes and the matching `a8784577…` digest. It is mapped as a separate supplement, not substituted for the authenticated original-execution chain or all-platform delivery acceptance.

Root also reported a forthcoming Windows test-fixture allocation correction. Any successor source requires its own source-bound native execution and qualification. **This packet remains an accepted predecessor for `ef1c472` only; unchanged audio does not transfer platform acceptance to that successor.**
