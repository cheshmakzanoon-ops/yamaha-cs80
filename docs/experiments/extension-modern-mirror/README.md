# Trusted ModernConfig mirroring: direct correctness

Research-only frozen V1. This package preserves bounded direct checks of a proposed optimization for the extension's 16-voice path. It does **not** establish integrated 16-voice/native behavior, CPU savings, calibration, DAW capacity or production readiness. No new compile or DSP execution was performed while preparing this export.

## Change and boundary

The original wrapper copies configuration from the first eight-voice Engine to the second, then updates transport. V1 adds an out-of-line `Engine::tryMirrorModernConfigFrom(const Engine&)` at that same setter position. On rejection the wrapper still calls the original setter; transport is still updated afterward.

Only a trusted, already-sanitized Engine-owned configuration can be accepted. The original public arbitrary-input `ModernLayer::setConfig` body stays unchanged. Admission requires the qualified strict GCC/x86-64 build, x87 CW `0x037f`, and MXCSR controls `0x1f80` or `0x9fc0`. It reads active MSEG point fields as integer bits and rejects nonfinite values and nonzero subnormals. Other contexts/builds decline before modifying destination state.

Acceptance removes repeated sanitization, not the trigger/sync/rebuild continuation. That continuation has observable phase, rate and reset effects and is retained. There are no new persistent fields, cache generations, allocations or object-layout changes. The eight-voice path is not a claimed beneficiary.

**Maintenance limit:** V1 duplicates a source-sensitive continuation. A separately retained V2 shared-tail extraction changed the original setter's instruction bytes and relocation positions and was rejected before execution. V2 is not included here. Unsupported V1 builds still incur an always-false wrapper call; that is not production acceptance.

## Saved direct results

See [direct-results.json](direct-results.json) for exact saved stdout and scope.

- 64 seeds × 8 FP contexts: 512 calls, 96 accepted and 416 transactional fallbacks, 27,873 checks.
- Comparison covers all 3,992 `ModernLayer` bytes, source immutability, FP/errno, and 24 subsequent modulation steps per call. It is not a full Engine render or PCM comparison.
- Recorded sizes: `ModernConfig` 1,480 bytes; `ModernLayer` 3,992; `Engine` 290,120. These are the tested GNU ABI sizes, not a cross-platform promise.
- The 48-cell control matrix accepts two strict contexts and rejects 46 without state/FP/errno changes. Three separately compiled negative method objects (`-ffast-math`, `-mfma`, `-ffinite-math-only`) reject all 48 cells.
- Pending x87/SSE-invalid rejection preserves the tested control/status and destination bytes. The pending-case branches do not independently assert errno; the matrix and actual-trap comparison do.
- Two actual unmasked SSE trap cases, invalid and denormal, each compare stock A/A and candidate-rejection-plus-stock-fallback. They match signal code, original PC within the forked executable, saved ucontext FP registers, errno and all `ModernLayer` bytes. Six children genuinely trap; these are not merely rejected exception settings.

The first development harness incorrectly assumed `fnclex` cleared x87 condition flags. It was corrected to compare captured pre-call flags; no candidate correction was required. Clang was unavailable. Private access is confined to diagnostic test TUs, not production observers. Signal children exit after capture; no post-trap continuation is claimed. Full CTest, arbitrary compiler/link contexts, integrated plugin execution and timing are outside these direct receipts.

## Apply and reproduce

Baseline: `fe13c25f354203838697b35adf3962bbe46735a4`. All 193 retained `src` files were independently matched to its Git blobs during export. The authentic layered stock archive is byte-identical to the archive used by these direct tests. Their hashes and omitted artifact identities are in [provenance.json](provenance.json); source fingerprints are in [source-inventory.json](source-inventory.json).

Use separate pristine stock and candidate checkouts, not a live production tree:

```sh
git -C candidate apply --check /path/to/candidate-v1.patch
git -C candidate apply /path/to/candidate-v1.patch
python3 verify.py --stock /path/to/stock
```

The patch changes only four source paths. It intentionally does not change the project's build system. To integrate the new TU in a research build, add it to the existing strict core target, **not** the JUCE/plugin optimization target:

```cmake
target_sources(cs80_core PRIVATE src/core/ModernLayerMirror.cpp)
```

Preserve stock GNU Release flags: `-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic`; preserve the existing per-source `-ffp-contract=off` on `Ig00156Batch.cpp`. Do not add global fast math, FMA, `-march=native`, LTO, vectorization-policy changes or relaxed FP flags. Header dependents and the real wrapper require normal dependency rebuilding for an eventual native gate; these direct tests do not substitute for that.

The portable recipe uses pristine stock headers for `StockOracle.cpp` and candidate headers for the new method/tests. Each driver is compiled and linked in one command. Its explicit object order is stock oracle, new method, untouched stock archive; the guard omits the unused oracle. No explicit `-no-pie`, vectorization-disable, `-frounding-math` or `-ftrapping-math` option is added. Trap children inherit the same process image through `fork`, so the PC comparison does not need a no-PIE link. The only experimental compile flags are applied to the separate negative method objects; the guard driver stays strict. Linux x86-64/glibc, GCC 14.2.0 and POSIX fork/pipe/sigaction/ucontext are required for the recorded trap fixture.

```sh
# Source/hash verification and command preview only:
python3 reproduce.py --stock /path/to/stock --candidate /path/to/candidate \
  --archive /path/to/stock/libcs80_core.a --output /path/to/new-direct-results

# Explicitly compile and run the bounded direct tests, when authorized:
python3 reproduce.py --stock /path/to/stock --candidate /path/to/candidate \
  --archive /path/to/stock/libcs80_core.a --output /path/to/new-direct-results --execute
```

The archive is not bundled. The recipe refuses a different archive hash; rebuilding with other artifacts is a new qualification and must not be silently represented as this receipt. Historical executable/object hashes identify omitted local artifacts, not downloadable evidence. The exported recipe was statically checked, not executed during packaging. Native, calibration and timing evidence belong to separate gates and are deliberately absent here.
