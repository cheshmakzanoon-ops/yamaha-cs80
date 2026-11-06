# CS-80 Reference

An independent software reconstruction of the Yamaha CS-80, written in C++20 with JUCE 8: eight dual-rank voices whose oscillators, filters, envelopes, VCAs, keyboard, touch and effects are modelled from the circuits in Yamaha's public service documentation, behind an editor laid out like the original panel. A modern layer (MSEGs, a modulation matrix, effects, microtuning, MPE, voice modes) sits behind it and switches off completely in PURE CS-80.

**What it is not:** a measured clone. No CS-80 has been measured for this project. Every law is either read from a schematic, manual or adjustment page, or is a declared engineering inference with its alternatives recorded. Nothing here claims measured hardware parity.

**State on 2026-11-06:** `main` includes the eight-workstream [improvement plan](docs/IMPROVEMENT_PLAN.md) and subsequent host, MIDI, modulation, state and editor corrections through [`f921f54`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/f921f548abaaaa31036fe8af007367288ce58566). Current scope and remaining work are in [docs/STATUS.md](docs/STATUS.md).

Since the October 31 integration:

- **VST3 automation:** eighteen key parameters preserve host sample offsets in modern circuit policy. Legacy, PURE CS-80, unsupported parameters and AU retain their existing block-rate behavior. See [host automation](docs/HOST_AUTOMATION.md).
- **MIDI and voices:** fixes cover channel-scoped pressure and MPE controllers, note-release ownership, short notes in the optional KAS scanner, partial CC121 sustain/selector reset, live pitch-bend range changes and shared unison glide onset.
- **Modulation and state:** fixes cover MSEG trigger and sustain boundaries, synced rate modulation across host blocks, modern FX transitions, A/B and undo, preset names, and state validation. The [pinned MTS-ESP client](docs/MTS_ESP_INTEGRATION.md) is now included; a separately installed external master is still needed.
- **Editor:** Studio LFO previews follow the selected waveforms. The screenshots below come from the actual editor, with source and run provenance.

**Historical handover (2026-10-31 PDF):** [CS80_PLAN_COMPLETION_AND_HANDOVER.pdf](docs/CS80_PLAN_COMPLETION_AND_HANDOVER.pdf) records the original plan delivery. It predates the corrections above; use the current status page for implemented features and open work.

## Downloads

### Native development bundles

The latest fully passing build verified for this update is [`ac27687`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/ac27687fe324c7a2b4eec1d53d78cb41f1a9adfb), from November 6 [build run 42165333280](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42165333280). All six build/validation jobs and its [DSP measurements](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42165333241) passed. These bundles include the recent fixes through live pitch-bend range edits, but predate the unison glide correction in `f921f54`. The [newer main build at `f921f54`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42175969425) built all four native platforms, but its [Linux validator job](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42175969425/job/127760198405) failed at the fixed-seed GUI step. It is not substituted for the fully passing bundle set below. Its [DSP measurements](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42175969485) and screenshot run passed; the failed build gate remains open.

| Platform | Architecture | Formats | Download |
| --- | --- | --- | --- |
| Windows | x64 | VST3, Standalone `.exe` | [`cs80-windows-install-bundle`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42165333280/artifacts/13479728008) |
| macOS Apple Silicon | arm64 | VST3, AU, Standalone `.app` | [`cs80-macos-15-install-bundle`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42165333280/artifacts/13479955402) |
| macOS Intel | x86-64 | VST3, AU, Standalone `.app` | [`cs80-macos-15-intel-install-bundle`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42165333280/artifacts/13480587593) |
| Linux (Ubuntu 24.04) | x86-64 | VST3, Standalone | [`cs80-linux-install-bundle`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42165333280/artifacts/13479666356) |

GitHub artifact downloads require a signed-in GitHub account. These four artifacts were present and unexpired when checked; their recorded expiry is **4 February 2027**. If a link has expired, check the [main build history](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/workflows/build.yml?query=branch%3Amain) for a completed successful run and its artifacts.

**These are development builds, not release-qualified installers.** macOS bundles are separate native builds and are ad-hoc signed, not Developer ID signed or notarized. No Windows publisher signature is claimed. Gatekeeper or SmartScreen may warn. Linux targets Ubuntu 24.04 and is not an AppImage or a portable Linux runtime. Passing automated checks is not DAW qualification and does not establish the cause of the intermittent Linux validator failure.

Unpack the outer ZIP, check `SHA256SUMS`, and follow [Install and quickstart](docs/INSTALL_AND_QUICKSTART.md). `manifest.json` and `SOURCE.txt` inside each native bundle identify its source commit; the version string `1.0.0` alone does not.

