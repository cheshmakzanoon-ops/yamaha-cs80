# A54 — Intel Mac installation and original native test evidence

## Outcome

**The actual Intel Mac installation artifact and original native test evidence are verified and bound to the original successful producer.** This is one platform's evidence; `release_qualified` remains `false`. A56 must qualify the Mac validator lanes and their payload identity, and the parent must establish all remaining release gates.

- Repository: `cheshmakzanoon-ops/yamaha-cs80`.
- Exact binary source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.
- Original native run: `45246863788`, attempt `1`.
- Producer: [job 137945806008](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788/job/137945806008), `macOS / macos-15-intel / VST3 + AU + Standalone`.
- Producer completed `success` at `2026-11-23T20:19:56Z`; it started at `19:26:06Z`.
- No repository source/workflow changes, CI reruns, native product executions, external writes, or child agents were performed by A54.

## Authentic artifact receipts

Original ZIP bytes were downloaded through the connected artifact download tool and Python `urllib`, then checked against the authentic API records. Signed download URLs were neither printed nor saved. The single-artifact JSON records preserve the corresponding objects from the authentic run-artifacts API response.

| Artifact | API ID | Downloaded bytes | SHA-256 of original ZIP |
|---|---:|---:|---|
| `cs80-macos-15-intel-install-bundle` | `14858878730` | 16,228,346 | `84e0b4f7bd7ea480ccee2bca0eb116b93250df37bd44aa1101d9abf9bd032d0a` |
| `cs80-macos-15-intel-test-evidence` | `14859865836` | 66,284 | `16ab696d2fea6e758ec2f919e7849b473eabcf54b4f6f76011b33cf17b9a82b4` |
| `cs80-pluginval-macos-15-intel` | `14859338331` | 10,329 | `8affb980a4d12a67213228a19dbba242174d52f95c1b86c2c160767739f7b358` |

All three API objects report `expired: false`, expire at `2027-02-21T19:14:35Z`, and identify the exact source and run above. Their names, IDs, final byte counts, SHA-256 digests and creation times match the original producer's successful upload steps and log messages. Original job metadata supplies the attempt-1 binding, which is not asserted by the artifact objects alone.

| Upload | Step started UTC | Step completed UTC | Original finalized-log timestamp |
|---|---|---|---|
| cs80-macos-15-intel-install-bundle | 2026-11-23T20:19:48Z | 2026-11-23T20:19:51Z | 2026-11-23T20:19:51.1117950Z |
| cs80-macos-15-intel-test-evidence | 2026-11-23T20:19:52Z | 2026-11-23T20:19:53Z | 2026-11-23T20:19:53.4129300Z |
| cs80-pluginval-macos-15-intel | 2026-11-23T20:19:51Z | 2026-11-23T20:19:52Z | 2026-11-23T20:19:52.2569250Z |

The log timestamps fall within each step's recorded seconds, allowing the remainder of the completed second for the log's subsecond precision. Every observed cumulative progress value is positive, increases monotonically, and ends at the exact final byte count. The install upload records `8,388,608` then `16,228,346` bytes. All comparisons are retained in `release-078c098/verified/macos-x86_64-producer-binding.json`.

The authentic run and job APIs both identify the expected source/run/attempt. Original checkout log line 194 contains the full expected SHA immediately after `git log -1 --format=%H`. The saved workflow bytes match `.github/workflows/build.yml` at that exact source commit (Git blob `70576cb10b78100b6aa98d67b37f00d0361c2a79`).

## Installation archive acceptance

The existing verifier was run with `--platform macos --architecture x86_64`, exact source/run, an explicit repository, and a fresh output. It exited `0` with `artifact_bytes_verified`:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 /workspace/scratch/dde4ad85ed5b/yamaha-cs80/docs/preset-evidence/2026.11.4/ci/verification-tools/verify-install-artifact.py \
  --archive /workspace/scratch/dde4ad85ed5b/release-078c098/input/macos-x86_64-install.zip \
  --artifact-api /workspace/scratch/dde4ad85ed5b/release-078c098/input/macos-x86_64-install-api.json \
  --source 078c0981ffa1b5472342e0fb5ab8702bc3bd2889 --run-id 45246863788 \
  --platform macos --architecture x86_64 \
  --repo /workspace/scratch/dde4ad85ed5b/yamaha-cs80 \
  --out /workspace/scratch/dde4ad85ed5b/release-078c098/verified/macos-x86_64-install.json
