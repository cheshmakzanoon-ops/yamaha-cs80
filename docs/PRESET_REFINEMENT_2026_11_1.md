# Circuit preset refinement 2026.11.1

## Result and publication status

This refinement gives **twelve Circuit programs** stronger defining details or
more useful stereo separation. It builds on the earlier **300-program 2026.11
redesign**, retaining the bank's 301 entries including Init Circuit and the 222
frozen Legacy programs. The comparison baseline for this refinement is
`7f55a95a7534b389927defe46866456560b3a878`, which already contains that redesign.
The [original revision report](PRESET_REVISION_2026_11.md) documents the larger
change from the older library.

**Final level calibration and all four integrated preset test suites have
passed. Native platform validation and publication of this revision's bundles
are pending.** The tables below
identify which evidence comes from the authoring stage. They do not certify a
pending download. The final publication update must record the actual source
commit and successful CI run; the library revision and application version
alone cannot identify a binary.

| Deliverable | Current verification status |
| --- | --- |
| Twelve authored refinements | Measured against the frozen baseline; implemented in the factory data |
| Character regression suite | 128/128 checks pass on the final calibrated bank; baseline fails 16 checks |
| Final calibrated bank and complete library audit | Two calibration passes converge; all 301 Circuit + 222 Legacy programs, 136 voicing checks and 9 capture-tool tests pass |
| Windows x64 VST3 / Standalone | Pending this revision's CI and archive verification |
| macOS Apple Silicon VST3 / AU / Standalone | Pending this revision's CI and archive verification |
| macOS Intel VST3 / AU / Standalone | Pending this revision's CI and archive verification |
| Linux x86-64 VST3 / Standalone | Pending this revision's CI and archive verification |
| 24-excerpt audition artifact | Capture pipeline verified; revised-bank CI artifact pending |

[Evidence index](preset-evidence/2026.11.1/README.md) ·
[Evidence provenance](preset-evidence/2026.11.1/provenance.json)

## What changed and why

The audit measured the contribution of each rank at the program's actual
balance. A separately programmed breath or strike can exist in isolation yet
remain too subordinate in the finished voice to express its intended role.
Likewise, chorus does not by itself establish a useful stereo image. The changes
address those specific relationships while retaining the existing synthesis
algorithms, family helpers, registrations and program identities.
[Authoring identity comparison](preset-evidence/2026.11.1/identity/summary.json)

### Sustained air: Flute and Breathy Flute

The flute body contains a strong sine that bypasses both filters. Its noise rank
therefore needed its own source, filter-output, VCA, sustain, touch and balance
adjustments. Flute now carries restrained sustained air; Breathy Flute has a
stronger aspirated component. Their existing body timing and pitch registration
remain intact. The already corrected Pan Flute keeps its different role: a
short chiff that disappears while the pitched body continues.
[Wind authoring](../src/core/curated/Winds.inc)

At **48 kHz, MIDI 67, velocity .8, pressure 0**, the weighted air/body RMS over
**300–500 ms** changes as follows:

| Program | Baseline | Refined authoring |
| --- | ---: | ---: |
| Flute | −50.77 dB | **−26.85 dB** |
| Breathy Flute | −34.64 dB | **−19.62 dB** |

[Matched center captures](preset-evidence/2026.11.1/winds/center.json)

Across MIDI **55/67/79**, velocities **.3/.9** and pressure **0/1**, Flute's
refined ratio spans −28.93…−25.02 dB and Breathy Flute's spans
−21.64…−17.45 dB. These pressure settings are fixed endpoints, not performed
crescendos. Weighted isolated ranks reconstruct the complete signals within
−142.3 dB relative RMS or better. An additional two-second, eight-note chord at
full velocity and pressure peaks at −2.506 dBFS for Flute and −4.353 dBFS for
Breathy Flute, using the baseline master volumes.
[Wind measurements and conditions](preset-evidence/2026.11.1/winds/summary.json)

Final-bank performance checks add 24 captures / 96 seconds of channel and
polyphonic pressure ramps, soft/hard repeats, overlap and release. Every voice
retires, all samples are finite, and the highest peak is **−7.824 dBFS**. Matched
pressure-step/control replays produce no immediate output or touch-node jump;
polyphonic pressure affects only its addressed note. These performed phrases
complement the fixed-endpoint authoring measurements.
[Performed wind evidence](preset-evidence/2026.11.1/wind-performance/summary.json)

