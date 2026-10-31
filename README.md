# CS-80 Reference

An independent, playable software reconstruction of the Yamaha CS-80, written in C++20 with JUCE 8. It combines eight dual-rank voices, expressive MIDI, a hardware-inspired editor and a modern performance layer. The JUCE-free core can also render and test audio offline.

**Development stage:** the redesigned editor and source-informed sound models are integrated into the working plugin. This is a circuit-informed behavioral reconstruction with documented engineering assumptions—not a completed component-level simulation, a measured 1:1 hardware match or a release-qualified commercial instrument.

## Development builds and current source

**Current engineering snapshot: October 30, 2026 (Atlantic time; October 31 UTC).** The current UI and engine are integrated, and all 42 JUCE-free local tests plus the package-integrity self-tests passed. Native rebuilds for the latest fix commit are running; no full-platform green result is claimed.

The table distinguishes **available binaries** from **a build still underway**. Its two existing redesigned-UI downloads come from `dc05f7c`; the subsequent `a6c919b` changes are restricted to CI/testing/documentation and leave shipping plugin/engine/editor source unchanged. Their plugin behavior is therefore the same, but package manifests must still identify the actual build commit. The project currently declares version `1.0.0`, so the version number alone does not identify a build; use `manifest.json` and `SOURCE.txt`.

**Latest source under native verification:** [`a6c919be93c16c7ffd195e7d8e70aec2ce5a81f0`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/a6c919be93c16c7ffd195e7d8e70aec2ce5a81f0)

**Build and validation run:** [Build run 41148634708](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41148634708) — running as of October 30, 2026 (Atlantic time)

| Platform | Architecture | Included formats | Current package |
| --- | --- | --- | --- |
| Windows | x64 | VST3, Standalone `.exe` | **Building for `a6c919b`**; no updated Windows bundle confirmed. [Follow native build](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41148634708) |
| macOS Apple Silicon | arm64 | VST3, AU, Standalone `.app` | [Download latest available redesigned-UI bundle (`dc05f7c`)](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41146838774/artifacts/13021930240); refreshed `a6c919b` bundle building |
| macOS Intel | x86-64 | VST3, AU, Standalone `.app` | **Building for `a6c919b`**; no updated Intel bundle confirmed. [Follow native build](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41148634708) |
| Linux | x86-64 | VST3, Standalone | [Download latest available redesigned-UI bundle (`dc05f7c`)](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41146838774/artifacts/13021393791); refreshed `a6c919b` bundle building |

GitHub artifact downloads can require sign-in and expire under repository retention settings. The macOS packages are separate native builds, **not universal binaries**; match the architecture of the DAW process. Linux packages are built on Ubuntu 24.04 and are not a self-contained AppImage. Older operating systems and emulation layers are not qualified by these builds.

Download the outer artifact ZIP, unpack it, verify `SHA256SUMS`, then unpack the inner platform archive without breaking its bundle directories or executable permissions. Follow [Install and quickstart](docs/INSTALL_AND_QUICKSTART.md) for installation locations, audio/MIDI routing, checksums and first play.

**These are development artifacts, not signed/notarized public releases.** Package availability does not imply that every required validator passed. Check the exact-commit results below and the [distribution and licensing limitations](docs/CI_DISTRIBUTION_STATUS.md). Keep a backup of an older plugin outside the DAW's scan directories before replacing it.

## What is implemented now

![The actual CS-80 Reference editor with the redesigned panel and populated performance controls](docs/screenshots/main-populated-1600x1000.png)

The image is rendered by the real JUCE editor, not a mockup. [Screenshot provenance](docs/screenshots/provenance.json) identifies its exact source commit and workflow run; [checksums](docs/screenshots/SHA256SUMS) identify the image files. Screenshots prove what that editor rendered, not analog fidelity or the state of a later binary.

### Instrument and host features

