# Source inputs and untimed reproduction

This is a research evidence cohort, not a standalone plugin project. No command in this document was run during export.

## Explicit inputs

- Two clean repository checkouts at `e9e8175f4278d2457279fcb8809cbba1ddb7be2b`, called `$STOCK` and `$CANDIDATE` below.
- `$D`: this experiment directory; `$OUT`: a new output directory chosen for the rerun.
- Recorded environment: GNU C++14.2, Linux x86-64, ordinary Release/PIC flags. No `-march=native`, FMA or unsafe-math additions.

Apply only `candidate.patch` to the candidate checkout, not the historical V1 patch or evidence files. Verify its SHA256 against `provenance.json`, run `git apply --check`, then apply it in that isolated checkout. The historical normal-arm receipt proves exact patch reconstruction across 193 source files. Do not apply evidence hooks to production source.

The normal core comprises `src/core/Engine.cpp`, `FactoryPresets.cpp` and `Ig00156Batch.cpp`. Compile each arm with:

```text
-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic -I<arm>/src
```

Add `-ffp-contract=off` only for `Ig00156Batch.cpp`, as in the recorded build. Archive the three objects with `ar rcsD`. Historical stock objects came from the sealed original archive; a fresh rebuild is a new artifact and must have its identity checked rather than inheriting the recorded archive hash.

## Structural gate first

The portable isolation checker accepts explicit object paths and writes a compact receipt:

```sh
python3 "$D/tests/check-scalar-isolation.py" \
  "$OUT/stock/Engine.cpp.o" "$OUT/candidate/Engine.cpp.o" \
  "$OUT/scalar-isolation.json"
```

It compares non-batch Engine bytes and symbolic relocations, resolving local constants to their bytes. It does not render audio. Retain the observed comparison even on failure. A different compiler's output does not inherit the recorded isolation result.

## Direct, trap and negative tests

`tests/component.cpp` and `tests/traps.cpp` are exact copies of the recorded fixtures. Compile/link them against the candidate headers and `Ig00156Batch.cpp.o`, with the ordinary flags above; no optional benchmark mode is needed. The component takes no arguments. The trap fixture takes one new output filename for its binary records. Keep stderr, stdout and exit status.

`tests/aa.cpp` is the recorded stock-versus-stock target-arm control. `tests/disabled.cpp` requires the backend **and fixture** to be compiled with `-DCS80_DISABLE_AVX2_FILTER_BATCH`; its expected hits and donors are zero. Do not substitute that disabled result for a positive-hit qualification.

The portable mutation tool constructs four separate copies under a new output directory and runs bounded untimed checks, never rewriting candidate sources:

```sh
python3 "$D/tests/mutations.py" "$CANDIDATE" "$OUT/mutations"
```

Both Python tools are editorial path adaptations of the original tools. Their syntax and exported paths were inspected, but they were not compiled/executed as a new qualification during export.

## Normal versus counted traces

The unchanged `tests/trace.cpp` includes the pinned baseline's `src/tools/source_factory_benchmark.cpp` through the arm's source include directory. Compile **only the trace translation unit** with `-fno-access-control`, then link it with the corresponding normal core archive. This exposes existing fields for observation without modifying core class definitions or objects.

For the counted candidate only, compile `evidence-hooks/counter.cpp`, add `-DCOUNTED` to the trace compilation, and link with:

```sh
-Wl,--wrap="$(cat "$D/evidence-hooks/wrapper-symbol.txt")"
```

Each trace executable takes a distinct output filename prefix. It writes ten scenes, two binary streams per scene and CSV counts. Stock and normal candidate counts are intentionally zero because the counter is absent, not because those modes prove no helper work. Compare all stock/candidate/counted stream lengths and SHA256 hashes; the existing receipt includes historical-stock comparisons and the actual counted hits.

The three sources under `tests/focused/` are pinned standard project tests. The recorded checks used their ordinary modes, not optional timing arguments. Compile/link against the normal candidate archive. Their results supplement, rather than replace, the direct and normal-arm comparisons.

## Cost sources are separate

`cost/cost.cpp` and its two `fixture/` imports are unchanged copies of the isolated cost fixture. Normal, diagnostic and counted modes must remain separate. The fixture has explicit identity/diagnose modes, but the presence of source code is not a new timing result or authorization to silently rerun a campaign. The [published fixed protocol and complete replay](cost/README.md) reproduce the measurements without running DSP.

No raw streams, executables or object archives are distributed here. Reproduction needs the pinned full checkout and compiler, not just these hashes.
