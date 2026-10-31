# Parallel workstreams for the improvement plan

The remaining steps of [IMPROVEMENT_PLAN.md](../IMPROVEMENT_PLAN.md) (Phases 1–7,
as summarised in [CS80_PLAN_AND_STATUS.pdf](../CS80_PLAN_AND_STATUS.pdf)) are split
into eight workstreams that are developed in parallel and integrated on one
branch. Each workstream records its result in its own `wsN-*.md` file here; the
integrator folds those into [STATUS.md](../STATUS.md) and
[HARDWARE_COVERAGE.md](../HARDWARE_COVERAGE.md).

| Id | Workstream | Plan steps |
| --- | --- | --- |
| ws1-voice | Voice circuit | 1.1b IG00156 nodal model, 1.2 oscillator promotion, 1.4 VCA, 2.5 tolerances and drift |
| ws2-engine | Engine I/O and modes | parameter smoothing, 1.6 oversampling quality, 1.7 engine default flip, Phase 3 output and noise, Phase 6 host parameter groups |
| ws3-keyboard | Voices, keyboard, touch and tuning | 2.1 free-running voices, 2.2 KAS scan and sample/hold, 2.3 keyboard contour, 2.4 touch, ribbon MIDI CC, Phase 6 tuning (MTS-ESP, Scala/KBM), MPE and voice modes |
| ws4-circuits | Original modulation and effect circuits | Phase 3 SUB (IG00150), ring modulator, OE1/OE2 chorus and tremolo, PRA wah/EXP |
| ws5-modern | Modern modulation and FX | Phase 6 MSEGs, LFO waveforms, mod matrix, keyboard modulation, FX, and their UI pages |
| ws6-library | Tones, memories and library | Phase 4 tone tabs, memory panels, curated library, Legacy Library, Program Change; library audition and prev/next; Phase 6 A/B compare and randomise |
| ws7-panel | Original panel UI | 5.1 conformance audit, 5.2 re-layout, 5.5 readability, 5.6 editor code health, 5.7 golden images; memory-lid store/copy and rank copy |
| ws8-studio | Modern UI layer | 5.3 toolbar and Studio view, 5.4 interaction polish, 5.7 UI tests, MIDI learn, undo/redo, learning tools and evidence view |

Phase 7 (release qualification) and the final `processInternalSample` split,
fast-math and structure-of-arrays CPU pass run after integration, when every
audible feature is in place and the CPU budget can be measured once.

## Shared rules

- **Legacy audio never changes.** `scripts/check-compat-gates.sh` must report
  `legacy default`, `legacy tails` and `v1 circuit numerics` byte-identical.
- **Circuit-engine sound changes are intentional and documented.** A `v2` difference
  is allowed only for a documented change. Do not edit the `SOURCE_AUDIO_V2` pin in
  `.github/workflows/dsp-measurements.yml`; the integrator re-pins it once.
- **The three compatibility probes** in `src/tools/*compatibility_probe.cpp` are
  compiled against old checkouts, so the engine API they use must keep compiling.
- **Parameters are append-only.** Add core parameters only below your marker in the
  enum and `kExtensionDefs` in `src/core/Params.h`, and host parameters only below
  your marker in `createParameterLayout()` with your AU version hint. Refer to new
  parameters by name, never by a literal index. Existing IDs, ranges and hints stay.
- **Shared files get additive edits.** CMake tests and plugin sources go below your
  marker in `CMakeLists.txt`. Keep hook-ins to `Engine.cpp`, `PluginProcessor.cpp`
  and the editor small; put new logic in new files.
- **PURE CS-80 strips every non-original feature.** Gate modern-layer behaviour on
  `isPureCs80(policy)` from `src/core/Engine.h`. New features default to off or
  neutral, so a default render is unchanged.
- **Evidence classes.** Every law cites a schematic or manual page, or is labelled a
  declared inference with its alternatives. Never present inference as measurement.
- **Status pages.** Do not edit `docs/STATUS.md` or `docs/HARDWARE_COVERAGE.md`;
  write dated entries in your `wsN-*.md` and the integrator moves them.

## Verification

```bash
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
scripts/check-compat-gates.sh
```

Plugin and UI self-test (needs JUCE in `deps/juce`, Xvfb, xauth and openbox):

```bash
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release --output-on-failure
```
