# CS-80 Reference

An independent software instrument based on the Yamaha CS-80. Eight voices play
two synthesizer layers per note, each with its own oscillator, high-pass and
low-pass filters, and amplifier. The ribbon, polyphonic aftertouch, ring
modulator, chorus and tremolo bring the original's performance controls into
a DAW or a standalone app.

[Downloads](#downloads) · [First sound](#play-your-first-sound) · [Preset guide](docs/PRESET_GUIDE.md) ·
[How it was modeled](#from-schematic-to-sound) · [CPU evidence](#how-we-reduced-cpu-work) ·
[Build from source](#build-from-source)

![CS-80 Reference editor showing the two synthesis layers, tone tabs, ribbon and keyboard](tests/golden/main-1600x1000.png)

*Rendered from the actual circuit-only editor. Versioned layout captures cover [1600 × 1000](tests/golden/main-1600x1000.png) and [1120 × 700](tests/golden/main-1120x700.png).*

## Downloads

Revision **2026.11.4** is committed and undergoing final cross-platform
qualification. The links below remain the preceding verified release until
the new native packages and all validators complete.
[Qualification checkpoint](docs/preset-evidence/2026.11.4/ci/source-078c098/qualification-status.json)

**2026.11.3 — all 521 existing musical presets revoiced.** The two initialization
patches and all 523 names, categories and program addresses are preserved.
No presets were added. These builds include the new instrument bodies and the
individual articulation, expression, decay and level refinements.

All four native build/test/package jobs and all **13 required strictness-10
pluginval lanes passed**, with **1,303 native CTest executions passed,
zero failed and 7 formally skipped** across 1,310 registered platform
executions. The 24 nested Python subtest skips are counted separately.
The validators completed with zero runtime validation retries. Actual installation and
audition archives were downloaded and checked against their API identities,
sizes, hashes and producing jobs.

| Platform | Included formats | Verified installation download |
| --- | --- | --- |
| Windows x64 | VST3, Standalone | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14822019089.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14820874764.zip) |
| macOS Intel (x86-64) | VST3, AU, Standalone | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14822214497.zip) |
| Linux x86-64 (Ubuntu 24.04 build) | VST3, Standalone | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14837191097.zip) |

Binary source:
[`cbbee6a940985ad3c550768414960d2e1b1e802e`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/cbbee6a940985ad3c550768414960d2e1b1e802e) ·
[Build 45176799427, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45176799427) ·
[Qualification, download hashes and exact artifact identities](docs/preset-evidence/2026.11.3/ci/source-cbbee6a/release-qualification.json)

**Hear the revised sounds:** [90 unnormalized WAV excerpts](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14836886459.zip)
cover **71 existing programs and 594 seconds (9 minutes 54 seconds)** of 48 kHz
stereo audio. They include all 24 later-bank keyboard programs, soft-strike and
register comparisons, pressure examples, original addresses and all 19 active
instrument-body models. The archive also contains all **523 preset vectors with
198 parameters each**, metrics, provenance and checksums.
[Actual audition verification](docs/preset-evidence/2026.11.3/ci/source-cbbee6a/audition/acceptance-summary.json)

