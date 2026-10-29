# CS-80 Reference

An independent, code-first recreation of the Yamaha CS-80 architecture: eight voices, two independent synthesis layers per voice, sixteen oscillator/filter/VCA chains, polyphonic pressure, ribbon control, ring modulation, sub-oscillator modulation, BBD-style chorus, and a modern performance layer.

The instrument is a C++20/JUCE VST3, AU and standalone synthesizer under active hardware-fidelity reconstruction. The panel is drawn from Yamaha's original instrument photographs and manuals, with colored lever caps, two slider ranks and horizontal tone tabs. It is **not yet a component-complete or measured 1:1 emulation**. Existing finite-output tests and plugin validation do not prove hardware equivalence.

The required end target is [1:1 analog circuit behavior](docs/ANALOG_FIDELITY_REQUIREMENT.md), with stage-level evidence and hardware validation. This target has not yet been achieved.

Start with [original hardware references](docs/HARDWARE_REFERENCES.md) and the [fidelity audit](docs/FIDELITY_AUDIT.md). They separate documented behavior, implemented corrections, provisional calibration and missing circuit models. Hardware tone names/order are now rank-specific; their numeric voicings remain explicitly provisional until the selector resistor networks and reference audio are validated.

The [dedicated screenshot workflow](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/workflows/screenshots.yml) captures the actual editor, tests overlay transitions and publishes exact-source provenance. Successful main-branch captures refresh the images below only when the source commit remains current. See docs/screenshots/provenance.json when present; never infer that a screenshot validates a newer commit.

![CS-80 Reference main panel](docs/screenshots/main-1600x1000.png)

## Status

| Area | Implementation |
| --- | --- |
| Voice architecture | 8 voices, 2 channels/ranks per voice, 16 oscillator-filter-VCA chains |
| Oscillators | PolyBLEP saw, variable-width pulse, fixed-width reset pulse, shaped sine, noise |
| Filters | Serial 12 dB high-pass into 12 dB low-pass, resonance on both, TPT state-variable topology |
| Envelopes | Independent filter and amplifier ADSRs with CS-80-style bent decay/release |
| Expression | Velocity, polyphonic aftertouch, channel pressure, MPE pitch bend/pressure with a separate range, mod wheel, ribbon |
| Modulation | Three global LFOs, sub-oscillator with VCO/VCF/VCA/AT routing, global ring modulator |
| Character | Per-voice tuning/cutoff/level/pan dispersion, slow drift, analog noise floor |
| Effects | BBD-style stereo chorus/tremolo plus three serial modern FX slots |
| Performance | Arpeggiator, sustain II, portamento/glissando, pitch-bend range, A4 tuning |
| Presets | 222 factory programs: 22 provisional hardware-tab studies + 200 original performance patches |
| Parameters | 229 host-automatable parameters: 122 original + 106 programmer/selector controls + AT VCF |
| Formats | VST3 and Standalone everywhere; AU on macOS |
| Verification | Core regression suite + numerical DSP probes + plugin/UI smoke test + pluginval 1.0.4 strictness 10 on Linux VST3, macOS VST3/AU, and Windows VST3; GUI tests on Linux and macOS AU |

Consult the exact-commit CI logs for the current check count and results. The UI smoke test separately validates APVTS exposure, preset/state round-trips, MIDI rendering, editor construction at 1600x1000 and 1200x760, every advanced page, screenshots, and user-preset storage.

## Screens

The main panel at the minimum supported size:

![CS-80 Reference at 1200x760](docs/screenshots/main-1200x760.png)

The modern layer is kept behind its own surface so the primary instrument remains a CS-80 panel rather than a generic plugin with hundreds of controls.

| Advanced page | Image |
| --- | --- |
| Modulation / expression | [advanced-page1.png](docs/screenshots/advanced-page1.png) |
| Sub oscillator / ring / chorus / analog voice | [advanced-page2.png](docs/screenshots/advanced-page2.png) |
| FX / macros / output | [advanced-page3.png](docs/screenshots/advanced-page3.png) |
| Arp / performance / about | [advanced-page4.png](docs/screenshots/advanced-page4.png) |
| Searchable preset library | [browser-1600x1000.png](docs/screenshots/browser-1600x1000.png) |

## Hardware programmer and reconstruction changes

