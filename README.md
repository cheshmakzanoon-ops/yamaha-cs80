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

**2026.11.4 — all 521 existing musical presets have revised sound controls.**
The library keeps all **523 names, categories and program addresses**, including
the two initialization programs. No presets were added. This revision refines
instrument-body filtering, attacks, releases, register balance and playing
response; the continuation also corrects tine dynamics and bowed-body motion.

| Platform | Included formats | Verified installation download |
| --- | --- | --- |
| Windows x64 | VST3, Standalone | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14856804857.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14855214289.zip) |
| macOS Intel (x86-64) | VST3, AU, Standalone | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14858878730.zip) |
| Linux x86-64 (Ubuntu 24.04 build) | VST3, Standalone | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14870428441.zip) |

All six jobs passed in [build 45246863788, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788). Across the four
native suites, **1,311 of 1,318 registered CTest executions passed, zero failed
and seven were formally skipped**; 24 nested Python skips are recorded
separately. All **13 required pluginval 1.0.4 lanes at strictness 10** completed
with their original process exit 0, with 13 validation invocations and 0 runtime validation retries.
The actual installation ZIPs passed API size/hash, source, inventory, native
architecture and validated-payload checks.
[Complete qualification, hashes and original evidence](docs/preset-evidence/2026.11.4/ci/source-078c098/release-qualification.json)

**Hear the revised sounds:** the verified [audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14870917591.zip) contains
**138 unnormalized 48 kHz stereo float WAVs from 74 existing programs**, totaling
**14 minutes 30 seconds**. Every sample is finite and below full scale. All
19 active body models appear, and its full **523-program × 200-parameter**
inventory matches the reviewed factory data exactly. These are representative
examples, not a listening assessment of all 523 programs.
[Actual archive acceptance](docs/preset-evidence/2026.11.4/ci/source-078c098/audition/acceptance-summary.json) ·
[Delivered inventory identity](docs/preset-evidence/2026.11.4/ci/source-078c098/audition/delivered-inventory-identity.json)