Unpack the outer ZIP, verify its included `SHA256SUMS`, and unpack the inner
archive while keeping each plug-in/app bundle intact. The qualification record
gives the outer ZIP hashes. Follow the
[installation guide](docs/INSTALL_AND_QUICKSTART.md#install-only-the-formats-you-need).
Choose the Mac build for your **host process architecture**. These are CI
installation bundles without publisher release signing or Apple notarization;
Linux requires compatible system libraries. The earliest recorded API expiry
is **21 February 2027 at 08:42:09 AST (12:42:09 UTC)**. These retained Actions artifacts
are not permanent release assets; the same run's Artifacts list is the fallback
while they remain retained.

**Select the presets again from Library after installing.** Old sessions restore
their stored settings, so simply opening a project does not replace its patch
with the revised factory recipe.

Each manifest and `SOURCE.txt` identifies the exact binary source. Later
screenshot or documentation commits on `main` do not change those bytes.
The [revision report](docs/CIRCUIT_LIBRARY_2026_11_3.md) explains the sound changes,
saved-state behavior and measured limits. The
[60-agent review ledger](docs/TEAM_REVIEW_2026_11_3.md) records the actual spawned
workers and their work. Earlier failures and builds retain their original
outcomes in the [distribution history](docs/CI_DISTRIBUTION_STATUS.md).

## Play your first sound

Launch **Standalone** and select your audio output and MIDI input, or install
VST3/AU and load **CS-80 Reference** on a software-instrument track. A fresh
instance opens **Brass Swell I**. Hold a chord, then
open **Library** to explore the other voices. With polyphonic aftertouch,
press just one note harder to brighten it; you can also drag down on a held
on-screen key to try pressure.

- **One circuit instrument** plays every program. The panel exposes the two ranks,
  22 tone tabs, four memory panels, ribbon and performance controls. There are
  no Legacy or Pure engine choices.
- **Library** contains all **523 existing programs** in one catalogue. The original
  222 addresses now use authored circuit voicings; their names and program numbers
  remain available alongside the later 301 programs.
- **Studio** exposes controls for a modulation matrix, drawable envelopes, MPE, microtuning,
  arpeggiation, effects and expanded voice modes. These are software additions,
  not features of the original hardware. See the [circuit and session contract](docs/PARITY_ISOLATION.md)
  and [editor gallery](docs/screenshots/).

## Instrument voicing revision — 2026.11.4

All **521 existing musical factory programs** have revised sound controls. The
two initialization patches and all **523 names, categories and program addresses**
are preserved. No presets were added. The work covers the original-address
collection as well as every musical program in the later collection.

An optional body for each synthesizer rank supplies independently evolving
partials for struck strings, electric pianos, winds, bowed strings, brass,
organs and bars. **259 existing programs use these bodies**, with 19 model
choices; the remaining 264 entries, including the two initialization patches,
retain circuit excitation. Each body now follows its rank’s panel high-pass and low-pass controls. Piano
partials evolve across the register; tine and reed pickup spectra evolve through
the decay. Bowed resonances remain stationary during vibrato and slides.
Synthetic pads, polysynths, leads, basses and effects also have individually
revised articulation, expression and motion.
The [revision report](docs/CIRCUIT_LIBRARY_2026_11_4.md) explains the sound changes,
controls, measured behavior and limits. The
[complete coverage record](docs/preset-evidence/2026.11.4/parameter-coverage.json)
checks every existing address.

| Try | Revised character and playing suggestion |
| --- | --- |
| Analog Piano / Stage Keys / Soft Keys | Hammer-shaped attacks, slightly stretched upper partials and separate string-course decay. Compare soft and firm notes, then hold a key as the upper content softens. |
| Electric Piano I / Electric Piano II / Tine Piano / Reed Piano | Distinct tine and reed spectra, a defined strike and more responsive touch. Compare soft and firm strikes in low, middle and high registers. |
| Guitar I / Guitar II / Nylon Pluck / Steel Pluck | Different pluck notches, body weighting and upper-partial losses. Listen to the onset and decay of single notes before playing chords. |
| Solo Violin / Solo Cello / Flute / Clarinet / Oboe / Bassoon | Sustained instrument bodies with controlled bow or air texture and pressure expression. Hold a note, then add aftertouch. |
| Vibraphone / Marimba / Celesta / Toy Piano | Distinct modal ratios and decay profiles. Marimba's upper mode fades substantially faster than Vibraphone's; compare sustained keys and early release. |
| Covered Horn / Trombone Glide | A sustained brass body and stronger fundamental, with the authored touch, slide and initial scoop. |
| Percussive Organ / Theatre Organ | Fading octave percussion or a gentle whole-registration tremulant, while the organ body keeps steady strike dynamics. |
| Pads / Polysynth / Leads / FX | Individually programmed attack, bloom, sustain, release and controller roles; environmental effects retain distinct noise bands and envelopes. |

The ten body controls are available in the DAW's parameter list and automation
editor. Model, partial decay and resonant size are captured on the next note;
mix and brightness are live. Partial decay applies to struck models; it has no
effect on sustained bowed, wind, brass or organ bodies. The custom Studio editor and standalone interface do not
yet have dedicated body-control widgets. Factory selection and saved-state
recall load these settings normally.

**Pressure Lead requires aftertouch** and is intentionally silent until pressure
is applied. Use channel/polyphonic aftertouch or drag downward on a held
on-screen key. For on-screen pressure, leave **KAS Scan Timing** off (the default);
MIDI aftertouch also works with scan timing enabled. The **modern** tag identifies
programs using optional effects, modulation or arpeggiation; those features remain
available in the same circuit instrument. The [preset guide](docs/PRESET_GUIDE.md)
explains registrations and playing techniques.

**Older sessions using Legacy or Pure intentionally change sound.** Their saved
controls and parameter identities are retained, but obsolete engine settings
resolve to the circuit engine with normal MIDI and optional Studio features.
There is no promise of identical old rendering. Use **Library → FACTORY LIBRARY → LOAD / RELOAD**, then play fresh notes,
to hear the revised voicing; merely opening a saved session does not
replace its stored patch with the current factory recipe. Keep the older binary
and a rendered reference when an existing project's exact sound must be retained.

Instrument names describe synthesized interpretations. The instrument bodies are
authored signal models with physically motivated spectra and losses; they are
not sampled instruments or complete measured simulations of instrument
mechanics. The software checks do not establish an indistinguishable acoustic
instrument or calibrated physical CS-80 match. Build and validator
results belong to the specific download and source commit listed above. The
earlier [2026.11.1 report](docs/PRESET_REFINEMENT_2026_11_1.md) also records a native
VST3 Program-queue controller dependency; **Library** selection is the direct
way to audition a current factory sound.

## From schematic to sound

The primary reference was the [Yamaha CS-80 Service Manual](https://therogoffs.com/cs80/manuals/CS80_Service_Manual/),
Yamaha's original scans and foldouts hosted by David Rogoff. The work starts by
tracing a signal or control through the drawings, then implementing the
documented relationship and checking the resulting behavior in software.
Where a drawing gives the route but not a full response curve, the model
records the assumptions.

| Manual evidence | How it was used |
| --- | --- |
| M voice card and harness, pp. 41 and 44 | The saw and pulse feed high-pass then low-pass; the sine bypasses both filters. The harness drawing resolves the HP-to-LP link. See the [oscillator](docs/OSCILLATOR_PROMOTION.md), [filter reconstruction](docs/IG00156_NODAL_MODEL.md) and [filter code](src/core/Ig00156Filter.h). |
| TWS and TRG, pp. 17, 19 and 23 | The drawings separate sustained initial brilliance and level from the transient pitch bend. The nominal 50 ms pitch path comes from the documented capacitor and resistor network; MIDI-to-touch behavior is modeled rather than measured. See the [circuit analysis](docs/INITIAL_PITCH_TRANSIENT.md) and [implementation](src/core/TouchCircuit.h). |
| OE1/OE2, pp. 55–58 | The detailed drawings identify the chorus delay send and four returns, and resolve a contradictory foldout pin label. See the [board trace](docs/OE2_CIRCUIT_EVIDENCE.md) and the [real-time chorus model](docs/BBD_CHORUS_MODEL.md) for what actually ships. |

Other sources answer questions the service drawings alone do not:

- The [Yamaha owner's manual](https://www.therogoffs.com/cs80/CS80E_2.pdf#page=25)
  describes how touch changes the sub-oscillator's filter modulation.
  That behavior informs the [per-note pressure route](docs/AFTERTOUCH_SUB_VCF.md).
  The [Yamaha museum photograph](https://www.yamaha.com/en/about/experience/innovation-road/collection/detail/2039/)
  and panel drawing guided the independently drawn [editor layout](docs/PANEL_CONFORMANCE.md).
- Yamaha's [adjustment sheets](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF)
  anchor the [offline ring-oscillator reference](docs/RING_TIME_REFERENCE.md) at nominal periods
  of 150 ms and 5 ms under specified service settings. These are calibration
  targets from the document, not voltages or timings measured by this project.
- The [Yamaha IC Guide Book](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf)
  helped trace the [oscillator reset and waveform shaper](docs/OSCILLATOR_CIRCUIT_EVIDENCE.md).
  Panasonic's [BBD handbook](https://www.ka-electronics.com/images/pdf/Panasonic_BBD.pdf)
  informed the [MN3001 clock and output timing reference](docs/MN3001_CLOCK_EVENT_DESIGN.md).

The [source ledger](docs/HARDWARE_REFERENCES.md) and [coverage map](docs/HARDWARE_COVERAGE.md)
separate documented facts, engineering estimates, software test results and
unresolved behaviors. No calibrated physical CS-80 was measured for this
project; the tests do not establish a hardware match.

## How we reduced CPU work

The engine does less redundant work without lowering the render quality:
the oscillator screens pairs of crossings before the same scalar solve,
filter controls reuse identical results within a sample, and the output
oversampling filter computes independent tap products together while keeping
the original sum order. The plug-in also avoids scanning unused automation
lanes. Qualified GCC/x86-64 builds also use an exact
[AVX2 filter path](docs/experiments/avx-default/) with a scalar fallback. The
[engineering reports](docs/experiments/cumulative-cpu-2026-11-19/continued-exact-hotpaths/)
describe the guards and fallbacks.

| Matched VST3 workload | Fewer guest instructions |
| --- | ---: |
| Eight held notes at varied velocities | 23.4–23.7% |
| Fresh silence | 45.5–46.5% |

These are [matched instruction counts](docs/experiments/cumulative-cpu-2026-11-19/compact-cutoff-publication/)
for runtime `0c4abd85` versus the historical `db02cd8` baseline on Linux x86-64
(GCC 14.2 Release, 48 kHz, 16/32/64-frame buffers). Native replay for the
latest optimization pass matched tested audio and exposed state bit for bit;
Standard still reports **67 samples (1.40 ms at 48 kHz)** of plug-in latency.
That is not round-trip latency. Instruction counts show work removed, not a
DAW CPU percentage. [Physical CPU-budget qualification](docs/experiments/cumulative-cpu-2026-11-19/q0-evidence/PHYSICAL-TARGET.md)
is still open.

## Build from source

Use a C++20 toolchain and CMake 3.25+ for the presets. The build pins JUCE
8.0.11. On Ubuntu 24.04, start with the [dependencies in the build workflow](.github/workflows/build.yml).

```sh
cmake --preset linux-release
cmake --build --preset linux-release --parallel
ctest --preset linux-release
```

Use `macos-release` with Xcode command-line tools and Ninja, or
`windows-release` with Visual Studio 2022. The plug-in and standalone outputs
are under `build/CS80Reference_artefacts/Release/`. For a JUCE-free core build,
configure with `-DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON`.

## Further reading

- [Engine architecture](docs/ENGINE.md), [model register](docs/STATUS.md#model-register)
  and [DSP measurements](docs/DSP_MEASUREMENTS.md)
- [Distribution and checksums](docs/CI_DISTRIBUTION_STATUS.md) and
  [release qualification](docs/release/RELEASE_QUALIFICATION.md)
- [Fidelity requirements and contribution standards](docs/ANALOG_FIDELITY_REQUIREMENT.md):
  preserve the documented circuit behavior, retain diagnostic references, and explain intentional sound changes.

## Licence

[GNU Affero General Public License v3](LICENSE). JUCE is used under its AGPLv3
option. CS-80 Reference is independent and is not affiliated with or endorsed
by Yamaha.