Initial pitch bend now uses a [source-derived nominal 50ms positive-onset response](docs/INITIAL_PITCH_TRANSIENT.md), with state independent of the live PB depth control. The [shared pressure-to-speed bus](docs/SHARED_PRESSURE_SPEED.md) uses the maximum active held-key pressure instead of the most recent event. Both are explicitly bounded circuit-informed approximations, not measured complete touch circuitry.

The missing [aftertouch-to-sub-filter route](docs/AFTERTOUCH_SUB_VCF.md) is implemented with an appended host parameter, preserving existing automation indexes. Its numerical depth remains provisional. [Unconditional velocity attenuation has been removed](docs/VELOCITY_GAIN_CORRECTION.md); saved sessions can sound louder below maximum velocity. The held initial-touch transfer and separate pitch-bend transient still need circuit validation; see [the corrected touch-path evidence](docs/INITIAL_TOUCH_CIRCUIT_EVIDENCE.md).

See the [complete hardware coverage audit](docs/HARDWARE_COVERAGE.md) and [IG00156 evidence and validation plan](docs/IG00156_MODEL_EVIDENCE.md).

MIDI CC120/123 are dispatched before ordinary controllers. Panic/release is channel-scoped, with explicit global-engine panic retained for API callers. Arpeggiator entries retain their originating channel and velocity; MIDI pedal behavior is an explicit software extension. Core boundaries reject nonfinite parameter values and unsupported sample rates before DSP/allocation; the accepted rate range is 8–768 kHz with a 48 kHz fallback for invalid input, not a claim of measured hardware fidelity across that range.


Voice assignment cycles through the eight circuits. A ninth simultaneously held or pedal-held note is rejected rather than stealing an occupied voice; released tails can be reused. MIDI note-off matching is channel-exact. Duplicate same-pitch note-ons on the same MIDI channel are a software-interface ambiguity, not an original physical-keyboard behavior.

Footage uses nominal harmonic ratios 0.5, 1, 1.5, 2, 3 and 4. The fifth stops are about 1.955 cents above equal-tempered fifths. These are nominal circuit targets, not measured tolerances of every original instrument.

The passive matrix model in src/core/HardwareControl.h records 150 independently reviewed tone/control cells and keeps six revision-dependent sampled cells unknown. Matrix-aware conditional calculations respect row-local shunts and require explicit loading for switch buses. It is tested independently and deliberately does not drive audio until receiver loading and IC conversion are established. See docs/PRESET_CIRCUIT_LEDGER.md for source locations and assumptions.


The output decimator now uses a 255-tap Kaiser FIR with 63 host samples of declared latency. This deliberately changes the former 8-sample compensation; projects should let the host refresh plugin delay compensation. CI verifies measured phase delay, pass-band ripple and stop-band rejection. These are software-quality tests, not hardware-match certification.


- Each tone-selector row now has the original **14 positions**: 11 named tone studies, two assigned memory banks, and PANEL. Selecting a source does not overwrite the physical manual sliders.
- **OPEN MEMORY LID** exposes a magnified view of four independent miniature programmer banks. Each bank has 26 controls, including waveform switches and touch response. Memories 1/3 feed rank I; memories 2/4 feed rank II. Footage stays live outside the memory, as on the instrument. Programmer values and selectors persist in plugin/user-preset state. [Automation and legacy migration](docs/MEMORY_AUTOMATION.md) are covered by plugin/UI regressions. All 104 miniature controls and both selectors are host-automatable; original parameter IDs and AU ordering are preserved.
- PANEL returns to the untouched manual controls. Loading a full digital preset selects PANEL on both ranks. Legacy states without a programmer section also use PANEL.
- Physical preset values now use JUCE's actual skewed conversion. The old linear conversion silently changed cutoff, envelope, and rate values during preset loading. Old saved states retain their stored physical values; reloading a named factory preset uses its corrected intended settings.
- Filter contours now start below steady cutoff, attack above it, decay to steady cutoff, and release toward the initial offset even after a long hold. Global sustain extends both amplitude and filter release. Exact analog tapers remain uncalibrated.
- GUI ribbon/wheel commands enter atomic mailboxes consumed by the audio callback. UI voice indicators use atomic snapshots. Oversized host blocks render in preallocated chunks rather than resizing the scratch buffer on the audio thread.

