# Reproducing the bounded qualification

## Integrity without local build artifacts

Run `python3 docs/experiments/mpe-released-targets/verify.py` from a checkout
whose source/test/CMake hashes match `evidence/source-pins.json`. This checks
archived logs and source identity, not product behavior. Run it again with
`python3 -O`; checks remain active. `self_test.py` corrupts only temporary
bundle copies and expects identical normal/optimized outcomes.

The optional `--external` mode requires the exact original absolute paths in
`external-inputs.json`. A fresh clone does not have those binaries, captures,
archives or mutant trees; that is expected, not evidence of a passing external
check. No archives are reconstructed or downloaded by the verifier.

## Standard core build from matching sources

With CMake, Ninja and a C++20 compiler available, use a new build directory:

```sh
cmake -S . -B ../mpe-release-repro-gcc -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF \
  -DCS80_BUILD_TESTS=ON -DCS80_ENABLE_AVX2_FILTER_BATCH=OFF
cmake --build ../mpe-release-repro-gcc --parallel 2
ctest --test-dir ../mpe-release-repro-gcc --output-on-failure --no-tests=error
```

The focused target is `cs80_mpe_released_target_tests`; its `--unchanged`
argument selects the 842-check compatibility subset. The unchanged baseline
runs require baseline `2ade213` headers and a baseline library rebuilt from
that same source. Never combine new headers with old objects.

For compiler profiles, set CMAKE_CXX_COMPILER to GCC or Clang and use a new
directory per compiler and flag choice. Generic Release uses `-O3 -DNDEBUG`;
native additionally uses `-march=native`. AVX2 filter batching is disabled in
all recorded profiles. Per-profile equality does not imply compiler/CPU-wide
bit identity.

For full-core instrumentation, use a separate GCC build with:

```sh
-DCMAKE_CXX_FLAGS_RELEASE='-O1 -g1 -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS'
```

Run with `ASAN_OPTIONS=detect_leaks=0` and `UBSAN_OPTIONS=halt_on_error=1`.
This instruments the linked core, not only a driver; it is not native JUCE or
leak-sanitizer qualification. Archived profile commands are authoritative for
the exact seven executed targets and configuration.

## Native processor, raw comparison and faults

The native build uses official JUCE 8.0.11 commit
`c352e2489000ffeb9e397051377d75eef2efe641` and previously installed local
native dependencies. Acquisition provenance is inherited from
`../../mpe-member-onset/evidence/native-bootstrap/README.md`. No new dependency
installation or system-setting change was needed here. Historical scripts
under `evidence/native/` contain absolute local JUCE/sysroot/tool paths;
provide equivalent dependencies and adapt **copies** of the scripts in a new
output directory. Run `CS80Snapshots --mpe-released-target-tests` and the
registered `cs80_mpe_released_target_host_tests` with `--no-tests=error`.
No display/editor is required for these processor tests.

The independent raw fixture is
`evidence/compatibility/CompatibilityCapture-v2.cpp`. Its driver extracts the
actual Engine compile flags and links each side to its own matching archive.
The exact final comparison is eight builds, two block partitions per build,
270 scenarios/run. Rebuild both sides and use fresh output paths before
adapting `run_capture.py`, `compare.py` and `finalize_evidence.py`. The included
original scripts intentionally preserve local source/output guards; they are
not claimed to run unmodified from a fresh clone.

Fault reproduction uses the exact archived `fault-controls/run-3003569/reproducer.py`
and mutation diffs when the completed fault bundle is present. It compiles a
private substituted Engine object and proves the original archive Engine member
was not linked. Preserve complete stdout/stderr/statuses and require 7,488
checks plus normal intended failures. Build errors, crashes, missing summaries,
and unexpected exits do not count as detections. New runs must not overwrite
old attempts. Rebuild libraries/objects from pinned sources before relocating
any historical paths; changing only path strings is not proof of provenance.

`archive.py` is a maintainer collection tool, not runtime reproduction. It
refuses to overwrite changed historical raw evidence. Use it only after
writers stop, then re-run integrity and mutation self-tests. Updating a
manifest after an unexplained corruption is not a valid repair of evidence.

## Explicit screenshot-only publication integration

Only in the fresh integration worktree based on the reviewed screenshot child,
after applying the frozen candidate and this bundle, run:

```sh
python3 docs/experiments/mpe-released-targets/reproducibility/record_integration.py \
  --screenshot-parent 4e01dcda8f0e6a721c492ef5e2ae4d952e7230c5
```

The tool reads Git objects to establish that the screenshot child's parent has
the exact original baseline tree, derives its 24-path screenshot-only change,
and requires the complete 70-path screenshot subtree to match the integrated
checkout. It still enforces all qualified source/test/CMake hashes and all
non-screenshot preserved paths. It writes only a new integration record,
`INTEGRATION.md` and the manifest. It refuses an existing integration record;
it does not rewrite historical source/run pins or claim runtime replay.

Then run normal, optimized, external (if retained local files are available)
and mutation checks again. The overlay self-tests use explicitly synthetic
schema records plus a changed-runtime-byte check, not fabricated render results.
The original baseline-only bundle remains valid without the optional overlay.
`seal.py` updates only the integrity index after explained documentation or
integration-record additions; it is not a way to legitimize unexplained data
changes. Do not rerun the original collector against the integration as though
it were the historical qualification worktree.