### Defining attacks: Harp, Kalimba, Sine Bass and Upright Pluck

Harp and Kalimba retain their body and upper-rank envelopes. Increasing only the
upper sine and rank balance gives their existing octave glint and ping more
weight. Sine Bass and Upright Pluck retain their waveform sources, timing and
touch routes; a modestly lower strike high-pass, stronger filter output and
revised balance make the short upper component less thin.
[Plucked voices](../src/core/curated/Plucked.inc) ·
[Bass voices](../src/core/curated/Bass.inc)

| Program | Increase in weighted upper/body RMS, 4–20 ms | Conditions |
| --- | ---: | --- |
| Harp | **8.85–8.99 dB** | MIDI 48/60/76; velocities .25/.9 |
| Kalimba | **8.08–8.21 dB** | MIDI 48/60/76; velocities .25/.9 |
| Sine Bass | **7.89–8.16 dB** | MIDI 36/48/60; velocities .3/.9 |
| Upright Pluck | **7.31–7.45 dB** | MIDI 36/48/60; velocities .3/.9 |

All use **48 kHz and zero pressure**. These are relative layer measurements
before final master calibration. The detail remains subordinate and continues
to decay ahead of the body. For example, at MIDI 60 and velocity .9, Kalimba's
upper/body ratio falls from −18.07 dB during 0–20 ms to −51.51 dB during
200–400 ms. The change does not turn its brief strike into a sustained layer.
[Pluck measurements](preset-evidence/2026.11.1/plucks/measurements.json) ·
[Bass measurements](preset-evidence/2026.11.1/bass/summary.json)

### Late texture: Ether Pad

Ether Pad increases its noise amount from .65 to .95 and rank balance from .19
to .28. Its sine, filters, envelopes and pressure routes stay unchanged. Across
MIDI **48/60/76** and pressure **0/.5/1**, velocity .8 at 48 kHz, the late air/body
ratio increases **7.693–7.696 dB**. At MIDI 60 without pressure it moves from
−32.67 to −24.97 dB over 1.5–2.5 seconds. The air still develops later than the
body; the dominant pitch bin remains unchanged in every comparison.
[Ether measurements](preset-evidence/2026.11.1/ether/summary.json)

### Stereo spread: five ensembles

These programs previously rendered almost mono despite descriptions of broad
ensembles. Individual opposing rank pans now supply a restrained spread while
the original chorus settings remain active. The measurements compare **MIDI 60,
velocity .8, pressure 0, 48 kHz**, over **1.0–2.8 seconds** of a held note.

| Program | Baseline side/mid RMS | Refined side/mid RMS |
| --- | ---: | ---: |
| Noir Swell Brass | −39.47 dB | **−19.28 dB** |
| Brass Ensemble 16 | −37.84 dB | **−21.79 dB** |
| Neo-Noir Pad | −38.92 dB | **−19.95 dB** |
| Arctic Pad | −38.07 dB | **−23.09 dB** |
| Synthwave Chords | −40.42 dB | **−20.14 dB** |

Additional captures at MIDI 48/72 and pressure 0/.85 retain maximum absolute
channel imbalance of **0.663 dB**, overall mono loss of **0.103 dB**, and
fundamental-band mono loss of **0.116 dB**. Mono is `(L+R)/2`, compared against
averaged stereo power. These are bounded single-note results. A final-bank performed check also
rendered five twelve-second, eight-voice chord sequences and matched controls.
The factory programs retained at least **3.817 dB** sample headroom; worst
active-window mono loss was **0.317 dB overall** and **0.481 dB in the combined
fundamental bands**. All voices retired by twelve seconds; the engine output
floor remained near −82 dBFS, so this does not mean digital silence.
[Stereo comparisons](preset-evidence/2026.11.1/stereo/initial.json) ·
[Register checks](preset-evidence/2026.11.1/stereo/summary.json) ·
[Performed chord evidence](preset-evidence/2026.11.1/stereo-chords/summary.json)

**Pure CS-80 centers both rank pans.** This authored stereo spread belongs to
ordinary Circuit mode; Pure intentionally removes it. The [mode contract](PARITY_ISOLATION.md)
and [preset guide](PRESET_GUIDE.md) explain that behavior.

