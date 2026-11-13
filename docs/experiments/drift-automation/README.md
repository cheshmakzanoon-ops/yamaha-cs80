# ANALOG DRIFT118 scoped qualification

Local qualification completed 2026-11-13. Publication is tracked separately.

The only executable production change appends existing `P_drift` at index 117,
preserving the literal 117-lane prefix. The noise fixture's forward guard is
source-reviewed only, not rerun. No DSP, UI, MEMORY, metadata or ID changes.
The fixed thermal and worn-sine models retain their existing laws. This is
sample-offset amount coverage, not model-switch automation or calibration.

## Provenance

The baseline is qualified NOISE117 local source
`84ac43a0452734dd1d71a872119fe959964980f1`. Its copied plugin module SHA256
is `1cd4bd75a6450d2649102a4669eceaeb9e0c1f84234e22081d0373d6e7c390a9`,
exactly the artifact recorded by that qualification; no baseline plugin rebuild
is claimed. The 5,673-file baseline inventory is retained in
`baseline-tracked-identities.json`, SHA256
`bc9d8261c71c0f9aade26e9611ebb9104f7f8c357f0b304690f6879460e4ac75`.
Identical new fixture/header copies were added after this original inventory.
Eleven reviewed sources, including the unchanged pre-run plan and staged runner,
are frozen in `tests/drift-source-freeze.json`. Every stage checks these identities
and previously completed binary/log/capture hashes before and after execution.

## Completed gates

Both explicit builds, six named native configurations and collection passed
once. No stage was retried. Configurations are 44.1/48/96kHz × realtime/offline;
four scenarios cover fixed thermal and worn-sine drift, each alone and with volume.

- 552 finite exact-size stereo float32 captures: 432 candidate, 120 baseline.
- 120 actual frozen117 PCM compatibility pairs byte-identical.
- 96 scalar/block comparisons: maximum error 0, below fixed 1e-8 ceiling.
- 48 reversed-queue comparisons and 24 initial holds byte-identical.
- 72 exact controls: empty/null, Legacy flattening and PURE drift masking.
- 24 overall effect witnesses: minimum 0.04186355788260698.
- 24 combined individual-lane witnesses: minimum 0.007753405719995499.
- 12 same-configuration thermal/worn scalar comparisons: minimum difference
  0.05892492085695267, showing the fixed model selection matters.

All effect witnesses exceed the predeclared 1e-6 floor. `collect/complete.json`
contains independently recomputable scenario and fixed-model metrics. Capture
inventories/pairs and source/binary/log identities are retained per stage. PCM
and binaries remain in local build directories; original absolute evidence paths
are not portable download links. Historical noise/ARP/FX evidence is unchanged.

## Scope

Fresh instances align deterministic histories. No reset/reprepare equivalence
is assumed for the worn-sine phase. Both models use fixed manual/manual sources,
amount .10–.70, ordinary held notes, ARP/scanner off and neutral modulation.
PURE comparisons retain identical volume motion and hold only drift.
No no-note silence, model-switch behavior, thermal statistics, long-term drift,
PSD, full-range, hardware, DAW/AU/cross-platform, state-readback, CPU or release
qualification is claimed. Cross-rate PCM identity is not asserted.