- **Eight voices, two ranks per voice:** 16 oscillator/filter/VCA chains; cyclic allocation, with a ninth simultaneously held key ignored and released tails available for reuse.
- **Original-instrument control layout:** independent oscillator, high-pass/low-pass filter, envelope and touch rows; ring modulation, SUB oscillator, footage, ribbon, sustain, portamento/glissando, keyboard contours and pedal/footswitch controls.
- **22 hardware tone selections plus PANEL and four memory panels:** each rank has 11 named tones, two memories and PANEL. The memory panels contain 104 independently automatable controls.
- **222 separate library programs:** 22 legacy hardware-tone studies and 200 original performance patches, with search, categories, favourites, user saving and overwrite confirmation. These library programs are not the source-model hardware tabs.
- **Modern performance layer:** three LFOs, four macros, three serial FX slots, an arpeggiator and optional MPE. These are software extensions, not claims about the original hardware.
- **245 append-only host parameters:** state migration, automation, safe preset saving, MIDI/voice activity, a notes-only PANIC control and final-output sample-peak warnings.
- **Native formats:** VST3 and Standalone for Windows/Linux/macOS; AU on macOS. No AAX or mobile build is supplied.

### Recent integrated changes

The current source includes the 2.5D/vector editor redesign, a source-policy OE2 BBD chorus, corrected phaser behavior, sample-rate-independent reverb timing, independent state for repeated FX-slot types, and continuity improvements when source-mode release tails are reused. The engine audit tool also uses portable C++20 mathematical constants for Windows builds.

Those are real changes to the plugin, not just new screenshots. Their behavior and limitations are recorded in [Engine sound-quality audit](docs/ENGINE_AUDIT.md), [BBD chorus model](docs/BBD_CHORUS_MODEL.md), [Active-tail continuity](docs/ACTIVE_TAIL_CONTINUITY.md) and [Engine architecture](docs/ENGINE.md). Historical byte-identical claims apply only to the commits and scenarios named in their evidence; they do not promise that every later source-model revision sounds identical.

### Implementation status versus fidelity

| Area | Current production state | Important boundary |
| --- | --- | --- |
| Voices and control routing | Executable eight-voice, dual-rank engine; tone/memory selection, MIDI and controller paths | Actual hardware scanner timing, sample/hold droop and key mechanics are not reconstructed |
| Source-derived hardware tones | All 22 tabs have audible source-model routes: 7 source-complete columns and 15 explicitly inferred columns | “Complete” describes the source matrix, not calibrated IC behavior; 25 unresolved cells use a separate hypothesis overlay |
| Oscillators and filters | PolyBLEP saw/pulse, shaped sine/noise and two serial 12 dB TPT high-pass/low-pass stages | Wave-shaping, nonlinear filter behavior and intermediate control laws remain approximate; the full circuit-reference models are not selected |
| Envelopes, touch and pedal/output networks | Source modes use precise/live envelope targets, active-tail continuity, circuit-derived PRA wah and loaded HIGH/LOW output models | Device/time/optical/loading laws include explicit engineering assumptions; no measured hardware residuals are claimed |
| Chorus and tremolo | Source-mode chorus uses the OE2 BBD delay/filter path; Legacy and tremolo retain their generic paths | Clock excursion, wet level and polarity are inferred; charge-transfer loss, clock feedthrough and complete OE1 behavior remain open |
| Host and editor | Working plugin state/automation, presets, resizable editor, performance controls and validation harnesses | Exact-commit CI and real DAW sessions are separate qualification steps; not universal host compatibility |
| Continuous idle circuits and reconstructed oscillator | Experimental kernels/reference models and dedicated tests exist | They are **not activated in the plugin**; inactive production oscillator/filter state still stops evolving |
| Measured hardware equivalence | Not established | Software tests and a familiar-looking panel are not measurements of a physical CS-80 |

The detailed [hardware coverage ledger](docs/HARDWARE_COVERAGE.md), [fidelity audit](docs/FIDELITY_AUDIT.md) and [roadmap](docs/PARITY_ROADMAP.md) retain dated evidence, source conflicts and unresolved model choices. Historical row/check counts are not a completion or fidelity percentage.

## Play the instrument

