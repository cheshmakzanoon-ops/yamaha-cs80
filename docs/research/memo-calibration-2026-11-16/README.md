# Compact memo request02 evidence

Read DECISION.md and INDEPENDENT-AUDIT.md first. raw-text-evidence.zip contains all calibration CSVs/logs, separate counted qualification CSVs, commands, exits, hashes, binding and compiler receipts. Extract with `python3 -m zipfile -e raw-text-evidence.zip extracted`. Never interpret counted qualification clocks as candidate cost data.

RAW-MANIFEST.json inventories every original artifact file, including explicitly omitted large codegen dumps, nine corpus binaries and three benchmark executables. These remain in the unchanged full original artifact, not this compact source-control package. SHA256 of the compact ZIP: ce45ec379ee80f4dd5a29ebc4c966c4d5ece477257d83f11c5c2629d831e66a9.

Archive timestamps are fixed1980, regular-file permissions0644 and member ordering is path-component sorted. The original workflow artifact has30-day retention; neither that URL nor this local workspace is a promise of permanent binary storage. No source/tooling/request changes, rerun or promotion accompanies preservation.

`recompute.py` is the audit script used against the full original artifact at the sibling `extracted/` directory, with `artifact.zip` alongside it. Its binary/corpus checks require that full artifact; the compact text ZIP alone is intentionally insufficient for those checks. RECOMPUTED.json preserves its observed results.
