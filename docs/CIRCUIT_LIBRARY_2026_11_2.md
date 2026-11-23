# Circuit factory library — 2026.11.2

Revision **2026.11.2** makes the existing factory catalogue one circuit
instrument. It replaces the randomized voicings at the original 222 program
addresses, refines the 24 keyboard programs and specific articulation, harmonic
and motion problems across the later collection, and removes the selectable
Legacy and Pure engine policies. **All 523 names and host program addresses
remain available. No presets are added.**

This report describes the source changes and their test coverage. Native build,
validator and download status belongs to the exact source commit linked in the
[README download table](../README.md#downloads). Earlier binaries do not acquire
these changes when the documentation or `main` branch advances.

## Why the original addresses needed a new voicing approach

The first bank previously included generated variations that could assign
unrelated attack, glide, detune and modulation settings to instrument names.
Those broad variations made a keyboard patch behave like a sustained synth or
pad even when the name suggested a struck instrument. Revoicing only the later
301-entry collection left those original programs unchanged.

The [new original-address builder](../src/core/OriginalCircuitLibrary.inc) uses
explicit named recipes from the authored circuit collection. A musical finish
then adjusts a small related set of controls: brightness, attack, decay, release,
stability or rank position. A finish does not randomly choose an oscillator
register, introduce portamento or replace an instrument's attack with an
unrelated envelope. A *Slow Keys* variation keeps a struck onset and extends its
decay; *Wide* separates the ranks in stereo without inventing pitch instability.

All 222 original addresses are included: the 22 named tone studies, the next 22
named programs and the 178 family variations. Intentional random movement in
programs such as Random Filter remains part of those sounds; it is separate from
randomly generating factory parameter data.

The 22 original sequence variations also describe their final note rate,
direction and octave span. Their descriptions no longer inherit contradictory
pacing from a source recipe. Fixed filter cycles and echo times are described
separately from the arpeggiator's note rate.

The tone studies also give the named rank its proper role. In particular,
**Tone I: E. Piano** emphasizes its electric-piano voice, and **Tone II: Bass**
emphasizes its bass voice. The supporting rank belongs to the same instrument
recipe, avoiding the earlier unrelated opposite-rank balance. These studies are
authored panel voicings. They do not replace or certify the separately reconstructed
hardware tone-selector source matrices.
[Library entry points](../src/core/FactoryPresets.cpp) ·
[Hardware source implementation](../src/core/SourceFactoryTones.h)

[Focused voicing evidence](preset-evidence/2026.11.2/voicing-review.json) records the measurement protocols, parameter changes, source hashes and limits behind the comparisons below.

## Keyboard articulation

The [24 keyboard recipes](../src/core/curated/Keys.inc) separate a struck upper
component from the resonant body. Each has its own amplitude and filter envelope,
and the velocity balance makes the attack more apparent on an appropriate firm
strike. Key release damps the sound instead of leaving a generic sustained bed.
Pitch variation and chorus are chosen for the named sound rather than applied
indiscriminately.

| Existing programs | Voicing intent in this revision |
| --- | --- |
| Electric Piano I | A rounded 8′ body with a more present filtered strike; the upper bark fades before the body. |
| Electric Piano II | A warm 8′ body and shorter 4′ edge, with restrained chorus and detune. |
| Tine Piano | An 8′ body plus a velocity-sensitive third harmonic from the 2⅔′ rank. Both track the played key; a fixed-frequency ring carrier no longer defines the strike. |
| Reed Piano, Stage Keys, Saw Pulse Piano, Velocity Keys | More distinct short attacks, touch-dependent brightness and decaying held notes. |
| Clavichord I, Clavichord II, Funky Clavichord | Dry pulse-based string bodies, brief tangent-like or resonant attacks, and prompt damping. |
| Harpsichord I, Harpsichord II, Lute Harpsichord | Deliberate plucked registrations with comparatively even strike levels; the upper course or quill-like edge fades independently. |
| Toy Piano, Celesta | Separate high-register body and strike partials that follow the keyboard. |
| Vibes Keys | A long 8′ body, shorter fourth-harmonic strike and gentle amplitude movement. |
| Honky Keys | Deliberate, controlled detuning between two decaying 8′ ranks. |

Soft Keys, Chorus Keys, Dulcimer, Glass Keys, Contour Keys and Pressure Keys retain
their separate registrations and performance roles, with individually authored
attack, decay and damping settings in the same source file. Tremolo Keys now
applies its 4.8 Hz pulse to the decaying body as well as retaining its BBD color.
The original-address keyboard programs inherit the corresponding revised
recipes, so Analog Piano and the older named Keys variations benefit too.

These are subtractive circuit-synth interpretations. The engine does not contain
piano samples, hammer/string/soundboard models or a physically modeled tine and
pickup. Stronger attack harmonics, tracked partials and sensible envelopes improve
the intended articulation; they do not establish an exact acoustic-instrument
match. No calibrated physical CS-80 comparison is claimed.

## Measured family refinements

The following comparisons identify specific sound-design problems and the
resulting changes. They use unnormalized 48 kHz circuit renders at **MIDI 60,
velocity 0.85 and zero pressure**, unless a row says otherwise. “Before” means
the recipe immediately before its focused edit during this revision. These
are development measurements of the recipes, separate from the final library
volume trims and the native binaries' qualification results.

| Existing programs | Change and measured result |
| --- | --- |
| [Sforzando Brass](../src/core/curated/Brass.inc) | The upper accent now decays fully into a lower sustained body. Attack-to-held contrast rises from **4.54 to 5.41 dB**; the later note is less dominated by rank beating. |
| [Covered Horn](../src/core/curated/Brass.inc) | A lighter, more stable supporting rank reduces held-level swing from **6.80 to 2.10 dB**, keeping the covered body centered. |
| [Solo Violin / Solo Cello](../src/core/curated/Strings.inc) | A short filtered bow-friction layer replaces an unused second-rank role, while continuous body hiss is reduced. The pitched held body changes by **less than 0.08 dB** in the comparison grid; the new friction layer fades rapidly after onset. |
| [Marcato Strings](../src/core/curated/Strings.inc) | A fully decaying upper accent and lower body sustain increase attack-to-held contrast from **1.64 to 3.58 dB**. |
| [Recorder / Clarinet](../src/core/curated/Winds.inc) | Short tongue/breath onsets are brought forward without turning the held body into a noise layer. Weighted onset-detail/body ratios change from **−48.10 to −27.10 dB** for Recorder and **−46.92 to −29.64 dB** for Clarinet. Their different pitched spectra remain. |
| [Rubber Bass](../src/core/curated/Bass.inc) | Removing the permanent sustain floor lets a held pluck decay completely. In the late 3.5–3.9 second window, the old body was **20.38 dB below its attack**; the revised body is **more than 120 dB below** it. |
| [Picked Bass](../src/core/curated/Bass.inc) | A longer body decay preserves the short pick while giving the note more substance afterward. Body-to-attack contrast improves by **7.31 dB**; the body still decays instead of becoming a sustained bass pad. |
| [Nylon Pluck](../src/core/curated/Plucked.inc) | More filtered string content and less sine masking raise upper-harmonic power relative to the fundamental from **−15.21 to −8.08 dB**, while retaining the finger transient and decay. |
| [Mandolin](../src/core/curated/Plucked.inc) | A shallow sine amplitude motion replaces the prominent 12 Hz square flutter. Measured fast-envelope variation falls from **0.958 to 0.115 dB RMS**, while the paired courses keep their separate decays. |
| [Vibraphone](../src/core/curated/Bells.inc) | The upper rank now follows the **fourth harmonic**, two octaves above the 8′ body, instead of the octave. Fourth-harmonic level relative to the fundamental rises from **−65.76 to −25.47 dB**. Across MIDI 48/60/72, the measured upper/body ratio is **4.0012–4.0013**; the late body remains unchanged. |
| [Touch Lead / Aftertouch Vibrato Lead / Glide Lead](../src/core/curated/Leads.inc) | Smaller supporting-rank balances reduce excessive held-level dips. C4 swing falls from **6.62 to 4.09 dB**, **4.33 to 2.57 dB** and **5.60 to 3.11 dB**, respectively. The presets retain their pressure and glide controls. |

Attack contrast uses 20–80 ms against a late held window: 2.5–2.8 seconds for
Sforzando Brass and 2.5–2.9 seconds for Marcato Strings. Wind onset ratios compare
the separately rendered, mix-weighted detail and body during 4–20 ms. Picked
Bass compares its 0.8–1.1 second body with its 20–80 ms attack. Nylon Pluck sums
harmonics 2–24 relative to the fundamental during 0.1–0.3 seconds; Vibraphone's
harmonic measurement uses 0.08–0.68 seconds. Mandolin's result is the 8–16 Hz
component of its log-RMS envelope during 0.2–2.2 seconds. Lead swing is the
range of 100 ms stereo RMS windows during 1.5–3.9 seconds. These measures
describe articulation and spectrum; they are not acoustic-similarity scores.

### Tremolo that reaches the note's body

The BBD tremolo modulates its filtered wet path. In several named presets, the
dry body dominated, leaving little rhythmic level movement. The revised recipes
retain that BBD color and add gentle original SUB-to-VCA modulation. Pitch and
filter modulation remain independently authored.
[BBD implementation](../src/core/BbdChorus.h) ·
[Organ](../src/core/curated/Organ.inc) · [Pads](../src/core/curated/Pads.inc) ·
[Polysynth](../src/core/curated/Polysynth.inc) ·
[Brass](../src/core/curated/Brass.inc) · [Keys](../src/core/curated/Keys.inc)

| Existing preset | Pulse rate | Measured C4 level movement, before → after |
| --- | ---: | ---: |
| Tremolo Organ | 3.2 Hz | **0.14 → 2.39 dB** held-window range |
| Tremolo Pad | 3.8 Hz | **0.11 → 2.92 dB** fitted peak-to-peak pulse |
| Tremolo Brass | 4.6 Hz | **0.094 → 2.426 dB** fitted peak-to-peak pulse |
| Tremolo Keys | 4.8 Hz | **0.004 → 2.372 dB** fitted peak-to-peak pulse |
| Tremolo Poly | 5.4 Hz | **0.111 → 2.367 dB** fitted peak-to-peak pulse |

The fitted measurements use 20 ms stereo RMS windows, separating the named
frequency from a slow envelope trend. Pad, Brass and Poly use 2–6.8 seconds;
Keys uses 0.4–3.2 seconds while its body is still decaying. The Organ comparison
uses the range of 50 ms windows during 0.8–2.8 seconds. Tremolo Brass retains
its existing pressure-only pitch depth; that pressure vibrato now shares the
4.6 Hz SUB rate. All of the figures above use zero pressure; pressing harder
can add that vibrato.

The review also preserved deliberate roles that should differ: Pressure Lead
remains silent without pressure, Glissando Lead retains semitone steps, Detuned
Lead retains its intended beating, and the FX collection keeps its named random,
robotic and metallic effects. Existing Modern programs retain their authored
routes and effects. A review does not require changing a recipe whose measured
behavior already supports its description.
[Leads](../src/core/curated/Leads.inc) · [FX](../src/core/curated/Fx.inc) ·
[Modern collection](../src/core/curated/Modern.inc)

## One circuit instrument

The plugin always uses the circuit engine. The toolbar, Studio and browser no
longer offer Legacy or Pure choices. The browser's **FACTORY LIBRARY** scope
contains all 523 entries, while **ALL PATCHES** also includes user patches.

Normal MIDI, pitch bend, expression, sustain, MIDI learn and optional MPE remain
available. Studio's effects, modulation, arpeggiator, tuning and voice controls
are ordinary parts of the instrument; an old engine flag cannot mask them or
center the authored rank pans. The original six SUB functions remain the factory
default. The separate **WAVEFORM SET** control retains its Original/Extended
selection.
[Browser](../src/plugin/PresetBrowser.cpp) ·
[Toolbar](../src/plugin/StudioToolbar.cpp) ·
[Circuit contract](PARITY_ISOLATION.md)

## Saved sessions and host automation

This revision intentionally changes the playback of old Legacy or Pure projects.
Their patch controls remain stored, subject to the normal state validation, but
the former engine policies are no longer available. An old Legacy session uses
the circuit models; an old Pure session can apply previously masked effects,
modulation, tuning or other saved additions. **State compatibility does not mean
identical old audio.** Keep the older binary and a reference bounce when that
audio must be retained.

Schema **4** records the fixed policy. Both old parameter identities retain their
host positions and Boolean types, preventing existing automation lanes from
being rebound to unrelated controls:

| ID | Host index | AU version hint | Effective value |
| --- | ---: | ---: | ---: |
| `parity_mode` | 243 | 6 | 0 |
| `source_performance` | 244 | 7 | 1 |

They are non-automatable reserved fields. Numeric and text writes are inert,
including obsolete values restored from older state. Factory recall, user
preset recall, both A/B slots and browser Revert normalize them to the same
policy. Such writes must not terminate held notes, clear controller ownership,
disable MPE or flatten sample-accurate automation. All factory Program Changes
stay on the same engine and retain attached automation curves.
[Fixed parameters](../src/plugin/CircuitPolicyParameter.h) ·
[State import](../src/plugin/PluginProcessor.cpp) ·
[Program recall](../src/plugin/PluginProcessorLibrary.cpp)

Factory recipes and serialized patch controls are separate. Opening a session
does not silently replace its stored controls with a new factory recipe.
Explicitly select the factory program again to hear the current voicing at its
address, and save a user copy to keep an edited patch.

## Program addresses

The two historical address ranges remain for host and MIDI continuity. They
identify collections, not different engines or browser modes.

| Existing collection | Host indexes | MIDI bank MSB | MIDI addressing within the collection |
| --- | --- | ---: | --- |
| Original 222 programs | 0–221 | 1 | Bank LSB selects a page of 128; Program Change selects its entry. |
| Later 301 programs | 222–522 | 0 | Bank LSB selects a page of 128; Program Change selects its entry. |

These are zero-based protocol values. Invalid pages or unassigned entries remain
unassigned. The [program contract](../src/core/FactoryPresets.h) defines the
mapping. The earlier [native Program-queue qualification](PRESET_REFINEMENT_2026_11_1.md#native-recall-and-host-program-qualification)
records a VST3 controller dependency; it is not a claim that every host program
transport is qualified by this revision. **Library** selection directly loads
the chosen factory recipe.

## Levels, captures and regression coverage

Measured level trims are separate from the authored instrument recipes. The
[original trim table](../src/core/OriginalCircuitLevels.inc) and
[later trim table](../src/core/CuratedLibraryLevels.inc) let the audit calibrate
phrase level and chord headroom without using volume differences as the primary
distinction between sounds. The audit is run on actual rendered audio.

| Coverage | What it checks |
| --- | --- |
| [Library audit](../tests/LibraryAuditTests.cpp) | All 523 addresses, original name/category/order identity, fixed circuit policy, parameter ranges, duplicate voicings, finite audio, phrase level and eight-note stress peaks. |
| [Preset voicing tests](../tests/PresetVoicingTests.cpp) | Instrument articulation and keyboard regressions, including tracked tine partials, strike/body decay and velocity behavior. |
| [Preset precision tests](../tests/PresetPrecisionTests.cpp) | Rendered fourth-harmonic placement, named tremolo motion, bass decay and nylon-string harmonic balance, with modified-voicing negative controls that must fail the same role criteria. |
| [Circuit host tests](../tests/CircuitPolicyHostTests.h) | Fixed host fields, obsolete writes, state import, held-note continuity, A/B, Revert and program recall. |
| [Native VST3 policy boundaries](../tests/Vst3AutomationPolicyBoundaryTests.h) | Direct and learned obsolete writes during held notes, exact comparison with omitted writes, and retained sample timing. |
| [Native VST3 program boundaries](../tests/Vst3AutomationProgramBoundaryTests.h) | Automation continuing across recall in both banks at independently specified event positions. |
| [Audition artifact tests](../scripts/test-capture-factory-auditions.py) | Complete inventory, exact capture settings, provenance and checksums; failures cannot announce a complete capture. |

The [audition generator](../scripts/capture-factory-auditions.py) produces **68
unnormalized 48 kHz stereo excerpts covering 56 existing programs**. It includes
all 24 keyboard programs, five soft-strike comparisons, 22 instrument examples,
eight original addresses, three pairs of zero/full-pressure examples, and Tine
Piano in three registers. Pressure Lead is intentionally silent in its
zero-pressure example. The inventory contains all 523 parameter vectors. Each
artifact records the binary hash, source commit, commands, measurements and
completion status. The
[preset guide](PRESET_GUIDE.md#reproduce-an-audition-from-source) gives local
capture commands and the expected durations.

The final local integration passed the full 523-program library audit after two level-trim
passes, plus 188 voicing, 128 character and 255 precision assertions. The highest
measured phrase peak was −5.0 dBFS and the eight-note stress peak was −1.5 dBFS
(rounded). Native VST3 automation, fixed-policy, source-controller, Revert,
preset-refresh and both layout-image checks also passed. The
[local validation record](preset-evidence/2026.11.2/local-validation.json)
records exact scope, source hashes and the local desktop limitation. Four-platform
build and validator qualification remains tied to the published source run;
these local checks alone do not establish a qualified downloadable release.

Passing these checks establishes the tested software behavior. It does not
establish exact acoustic realism, every possible chord/controller history,
unbounded release duration, signing or native host qualification. Use the
specific download's workflow and package evidence for platform status.
