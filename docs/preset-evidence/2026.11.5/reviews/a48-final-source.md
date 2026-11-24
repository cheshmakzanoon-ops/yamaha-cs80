# Worker 48 — final c76 source equivalence supplement

## Finding and scope

**The original screenshot qualification remains applicable to the unchanged inputs consumed by the screenshot workflow at `c76ad288e02fbeca48f39652bf4fcdd99837f793`.** This is a static source comparison, not a new execution. The actual qualified run remains **45344888473, attempt 1, job 138276946587**, with artifact **14899141895**, executed at **`09af5358d34a7ea431de7e8a42e35dae66e52159`**. Its original API receipts, logs, image measurements and acceptance are unchanged. [Original qualification](a48.md) · [new machine-readable supplement](../ci/source-09af535/screenshots/source-c76ad28-equivalence.json).

The candidate tree is **`416c1f6bfb4cd9f7dd5dbd9f5cbc211edda2bf00`**. The prior comparison to `db6b5a76b3c5e38dd9444d0cf6065a14f6302814` is preserved in full. The complete Git diff from 09af to c76 contains exactly these three files:

| Changed path | Accounting in this supplement |
|---|---|
| `docs/INSTALL_AND_QUICKSTART.md` | Existing bundled-guide change already present at db6b; no additional change from db6b to c76. |
| `docs/PRESET_GUIDE.md` | Existing bundled-guide change already present at db6b; no additional change from db6b to c76. |
| `tests/test_parity_capture.py` | New difference from db6b. Corrects the fixture's expected core parameter count from 200 to 202 and adds strict body-variant metadata negative controls. Its real CTest dependency is explicitly retained below. |

Every changed path's old/new Git object, byte count and SHA-256 is recorded in the supplement. No production C++, UI, CMake, workflow, wrapper, font or golden-image file changed. [Complete source delta](https://github.com/cheshmakzanoon-ops/yamaha-cs80/compare/09af5358d34a7ea431de7e8a42e35dae66e52159...c76ad288e02fbeca48f39652bf4fcdd99837f793).

## The original 821-entry inventory is not wholly identical to c76

The earlier inventory deliberately includes a broad selection of tests. For this comparison I re-read **all 821 original source blobs**, checked their sizes and SHA-256 values against the existing inventory, verified their original equality to db6b, then compared each candidate tree entry's mode, type and blob identity.

- **820 entries are unchanged**, with a complete enumerated list in the new JSON.
- **One entry changes: `tests/test_parity_capture.py`.** It has not been removed from the comparison or counted as identical.
- The original **821-of-821 db6b comparison remains unchanged**. The new supplement explicitly sets `entire_821_inventory_identical` to `false` for c76.

| Python fixture | Bytes | SHA-256 |
|---|---:|---|
| Original 09af / prior db6b | 23,917 | `f0e878d69d1faf1f3965d733775a994b2ed1e11868f4a2e9c2691e10a21ca89e` |
| Candidate c76 | 26,831 | `53fbfc69eb98b4e39d9fe2675c2a5974ac0e9964279ee85a5f92371b234b2113` |

[Original inventory](../ci/source-09af535/screenshots/source-contract.json) · [complete 820/1 comparison and exact Python diff](../ci/source-09af535/screenshots/source-c76ad28-equivalence.json).

## The changed fixture has a real dependency outside this execution

The unchanged CMake file registers **`cs80_parity_capture_tests`**, which runs `tests/test_parity_capture.py --capture <cs80_parity_capture executable>`. The companion executable is a separate target built from `src/tools/parity_capture.cpp` and linked to `cs80_core`. Registering that CTest command writes the script's path into generated test metadata; it does not read or execute the script body. This dependency is acknowledged, and this supplement does **not** qualify the revised Python fixture. [Pinned CMake registration](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/c76ad288e02fbeca48f39652bf4fcdd99837f793/CMakeLists.txt#L992-L1001).