### Historical installers: October 31 source

The latest installer workflow is still [release run 41218772353](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218772353), built from [`55bb043`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/55bb04314231a8da78554317095cbc6bbf4ff092). **These installers do not include the November corrections above.** All three packaging jobs passed, but the macOS and Windows installers are unsigned and macOS is not notarized. Their artifact expiry is **29 January 2027**.

| Platform | Contents | Historical download |
| --- | --- | --- |
| macOS 11+ (universal arm64 + x86-64) | `.pkg` installer (VST3, AU, Standalone), labelled `-UNSIGNED` | [`cs80-macos-universal-installer`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218772353/artifacts/13049424181) |
| Windows x64 | setup `.exe` (VST3, Standalone), labelled `-UNSIGNED` | [`cs80-windows-installer`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218772353/artifacts/13050675497) |
| Linux x86-64 | `.deb` package and tarball | [`cs80-linux-packages`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218772353/artifacts/13049562316) |

No tagged GitHub Release is published. The native bundles and historical installers above are GitHub Actions artifacts, with separate source revisions and retention dates.

## What is modelled, and how

The engine has three render policies:

- **ENGINE: CIRCUIT** (host parameter "Engine", `source_performance`) renders the circuit models. Fresh instances, INIT and new presets use it.
- **ENGINE: LEGACY (compat)** retains the earlier synthesis voicing and is selected for sessions saved before state schema 3 and the Legacy Library. Compatibility gates cover pinned render cases; the event-handling corrections described below can change affected performances.
- **PURE CS-80** (`parity_mode`) runs the circuit models with every modern addition removed (see below).

| Subsystem | Circuit engine | Reference |
| --- | --- | --- |
| Oscillator | event-driven wave-shaping-comparator saw, comparator pulse, piecewise-linear sine, shared residual basis | [OSCILLATOR_PROMOTION.md](docs/OSCILLATOR_PROMOTION.md) |
| HP / LP filters | IG00156 nodal stage: input networks, 1.2 MOhm / 180 pF branch, 1 uF couplings, in-loop OTA, Newton solve per sample | [IG00156_NODAL_MODEL.md](docs/IG00156_NODAL_MODEL.md) |
| Envelopes | IG00159 single-RC law | [ENVELOPE_RC_MODEL.md](docs/ENVELOPE_RC_MODEL.md) |
| VCA | IG00151: LI node (0.54 ms), exponential SUB input, pedestal, AC coupling, constant-power pan | [IG00151_VCA_MODEL.md](docs/IG00151_VCA_MODEL.md) |
| Voice variation | 5 % R / 10 % C card draws into tune, cutoff, Q, level and EG times; 1/f thermal drift with warm-up; "worn unit" sine option | [TOLERANCE_DRIFT_MODEL.md](docs/TOLERANCE_DRIFT_MODEL.md) |
| Idle voices | all 16 chains free-run between notes (closed-form idle oscillator, decimated stages, snapshot replay) | [ws3 report](docs/workstreams/ws3-keyboard.md) |
| Keyboard and touch | KAS scan and SH7 hold with droop, ninth-key reconsideration, KBC contour network, TWS touch nodes and TRG5 bus | [ws3 report](docs/workstreams/ws3-keyboard.md) |
| Tone tabs | 22 tabs from the tone-board matrix: 7 complete, 15 with inferred cells; 25 cells stay labelled hypotheses | [PRESET_CIRCUIT_LEDGER.md](docs/PRESET_CIRCUIT_LEDGER.md) |
| SUB, ring modulator | IG00150 exponential converter; ADJ-anchored ring converter with declared carrier feedthrough | [ORIGINAL_SUB_OSCILLATOR.md](docs/ORIGINAL_SUB_OSCILLATOR.md) |
| Chorus / tremolo | OE2 BBD with true L/R paths; OE1 four-VCA tremolo | [BBD_CHORUS_MODEL.md](docs/BBD_CHORUS_MODEL.md) |
| Wah / expression | PRA circuit with declared LDR optics; expression through the same cell | [REALTIME_PRA_WAH_MODEL.md](docs/REALTIME_PRA_WAH_MODEL.md) |
| Noise, output | one SUB29 noise source with declared PSD; -6 dB reference, driver ceiling, output taps; GENERAL / PHONES listening outputs | [OUTPUT_REALTIME_PROPOSAL.md](docs/OUTPUT_REALTIME_PROPOSAL.md) |
| Control and rate | 5 ms smoothing on continuous controls; oversampling Eco 2x / Standard 2x / HQ 8x; reported latency 63 host samples plus the oscillator's 8 internal samples (8/N at Nx) | [STATUS.md](docs/STATUS.md) |