```

The outer ZIP contains exactly the inner tarball, matching manifest sidecar and `SHA256SUMS`. The inner archive is `cs80-reference-macos-x86_64-078c0981ffa1.tar.gz`, **16,219,587 bytes**, SHA-256 `caaef4830fb24dcca5dfa95075a92fc3538971749061959c452f47ad7542bb82`. Its complete **39-entry inventory** and checksums pass. `SOURCE.txt`, source/run declarations, preset revision `2026.11.4`, JUCE `8.0.11`, clean JUCE source SHA `c352e2489000ffeb9e397051377d75eef2efe641`, and embedded provenance pass. The production checker and packager bytes were compared to the exact source commit.

All three distributed executables pass native Mach-O 64-bit header checks for `x86_64` (CPU type `0x1000007`):

| Distributed executable | Bytes | Actual executable SHA-256 |
|---|---:|---|
| VST3 | 15,533,376 | `b5803183278b81745e476de113ec45b35932daf2a6a1db93563ec8adbf0c13bf` |
| AU | 15,233,024 | `2d820e1147b5d6c0ec4372b1e9f107c7d8c19ab0935c850acf62937c0db61876` |
| Standalone | 14,941,448 | `9c1f10a9d49f1f0dd72da9a3539e7a0e0a200d8ff24021e079d5211aa7e4bb36` |

The actual distributed AU executable is `AU/CS-80 Reference.component/Contents/MacOS/CS-80 Reference`. Its digest and byte count are saved for A56 in `release-078c098/verified/macos-x86_64-distributed-au.json`. Signing/notarization and a DAW installation were not evaluated by this byte-level verifier.

## Original native test evidence

The actual test-evidence ZIP contains `ctest-results.xml` (111,138 bytes), `Testing/Temporary/LastTest.log` (330,105 bytes), and `ci-juce-provenance.txt` (612 bytes). The existing A45 parser reconciled the exact JUnit cases with the complete LastTest output and exited `0`.

| CTest result | Executions |
|---|---:|
| Registered | 251 |
| Passed | 249 |
| Formal skipped | 2 |
| Failed | 0 |
| Disabled | 0 |
| Unknown | 0 |

The two formal skips are `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`, both recorded as `notrun` with `SKIP_RETURN_CODE=77`. JUnit remains authoritative for formal skips even where LastTest uses a passing result marker.

There are **5 separate nested Python unittest skips**. These are counted from LastTest's terminal unittest summaries only; neither duplicate JUnit output nor individual skip reason lines add to the count. They are not added to CTest executions or internal assertions.

| Containing CTest case | Nested Python skips |
|---|---:|
| `cs80_package_binary_identity_tests` | 1 |
| `cs80_packaging_notice_tests` | 1 |
| `cs80_windows_installer_preflight_tests` | 2 |
| `cs80_render_cli_tests` | 1 |

The original `Test` step completed `success`, from `2026-11-23T19:43:01Z` to `20:15:59Z`. Its exact-source workflow invokes CTest directly with `--output-on-failure --no-tests=error --output-junit`, without `continue-on-error`. The original console reports `100% tests passed out of 251`, followed by the two skipped test names. Console duration is **1,978.21 seconds**; JUnit records **1,978 seconds**. No replacement native test was run. The parser intentionally leaves the standalone numeric process-exit field null; original successful step metadata and direct source-pinned command propagation provide the external exit evidence.

The parser inputs specify only `macos-x86_64`; `--require-complete` was intentionally omitted for this explicitly single-platform review. No validator lanes were parsed by A54. Exact-source/run, clean JUCE and version fields in native provenance also agree with the installation evidence.

## A56 handoff and retained paths

All paths below are under `/workspace/scratch/dde4ad85ed5b/`:

| Purpose | Path |
|---|---|
| Compact A54 summary | `a54-review.json` |
| Full producer/upload binding | `release-078c098/verified/macos-x86_64-producer-binding.json` |
| Installation verification | `release-078c098/verified/macos-x86_64-install.json` |
| Native parser observations | `release-078c098/verified/macos-x86_64-native-observations.json` |
| Distributed AU executable digest | `release-078c098/verified/macos-x86_64-distributed-au.json` |
| Original install ZIP / authentic API object | `release-078c098/input/macos-x86_64-install.zip` / `release-078c098/input/macos-x86_64-install-api.json` |
| Original native evidence ZIP / authentic API object | `release-078c098/input/macos-x86_64-test-evidence.zip` / `release-078c098/input/macos-x86_64-test-evidence-api.json` |
| Original pluginval ZIP / authentic API object | `release-078c098/input/macos-x86_64-pluginval.zip` / `release-078c098/input/macos-x86_64-pluginval-api.json` |
| Original producer job / run API | `release-078c098/input/macos-x86_64-native-job-api.json` / `release-078c098/input/macos-x86_64-native-run-api.json` |
| Original decoded producer log | `release-078c098/input/macos-x86_64-native-job.log` |
| Exact-source workflow | `release-078c098/input/macos-x86_64-native-workflow.yml` |
| Native parser input manifest | `release-078c098/input/macos-x86_64-native-inputs.json` |

The pluginval ZIP/API is ready for A56 with all four original Intel Mac lane reports and console logs, the pluginval download record, and AU before/after-validation digests. A54 verifies its ZIP byte/API/upload identity; A56 owns lane and validated-payload qualification.

## Inspector correction and limits

The first local binding-helper execution stopped because it expected one `Uploaded bytes` progress line. The actual 16 MB upload has two cumulative progress lines. The scratch helper was corrected to validate their monotonic sequence, timestamps and exact final count. It also accepts the observed macOS CTest summary wording above, while requiring zero JUnit failures and original Test success. Its final execution passed on the same original inputs. No product source, CI fixture, workflow or native run was changed to resolve these local parsing assumptions.

One successful Intel platform does not establish overall release success. The complete release still requires all original native platform gates, all **13 validator lanes**, payload identity and supporting evidence. This review makes no all-platform, signing, notarization or DAW-qualification claim.
