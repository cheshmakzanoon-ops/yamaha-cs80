# CS-80 Reference

An independent software synthesizer inspired by the Yamaha CS-80, combining a
circuit-modeled dual-layer analog engine with a built-in 4-slot cinematic FX
rack and 1,023 factory presets (including 500 dedicated Film Scoring patches).
Available as VST3, AU (macOS), and Standalone for Windows, macOS, and Linux.

[Downloads](#downloads) · [Highlights](#highlights) · [Quick Start](#quick-start) · [Build from Source](#build-from-source) · [Docs](docs/INSTALL_AND_QUICKSTART.md)

![CS-80 Reference main panel](tests/golden/main-1600x1000.png)

## Downloads

Latest verified release: **[v2026.11.9](https://github.com/cheshmakzanoon-ops/yamaha-cs80/releases/tag/v2026.11.9)** — built and validated across all four platforms (`pluginval` strictness 10).

| Platform | Formats | Download |
| --- | --- | --- |
| Windows x64 | VST3, Standalone | [cs80-reference-windows-x86_64-v2026.11.9.zip](https://github.com/cheshmakzanoon-ops/yamaha-cs80/releases/download/v2026.11.9/cs80-reference-windows-x86_64-v2026.11.9.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | [cs80-reference-macos-arm64-v2026.11.9.tar.gz](https://github.com/cheshmakzanoon-ops/yamaha-cs80/releases/download/v2026.11.9/cs80-reference-macos-arm64-v2026.11.9.tar.gz) |
| macOS Intel (x86-64) | VST3, AU, Standalone | [cs80-reference-macos-x86_64-v2026.11.9.tar.gz](https://github.com/cheshmakzanoon-ops/yamaha-cs80/releases/download/v2026.11.9/cs80-reference-macos-x86_64-v2026.11.9.tar.gz) |
| Linux x86-64 (Ubuntu 24.04) | VST3, Standalone | [cs80-reference-linux-x86_64-v2026.11.9.tar.gz](https://github.com/cheshmakzanoon-ops/yamaha-cs80/releases/download/v2026.11.9/cs80-reference-linux-x86_64-v2026.11.9.tar.gz) |

Verify downloads against [SHA256SUMS](https://github.com/cheshmakzanoon-ops/yamaha-cs80/releases/download/v2026.11.9/SHA256SUMS). See the [installation guide](docs/INSTALL_AND_QUICKSTART.md) for platform plugin paths and macOS Gatekeeper notes.

## Highlights

- **Dual-Layer CS-80 Circuit Engine** — 8 to 16 voices with two independent ranks per note (VCO, 12 dB/oct resonant HPF & LPF, VCA), polyphonic aftertouch, pitch ribbon, ring modulator, sub-oscillator, hardware chorus/tremolo, voice dispersion, thermal drift, and physical acoustic instrument bodies.
- **4-Slot Cinematic & Studio FX Rack** — Four serial post-chorus slots featuring **Lush Hall Reverb**, **Shimmer Reverb**, **Tempo-Synced Stereo/Ping-Pong Delay**, **Dimension Stereo Chorus**, **Analog Flanger**, **Studio EQ & Bass-Safe Stereo Widener**, and **Bus Compressor / Glue**, alongside classic/modern effects, **Master FX Wet**, and a one-click **`FX`** bypass on the main toolbar.
- **1,023 Factory Presets** — Includes **500 Film Scoring presets** spanning 9 scoring roles (*Brass & Braams*, *Pads & Atmospheres*, *Strings & Choirs*, *Pulses & Ostinatos*, *Drones & Subscapes*, *Leads & Themes*, *Keys, Plucks & Bells*, *Impacts & Risers*, and *Sci-Fi & Noir*), plus **521 overhauled musical presets** with organic analog motion and tailored studio FX, and 2 clean Init patches.
- **Studio Expansion** — Open **STUDIO** for the modulation matrix, drawable MSEGs, MPE, Scala/KBM/MTS-ESP microtuning, arpeggiator, and full 4-slot FX controls.

## Quick Start

1. **Install** — Unpack the archive for your platform and copy the VST3/AU bundle into your DAW plugin folder (or launch the Standalone app directly).
2. **Browse Presets** — Click **LIBRARY** to explore all 1,023 presets, or select the **FILM SCORING** category and its 9 role sub-tags for instant cinematic sounds.
3. **Shape FX & Expression** — Toggle **`FX`** on the top toolbar to A/B the built-in effects chain, open **STUDIO → FX → MODERN FX** to tweak all 4 slots and Master Wet, and use polyphonic aftertouch or the pitch ribbon for expressive swells.

## Build from Source

Requires CMake 3.22+, a C++20 compiler, and Git (JUCE is fetched automatically).

```bash
# Linux (Ubuntu 24.04)
cmake --preset linux-release
cmake --build --preset linux-release

# macOS (Apple Silicon / Intel)
cmake --preset macos-release
cmake --build --preset macos-release

# Windows (Visual Studio / Ninja)
cmake --preset windows-release
cmake --build --preset windows-release
```

Run the test suite with `ctest --test-dir build --output-on-failure`.

## Documentation

- [Installation & Quickstart](docs/INSTALL_AND_QUICKSTART.md)
- [Preset Guide](docs/PRESET_GUIDE.md)
- [Technical & Circuit Reference](docs/)