[View circuit derivation and calibration evidence](docs/CIRCUIT_MODEL_SPEC.md). These are incremental, source-backed repairs, not a finished claim of electrical equivalence.

## Build

Requirements:

- CMake 3.22 or newer
- A C++20 compiler
- Ninja (recommended) or Visual Studio 2022
- Linux: ALSA, X11, FreeType, Fontconfig, OpenGL development packages
- macOS: Xcode command-line tools
- JUCE 8.0.6 is fetched by CMake when `deps/juce` is absent

Linux package example (Ubuntu 24.04):

```bash
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
  libasound2-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxinerama-dev libxrender-dev libxcomposite-dev libgl1-mesa-dev \
  libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev \
  libwebkit2gtk-4.1-dev fonts-dejavu-core
```

Configure and build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Or use the checked-in presets:

```bash
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

The plugin artifacts are under:

- `build/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3`
- `build/CS80Reference_artefacts/Release/Standalone/CS-80 Reference`
- `build/CS80Reference_artefacts/Release/AU/CS-80 Reference.component` on macOS

The repository contains no JUCE checkout. A normal configure downloads the pinned JUCE tag. The local `deps/` directory and build trees are ignored by Git.

### Headless assets

```bash
./build/cs80_render --out artifacts/cs80-demo.wav --seconds 12
./build/CS80Snapshots_artefacts/Release/CS80Snapshots \
  --out artifacts/ui --self-test