## Regression sensitivity and compatibility

The new [character tests](../tests/PresetCharacterTests.cpp) measure the finished
sound: sustained breath, present but subordinate strikes, earlier disappearance
of the attack layer, stereo information, channel balance and mono retention.
They also require weighted isolated ranks to reconstruct each complete patch
within **1% RMS**, both over the full capture and the exact measurement windows.
This prevents a local diagnostic mismatch from being hidden by a long average.

| Identical suite against | Passed | Failed |
| --- | ---: | ---: |
| Frozen 2026.11 baseline | 112/128 | 16 |
| Final calibrated 2026.11.1 bank | **128/128** | **0** |

The same 31.4-second, 44.1 kHz fixture covers eleven programs; Ether Pad has its
separate authoring matrix. No character threshold was relaxed after execution.
The final integrated run also passes the full 523-program audit, 136 voicing
checks and nine capture-tool tests. Two level-calibration passes converged;
only Ether Pad, Kalimba, Sine Bass and Upright Pluck needed master-level changes.
These bounds describe the intended roles of these programs,
not universal hearing thresholds or acoustic-instrument similarity scores.
[Character evidence](preset-evidence/2026.11.1/character/summary.json) ·
[Independent review](preset-evidence/2026.11.1/review/summary.json) ·
[Final integrated results](preset-evidence/2026.11.1/final/summary.json)

The final compiled-bank identity comparison finds exactly **38 voicing values
and four master levels changed across twelve programs**, with the other **289
Circuit value vectors** intact. UFO Landing also has a description-only
correction, leaving 288 complete Circuit records unchanged. All names,
categories, tags, ordering and host indexes remain stable. All **222 Legacy
records**, including their complete 190 float bit patterns and metadata, match
the baseline. All **49,152 tested MIDI addresses**, 830 index lookups, the
296-parameter host layout and state schema 3 remain unchanged.
[Final identity evidence](preset-evidence/2026.11.1/final-identity/summary.json)

Saved sessions, user presets, A/B and browser Revert restore their serialized
sound values. A saved factory index is metadata and does not reload the bank.
Explicit factory selection or a later MIDI Program Change loads the current
voicing at its stable address; a host saving only a program number therefore
selects the revised sound. Startup remains Brass Swell I.
[Restore control-flow review](preset-evidence/2026.11.1/compatibility/summary.json)

A separate audio screen compared **45,150 pairs across all 301 programs** and
found no exact raw or gain-normalized duplicates in its stated fixture. Ten
follow-up captures distinguish the closest pairs through register response,
decay or onset. This is a bounded distinctness check, not a perceptual quality
score or proof that related instruments should sound unrelated.
[Distinctness evidence](preset-evidence/2026.11.1/distinctness/summary.json)

## Playing instructions and audition delivery

UFO Landing's description now correctly explains its shared ring sweep. In it,
Gong, Temple Bell and Ring Sweep Bell, overlapping notes continue the same
carrier envelope. Release all keys and any sustain hold before the next attack
to restart the sweep. Matched continuous/overlapping/detached phrases confirmed
identical carrier histories for overlap and a fresh envelope for detached
attacks; the sustain qualification follows the source gate logic.
[Ring evidence](preset-evidence/2026.11.1/ring/summary.json)

The [build workflow](../.github/workflows/build.yml) now defines **24 audition
WAVs**, retaining the original twelve examples and adding every refined program.
Seven slow examples use five-second holds and two-second release excerpts; the
others use three-second holds and one-second releases. Captures retain the
factory settings without normalization. Provenance includes source commit,
binary hash, selection, timing, completion status and checksums. A pipeline
smoke check produced 24 WAVs and verified 32 checksummed files; that check used
the frozen baseline executable solely to validate delivery. The revised-bank
artifact still requires the final CI run.
[Pipeline evidence](preset-evidence/2026.11.1/audition/summary.json)

Instrument names describe CS-80 synthesized interpretations. These measurements
establish specific software behavior and reproducible differences; they do not
establish a listening-panel verdict, acoustic-instrument identity, calibrated
physical-CS-80 matching or compatibility with every DAW and performance history.
