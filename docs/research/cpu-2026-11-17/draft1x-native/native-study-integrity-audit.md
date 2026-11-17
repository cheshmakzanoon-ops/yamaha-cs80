# Draft1x native study: independent retained-evidence integrity audit

Run 44090859110, revision `9dd1352393ed510e41ac1855f7503bb738cfc7a0`.

PASS within read-only integrity/provenance scope. No project executable was run, no recompilation or timing retry was performed, and this audit does not independently recompute CPU metrics.

- Downloaded ZIP SHA256 exactly `9058a140b40c8bef9b350ee1b1b11d7043705afb0e3ddba8563b94b51649d90b`.
- All 1,991 evidence-manifest entries rehashed, with no missing or extra files except the manifest itself. No symlinks.
- All 23 frozen admission inputs rehashed. Admission SHA256 `83dfe533ba13c7b70036a44d3e4cb9b4cdd2dccc47722f6e879700e479c22f8d`.
- All 193 stock source files and 546 tests independently matched immutable Git tree blob identities. Both full source manifests matched retained files.
- Frozen patch applied exactly in memory, checking every original hunk line. Candidate differs only in `Dsp.h`, `Engine.cpp`, `Engine.h` and `ExternalModulationInterpolator.h`. Parameter definitions remain unchanged; copied stock frame-builder identity independently verified.
- 424 bindings map to retained bytes and match. All 12 retained ELF objects/executables match recorded hashes. FactoryPresets and Ig00156Batch objects match across arms. Compile/link ledger flags match normal prototype O3/PIC settings, with contraction-off only for Ig00156Batch and candidate setup macro only in candidate harness links.
- All 126 PCM dedup references (18 dry + 108 timed) match 12 canonical SHA-named files, each 23,040,000 bytes, totaling 276,480,000 retained PCM bytes. No per-run raw duplicates remain.
- All 159 ledger commands report exit 0.

## Limits

The compiler driver and cc1plus binaries were not retained. Their two hashes are runner receipts, not independent local byte verification. Remote active paths and runtime checks cannot be re-observed from an artifact; retained files reproduce their recorded bindings. ZIP transport does not establish executable permissions. This core research artifact does not certify plugin hosts/PDC, production real-time behavior, full-suite compatibility, other platforms or sonic acceptance. CPU/PCM functional interpretation belongs to the separate raw-results audit.