```

On a bare Linux container, set `JUCE_FONT_PATH` to a directory containing DejaVu Sans so JUCE can resolve its default typeface. The CI workflow does this automatically.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

The suite covers:

- parameter-table integrity, ranges, IDs and normalisation
- oscillator pitch and footage octave ratios
- filter stability under high resonance
- ADSR attack/sustain/release timing
- cyclic eight-voice allocation, held-key overflow rejection and released-voice reuse
- finite, bounded, DC-blocked audio and release tails
- velocity response
- 44.1, 48, 96 and 192 kHz operation
- arpeggiator clock, modes, octave transposition and release
- every modern effect type, including stereo output
- macro layer behaviour
- all 222 factory presets rendering within bounds
- APVTS exposure, preset/state round-trips, MIDI rendering, editor size, all advanced pages and user preset storage
- pluginval 1.0.4 at strictness 10 against the Linux VST3 (headless + GUI under Xvfb), macOS VST3, macOS AU (headless + GUI), and Windows VST3 (headless)

## Using the instrument

### Main panel

- **MASTER**: tuning, detune of channel II, channel mix, brilliance, resonance, filter sustain, output volume and initial pitch bend.
- **CHANNEL I / II**: independent VCO, HPF, LPF, filter contour, VCA contour, pressure/velocity response and pan. Each channel is a complete rank; the two channels are mixed and panned in the stereo field.
- **TONE SELECTOR**: eleven distinct named positions on each channel, in the original hardware order. Numeric voicings are provisional. Selecting a tone reprograms that channel immediately.
- **PERFORMANCE**: glide, key release, wheel-to-sub, wheel-to-VCF, bend range, A4 tuning, arp rate, FX 1 mix and the sustain/arp/chorus/tremolo/stereo switches.
- **RIBBON**: first contact establishes the reference; relative motion bends pitch, and release restores neutral. The documented upward endpoint is approximately one octave.
- **KEYBED**: 61-key on-screen keyboard with velocity. External MIDI provides polyphonic aftertouch; a per-key pressure gesture on the on-screen keyboard remains to be implemented.

Double-click any knob to return it to its default. The preset arrows step through the complete bank; **LIBRARY** opens the searchable browser and **SAVE** opens the same browser with the save field focused.

### Advanced layer

- Three routable LFOs (pitch, VCF, VCA, PWM, pan)
- Sub-oscillator waveform, speed, VCO/VCF/VCA routing and pressure control
- Ring modulator speed, depth, envelope and dry/wet amount
- BBD chorus/tremolo and analog dispersion/drift/noise
- Three serial FX slots: Chorus, Phaser, Tape Delay, Plate Reverb, Drive
- Macros: Brightness, Motion, Width, Space
- MPE with independent wheel and per-note bend ranges, glissando/portamento, sustain II, stereo/mono output
- Arpeggiator with Up, Down, Up/Down, Random and Played modes, 1-4 octaves

MPE mode maps per-note pitch bend and pressure on member channels; the wheel range and the MPE per-note range are separate controls, with the MPE range defaulting to 48 semitones. Polyphonic aftertouch works without MPE and is channel-scoped when MPE is active. CC1 is the modulation wheel, CC11 is expression, CC74 is timbral brightness, and CC64 is the sustain pedal.

## Repository map

```text
src/core/Params.h          parameter table shared by engine, UI, presets and tests
src/core/Dsp.h             oscillator, filters, envelopes, LFO, chorus, phaser, reverb, decimator
src/core/Engine.*          voice allocation, dual-rank engine, modulation, ring, FX, arp
src/core/FactoryPresets.*  22 provisional hardware-tab studies and the 200-patch performance bank
src/plugin/PluginProcessor  APVTS, MIDI, preset/state persistence, host integration
src/plugin/PluginEditor     main panel, keyboard, ribbon, wheels
src/plugin/UiComponents     themed knobs, LEDs, ribbon, wheels, look and feel
src/plugin/AdvancedPanel    modern modulation/FX/arp layer
src/plugin/PresetBrowser    searchable factory/user library
src/tools/render_cli.cpp    offline performance renderer for CI
src/tools/ui_snapshot.cpp   headless screenshots and plugin/UI smoke test
tests/CoreTests.cpp         deterministic DSP and preset tests
docs/ENGINE.md              signal path and implementation notes
docs/RESEARCH.md            source notes and design references
```

`docs/ENGINE.md` explains the signal flow and design decisions in detail. `docs/RESEARCH.md` records the instrument and reference research, including which Arturia V4 features were deliberately scoped in or out.

## CI

`.github/workflows/build.yml` builds and tests on Linux, macOS and Windows. The Linux job also uploads the VST3 and the rendered WAV/screenshots. A separate `pluginval` job downloads that VST3, runs pluginval 1.0.4 at strictness 10 both headless and with GUI tests under Xvfb, and fails the workflow on any validator report. The Linux wrapper permits at most three full attempts for exit 139 only when a test phase started and the last phase completed, with neither SUCCESS nor FAILED in the log. This is retry eligibility, not proof of crash ownership or completion of the whole suite. A passing attempt must exit 0 and print SUCCESS without FAILED; failed tests and log-capture failures are fatal, and earlier attempt logs are preserved. macOS builds VST3, AU and Standalone, registers the built AU with `AudioComponentRegistrar`, and runs pluginval at strictness 10 against both the VST3 and AU, including AU GUI tests. Windows builds VST3 and Standalone, runs the console UI smoke test, uploads the VST3, and a dependent job runs pluginval at strictness 10 against it.

## Honest scope

This is a circuit-informed behavioral model, not a component-level SPICE simulation and not a byte-for-byte copy of Arturia's product. It uses original drawing code and original provisional voicings rather than copying a competing software product. Yamaha-authored documentation is cited as hardware evidence. The oscillator/filter/envelope topologies, dual-rank voice structure, pressure routing and performance controls follow the CS-80 architecture; the modern FX, macros, MPE and arpeggiator are original additions.

AAX is not built because the AAX SDK is not redistributable in this repository. The plugin is tested as a standalone processor and a headless JUCE editor; it should also be exercised in the target DAW before a release. The 200 performance patches are deterministic original variations built from the same circuit model, with 22 dedicated hardware-tone programs on top.

## References

- [Yamaha CS-80 — Wikipedia](https://en.wikipedia.org/wiki/Yamaha_CS-80)
- [Yamaha CS-80 history — Perfect Circuit](https://www.perfectcircuit.com/signal/yamaha-cs-80-history)
- [Yamaha CS-80 Owner's Manual (PDF)](https://synthfool.com/docs/Yamaha/CS_Series/Yamaha%20CS-80%20Owners%20Manual.pdf)
- [Arturia CS-80 V overview](https://www.arturia.com/products/software-instruments/cs-80v/overview)
- [Arturia CS-80 V 4 manual (PDF)](https://dl.arturia.net/products/cs-80v/manual/cs-80-v_Manual_4_0_1_EN.pdf)

This project is not affiliated with or endorsed by Yamaha or Arturia. JUCE is used under its applicable AGPL/commercial license.