1. Install the appropriate package or open Standalone, then choose an audio device and MIDI input. Start with low monitoring volume.
2. For the current source-informed sound with ordinary expressive MIDI, open **Advanced → About**, turn **SOURCE PERFORMANCE on** and leave **PARITY ISOLATION off**.
3. Choose the hardware tone tabs for each rank. Their source/model status is shown in the toolbar. Loading a library program instead returns to Legacy/PANEL unless a saved user preset explicitly selects Source Performance.
4. Use velocity, aftertouch, the ribbon and pedal controls. On the on-screen keybed, drag down a held key to apply pressure. For MPE, configure both the host and plugin and match their bend ranges.

### The three render modes

| Mode | Switches | Sound models | MIDI and software layer |
| --- | --- | --- | --- |
| **Legacy Performance** | Both off | Existing legacy panel/preset behavior | Ordinary MIDI, optional MPE and modern modulation/FX |
| **Source Performance** | SOURCE PERFORMANCE on; PARITY ISOLATION off | Source-tone networks, live envelopes, PRA/output models and BBD chorus | Ordinary MIDI, optional MPE and modern modulation/FX remain available |
| **Comparison Isolation** | PARITY ISOLATION on; takes priority over either source setting | Source models with the comparison contract | Software overlays and expressive mappings are masked as specified by the isolation harness |

Both switches default off. Changing the **effective mode stops existing notes and resets controller/effect history**. Source Performance is part of a sound preset; isolation is a separate session policy. Selecting a hardware source does not overwrite the manual PANEL controls. See the [Source Performance contract](docs/SOURCE_PERFORMANCE.md) and [Isolation contract](docs/PARITY_ISOLATION.md).

**Leave output headroom.** Source modes intentionally omit the legacy output soft conditioner and can exceed digital full scale; they are not tone-normalized or safety-limited. The output display measures **sample peak**, not true peak or loudness, and never modifies audio. PANIC stops existing notes and sustain/arpeggiator holds at the next audio block, but retained effects and noise may remain. See [Output meter](docs/OUTPUT_METER.md) and [Panic behavior](docs/PANIC_CONTROL.md).

MPE currently uses channel 1 as master and channels 2–16 as members, with manually matched bend ranges. There is no automatic RPN zone/range negotiation or separate upper-zone implementation. Host MIDI aftertouch remains available when on-screen pressure is conservatively suppressed during arpeggiation or ambiguous host-note overlap.

## The redesigned editor

The editor is original vector artwork drawn with JUCE paint calls: walnut cheeks, a dark textured chassis, cream/gold legends, long color-coded faders, backlit tone tabs and a 61-key keyboard. It is hardware-inspired, not a pixel-for-pixel copy or a photographic rendering.

| Area | Controls and feedback |
| --- | --- |
| Toolbar | Preset source/slot/edited status, library/save/init, Advanced, eight voice lamps, MIDI activity, output meter and PANIC |
| Rank I and II | Oscillator/filter/amplifier/touch controls, waveform switches and a live signal-flow/contour display |
| Center and performance deck | Ring, SUB, footage, tone tabs, touch/global controls, sustain/porta/glide, chorus/tremolo, ribbon, pedal and output switches |
| Overlays | Searchable preset library, four-page Advanced panel and four miniature memory-programmer panels |
| Sizing | Display-aware opening from 1120×700 to 1600×1000 logical pixels; a scaled 1600×1000 design space |

The smallest legends are tight at the minimum size. Displays with less than 1120×700 usable logical pixels cannot fit the current layout. UI automation checks do not replace interaction testing in a real DAW.

| Additional view | Verified capture |
| --- | --- |
| Minimum-size panel | [1120×700](docs/screenshots/main-1120x700.png) |
| Preset browser | [Library](docs/screenshots/browser-1600x1000.png) |
| Memory programmer | [Selected memory](docs/screenshots/memory-programmer-selected-1600x1000.png) |
| Advanced controls | [Populated modulation page](docs/screenshots/advanced-populated-page1.png) |
| Source Performance | [Panel](docs/screenshots/main-source-performance-1600x1000.png), [About page](docs/screenshots/advanced-source-performance-about.png) |