The full register, with what Legacy does for each row, is in [STATUS.md](docs/STATUS.md#model-register); row-by-row evidence and dated findings are in the [coverage ledger](docs/HARDWARE_COVERAGE.md).

### Evidence classes

Laws are labelled with one of these classes in the model documents, the code comments, the editor's evidence view (Studio > SETTINGS) and the per-control help:

| Class | Meaning |
| --- | --- |
| Source-derived | read from a Yamaha schematic, service manual or adjustment page |
| Source-derived route, inferred law | the circuit path is from the sources; how strongly it acts is a declared estimate |
| Engineering inference | a declared model where the public sources leave the behaviour open, with alternatives recorded |
| Modern extension | not on the original instrument; inert in PURE CS-80 |

The [fidelity requirement](docs/ANALOG_FIDELITY_REQUIREMENT.md) also names numerically verified results (what the test gates supply against the slow truth models) and hardware-measured results; nothing in this build is hardware-measured.

## The editor

### Original panel

![The editor at 1600x1000 on the circuit engine, playing a chord on the STRING 3 / STRING 2 tone tabs](docs/screenshots/main-source-performance-1600x1000.png)

The first surface is the CS-80 panel: the two rank rows, the tone tabs, the centre and performance deck, the ribbon and the keyboard, re-laid out against the original's control order and legends ([conformance audit](docs/PANEL_CONFORMANCE.md)). It is original vector artwork drawn by the real JUCE editor, not a photograph or a copy. The editor opens between 1120x700 and 1600x1000 logical pixels:

![The same panel at the 1120x700 minimum size](docs/screenshots/main-source-performance-1120x700.png)

The toolbar along the top ([toolbar on the first library program](docs/screenshots/studio-main-1600x1000.png)) carries program PREV / NEXT and the library, A/B compare, undo / redo, ENGINE CIRCUIT / LEGACY, PURE CS-80, STUDIO and a menu (save, INIT, randomise within the original ranges, display size, voice monitor, controllers strip, computer-keyboard play, MIDI learn, the tour and the evidence register). The memory lid opens the four memory panels for editing:

![The memory lid open: the four miniature memory panels, two per channel](docs/screenshots/memory-programmer-1600x1000.png)

### Library

![The program library with filters, patch list and inspector](docs/screenshots/browser-1600x1000.png)

523 programs: 301 curated circuit-engine programs and the 222-program Legacy Library, with search, categories, favourites and user saving. Program Change selects them from MIDI.

### Studio layer

STUDIO opens the modern layer as a second panel behind the first, so the primary surface stays a CS-80. Its tabs are MODULATION, KEYBOARD, FX, ARP and SETTINGS.

| Modulation: MSEG and 16-slot matrix | Keyboard: tuning and voice modes | FX: gain-compensated modern effects |
| --- | --- | --- |
| ![Studio MODULATION: MSEG editor, LFOs and the 16-slot matrix](docs/screenshots/studio-modulation-3-1600x1000.png) | ![Studio KEYBOARD: tuning source, Scala loading and voice modes](docs/screenshots/studio-keyboard-2-1600x1000.png) | ![Studio FX: three serial modern effect slots after the CS-80 chorus](docs/screenshots/studio-fx-2-1600x1000.png) |

The LFO / WHEELS page now previews each selected waveform instead of always drawing a sine:

![Studio LFO previews showing triangle, square and smooth-random selections](docs/screenshots/studio-lfo-selected-waves-1600x1000.png)

The voice monitor and controllers strip open from the toolbar menu:

![The voice monitor drawer and the controllers strip over the panel](docs/screenshots/studio-drawers-1600x1000.png)

The current set of 68 screenshots was rendered by the real editor (`CS80Snapshots`, the JUCE software renderer) from source [`f921f54`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/f921f548abaaaa31036fe8af007367288ce58566) in successful [run 42175969590](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42175969590), then committed in [`f447493`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/f447493f0c0b384600d45ebce598cd37d0ec7d28). [provenance.json](docs/screenshots/provenance.json) records the source and run; all 68 image hashes match [SHA256SUMS](docs/screenshots/SHA256SUMS). The [screenshot directory](docs/screenshots/) holds the complete set. Screenshots show what the editor draws, not how it sounds.

## PURE CS-80

PURE CS-80 (toolbar key, host parameter `parity_mode`) is the circuit engine with every software addition removed:

- masked: the modern FX slots, LFOs 1-3, the macros (held neutral), MSEG, the matrix, the arpeggiator, MPE, microtuning, voice modes, MIDI learn and Program Change;
- ignored: MIDI pitch bend, mod wheel and CC1/11/64/74; the ribbon, polyphonic and channel pressure, the pedals and expression stay, as transports for the original's own inputs;
- zeroed: voice dispersion, drift and background noise, so the eight cards are identical; the original SUB is forced and the controllers strip is hidden.

It is a session policy: program recall keeps it, and switching it off applies the stored ENGINE choice and the player's settings again. Outside PURE CS-80 the modern features are neutral by default, so a fresh instance also starts from the original behaviour. Contract and checks: [PARITY_ISOLATION.md](docs/PARITY_ISOLATION.md).

## Recorded scorecard: October 31 baseline

`cs80_engine_audit --suite all --rates 44100,48000,96000` and `cs80_source_factory_benchmark` (8 held voices, 48 kHz, 256 frames, minimum of 3 rounds) at the audio commit `00ceccf`, circuit engine, on a shared 4-core cloud container:

| Metric | Target | Before the plan | Recorded at `00ceccf` |
| --- | --- | --- | --- |
| Worst saw/pulse non-harmonic content, ring off | <= -70 dB | -33.8 dB | **-88.3 dB** (48 kHz), -88.5 (44.1), -89.5 (96): met |
| Worst tab/program peak at default volume | <= 0 dBFS | +3.9 dBFS | **-0.24 dBFS**: met |
| Worst voice-steal click score | <= 1 | 1.17 | **0.92**: met |
| Worst retrigger click score | <= 1 | — | **1.51** on one legacy-bank program rendered on the circuit engine; 281 of 283 rows <= 1: not met |
| CPU, 8 voices | 0.15-0.20 x realtime | 0.27 | **0.51-0.61** median, p99 0.66-1.00: not met |
| Idle CPU | <= 2 % | 6-7 % | **11 %**: not met |

Ring-modulator rows read -43 dB at every oversampling factor: that is the declared carrier feedthrough, a spectral line, not aliasing. Legacy runs the same row at 0.21-0.27 x realtime.

These are historical measurements of the named audio commit, not a new benchmark of current `main`. The CPU and click targets remain open until new measurements demonstrate otherwise.

`dsp-measurements.yml` checks the pinned Legacy default/tail renders against `d56911a` and circuit-engine compatibility cases against `00ceccf`; `scripts/check-compat-gates.sh --strict-v2` runs the same comparisons locally. This is not a guarantee that every earlier MIDI performance is unchanged. In particular, the optional KAS scanner now preserves same-pitch event arrival order, which can change allocation and playback in affected saved performances ([compatibility note](docs/workstreams/ws3-keyboard.md)).

## Open items

- **CPU:** the oscillator's event solver and the nodal stages' Newton solve dominate. A structure-of-arrays, segment-cached oscillator path is the route to the target; not done.
- **Validator:** the Linux pluginval run has intermittently exited 139 after printing SUCCESS. Code review and a valgrind teardown probe found no fault in project code; the cause is not established ([validator diagnostics](docs/VALIDATOR_DIAGNOSTICS.md)).
- **Clicks:** retrigger 1.51 on "Bass Stack"; note-off click scores up to 1.65 on percussive circuit-engine programs.
- **Filter:** top-octave small-signal accuracy at 2x/4x internal rate; the ADJ p11 HP point.
- **MTS-ESP:** the [pinned 0BSD client](src/vendor/mts-esp/README.md) is included. A separately installed master and platform library are required. Live external-master DAW qualification and per-channel master tables remain open.
- **Automation:** sample-offset handling is limited to eighteen key VST3 parameters in modern circuit policy. AU scheduled ramps, unsupported parameters and DAW-level automation qualification remain open ([scope](docs/HOST_AUTOMATION.md)).
- **Not yet:** memory-panel electrical model driving audio; the 25 tone-tab hypothesis cells; tremolo/chorus both-on and both-off panel states. CC121 currently resets sustain and parameter selectors only, not every controller ([scope](docs/CONTROLLER_RESET.md)).
- **Release:** signing and notarization need the owner's certificates; real-DAW sessions, trademark review and AAX are owner actions ([release qualification](docs/release/RELEASE_QUALIFICATION.md)).

## Build from source

Requirements: a C++20 compiler, Git, CMake 3.22+ (3.25+ for the presets), Python 3.10+ for tests, Ninja or Visual Studio 2022; Xcode command-line tools on macOS. CMake fetches the pinned JUCE 8.0.11 commit when `deps/juce` is absent.

Linux packages, as on the Ubuntu 24.04 CI runners:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git python3 \
  libasound2-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxinerama-dev libxrender-dev libxcomposite-dev libgl1-mesa-dev \
  libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev \
  libwebkit2gtk-4.1-dev fonts-dejavu-core xvfb xauth openbox x11-utils
```

Plugin and tests with the preset for your platform (`linux-release`, `macos-release`, `windows-release`):

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
```

The JUCE-free core, its tests and the offline tools:

```bash
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
./build-core/cs80_engine_audit --out artifacts/audit --suite all --rates 44100,48000,96000
./build-core/cs80_source_factory_benchmark --out artifacts/bench --source-commit "$(git rev-parse HEAD)" --rate 48000 --block 256
scripts/check-compat-gates.sh --strict-v2
```

Screenshots, headless on Linux:

```bash
JUCE_FONT_PATH=/usr/share/fonts/truetype/dejavu bash scripts/run-ui-smoke-linux.sh \
  ./build/CS80Snapshots_artefacts/Release/CS80Snapshots --out artifacts/ui --self-test
```

Plugin outputs are under `build/CS80Reference_artefacts/Release/` (`VST3/`, `Standalone/`, and `AU/` on macOS). [CI distribution status](docs/CI_DISTRIBUTION_STATUS.md) explains the package manifests and checksums.

## Documentation

| Read | For |
| --- | --- |
| [STATUS.md](docs/STATUS.md) | current integration state, model register, historical scorecard, compatibility pins, open issues |
| [HOST_AUTOMATION.md](docs/HOST_AUTOMATION.md), [MTS_ESP_INTEGRATION.md](docs/MTS_ESP_INTEGRATION.md) | current VST3 automation scope and embedded tuning client |
| [CONTROLLER_RESET.md](docs/CONTROLLER_RESET.md), [MSEG_TRANSPORT_RATE.md](docs/MSEG_TRANSPORT_RATE.md) | recent MIDI reset and modulation timing corrections |
| [IMPROVEMENT_PLAN.md](docs/IMPROVEMENT_PLAN.md), [workstreams/HANDOVER.md](docs/workstreams/HANDOVER.md) | the plan and how the eight workstreams were merged |
| [HARDWARE_COVERAGE.md](docs/HARDWARE_COVERAGE.md), [HARDWARE_REFERENCES.md](docs/HARDWARE_REFERENCES.md) | row-by-row behaviour, dated findings, primary sources |
| [ANALOG_FIDELITY_REQUIREMENT.md](docs/ANALOG_FIDELITY_REQUIREMENT.md) | the target and the evidence rules |
| [ENGINE.md](docs/ENGINE.md), [ENGINE_AUDIT.md](docs/ENGINE_AUDIT.md) | signal paths and the audit tool |
| [INSTALL_AND_QUICKSTART.md](docs/INSTALL_AND_QUICKSTART.md), [release/RELEASE_QUALIFICATION.md](docs/release/RELEASE_QUALIFICATION.md) | installing builds and what a release still needs |
| [VALIDATOR_DIAGNOSTICS.md](docs/VALIDATOR_DIAGNOSTICS.md) | pluginval history and the exit 139 investigation |

`src/core/` holds the engine, `src/plugin/` the JUCE plugin and editor, `src/reference/` the slow truth models the tests compare against, `src/tools/` the render, audit, benchmark and snapshot tools, `tests/` the checks, `scripts/` the CI and analysis helpers.

## Contributing

- Host parameters are append-only, in workstream blocks; missing state restores to documented neutral defaults.
- Keep the evidence classes honest: never relabel an inference, a recording or a passing test as a measurement.
- Legacy audio must stay byte-identical. Record every intentional circuit-engine sound change and the commit that reproduces earlier renders.
- Do not weaken or skip a gate to get a green run.

## Licence

CS-80 Reference is licensed under the [GNU Affero General Public License v3](LICENSE). JUCE is used under its AGPLv3 option; every package carries JUCE's licence as `licenses/JUCE-LICENSE.md`. The editor artwork, code and programs are independent work; no Yamaha or Arturia artwork, code or preset banks are copied. Yamaha manuals and schematics are cited as hardware evidence. This project is not affiliated with or endorsed by Yamaha; "CS-80" is used to identify the instrument being modelled.
