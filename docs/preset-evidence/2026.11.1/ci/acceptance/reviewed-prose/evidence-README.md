# Preset refinement evidence — 2026.11.1

This directory preserves the authoring measurements and independent reviews for
twelve targeted Circuit-program refinements. The baseline is
`7f55a95a7534b389927defe46866456560b3a878`, which already contains the substantial
2026.11 redesign of all 300 musical Circuit programs. These results describe the
additional 2026.11.1 refinement, not a second claim that all 300 were reauthored.

The [refinement report](../../PRESET_REFINEMENT_2026_11_1.md) explains the resulting
sounds. [Provenance](provenance.json) records every archived input, its original
SHA-256, the stored SHA-256 and any textual transformation.

**All four current installation archives and the actual 24-WAV audition are
verified and downloadable. Linux remains experimental and unqualified because
its GUI validator failed; the versioned prerelease is blocked.** Windows and
both Mac native suites and required strictness-10 validators passed on attempt 1
for source `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`,
[build 45044040694](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45044040694).
Linux's native build/test/package job passed, but its separate validator job
failed. [Release qualification](ci/release-qualification.json)

The [integration lineage](ci/source-lineage.json) retains the cancelled predecessor
and its actual workflow-snapshot failure on all four native runners. Correcting
that expectation changed no production bank, packaging code or required
validator command. The older [2026.11 qualification](../2026.11/ci/release-qualification.json)
remains separate and does not qualify these newer binaries.

## Current development platforms and delivery

| Target | Native result in attempt 1 | Development download and evidence |
| --- | --- | --- |
| Windows x64, VST3 / Standalone | 221 registered CTest entries: 219 passed, zero failed, two formal skips; both strictness-10 VST3 gates passed. [Native record](ci/native-windows-x86_64.json) | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763872941.zip) · [Archive record](ci/install-windows-x86_64.json) |
| macOS Apple Silicon, VST3 / AU / Standalone | 243 registered CTest entries: 241 passed, zero failed, two formal skips; VST3, AU and AU GUI strictness-10 gates passed. [Native record](ci/native-macos-arm64.json) | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763761497.zip) · [Archive record](ci/install-macos-arm64.json) |
| macOS Intel, VST3 / AU / Standalone | 243 registered CTest entries: 241 passed, zero failed, two formal skips; VST3, AU and AU GUI strictness-10 gates passed. [Native record](ci/native-macos-x86_64.json) | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763644216.zip) · [Archive record](ci/install-macos-x86_64.json) |
| Linux x86-64, VST3 / Standalone | 579 registered CTest entries: 578 passed, zero failed, one formal skip; native packaging passed. **Experimental / unqualified: GUI validator failed.** [Native record](ci/native-linux-x86_64.json) | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14768807632.zip) · [Validator result](ci/final-linux-verification.json) |
| Four-platform archive integrity and delivery | **PASS:** 114 inventory entries, 10 native headers; exact source/run/revision, guides and `SOURCE.txt`; all four public downloads match API sizes and SHA-256 | [Aggregate archive record](ci/final-install-artifacts.json) · [Public download records](ci/download-links.json) |
| Actual 24-WAV audition | **PASS:** revised-bank capture from this source/run/attempt, 117 seconds, 32 checksum entries and verified public download | [Audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14769036590.zip) · [Actual verification](ci/final-audition.json) · [Public download](ci/audition-public-download.json) |
| Versioned `presets-2026.11.1` prerelease | **BLOCKED:** required Linux GUI gate failed; publisher skipped; no exact tag or release in the final API check | [Qualification record](ci/release-qualification.json) |

The four native suites total **1,286 registered CTest entries: 1,279 passed,
zero failed and seven formal skips**. **24 nested Python subtest skips** remain
separate: 11 Windows, five on each Mac and three Linux. They are not extra formal
CTest skips or passing subtests. The validator failure is outside these native
CTest totals.

The unchanged version-3 aggregate verifies all four actual installation archives.
The complete Linux and Windows VST3 inventories also match the separate artifacts
actually downloaded by their validator jobs. All four installation ZIPs and the
audition ZIP passed full unauthenticated public GET checks against API sizes and
SHA-256 values. Archive integrity and input identity do not establish runtime
success. [Complete validator payload identity](ci/final-validator-payload-identity.json)