The [screenshot directory](docs/screenshots/) contains the complete capture set. The [UI design specification](docs/UI_DESIGN_SPEC.md) records the layout and design intent.

## Verification and known limits

The rows below must be read against their exact source commit. A passing core test, screenshot capture, package upload or validator invocation is evidence for its own scope—not proof of hardware equivalence or a qualified public release.

| Gate | Latest evidence | What it covers |
| --- | --- | --- |
| Native builds, tests and packaging | [`a6c919b` native run](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41148634708) was in progress when documented. Earlier `dc05f7c` [Linux and Apple Silicon passed; Windows UI smoke failed and Intel remained running](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41146838774). Windows smoke diagnostics and stack budget were corrected in `a6c919b`, pending native confirmation. | Linux, Windows and separate native Intel/Apple Silicon macOS products |
| Host validators | Current [`a6c919b` pluginval results](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41148634708) pending native dependencies. Earlier `dc05f7c` Linux pluginval passed; an earlier Windows validation was skipped after smoke failure. This is not an all-platform pass claim. | pluginval 1.0.4 at strictness 10, with platform-specific VST3/AU and GUI/headless checks |
| DSP and compatibility | [`a6c919b` DSP run](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41148634692): legacy default-off byte comparison, V1 source-circuit + V2 whole-audio compatibility, numerical measurements and synthetic comparison **passed**; sanitizer job still running when documented. The old V1 whole-audio failure is preserved as historical evidence, with [causal proof and split contracts](docs/SOURCE_COMPATIBILITY_CHECKPOINT.md). | Numerical tests, pinned-baseline audio comparisons and sanitizer jobs; inspect every job conclusion |
| Editor captures | [Source/run manifest](docs/screenshots/provenance.json) | Real editor renders plus plugin/UI self-tests before screenshot publication |

The test suite covers the core engine, parameter/state migration, voice allocation, envelopes, loaded control networks, source-tone conversion, expression, output metering, keyboard-event handoff, presets and UI interactions. CTest totals differ by platform because some tests are platform-specific. Detailed historical assertion counts belong to their source-pinned evidence, not an unqualified “current tests passed” counter.

Remaining limits include:

- **Fidelity:** oscillator, nonlinear-filter, envelope, touch, variation and effect laws still contain provisional assumptions. There is no admitted calibrated hardware-capture corpus.
- **Audio quality:** the engine audit records high-note nonlinear-filter aliasing, source-tone headroom above full scale, substantial default drift and unresolved variation laws. These are known engineering/model issues, not measured deviations from hardware.
- **Idle continuity:** active source-mode tails retain selected audio state, but the separate continuous-idle kernel and reconstructed oscillator remain unqualified for production activation.
- **Realtime performance:** source-pinned cloud benchmarks include deadline exceedances, especially at high rates. They are not a glitch-free DAW guarantee; no whole-plugin hard-realtime or zero-CPU claim is made. The plugin reports 63 samples of latency and an unbounded tail to accommodate supported continuous noise.
- **Validation history:** earlier Linux pluginval runs exited 139 after reporting SUCCESS. Later passing runs do not establish crash ownership or prove that intermittent failure repaired. [Validator diagnostics](docs/VALIDATOR_DIAGNOSTICS.md) preserves the failed evidence.
- **Distribution:** no universal macOS binary, signed installer, Apple notarization, automatic updater or comprehensive real-DAW installation/session qualification is supplied. Project/dependency licensing also needs explicit resolution before a public binary release.

[Playable release readiness](docs/PLAYABLE_RELEASE_READINESS.md) preserves older checkpoints and their limits. [Engine audit](docs/ENGINE_AUDIT.md) reports software measurements and reproducible tools; it does not calibrate the plugin against an original instrument.

## Build and test from source