The screenshot job instead performs the following unchanged sequence:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target CS80Snapshots cs80_tests --parallel 2
./build/cs80_tests
scripts/run-ui-smoke-linux.sh .../CS80Snapshots --out artifacts/ui --self-test
```

It does not invoke CTest, the parity Python script or the parity-capture executable. `cs80_tests` is built from `tests/CoreTests.cpp` and `cs80_core`. `CS80Snapshots` is built from the explicit shared plugin C++ list plus `src/tools/ui_snapshot.cpp`, linked with the core, JUCE, libxml2, platform libraries and the embedded panel fonts. Its golden fixture reads `tests/golden`. The changed Python fixture is absent from these source lists, link dependencies, custom build actions and direct execution commands. All relevant C++ and header inputs remain unchanged. [Pinned screenshot workflow](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/c76ad288e02fbeca48f39652bf4fcdd99837f793/.github/workflows/screenshots.yml) · [pinned screenshot target](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/c76ad288e02fbeca48f39652bf4fcdd99837f793/CMakeLists.txt#L2509-L2542).

## Configure-time content reads and source hashes

I also checked configuration itself, rather than relying only on which executables the job runs. The project has **13 `file(SHA256 ...)` command sites**: ten direct fixed paths and three finite input loops. Expanding those lists yields **18 unique repository files**. Every one is unchanged between 09af and c76. The lists cover the offline truth renderer, amplitude/filter production tracers, VCA reference, shared WAV/decimal helpers and their explicit source/model documents. **None of the three changed files is in those lists.** The complete expanded input set, hashes, command-site counts and source excerpts are retained in the new JSON. [Pinned source-hash configuration](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/c76ad288e02fbeca48f39652bf4fcdd99837f793/CMakeLists.txt#L2788-L2931).

The reviewed project configuration has two `execute_process` sites. They call the unchanged JUCE automation-wrapper and parameter-state-mirror generators. The first reads explicitly selected, hash-pinned JUCE wrapper sources; the second reads three explicitly pinned JUCE audio-processors sources. They may compare existing generated outputs. Neither imports or scans project Python fixtures or guides. The XML parser configuration operates on its separately pinned libxml2 source; the host-automation test module defines separate compatibility witnesses. Their source hashes are retained as additional configuration evidence, including the two generator scripts that were outside the original broad 821-entry inventory. [Automation configuration](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/c76ad288e02fbeca48f39652bf4fcdd99837f793/cmake/HostAutomation.cmake) · [state-mirror configuration](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/c76ad288e02fbeca48f39652bf4fcdd99837f793/cmake/ParameterStateMirror.cmake).

The changed guides retain the same bounded exclusions as the db6b comparison: they are bundled packaging documentation, not input to the selected C++ targets, snapshot invocation or configure-time hashes. The screenshot job does not run installers, packaging operations or the separately registered packaging/document tests. This exclusion does not claim that those guides are irrelevant to distribution packages.

## Trigger and execution attribution

The unchanged screenshot push filter includes `tests/*.h`, not `tests/test_parity_capture.py`; the two changed Markdown guides also match none of its seven explicit paths. Static evaluation therefore finds **zero matching changed paths**. This explains why these changes do not select another screenshot push run under that workflow. Manual dispatch remains available, but no dispatch or new execution was performed or claimed for this supplement. [Pinned trigger](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/c76ad288e02fbeca48f39652bf4fcdd99837f793/.github/workflows/screenshots.yml#L3-L16).

The original 09af artifact remains the observation. The original main-branch guard outcome, image provenance and gallery-publication status remain unchanged. This source extension does not relabel those images or the run as c76, establish pixel equality across operating systems, qualify native release packages, or measure instrument acoustic realism.

## Preservation and new files

The supplement records SHA-256 and byte size for **all 19 pre-existing screenshot evidence/review files** and checks that they remain identical before and after this review. That includes the original inventory, acceptance, raw receipts, logs, verifier, artifact metadata, file inventory, image review, README and `a48.md`.

Only two fresh files were written: this review and `source-c76ad28-equivalence.json`. No existing report or manifest was edited; no images, downloads, builds, CI runs, gallery changes, commits or pushes were made. The genuine dependency found is the separate registered parity CTest described above; it does not invalidate the source equivalence for the commands actually executed by the original screenshot workflow.
