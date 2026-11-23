# Instrument voicing revision — 2026.11.3

Revision **2026.11.3** reprograms the existing **521 musical factory programs**
while preserving **Init Patch** and **Init Circuit** as starting points. All
**523 names, categories and host program addresses remain in their original
order. No presets are added.** The work covers the 300 musical programs in the
later collection and the 221 musical programs at the original addresses.
[Factory catalogue](../src/core/FactoryPresets.cpp) ·
[Parameter coverage](preset-evidence/2026.11.3/parameter-coverage.json)

The main change is an optional instrument excitation for each synthesizer rank.
It gives struck strings, electric pianos, reeds, winds and bars their own
harmonic structures and evolving spectra. **259 existing programs use these
bodies; the other 264 entries, including the two initialization patches, retain
circuit excitation.** Synthetic pads, leads, basses, polysynths and effects have
also been individually reprogrammed for their intended attacks, sustained
tones, expression and motion.
[Body implementation](../src/core/InstrumentBody.h) ·
[Authored recipes](../src/core/CuratedLibrary.inc)

This report describes the source revision and its development evidence. The
final level audit and native build qualification are still being completed at
the time of this report's preparation. A downloadable build is qualified only
by its exact source commit, workflow results and package evidence in the
[download section](../README.md#downloads); earlier archives do not acquire new
sounds when this document changes.

## What changes in the sound

A static oscillator spectrum and one overall decay cannot independently soften
a piano's upper strings, retain a tine's ringing center, or separate a wooden
bar's brief knock from its lower note. The new excitation layer assigns separate
weights and losses to its partials. The existing rank envelope still controls
the overall note, including sustain, key release and pedal behavior.
[Model construction](../src/core/InstrumentBody.h) ·
[Rank integration](../src/core/Engine.cpp)

There are **19 active model choices**, plus Off:

| Body choices | Programmed distinction | Existing examples |
| --- | --- | --- |
| Piano strings | Velocity-dependent hammer spectrum, strike-position notches, slightly stretched upper partials and closely spaced string courses | Analog Piano, Stage Keys, Honky Keys, Soft Keys, Velocity Keys |
| Tine electric piano; reed electric piano | Sparse predominantly odd tine content versus a denser even/odd reed spectrum, with different attack components and partial losses | Electric Piano I/II, Tine Piano, Reed Piano, Kalimba |
| Clavichord; harpsichord | Tangent or quill excitation, notched string spectra and separately fading upper content; the harpsichord model keeps even strike dynamics | Clavichord I/II, Funky Clavichord, Harpsichord I/II, Lute Harpsichord |
| Nylon strings; steel strings; plucked bass | Different excitation notches, harmonic falloff, body weighting and upper-partial damping | Guitar I/II, Nylon Pluck, Steel Pluck, Harp, Upright Pluck, Picked Bass |
| Bowed strings | Sustained harmonic weighting with quiet contact texture; rank envelopes and expression establish the bow gesture | Solo Violin, Solo Cello, Chamber Strings, Ensemble Strings |
| Flute; clarinet; oboe; bassoon | Dominant-fundamental flute, odd-harmonic clarinet and denser double-reed spectra, with restrained air/contact components | Flute, Clarinet, Bass Clarinet, Oboe, English Horn, Bassoon |
| Brass; organ | Sustained brass harmonic color or stable organ registrations; organ excitation does not vary with strike velocity | Covered Horn, Trombone Glide, Flute Organ 8+4, Pipe Organ |
| Bell; vibraphone; marimba; celesta | Different modal ratios and independent losses; vibraphone and marimba include a mode at four times the fundamental, two octaves above it | Ring Bell I/II, Vibraphone, Marimba, Celesta, Toy Piano |

These are **authored signal models**, with physically motivated features. They
are not sampled instruments, measured instrument-specific resonators, or a
complete simulation of hammer/string/soundboard, reed/airflow, bow/string or
lip/bore interaction. Several named instruments share a model family and gain
their separate identities from register, brightness, rank balance, envelopes,
noise, modulation and effects. A horn and a trumpet do not have separately
measured bores here; a violin and a cello do not have separately measured body
geometries. The revision improves the available timbral structure and playing
behavior without establishing an indistinguishable acoustic or physical-CS-80
match. [Declared model scope](../src/core/InstrumentBody.h)

### Articulation and expression receive individual attention

The instrument layer is only part of the revision. Every musical recipe has
changed sound controls beyond its master volume. The
[coverage comparison](../scripts/compare-factory-revisions.py) checks those
changes and stable identities; parameter differences alone are not evidence of
perceptual improvement.

| Existing family or program | Intended result |
| --- | --- |
| Electric Piano I/II and Kalimba | A firm strike increases body level as well as harmonic color. The revised VCA balance avoids compressing the amplitude response as the excitation brightens. |
| Piano and plucked keyboards | A prompt strike, independently softening upper content, a decaying held note and deliberate damping on release. |
| Solo strings and winds | Stable unpressed pitch, a distinct onset layer, a sustained harmonic body and controlled pressure expression. |
| Trombone Glide | A stronger, steadier fundamental from a full body on rank I and a quieter supporting rank; slide and initial scoop remain. |
| Percussive Organ | The separate octave percussion fades while the lower registration stays steady. |
| Theatre Organ | A gentle 5.6 Hz amplitude route moves the whole registration, alongside the shallow stereo effect. |
| Synthetic pads and polysynths | Separate attack and bloom roles, intentional sustained tones, clearer chord registrations and individually chosen releases. |
| Leads and synthetic basses | Distinct sine, square, pulse, brass-like and resonant foundations; deliberate velocity, pressure, glide and pitch-motion roles. |
| Wind, Rain, Surf and Thunder | Different noise bands and envelopes, including a short crack over a slower rumble for Thunder. These remain synthesized environmental effects. |
| Modern hybrids | The new bodies feed the existing modulation and effect paths. Arpeggiator rates, gates and octave spans retain their authored musical roles. |

[Keys](../src/core/curated/Keys.inc) ·
[Strings](../src/core/curated/Strings.inc) ·
[Winds](../src/core/curated/Winds.inc) ·
[Brass](../src/core/curated/Brass.inc) ·
[Organ](../src/core/curated/Organ.inc) ·
[Pads](../src/core/curated/Pads.inc) ·
[Polysynth](../src/core/curated/Polysynth.inc) ·
[Leads](../src/core/curated/Leads.inc) ·
[Bass](../src/core/curated/Bass.inc) ·
[FX](../src/core/curated/Fx.inc) ·
[Modern hybrids](../src/core/curated/Modern.inc)

The original-address builder inherits these recipes. Its adjective finishes
also adjust body brightness and partial decay, so a darker or longer variation
affects the added excitation. Row-II tone studies swap all four body controls
along with the existing channel controls. Original keyboard variations retain
struck onsets: *Slow Keys* extends a decay instead of introducing a pad entrance
or unrelated portamento. [Original-address recipes](../src/core/OriginalCircuitLibrary.inc)

## Where the body sits in a rank

The added body blends with the rank's **filtered oscillator signal and sine
tap before the VCA**. It then uses the existing touch-controlled level and
amplitude envelope, with the preset's pan, ring processing, chorus and insert effects. The body
does not pass through the original high-pass/low-pass circuit stages as another
oscillator input. Instead, brilliance, low-pass contour and relevant expression
controls also influence its spectral brightness. This distinction matters when
editing a preset with a high body mix: changing its oscillator waveform may have
only a small effect. [Signal routing](../src/core/Engine.cpp)

Body pitch follows the sounding rank, including footage, tuning, pitch bends,
ribbon and glide. The onset's spectral register uses the target note, footage,
master/rank tuning and tuning reference, including the selected Scala/MTS
tuning. Vibrato and transient bends move the sounding pitch without rebuilding
the excitation. Modes approaching the host Nyquist limit fade out even when
the engine runs at a higher internal rate.
[Register and sample-rate tests](../tests/InstrumentBodyEngineTests.cpp) ·
[Model tests](../tests/InstrumentBodyTests.cpp)

On a reused voice with an audible release, a **3 ms crossfade** preserves the
outgoing oscillating body while the new strike begins. It retains the old
body's pitch during the handoff, reducing a discontinuity caused by abruptly
resetting the resonance under an already-open VCA. The outgoing state uses
fixed storage and is active only for that short transition.
[Handoff implementation and regressions](../tests/InstrumentBodyEngineTests.cpp)

### Editing the body controls

Each rank has four saved, automatable controls in the host parameter groups
**Instrument body I** and **Instrument body II**:

| Control | Range | Behavior |
| --- | --- | --- |
| Model | Off plus 19 choices | Captured on the next accepted note, including a change to Off. Existing notes retain their current excitation. |
| Mix | 0–1 | Live blend between the circuit/sine signal and the selected body. |
| Brightness | 0–1 | Live balance of fundamental and upper modes, with the preset's other brightness/expression routes still participating. |
| Partial decay | 0.25–4× | Captured on the next note. Larger values let decaying modes ring longer; the rank ADSR remains responsible for overall duration and release. Sustained wind/organ modes do not acquire a decay from this control. |

Use the DAW's parameter list or automation editor to edit these eight controls.
The custom Studio editor and standalone interface do not yet have dedicated
body-control widgets; factory selection and saved-state recall still load them.

The layer is available only on **PANEL sources** in the playable circuit
instrument. Hardware tone tabs, memory selectors and diagnostic isolation
policies suppress it. Applying a hardware tone clears the selected rank's body
controls to their off defaults. These additions do not create a new engine-mode
selector or change the separate hardware source matrices.
[Control definitions](../src/core/Params.h) ·
[Per-control help](../src/plugin/EvidenceRegister.h) ·
[Hardware-tone application](../src/core/FactoryPresets.cpp)

## Saved patches, program addresses and automation

**Reload a factory program from Library to hear its revised recipe.** A session
or user preset restores its saved controls; it does not silently replace those
controls with the current factory definition. Old states without body fields
default both models and mixes to Off/zero. Save a user copy after editing a
revised factory sound. [State and program handling](../src/plugin/PluginProcessor.cpp)

The original **190 core parameter IDs** and **296 flattened host parameter
positions** keep their spelling and order. Eight controls are appended, giving
**198 core parameters and 304 host parameters**. All new host controls use AU
version hint **16**:

| ID | Core index | Host index |
| --- | ---: | ---: |
| `body1_model` | 190 | 296 |
| `body1_mix` | 191 | 297 |
| `body1_brightness` | 192 | 298 |
| `body1_decay` | 193 | 299 |
| `body2_model` | 194 | 300 |
| `body2_mix` | 195 | 301 |
| `body2_brightness` | 196 | 302 |
| `body2_decay` | 197 | 303 |

Both A/B slots, user-preset files, session state, factory recall and MIDI Program
Change include the new fields. The program mapping remains **host 0–221 / MIDI
bank MSB 1** for the original collection and **host 222–522 / bank MSB 0** for the
later collection. Bank LSB selects a 128-entry page; Program Change selects its
entry. These are zero-based protocol values.
[Host compatibility tests](../tests/InstrumentBodyHostTests.h) ·
[Parameter identity tests](../tests/ParameterStateChildIndexTests.cpp) ·
[Program contract](../src/core/FactoryPresets.h)

This revision does not reverse the earlier retirement of Legacy/Pure engine
policies. Their existing schema-4 migration remains documented in the
[2026.11.2 report](CIRCUIT_LIBRARY_2026_11_2.md#saved-sessions-and-host-automation).
That earlier migration and the new optional body defaults are separate matters.

## Evidence and practical limits

| Check | Scope |
| --- | --- |
| [Inventory comparison](../scripts/compare-factory-revisions.py) | Stable 523 ordered identities, the preserved 190-ID prefix, exactly eight appended controls, 521 changed musical programs, preserved initialization patches and no exact duplicates after excluding volume. |
| [Instrument model tests](../tests/InstrumentBodyTests.cpp) | All 19 active models across registers, velocities and sample rates; spectral distinctions and decay, brightness, tuning, Nyquist rejection, long holds, resets and malformed numeric inputs. |
| [Engine integration tests](../tests/InstrumentBodyEngineTests.cpp) | Scalar/batched and block-partition equality, sounding-register initialization, source isolation, default-off behavior, next-note selection and audible-tail handoff. |
| [Voicing](../tests/PresetVoicingTests.cpp), [character](../tests/PresetCharacterTests.cpp) and [precision](../tests/PresetPrecisionTests.cpp) tests | Actual rendered articulation, velocity, pressure, transients, sustained bodies, tracked partials, stereo behavior and named tremolo. Negative controls retain the diagnosed defects and must fail the relevant criteria. |
| [Host tests](../tests/InstrumentBodyHostTests.h) | Automation identities/types, old-state defaults, session/file/A-B/undo recall, all factory and MIDI programs, and hardware-source separation. Native execution remains a platform qualification gate. |
| [Complete library audit](../tests/LibraryAuditTests.cpp) | All 523 programs: finite audio, authored ranges, phrase level, eight-voice stress headroom and the unchanged level acceptance window. Final trim/voicing acceptance belongs to the completed source run. |
| [Audition artifact tests](../scripts/test-capture-factory-auditions.py) | Roster, inventory, exact settings, unchanged WAV bytes, clean-source/executable provenance and failure boundaries. |

A separate development comparison built the previous source commit
`73dc17b8d93d027a8a7de19a9eaeb0915122c621` and the revised source independently.
Five fixed stimuli, including eight-voice controller/pedal/reuse cases and
hardware/memory switching, produced **760,200 matching floating-point audio
samples per version** with bodies off. The live-envelope event trace also
matched. This is exact evidence for those tested paths, not an exhaustive claim
about every possible input or the deliberately changed factory recipes.
[Development comparison data](preset-evidence/2026.11.3/development-default-off-comparison.json)
retains its uncommitted-development label; final source-bound audition and
native-build evidence are separate.

### CPU cost

The models use fixed arrays, bounded mode counts and arithmetic phase rotations.
A development probe intercepted **zero C++ allocations** during its note,
render and released-voice-reassignment cases. The added processing nevertheless
has a measurable cost. At **48 kHz, eight voices and Standard 2×**, five tested
programs used approximately **0.51–0.70 CPU-seconds per audio-second**, versus
**0.42–0.59** with their bodies off, on the Linux development worker. Depending
on the model, that was about **3–23% additional thread CPU time**.

The measurement used thread CPU time, 128/512-frame blocks, a short warm-up and
250 ms measured passages. Shared cache and processor-frequency variation still
apply. The sixteen-voice figures use two sequential eight-voice engines as a
proxy, excluding host bookkeeping and the final sum. HQ 8× and many of those
sixteen-voice proxy cases exceeded one CPU-second per audio-second on that
worker even with bodies off; the added layer raises that cost further.
These observations do not guarantee real-time performance on another machine.
Choose voice count and internal quality according to the available audio budget;
the model choice itself does not replace those engine settings.
[Development performance data](preset-evidence/2026.11.3/development-performance.json)
is supporting development measurement evidence, not a qualification of the
eventual release binaries.
[Fixed body storage](../src/core/InstrumentBody.h) ·
[Voice and quality controls](../src/core/Params.h)

## Auditions from the committed source

The [audition generator](../scripts/capture-factory-auditions.py) is configured
to produce **90 unnormalized 48 kHz stereo float WAVs**, covering **71 existing
programs** and **594 seconds (9 minutes 54 seconds)** in 12 folders. The roster
includes all 19 active body models and retains the previous keyboard, family,
original-address and register examples.

| Excerpts | Files |
| --- | ---: |
| All 24 later-bank keyboard programs at firm velocity | 24 |
| Soft electric/reed/stage keyboard comparisons | 5 |
| Existing instrument-family examples | 22 |
| Original-address examples | 8 |
| Seven paired zero/full-pressure programs | 14 |
| Trombone Glide, Theatre Organ, Oboe, Bassoon, Guitar I/II and Kalimba | 7 |
| Evolving Pad, Breathing Pad, Thunder, Rain, Surf and Wind | 6 |
| Soft Kalimba | 1 |
| Tine Piano at MIDI 48, 60 and 84 | 3 |

The archive includes all 523 parameter vectors, exact commands, per-folder
metrics, source commit/tree, executable hash and checksums. The script rejects
tracked source changes, a different checkout/workflow commit, or an executable
that changes during capture. It marks completion only after those checks and
all requested WAVs succeed. **Pressure Lead is intentionally silent in its
zero-pressure example.** [Capture instructions](PRESET_GUIDE.md#reproduce-an-audition-from-source)

The configured roster is not a claim that an archive has already been rendered
or published. A completed capture provides reproducible listening material; it
does not prove acoustic similarity, every chord/register/controller history,
native host validation, signing or notarization. Use the exact published build
and artifact qualification record for those platform facts.
