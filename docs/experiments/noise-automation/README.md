# ANALOG NOISE117 scoped qualification

Local qualification completed 2026-11-13. Publication is tracked separately.

The only executable production change appends existing `P_analogNoise` at index 116,
preserving the literal 116-lane prefix. The existing ARP fixture guard is forward-
compatible by source review only; it is not rerun. DSP, UI, MEMORY, scanner,
parameter IDs and normalization remain unchanged.

The source baseline is qualified ARP116 local commit
`b0ecf0dd70ad72f0ed7bb5bc9a9c44c623f3e7b5`. The copied baseline module SHA256 is
`161c8679b4136d5cbbc65ce1758d0de1c188c7cee9d066608297a97568019098`, exactly the
module recorded by that qualification. No baseline plugin rebuild is claimed.
The original 5,625-file inventory is pinned by SHA256
`659df23b70f709671723c92d719ac6a5667cf709429b0e45880bde34c6a6f074`.
The new fixture/header copies were added after that inventory; their equality to
the candidate is checked before/after each stage. Eleven reviewed files are
frozen in `tests/noise-source-freeze.json`, including the unchanged pre-run plan
and staged runner.

The scope is fixed source-performance policy, manual sources, amount .10–.70,
alone and with existing volume. Fresh instances align deterministic hiss draws.
No-note renders are positive hiss witnesses, not silence/inertness controls.
PURE holds only noise constant while preserving any volume motion. The new
predeclared scalar error ceiling is 1e-8, stricter than the historical host 2e-6
ceiling and below the 1e-7 effect floor. Neither threshold establishes audibility,
spectral calibration, hardware fidelity or full-range coverage.

Only named builds and six configurations at 44.1/48/96kHz × realtime/offline are
permitted. Raw PCM and binaries are retained locally; Git evidence contains
original log/capture paths and hashes, not portable download links. No broad
native suite, state readback, DAW/AU/cross-platform, CPU or release qualification.

## Completed bounded gates

Both explicit builds, six named runs and collection completed once. No stage
was retried. The collector reread all captures and recomputed metrics:

- 300 finite exact-size PCM captures: 240 candidate and 60 frozen116 baseline.
- 60 actual frozen116 compatibility pairs byte-identical.
- 48 scalar/block comparisons: maximum error 0, below the new 1e-8 ceiling.
- 24 queue-order comparisons and 12 initial holds byte-identical.
- 36 exact control pairs: empty/null, Legacy flattening, and PURE noise masking.
- 12 overall effect witnesses: minimum 0.0007557754870504141.
- 12 combined individual-lane witnesses: minimum 0.0008682366460561752.
- 12 positive no-note hiss witnesses: minimum 0.0007557754288427532.

All effect minima exceed the fixed 1e-7 floor. No-note pairs are positive
controls and are not included in the 36 exact/inert control pairs. Collector
metrics and retained hashes are in `collect/complete.json`; each stage retains
source/binary/evidence identities, and the baseline inventory is preserved as
`baseline-tracked-identities.json`. All historical ARP/FX evidence remains
unchanged; the ARP forward guard is source-reviewed only.
