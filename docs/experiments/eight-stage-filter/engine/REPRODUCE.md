# Offline verification and bounded replay recipe

No tool below downloads anything or changes the original checkout. Use the GNU C++ 14.2.0 Linux x86-64 environment described in `provenance.json`; another toolchain is a new qualification, not the recorded result.

## Offline only

```sh
python3 -B verify.py
python3 -B -m unittest test_verify.py
python3 -B verify.py --stock-src /path/to/authentic/src
python3 -B prepare.py --stock-src /path/to/authentic/src --out /new/replay-directory
```

`verify.py` checks the exact package file set, digests, receipt arithmetic and distinct scopes. With `--stock-src`, it verifies all 193 source files, applies the exact patch in memory without fuzz, verifies all 196 resulting files and reconstructs the exact recorded reporting source. `prepare.py` writes copies only into a new directory, adds portable fixture includes from the authenticated `src/tools` source, and runs no compiler or DSP.

The portable fixture files differ from the recorded versions only by the documented include-path normalization, or are byte-identical. Their original and portable hashes are in `provenance.json`. Original `Work` constructor/step and frame-before-MIDI cadence are retained; imported unused timing routines are never called by these drivers. `ObserverRuntime.inc` is an inert legacy reporting dependency of the archived directed fixture, not oscillator-vector DSP or an enabled candidate feature.

## Explicit replay commands, not performed for this export

From the new replay directory, build each arm separately. These commands deliberately contain no native/fast-math/global-AVX flags. Each compile/run should be bounded, for example 60 CPU seconds and a 90-second watchdog, and stop immediately on failure or mismatch.

```sh
CXX=c++
FLAGS='-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic'
mkdir stock-objects candidate-objects
for arm in stock candidate; do
  for name in Engine FactoryPresets Ig00156Batch; do
    EXTRA=''
    test "$name" != Ig00156Batch || EXTRA='-ffp-contract=off'
    $CXX $FLAGS $EXTRA -I"$arm/src" -c "$arm/src/core/$name.cpp" -o "$arm-objects/$name.cpp.o"
  done
done
$CXX $FLAGS -Icandidate/src -c candidate/src/core/EngineFilterEight.cpp -o candidate-objects/EngineFilterEight.cpp.o
$CXX $FLAGS -ffp-contract=off -Icandidate/src -c candidate/src/core/Ig00156Eight.cpp -o candidate-objects/Ig00156Eight.cpp.o
ar rcs stock-objects/libcs80_core.a stock-objects/Engine.cpp.o stock-objects/FactoryPresets.cpp.o stock-objects/Ig00156Batch.cpp.o
ar rcs candidate-objects/libcs80_core.a candidate-objects/Engine.cpp.o candidate-objects/FactoryPresets.cpp.o candidate-objects/Ig00156Batch.cpp.o candidate-objects/EngineFilterEight.cpp.o candidate-objects/Ig00156Eight.cpp.o
$CXX -O3 -DNDEBUG -std=gnu++20 -fPIC -Icandidate/src -Icandidate/src/core -c EngineFilterEight-observed.cpp -o EngineFilterEight-observed.o
```

Compare the resulting normal object/archive hashes to `provenance.json` before interpreting replay as the same binary. The original backend object must be the genuine four-stage code; do not rebuild it with AVX512 or native target flags. The normal archive has no observer callbacks.

Compile `fixtures/held-trace.cpp` and `fixtures/scene-trace.cpp` using `-fno-access-control` **only for those diagnostic driver TUs**, never the core. Their callbacks satisfy the reporting object. Link the following selections, retaining map files:

- Stock: stock `Engine.cpp.o`, driver, stock archive.
- Normal: candidate `Engine.cpp.o`, driver, candidate archive.
- Reverse: candidate `Ig00156Eight.cpp.o`, `EngineFilterEight.cpp.o`, `Engine.cpp.o`, driver, candidate archive.
- Observed: `EngineFilterEight-observed.o`, candidate `Engine.cpp.o`, driver, candidate archive.

Use `-no-pie` for the archived signal fixture. Check the selected sole oscillator tick is 12,063 bytes and the two original sample bodies are 32,154/37,511 bytes. Sizes alone do not replace the recorded byte/relocation audit.

Driver arguments:

```text
held-trace OUTPUT.bin
scene-trace OUTPUT.bin SCENE
    SCENE: 0 factory-moving, 2 panel-moving, 3 pure-held, 4 sparse-three
traps OUTPUT.bin SIGNALS.csv
directed OUTPUT.bin
stock-census SCENE OBSERVED OUTPUT.bin
    SCENE: 1 factory-held, 0 factory-moving, 2 panel-moving, 3 pure-held
    OBSERVED: 0 ordinary stock, 1 original-kernel reporting wrapper
```

`traps.cpp` defines the two reporting callbacks and asserts zero eight calls in its unsupported-FP target samples. Compile it with diagnostic-only access override; link stock/normal/reverse/observed as above. Normalize actual PCs to function+offset before comparison, not absolute ELF addresses. The archived `directed.cpp`/`ObserverRuntime.inc` need no eight reporting object; link stock/normal/reverse. Its fresh recorded observer object was compiled against authenticated stock headers, with no access override. Never substitute its unused legacy `run()` function for its actual `main()`.

Compare complete raw traces and counts, not selected endpoints or hashes of only a prefix. Include the continuation. For `stock-census.cpp`, use the same diagnostic-only access override but link only the genuine stock Engine object/archive; its OBSERVED argument selects an integer reporting wrapper around the original four-stage kernel, never an eight-stage candidate.

The fixtures have no measurement or timing mode. A new result, even if favorable, must not replace adverse evidence or silently extend this checkpoint's scope.
