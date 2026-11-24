# Windows qualification preparation — source fcc3c8e

## Final Windows outcome for this source: failed

A60 subsequently inspected the original completed Windows job and its uploaded native evidence. The actual roster is **251 registrations: 248 passed, one failed, and two formal skips**, with **11 nested Python skips** counted separately. The new inventory-comparison suite failed its pinned baseline-byte check after LF-to-CRLF translation. Packaging and Windows validator execution were skipped. This source has **no accepted Windows install artifact**.

See [failed-native-qualification.json](failed-native-qualification.json), [verify_failure.py](verify_failure.py), and the [A60 independent review](../../../reviews/a60.md). The [A58 correction](../../../reviews/a58.md) preserves the exact baseline bytes on checkout; local verification does not qualify a replacement Windows build. Fresh source and native/validator evidence are required. The preparation below is retained as historical preparation, not a current pending or successful outcome.

## Historical A46 preparation

**Pending actual Windows execution and artifacts.** A46's authenticated snapshots show native run `45366603800`, attempt 1, producer job `138349817051` still executing `Test`. No Windows validator job or Windows artifacts were present. The successful Apple Silicon results do not establish a Windows result.

`source-contract.json` records the actual prior Windows roster and the seven additions in the exact source CMake diff. It derives **251 expected registrations**; this is not an observed native count. The later acceptance must parse all 251 actual JUnit/LastTest cases, require 249 passes with the two reviewed return-code-77 fixtures, preserve all prior cases and require all 22 current family/material gates. Eleven nested Python skips were observed in the prior Windows run and are checked separately by suite; any changed skip requires independent review.

Windows has two **in-process, headless** strictness-10 VST3 lanes: randomized and fixed seed `0x5037a0c`. Each must independently succeed, expose 523 programs, and match the original successful workflow step. This platform has no out-of-process or GUI validator lane.

## Acquire actual evidence

Use authenticated GitHub APIs for the exact source/run/attempt. Retain raw returned JSON, not reconstructed summaries:

- `run-attempt-final-api.json`: `/actions/runs/45366603800/attempts/1`.
- `jobs-final-api.json`: `/actions/runs/45366603800/attempts/1/jobs?per_page=100`.
- `artifacts-final-api.json`: `/actions/runs/45366603800/artifacts?per_page=100`.
- `validator-run-attempt-api.json` and `validator-attempt-jobs-api.json`: authenticated exact-attempt snapshots after the Windows validator completes.
- `producer-job.log` and `validator-job.log`: original decoded GitHub job logs; no synthetic terminal output.

Download four original artifact ZIPs into `/workspace/scratch/b9c11dd92abc/native-artifacts/ARTIFACT_ID.zip`, retaining their exact archive bytes: `cs80-windows-install-bundle`, `cs80-windows-vst3`, `cs80-windows-test-evidence` and `cs80-pluginval-windows`. The VST3 payload archive is distinct from the validation-report archive. Neither downloaded executable is run locally.

## Inspect and qualify

From the repository root:

```bash
python3 docs/preset-evidence/2026.11.6/ci/source-fcc3c8e/windows/inspect_archives.py \
  --downloads /workspace/scratch/b9c11dd92abc/native-artifacts
python3 docs/preset-evidence/2026.11.6/ci/source-fcc3c8e/windows/qualify_windows.py \
  --downloads /workspace/scratch/b9c11dd92abc/native-artifacts/windows-fcc3c8e
```

The first helper checks actual archive identities against API objects, makes regular-file hardlink aliases, runs the pinned A52 parser and binder, and invokes the source-pinned installation and VST3-payload inspectors. Existing outputs are refused. A failed inspection remains evidence of failure; preserve it and investigate before rerunning.

The second helper requires successful original build/test/package/upload/validator steps, source-pinned failure propagation, exact-attempt metadata, original upload/download receipts and timestamps, actual native/validator member hashes, pinned clean JUCE, actual body-host recall phase, full archive inventories and exact distributed/validated VST3 identity. It requires the actual x86-64 Windows product architecture and source-pinned package bytes through the installation inspector. It writes `platform-qualification.json` only after all checks succeed. It always leaves overall `release_qualified` false.

Both helpers are reviewed preparation, syntax checked only at this stage. They have not accepted this run. Source-specific API/log details must be inspected if a concrete assumption fails. Do not weaken a native or validator requirement to obtain a green report. Source/documentation-only descendants of the compiled commit do not change the source identity recorded here.

## Limits

Native execution, package acceptance, acoustic accuracy and overall release acceptance are separate facts. No result here establishes human listening or exact reproduction of an acoustic instrument. Windows signature and external DAW coverage are not asserted. Native production, validated payload equivalence and all four platform publication gates remain required.
