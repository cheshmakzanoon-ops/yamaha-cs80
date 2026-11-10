# Result: unchanged paths agree exactly

All four paired GCC/Clang generic/native comparisons pass for baseline `2ade213` versus candidate core `c0219bd`. Every within-profile baseline/candidate capture is byte-identical for each of the two block partitions, and every revision's two partitions are also identical. Compiler versions and flags match within each pair. No cross-profile identity claim is made.

- Final source: `CompatibilityCapture-v2.cpp`, SHA-256 `bfd0a0c09d4a1a9af7defcaa1b3413c595b938aea66062de4aea8b8a8b9f4bae`.
- Per run: 270 scenarios; 6,529,556 checks (predominantly finite-scalar checks, plus meaningful routing/non-silent/state assertions); zero failures; 7,167,448 serialized scalar fields; 57,339,584 bytes.
- Final matrix: 8 builds and 16 successful runs, with all compiler logs empty. Baseline and candidate use their own headers and archives.
- GCC generic capture SHA-256: `6e4bf61d4644590adbe9455ca9319a80e056a99b4d9febad78771102ada38ed0`.
- GCC native capture SHA-256: `69e0b87192cbd2263075184dffd6ba56ed65956e519e508cfbef1e3917795ad3`.
- Clang generic capture SHA-256: `6e4bf61d4644590adbe9455ca9319a80e056a99b4d9febad78771102ada38ed0`.
- Clang native capture SHA-256: `fbf4ec03309a87d01bea432c6a1b2be2eb9460f474c78547b48e63ed31582de7`.

The artifact directory retains each binary, complete raw field capture, compile log, run log, exact command record, source/header/archive hashes, and compiler version. `comparison.json` records direct byte-comparison outcomes. `runtime-provenance.json` verifies baseline header/archive source identity and the candidate frozen runtime source identity. `integrity.json` lists retained file hashes. Initial successful v1 evidence with a harmless indentation warning is also retained; v2 changes formatting of main only. There were no failed fixture attempts.

Read `COVERAGE.md` for exact cases and limitations. This verifies the sampled unchanged paths, **not exhaustive behavior or all private state**. Ordinary post-release expression on a current effective MPE member is deliberately absent. Native plugin/16-voice host paths, deferred scanner/ARP timing, and enabled AVX2 filter batching are outside this witness. No production/test/CMake edits or pushes were made by this task.

Reproduction commands (existing run directories are intentionally never overwritten):

    python3 run_capture.py baseline --source CompatibilityCapture-v2.cpp
    python3 run_capture.py candidate --source CompatibilityCapture-v2.cpp
    python3 compare.py
    python3 finalize_evidence.py

Archive build source paths are retained in each record; source/archive provisioning is owned by the parent investigation. Fresh reruns should copy the frozen fixture to a distinct basename and pass that basename to the driver, preserving these original captures; adapt the comparison directory name accordingly.