Unpack the outer ZIP, verify its included `SHA256SUMS`, and unpack the inner
archive while keeping plug-in/app bundles intact. Follow the
[installation guide](docs/INSTALL_AND_QUICKSTART.md#install-only-the-formats-you-need).
Choose the Mac build for your **host process architecture**. These are CI
installation bundles without publisher release signing or Apple notarization;
Linux requires compatible system libraries. The earliest advertised-artifact
expiry is **21 February 2027 at 15:14:35 AST (19:14:35 UTC)**. These retained Actions downloads are not permanent
release assets. The [same run's Artifacts list](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788#artifacts) is the fallback
while retained. Archives were acquired through the GitHub connector and hashed;
an unauthenticated nightly.link route has not been independently verified.

**After installing, use Library → FACTORY LIBRARY → LOAD / RELOAD, then play
fresh notes.** Older projects restore stored settings. Clicking an already-loaded
row with Audition enabled may retain those settings.

Binary source is [`078c098`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/078c0981ffa1b5472342e0fb5ab8702bc3bd2889);
later documentation/evidence commits on `main` do not change those bytes. Each
bundle's `SOURCE.txt` and manifest identify the full source. The bundled guides
are source-time snapshots; the online qualification records completed results.
The [revision report](docs/CIRCUIT_LIBRARY_2026_11_4.md) explains the sound changes,
saved-state behavior and measured limits. The
[60-worker review](docs/TEAM_REVIEW_2026_11_4.md) records actual assignments,
outcomes and evidence. Earlier outcomes remain in the
[distribution history](docs/CI_DISTRIBUTION_STATUS.md).

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

## Instrument voicing revision — 2026.11.6

The cumulative revision changes sound controls in **all 521 existing musical
factory programs** relative to the published 2026.11.4 bank. The
**523 names, categories and program addresses** remain intact, and both Init
programs preserve their preceding sound. No presets were added. The changes
cover the original 222-address collection and the later 301-program collection.

The programming now gives each family a more appropriate strike, sustained
body, release and playing response. Soft hammer strikes retain their lower
energy and softer upper spectrum. Held piano and electric-piano notes leave
room for their independently decaying partials; releasing a key damps them.
Bow and breath entrances respond to playing effort and register. Xylophone,
Steel Drum, Choir Pad and Dark Choir use distinct material or vowel profiles.
Environmental effects have separate evolving contours, and sequence voices
shape their dry articulation around their actual note spacing.

The 2026.11.6 continuation stabilizes piano string-course tuning while keeping
varied strikes, and preserves fixed-frequency wind/brass body color during
pitch changes. Nine existing voices gain dedicated material profiles: Tubular
Bells, Gong, Temple Bell, Agogo, Music Box, Kalimba, Banjo, Koto and Harp. Their
contact and ringing body now use the intended material in both ranks.

**272 existing programs use instrument bodies**; the other 251 entries,
including both Init programs, retain circuit excitation. The 19 active model
choices keep their published addresses. Four optional variants add xylophone
bar, steelpan shell, choir Ah and choir Oo behavior within those existing
models. Nine Material profiles specialize compatible models without changing
either the Model or Variant range. Standard profiles remain available.
Deliberately synthetic sounds keep their ring modulation, PWM, interval, motion or pressure-led identities.

The [revision report](docs/CIRCUIT_LIBRARY_2026_11_6.md) explains the implementation,
measurements and limits. The
[complete coverage record](docs/preset-evidence/2026.11.6/verification/a15/parameter-coverage.json)
checks every existing address, and the
[final audio accounting](docs/preset-evidence/2026.11.5/development/long-pad-headroom/final-bank-audio-accounting.json)
records the retained matched-condition comparisons and the two pad-level corrections.
The [current complete-bank audit](docs/preset-evidence/2026.11.6/verification/root-integration/)
passes the existing level and headroom limits. Native qualification and the
actual expanded audition archive remain pending; the Downloads table above
continues to identify the published 2026.11.4 packages.

| Try | Revised character and playing suggestion |
| --- | --- |
| Analog Piano / Stage Keys / Soft Keys / Honky Keys | A softer quiet strike, stronger touch contrast, stable string-course tuning and more audible lower-partial aftersound. Compare soft and firm notes across the keyboard; hold a note, then release it or use the pedal. |
| Electric Piano I / Electric Piano II / Tine Piano / Reed Piano | Distinct tine and reed spectra, revised strike balance and independent partial decay. Compare low, middle and high registers at several velocities. |
| Guitar I / Guitar II / Nylon Pluck / Steel Pluck | Separate pluck, body and upper-partial loss profiles, with clearer touch contrast and key damping. Try single notes before chords. |
| Harp / Banjo / Koto / Kalimba | Dedicated finger, membrane, plectrum or supported-tine profiles. Harp and Kalimba now keep both ranks at the played pitch; vary touch, release and register. |
| Solo Violin / Solo Cello / Flute / Clarinet / Oboe / Bassoon | Shaped bow or breath entrances, register-aware response and pressure expression. Hold a note, increase aftertouch, then ease it back. |
| Xylophone / Marimba / Steel Drum / Vibraphone | Distinct wooden-bar, metal-bar and shell resonances. Xylophone emphasizes a near-three-times upper mode; Steel Drum has octave and twelfth modes. Compare held notes, early release and pedal-held ringing. |
| Tubular Bells / Gong / Temple Bell / Agogo / Music Box | Separate tube, developing plate, beating shell, compact bell and comb-tooth spectra. Compare soft and firm strikes, then leave room for their different aftersounds. |
| Covered Horn / Trombone Glide | Revised breath and fundamental balance with controlled pressure, slide and initial scoop. |
| Percussive Organ / Theatre Organ | Steady strike dynamics with the intended footage, fading percussion or registration tremulant. Compare equal notes at different velocities. |
| Choir Pad / Dark Choir | Separate Ah and Oo resonance profiles, shaped entrances and breath/pressure response. Play several registers and use gentle pitch movement. |
| Pads / Polysynth / Leads / Sequences | Individually programmed bloom, touch, pressure and release. Long dense chords have additional headroom checks; sequence attacks and releases follow their actual gates. |
| Wind / Rain / Surf / Thunder / Clockwork | Irregular gusts, smooth rain motion, a surge with draining foam, a crack with a decaying low roll, or separated ticking. Hold long enough to hear each contour. |

The **14 body controls** are available in the DAW's parameter list and automation
editor. Model, partial decay, resonant size, variant and material are captured
on the next accepted note; mix and brightness are live. Partial decay applies to struck
models; it has no effect on sustained bowed, wind, brass or organ bodies. The
custom Studio editor and standalone interface do not yet have dedicated
body-control widgets. Factory selection and saved-state recall load these
settings normally.

**Pressure Lead and Electric Leads 136 require aftertouch** and are intentionally
silent until pressure is applied. Use channel/polyphonic aftertouch or drag downward on a held
on-screen key. For on-screen pressure, leave **KAS Scan Timing** off (the default);
MIDI aftertouch also works with scan timing enabled. The **modern** tag identifies
programs using optional effects, modulation or arpeggiation. The
[preset guide](docs/PRESET_GUIDE.md) explains registrations and playing techniques.

**To hear the revised factory recipes after updating:** open **Library → FACTORY
LIBRARY**, select a program and press **LOAD** (or **RELOAD**, if shown), even if
the same name is already displayed. Release held notes and the pedal, then play
fresh notes. Save the project to retain the new settings. Existing projects,
user patches and A/B slots keep their stored controls; merely selecting a row
or reopening an old project does not replace them. REVERT can restore the
settings present when Library opened.

The two Material parameters append after all **202 preceding core definitions**
and **308 host positions**, bringing the totals to **204 core parameters** and
**310 host positions**. Earlier IDs, ranges and choice meanings stay intact;
missing Material or Variant values in older state default to Standard. The
[compatibility review](docs/preset-evidence/2026.11.6/reviews/a34.md) checks the
previous metadata and host layout. Older sessions using Legacy or Pure still
resolve to the circuit engine under the
preceding migration contract. Their saved controls remain, but their old
rendering is not promised. Keep the older binary and a rendered reference when
an existing project's exact sound must be retained.

Instrument names describe synthesized interpretations. These are authored
signal models with physically motivated spectra and losses, rather than
sampled instruments or complete measured simulations of instrument mechanics.
The software checks establish the measured behavior in the linked evidence;
they do not establish indistinguishability from acoustic instruments, human
listening approval, or a calibrated physical CS-80 match. Build and validator
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