The actual audition contains ten standard examples, one Pressure Lead example
and six refined examples at four seconds each, plus seven slow examples at seven
seconds each. It uses the final 301-program inventory and unnormalized 48 kHz
stereo float captures. Pressure Lead uses aftertouch 0.65; the others use zero
pressure. This actual revised-bank artifact is separate from the historical
baseline-executable pipeline smoke below.

### Linux failure and blocked publication

The first-attempt GUI/Xvfb gate in job `137324977642`, seed `0x4369262`, printed
`SUCCESS` and then terminated by **SIGSEGV, raw −11 / wrapper exit 139**. The
headless gate at `0x3e72560` and fixed GUI regression gate at `0x2da72db` exited
normally with code 0. GDB and fault-trace replays of the failed seed were bounded
nonreproductions. They do not replace the failed gate or demonstrate a fix;
crash ownership remains unresolved. [Linux verification](ci/final-linux-verification.json)

[Publisher run 45060638607](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45060638607)
was skipped. The final API check found no tag or release named
`presets-2026.11.1`; qualified versioned publication remains blocked. These are
CI development bundles without release signing or notarization, and the Linux
bundle is explicitly experimental. No complete required-validator pass or
universal DAW compatibility is claimed.

The retained 2026.11 four-platform downloads and twelve-WAV audition use the older
`d0c3c23` source. Its unresolved Linux failure, successful second attempt and
bounded nonreproductions remain in the [original qualification history](../../PRESET_REVISION_2026_11.md#linux-validator-history-and-remaining-limitation).
The current refinement's [VST3 Program-queue controller dependency](../../PRESET_REFINEMENT_2026_11_1.md#native-recall-and-host-program-qualification)
also remains part of the stated host-compatibility limits. Explicit **Library**
selection loads the current factory voicing; existing sessions retain their
serialized sound. The [installation guide](../../INSTALL_AND_QUICKSTART.md)
retains complete-bundle extraction, host-process architecture selection and
system-library requirements.

## Read the evidence at its recorded stage

The family measurements use the **frozen baseline engine and bank**, with the
exact candidate parameter overrides recorded in their JSON and scripts. They
retain the baseline master levels. This makes the measured relative layer and
stereo changes causal; it does not make these captures the final calibrated
plug-in build. Later calibration can change absolute dBFS values.

Final core calibration and selected bank tests are archived under [final](final/summary.json).
Performed wind phrases and stereo chords also use the final calibrated bank.
Native-host tests and published CI artifacts are recorded separately by the integration pass. A successful local
candidate or pipeline smoke test is not proof that an operating-system package
has been published or validated. The source commit and package manifests identify
the download that was actually built.

| Record | What it establishes | Qualification |
| --- | --- | --- |
| [Winds](winds/summary.json), [matrix](winds/matrix.json), [center](winds/center.json) | Flute and Breathy Flute sustained air/body balance across register, velocity and fixed pressure endpoints; eight-note stress | Candidate overrides at baseline master levels; pressure endpoints are not performed crescendos |
| [Plucks](plucks/measurements.json) | Harp and Kalimba upper-mode onset and independent decay across three notes and two velocities | Baseline onset rows come from the separately archived [strike screen](strike-baseline/measurements.json) |
| [Bass](bass/summary.json), [matrix](bass/matrix.json) | Sine Bass and Upright Pluck transient contribution and disappearance, plus eight-note stress | `woody` labels the accepted candidate; `gain` in the center exploration is an alternative that was not adopted |
| [Stereo](stereo/initial.json), [register checks](stereo/registers.json) | Five ensembles' side/mid energy, channel balance, mono power and fundamental-band retention | Single held notes with full factory effects; chord and long-tail integration is separate |
| [Ether Pad](ether/summary.json), [measurements](ether/measurements.json) | A stronger late air layer with the pitched body still dominant | Three registers and three fixed pressure levels, before final master recalibration |
| [Character tests](character/summary.json), [current](character/current.log), [baseline](character/baseline.log) | Identical rendered-behavior checks distinguish the candidate bank from the previous bank | The hardened snapshot has **128 checks**; the baseline fails 16. The final integrated rerun is separate |
| [Identity](identity/summary.json) | Exactly twelve changed vectors, 289 unchanged Circuit vectors, all 222 complete Legacy records and 49,152 MIDI addresses preserved | Pre-calibration snapshot; its four description changes precede the later UFO Landing wording correction |
| [Ring phrase behavior](ring/summary.json), [measurements](ring/measurements.csv) | Overlapping keys retain the shared ring sweep; a detached phrase retriggers it | Sustain wording is source-backed; this particular probe did not exercise the sustain pedal |
| [Audition pipeline](audition/summary.json), [success smoke](audition/smoke-result.json), [failure control](audition/failure-result.json) | Complete 24-file selection, capture settings, checksums and explicit failed provenance | Pipeline smoke used the **baseline executable**, not the final revised bank |
| [Adversarial review](review/summary.json) | Independent review of the mix, spectrum and stereo calculations and test guards | Preserves the earlier 116-check snapshot; its requested same-window reconstruction checks produced the later 128-check suite |
| [Session compatibility](compatibility/summary.json) | Serialized sound restoration remains distinct from explicit selection of a current factory program | Source-control-flow review, not a new native-host run |
| [Final identity](final-identity/summary.json), [method](final-identity/README.md) | Twelve changed vectors / 42 floats, 289 unchanged vectors, 288 unchanged complete Circuit records; all 222 Legacy records, 190 parameter IDs and 49,152 MIDI addresses preserved | Five descriptions change, including UFO Landing without a value change. Host parameter count is derived from unchanged layout; native-host execution remains separate |
| [Final core integration](final/summary.json), [complete stdout](final/ctest-full.log), [JUnit](final/ctest-junit.xml) | Four selected suites pass: all 523 factory records audited, 136 voicing checks, 128 character checks, and nine capture CLI tests; two applied calibration passes converge | Exact final inventory, executable and source hashes are recorded; native-host and published-package verification remain separate |
| [Whole-bank distinctness](distinctness/summary.json), [focused pairs](distinctness/focused.json) | 301 programs, 45,150 pair comparisons, no raw or peak-normalized duplicates; closest pairs differ in register, decay or onset | The original snapshot label is retained; its inventory hash matches the final inventory. Correlation is a diagnostic ranking, not a quality score |
| [Performed winds](wind-performance/summary.json), [phrases](wind-performance/sequences.csv), [pressure controls](wind-performance/pressure-step.csv) | 24 renders / 96 seconds: channel and poly pressure, soft/hard repeats, overlap and complete release; matched pressure-step controls isolate continuity | Final factory parameters at 48 kHz; no instrument-equivalence or listening verdict |
| [Stereo chords](stereo-chords/summary.json), [phase measurements](stereo-chords/phase-measurements.json), [captures](stereo-chords/capture.csv) | Ten 12-second factory/control sequences reach eight voices and retire them; worst factory peak −3.817 dBFS, mono loss 0.317 dB overall / 0.481 dB in fundamental bands | Bounds apply to the recorded staggered chords and pressure timeline; retired voices leave an approximately −82 dBFS output floor |

The 128-check character suite covers eleven programs. Ether Pad has separate
multi-register authoring measurements; it is not silently counted as a twelfth
program in that test target. Measurements support bounded software behavior,
not acoustic-instrument identity, physical CS-80 calibration or a listening-panel
verdict. Raw audio and executables are omitted from this compact source dossier.
The actual [24-WAV CI audition](ci/final-audition.json) is a separately verified
revised-bank artifact, linked above; it does not rewrite these authoring stages
or turn the baseline pipeline smoke into a final-bank capture.

## Measurement conventions

Layer comparisons weight isolated ranks by the actual factory blend and compare
them with the complete patch. The reconstruction check prevents a different
oscillator or circuit history from being mistaken for a contribution in the
finished mix. Detailed scripts preserve their precise float conversion choices;
the permanent character tests additionally gate reconstruction in each window
used for a layer claim.

Audio is unnormalized, stereo floating point. RMS uses the average power of the
two channels, avoiding cancellation in the measurement itself. Stereo tests use
`mid = (L + R) / 2`, `side = (L - R) / 2`, and compare mono power with averaged
stereo-channel power. Fundamental bands and windows are recorded in their
scripts. A low component ratio is an authoring observation, not a universal
threshold of human audibility.

The authored pan spread is available in ordinary Circuit mode. **Pure CS-80
centers both rank pans**, so it deliberately removes that spread. The engine and
mode-isolation rules were not modified for these presets.

## Reproduce the authoring comparisons

Use a fresh working checkout or fresh `build-*` output directories. Some original
measurement scripts reuse existing captures, so reusing a directory containing
other settings would invalidate a new comparison. Python analysis requires
NumPy and SciPy; the core and probes require CMake and a C++20 compiler. The
recorded baseline was built with GCC 14.2 in Release mode. Exact executable hashes
can vary with toolchain and build environment.

From the repository root, build the immutable baseline separately:

```sh
git fetch origin 7f55a95a7534b389927defe46866456560b3a878
git worktree add --detach build-preset-baseline-source 7f55a95a7534b389927defe46866456560b3a878
cmake -S build-preset-baseline-source -B build-preset-baseline-source/build-core \
  -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF
cmake --build build-preset-baseline-source/build-core --target cs80_preset_capture --parallel 2

mkdir -p build-baseline build-probe
cp build-preset-baseline-source/build-core/libcs80_core.a build-baseline/
cp build-preset-baseline-source/build-core/cs80_preset_capture build-baseline/
./build-baseline/cs80_preset_capture --inventory build-baseline/inventory.json
cp docs/preset-evidence/2026.11.1/reproduce/build-probe/diagnostic_capture.cpp build-probe/
c++ -std=c++20 -O2 -I build-preset-baseline-source/src \
  build-probe/diagnostic_capture.cpp build-baseline/libcs80_core.a -o build-probe/capture
```

`build-probe/capture` is an archived diagnostic extension of the capture CLI. Its
repeatable `--set parameter_id=value` option applies overrides before rank
isolation. It deliberately does not provide the production parameter-range
checks; the final library audit owns those gates. Do not substitute a newly
calibrated library for the frozen baseline when reproducing the authoring tables.

Restore the original script directory layout, then run the requested measurement
families. Copying the archived `build-*` folders into the repository root preserves
the scripts' relative-path assumptions. The strike screen must run before the
pluck comparison, because the latter reads its baseline rows.

```sh
cp -R docs/preset-evidence/2026.11.1/reproduce/build-w02-strikes .
cp -R docs/preset-evidence/2026.11.1/reproduce/build-w02-winds .
cp -R docs/preset-evidence/2026.11.1/reproduce/build-w02-plucks .
cp -R docs/preset-evidence/2026.11.1/reproduce/build-w02-bass .
cp -R docs/preset-evidence/2026.11.1/reproduce/build-w02-stereo .
cp -R docs/preset-evidence/2026.11.1/reproduce/build-w03-ether .

python3 build-w02-strikes/measure.py
python3 build-w02-winds/analyze.py
python3 build-w02-winds/analyze.py --matrix
python3 build-w02-plucks/measure.py
python3 build-w02-bass/measure.py
python3 build-w02-bass/measure.py --matrix
python3 build-w02-stereo/measure.py
python3 build-w02-stereo/registers.py
python3 build-w03-ether/measure.py
```

The archived wind and bass stress sources can be compiled against the same
baseline headers and `build-baseline/libcs80_core.a`; each applies its stated
candidate overrides internally. The ring probe does likewise for its unchanged
engine diagnostic. The identity helper serializes complete Legacy records and
MIDI mappings in the byte format specified by `identity/summary.json`; compile it
separately against the baseline and the candidate library being compared.

The [archived character source](reproduce/character/PresetCharacterTests.cpp)
corresponds to the test-source hash in `character/summary.json`. Linking that test
against only the frozen baseline core reproduces its negative control. Linking
against the current `cs80_core` tests the current compiled factory bank. The
standard CMake target is `cs80_preset_character_tests`. A later integrated run can
have different absolute levels after calibration without changing these broad
behavioral bounds.

The archived audition scripts preserve the pipeline implementation and its
baseline smoke harness. The actual CI workflow supplies `GITHUB_SHA`, repository,
run ID and run-attempt variables and verifies the checkout commit. **Do not use
the baseline smoke harness to claim a revised-bank or published CI audition.**

## Reproduce the final core and performed checks

Build the selected tests from the revision under review, preserving fresh output
directories. The integrated run used Release mode and the final calibrated
factory values. Both archived applied calibration tables are byte-identical to
the recorded factory level table; four programs changed master level. The full
[CTest log](final/ctest-full.log) retains the check counts that JUnit truncates.

```sh
cmake -S . -B build-audit -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-audit --parallel 2 --target cs80_preset_capture \
  cs80_library_audit_tests cs80_preset_voicing_tests cs80_preset_character_tests
./build-audit/cs80_preset_capture --inventory build-audit/inventory-final.json
ctest --test-dir build-audit \
  -R 'cs80_(library_audit|preset_voicing|preset_character|preset_capture)_tests' \
  --output-on-failure --output-junit preset-final-results.xml

cp -R docs/preset-evidence/2026.11.1/reproduce/build-w04-wind-performance .
c++ -std=c++20 -O2 -I src build-w04-wind-performance/probe.cpp \
  build-audit/libcs80_core.a -o build-w04-wind-performance/probe
./build-w04-wind-performance/probe

cp -R docs/preset-evidence/2026.11.1/reproduce/build-w05-stereo-chords .
cp build-audit/libcs80_core.a build-w05-stereo-chords/libcs80_core-final.a
cp build-audit/inventory-final.json build-w05-stereo-chords/
c++ -std=c++20 -O2 -I src build-w05-stereo-chords/probe.cpp \
  build-w05-stereo-chords/libcs80_core-final.a -o build-w05-stereo-chords/probe
./build-w05-stereo-chords/probe
python3 build-w05-stereo-chords/analyze.py
```

The compact chord archive preserves all per-program bounds and six named phases.
Its [selection record](stereo-chords/phase-measurements.json) hashes the original
full per-window analysis. Rerunning the archived analyzer regenerates every
250 ms window. The exact source preserves its original working-tree provenance
wording; record the new checkout commit and generated hashes for any later run.

For the whole-bank screen, restore the two scripts, freeze the current capture
executable, render the explicit fixture and run both analyses:

```sh
cp -R docs/preset-evidence/2026.11.1/reproduce/build-w04-distinctness .
cp build-audit/cs80_preset_capture build-w04-distinctness/capture-snapshot
./build-w04-distinctness/capture-snapshot \
  --inventory build-w04-distinctness/inventory.json
./build-w04-distinctness/capture-snapshot --out build-w04-distinctness/full \
  --all --wav --rate 48000 --note 60 --velocity .8 --pressure .5 --hold .7 --tail .5
python3 build-w04-distinctness/analyze.py
python3 build-w04-distinctness/focus.py
```

The [omitted-artifact hashes](distinctness/omitted-artifacts.json) identify the
original 301-program analysis, inventory and capture binary. The summary retains
the initial after-first-pass label, while its inventory is byte-identical to the
final integrated inventory because the second applied level pass converged.

The [final identity method](final-identity/README.md) documents the compiled-record
comparison. Restore `reproduce/build-w05-final-identity` to the repository root,
recreate its frozen baseline/final libraries and inventories as named there, and
run the two C++ serializers followed by `compare.py`. The final report records
42 changed float32 values: 38 voicing values and four master volumes. Its 289
unchanged Circuit value vectors include UFO Landing, whose description changes;
therefore 288 complete Circuit records including metadata are unchanged. All
222 complete Legacy records remain byte-identical.

## Archive integrity

`provenance.json` lists original and stored hashes. For text data, only the
original absolute checkout prefix is removed where present; numeric values and
relative path suffixes remain unchanged. Reproduction sources are verbatim.
The manifest also covers final core calibration, integrated selected tests and
performed audio evidence added by assignment 28. Native-host CI, package
verification and agent records can be added without rewriting historical
measurements. The final CTest log abbreviates its producing Python runtime path;
its measurements and printed timestamps are preserved. Compact phase indexes
select existing measurements without recalculation and retain the full source
file hash.