Requirements: a C++20 compiler, Git, CMake **3.22+ for explicit commands** or **3.25+ for the checked-in preset schema**, Python 3.10+ for tests, and Ninja or Visual Studio 2022. macOS needs the Xcode command-line tools. CMake fetches the pinned official JUCE 8.0.11 commit when `deps/juce` is absent.

### Linux dependencies

The following matches the Ubuntu 24.04 CI environment, including the private virtual desktop used by GUI tests:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git python3 \
  libasound2-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxinerama-dev libxrender-dev libxcomposite-dev libgl1-mesa-dev \
  libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev \
  libwebkit2gtk-4.1-dev fonts-dejavu-core xvfb xauth openbox x11-utils
```

### Native presets

| Platform | Preset |
| --- | --- |
| Linux | `linux-release` |
| macOS | `macos-release` |
| Windows x64 / Visual Studio 2022 | `windows-release` |

For example, on Linux:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
```

Use the corresponding preset name on macOS or Windows. Presets enable both plugins and tests and select Release for the build/test steps. To use explicit CMake commands instead:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=ON -DCS80_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Windows use `-G "Visual Studio 17 2022" -A x64` at configure time, `--config Release` for the build and `-C Release` for CTest. Linux CTest invokes `scripts/run-ui-smoke-linux.sh` for the plugin/UI smoke test; do not bypass its Xvfb/window-manager setup in a headless environment.

### JUCE-free engine work

```bash
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --config Release --parallel
ctest --test-dir build-core -C Release --output-on-failure
```

This builds the core, numerical/reference tests and offline tools without the plugin/editor dependency. It does not qualify host wrappers or GUI behavior.

### Render audio and editor test assets

```bash
./build/cs80_render --out artifacts/cs80-demo.wav --seconds 12
JUCE_FONT_PATH=/usr/share/fonts/truetype/dejavu \
  bash scripts/run-ui-smoke-linux.sh \
  ./build/CS80Snapshots_artefacts/Release/CS80Snapshots \
  --out artifacts/ui --self-test
```

The snapshot command above is the Linux headless route. Native plugin outputs are under `build/CS80Reference_artefacts/Release/`: `VST3/`, `Standalone/`, and `AU/` on macOS. [Packaging documentation](docs/CI_DISTRIBUTION_STATUS.md) explains manifests, checksums and the source-verification prerequisite; packaging existing binaries does not itself run host qualification.

## Current direction

The project has moved beyond a panel mockup or offline-only preset study: the editor is playable, all 22 source-tone routes are audible, and the source-policy envelope, wah, BBD-chorus and output paths are integrated. The remaining work is to improve and qualify the instrument, not to describe those completed integrations as future features.

The [parity roadmap](docs/PARITY_ROADMAP.md) remains the engineering plan. Current priorities are to:

1. Keep exact-commit platform, host, state and compatibility gates green as audio models evolve; document intentional sound changes separately from regressions.
2. Improve the production oscillator and nonlinear VCF behavior, including high-note aliasing and the provisional shaping/control laws.
3. Qualify continuous idle-circuit behavior for the full engine, including state ownership, numerical convergence, compatibility and callback cost, before activating the experimental kernel.
4. Refine source-envelope/touch laws, unresolved source-tone cells, OE1/BBD behavior and voice variation with stronger public evidence and explicit uncertainty.
5. Complete real-DAW sessions, distribution/licensing decisions and any signing/notarization needed for a public release.

**Hardware access is not a development gate.** The standing goal is the closest reconstruction achievable from public knowledge and sound engineering. Use primary documentation and circuit derivation where available; fill remaining known behavior with explicitly justified, revisable models. Hardware measurements, if obtained, can refine those models and support measured-equivalence claims. Never relabel an assumption, a public recording or a successful software test as calibrated hardware evidence.

## Documentation map

| Start here | Purpose |
| --- | --- |
| [Install and quickstart](docs/INSTALL_AND_QUICKSTART.md), [distribution status](docs/CI_DISTRIBUTION_STATUS.md) | Choose the right architecture, verify packages, install and understand development-build limitations |
| [Source Performance](docs/SOURCE_PERFORMANCE.md), [comparison isolation](docs/PARITY_ISOLATION.md) | Mode selection, preset/controller semantics and compatibility boundaries |
| [UI specification](docs/UI_DESIGN_SPEC.md), [engine architecture](docs/ENGINE.md) | Current editor and audio/control signal paths |
| [Engine audit](docs/ENGINE_AUDIT.md), [DSP measurements](docs/DSP_MEASUREMENTS.md) | Reproducible software measurements, fixes and remaining engineering issues |
| [Fidelity requirement](docs/ANALOG_FIDELITY_REQUIREMENT.md), [roadmap](docs/PARITY_ROADMAP.md) | Target, milestones, assumptions and acceptance criteria |
| [Coverage ledger](docs/HARDWARE_COVERAGE.md), [fidelity audit](docs/FIDELITY_AUDIT.md), [hardware references](docs/HARDWARE_REFERENCES.md) | Row-by-row behavior, dated findings and primary sources |
| [Source-tone runtime](docs/SOURCE_FACTORY_RUNTIME.md), [tone integration](docs/FACTORY_TONE_INTEGRATION_PROPOSAL.md) | Seven complete/fifteen inferred columns, network conversion, source ownership and headroom |
| [Source compatibility checkpoint](docs/SOURCE_COMPATIBILITY_CHECKPOINT.md) | Why the V1 whole-audio assertion changed, causal proof, and separate immutable circuit/audio gates |
| [Live envelope levels](docs/LIVE_ENVELOPE_LEVEL_MODEL.md), [active-tail continuity](docs/ACTIVE_TAIL_CONTINUITY.md) | Current source-policy envelope and tail behavior |
| [Realtime PRA](docs/REALTIME_PRA_WAH_MODEL.md), [loaded output](docs/OUTPUT_REALTIME_PROPOSAL.md), [BBD chorus](docs/BBD_CHORUS_MODEL.md) | Integrated source-model paths and their inferred boundaries |
| [Voice continuity experiment](docs/VOICE_CIRCUIT_CONTINUITY.md), [experimental oscillator](docs/OSCILLATOR_EXPERIMENTAL_CHECKPOINT.md) | Tested but non-production model work and failed/unqualified integration gates |
| [Release readiness](docs/PLAYABLE_RELEASE_READINESS.md), [validator diagnostics](docs/VALIDATOR_DIAGNOSTICS.md) | Historical checkpoints, failures and the difference between tests and release qualification |

`src/core/` contains the engine and shared parameters, `src/plugin/` the JUCE host/editor layer, `src/reference/` the conditional reference models, `src/tools/` the render/measurement/snapshot tools, `tests/` the executable checks and `scripts/` the CI/analysis/packaging helpers.

The branch-consolidation history is retained in Git. The old screenshot-only branch did not replace newer verified captures. The separate one-off Windows diagnostic is archived at [docs/archive/diag-win-legacy.yml](docs/archive/diag-win-legacy.yml), not enabled as an active workflow.

## Contributing and attribution

- Keep host parameter IDs/order append-only and restore missing new state to documented neutral defaults. Do not silently change old preset semantics.
- Separate document-derived facts, circuit results, engineering assumptions and actual measurements. Retain conflicting sources and record provenance.
- Test changes at module, engine, state and host levels. Preserve failed evidence and mutation checks; do not weaken a gate simply to obtain a green badge.
- Record intentional audible changes, their scope and the historical commit needed to reproduce earlier renders.

The editor, code and performance patches are independent work; no Yamaha or Arturia artwork/code/preset banks are copied. Yamaha-authored manuals and schematics are cited as hardware evidence. This project is not affiliated with or endorsed by Yamaha or Arturia.

JUCE has its own AGPL/commercial licensing terms. The repository has not yet declared a project `LICENSE`/`COPYING`/`NOTICE` file; including JUCE's license and a source link in a development archive does not decide the project's license or complete a distribution audit. See [CI distribution status](docs/CI_DISTRIBUTION_STATUS.md) before treating these artifacts as a public binary release.
