# Factory preset guide

The **2026.11.2 factory library** contains **523 existing programs**, all played
by the circuit engine. The original 222 addresses have been revoiced using
explicit instrument recipes; the later collection contains 300 musical programs
and **Init Circuit**, including the 24 revised keyboard programs and further
articulation, harmonic and motion refinements. No names or program addresses
have been added or removed. Instrument names describe synthesized interpretations: a violin-like
bow, a plucked-string decay or a bell's metallic color. They do not identify
samples or establish a match to an acoustic instrument or a physical CS-80.
The [2026.11.2 circuit library report](CIRCUIT_LIBRARY_2026_11_2.md) explains the
current sound changes and intentional migration of old engine settings. The
[2026.11.1 refinement report](PRESET_REFINEMENT_2026_11_1.md) and
[original revision report](PRESET_REVISION_2026_11.md) retain the earlier history.

## Start with your hands

A fresh instance opens **Brass Swell I**. Hold a three-note chord around MIDI
48–67, wait for the brighter layer to arrive, then gradually add pressure.
Release the pressure before releasing the keys to hear the chord settle back.
Open **Library** to choose the following programs.

Try each sound in three ways: strike softly and firmly; hold long enough to hear
what remains after the attack; then release and listen to its damping or tail.
For a repeatable velocity comparison, try MIDI velocities 40 and 110. These are
suggested playing values, not required settings. Some organ and harpsichord
programs intentionally keep a comparatively even strike level.

**Velocity** describes the initial strike. **Aftertouch**, or pressure, changes
a note after that strike. Polyphonic aftertouch lets you emphasize one held note;
channel aftertouch affects notes on that MIDI channel together. Polyphonic
aftertouch does not require MPE. Without a pressure keyboard, hold an on-screen
key and drag downward along it. See the [playing and performance instructions](INSTALL_AND_QUICKSTART.md#one-circuit-instrument).

For on-screen pressure, leave **KAS Scan Timing** off; if it is on, use MIDI
aftertouch instead. This optional physical setup setting is retained when you
change presets.

**Pressure Lead is intentionally silent without pressure.** Hold a key and apply
polyphonic or channel aftertouch, or drag down on the on-screen key. A harder
strike alone will not produce its sound. Pressure brings up its level and
brightness; easing off fades it again. [Lead voicings](../src/core/curated/Leads.inc)

## Find a family

Counts below cover the complete **523-program catalogue**. The **modern** tag is
a playing-technique label, so those programs appear in their musical categories.
The [original-address recipes](../src/core/OriginalCircuitLibrary.inc) and
[later collection](../src/core/CuratedLibrary.inc) define the stable order.
Use **ALL PATCHES** to browse factory and user patches, or **FACTORY LIBRARY** to
show just the factory catalogue.

| Family | Programs | Useful starting points |
| --- | ---: | --- |
| [Brass](../src/core/curated/Brass.inc) | 59 | Brass Swell I for held chords; Brass Stab for short accents. |
| [Strings](../src/core/curated/Strings.inc) | 59 | Solo Violin for melody; Chamber Strings for a smaller section. |
| [Pads](../src/core/curated/Pads.inc) | 63 | Slow Pad for overlapping chords; Sweep Pad for a long opening. |
| [Polysynth](../src/core/curated/Polysynth.inc) | 25 | Velocity Poly for touch dynamics; Poly Pluck for decaying chords. |
| [Keys](../src/core/curated/Keys.inc) | 45 | Electric Piano I for soft-to-firm playing; Analog Piano for a revoiced original address. |
| [Organ](../src/core/curated/Organ.inc) | 15 | Flute Organ 8+4 for steady chords; Percussive Organ for separated notes. |
| [Winds](../src/core/curated/Winds.inc) | 14 | Clarinet for hollow reed lines; Pan Flute for a distinct breath onset. |
| [Plucked](../src/core/curated/Plucked.inc) | 17 | Guitar I for picked phrases; Harp for ringing broken chords. |
| [Bass](../src/core/curated/Bass.inc) | 50 | Analog Bass for body and bite; Sub Bass for a plain low foundation. |
| [Leads](../src/core/curated/Leads.inc) | 56 | Touch Lead for expression; Ribbon Glide Lead for connected slides. |
| [Bells](../src/core/curated/Bells.inc) | 23 | Marimba for rounded mallets; Clock Bells for paired bell tones. |
| [FX](../src/core/curated/Fx.inc) | 46 | Wind for sustained air; Random Filter for stepped motion. |
| [Sequences](../src/core/curated/Modern.inc) | 27 | Arp Poly, Arp Bells, Arp Bass and the revoiced Arp Motion. |
| [Hardware Studies](../src/core/OriginalCircuitLibrary.inc) | 22 | Tone I: E. Piano and Tone II: Bass; panel voicings named after the original tone families. |
| [Init](../src/core/OriginalCircuitLibrary.inc) | 2 | Init Patch and Init Circuit provide starting points for programming. |

## Hear the differences

The ranges here are suggested **played MIDI notes**, not acoustic instrument
limits. This guide calls MIDI 60 C4; some hosts use a different octave label.
A *rank* means one of the two synthesizer layers sounding a note.

### Swells, bows and sustained chords

**Brass Swell I / Brass Swell II / Pressure Swell Brass:** try MIDI 48–67.
Swell I layers a rounded onset and a later bright bloom at the same register.
Swell II adds a 16′ foundation below its 8′ upper voice, making the same keys
sound broader and lower. Pressure Swell Brass starts as a quiet bed; use your
fingers to perform most of its crescendo. [Brass source](../src/core/curated/Brass.inc)

**Sforzando Brass** gives a distinct initial accent that settles into a quieter
body. **Covered Horn** keeps a steadier held body with a restrained supporting
rank. Try separated notes, then a long hold, to compare the accent and sustain.
[Brass source](../src/core/curated/Brass.inc)

**Solo Violin / Solo Cello:** begin around MIDI 60–84 for violin and 48–72 for
cello. Each uses one dry pitched rank and a short bow-friction layer, with
pressure introducing restrained vibrato. Cello's 16′ registration sounds an
octave below the played key. **Marcato Strings** has a stronger opening accent
that decays into its sustained body.
**Chamber Strings** uses closely tuned sections without chorus;
**Wide Strings** adds greater stereo breadth and chorus. Compare the same
open chord to hear the difference in ensemble size. [String source](../src/core/curated/Strings.inc)

**Slow Pad / Sweep Pad:** play open voicings around MIDI 48–72. Slow Pad lets
its brighter second layer enter behind the body. Sweep Pad needs more than four
seconds for both brightness crests to emerge. Leave a gap between phrases to
hear their releases rather than continually covering them with new chords.
[Pad source](../src/core/curated/Pads.inc)

### Strikes, plucks and breath

**Electric Piano I / Electric Piano II:** try MIDI 48–84, first softly, then
firmly. I keeps a rounded 8′ body under a brief bark. II adds a shorter 4′
tine-like edge and light chorus. Hold the key to hear the strike disappear
before the body; release early to damp the decay. **Tine Piano** places a shorter
third-harmonic strike over its 8′ body. Play MIDI 48, 60 and 84 to hear both
partials follow the key; a firm strike brings the upper partial forward.
**Reed Piano**, **Stage Keys** and **Saw Pulse Piano** emphasize different short
attacks and then decay while the key is held. These are circuit-synth voicings,
not sampled or physically modeled acoustic pianos. [Key source](../src/core/curated/Keys.inc)

**Analog Piano / Bright Keys / Slow Keys 106:** these original-address programs
now draw on specific keyboard recipes. Try the same soft/firm comparison around
MIDI 48–84. A name such as *Slow Keys* lengthens the decay while retaining a
struck onset; it no longer randomly assigns a pad-like swell or portamento.
[Original-address recipes](../src/core/OriginalCircuitLibrary.inc)

**Guitar I / Guitar II / Nylon Pluck / Steel Pluck:** start around MIDI 48–72.
I gives a warm body and short pick; II is brighter and rings longer. Nylon Pluck
has a mellow finger transient and a centered, unchorused body. Steel Pluck
adds a faster-fading octave course. **Harp** pairs a long 8′ body with a shorter
4′ glint: spread the notes of a chord and vary key release for damping.
The revised Nylon Pluck brings more of its filtered string harmonics into the
body. **Mandolin** keeps its paired-course decay with gentler amplitude motion;
try repeated picked notes rather than relying on a deep automatic flutter.
[Plucked source](../src/core/curated/Plucked.inc)

**Flute / Pan Flute:** try MIDI 60–84. Flute keeps a separate air layer during
the held note; pressure can bring it forward. Pan Flute's short breath attack
dies away while the pitch continues. Similarly, **Percussive Organ** has an
8′ sustained body and a disappearing 4′ strike, whereas **Flute Organ 8+4**
keeps both stops sounding. Try the organs around MIDI 48–72 and release between
notes to hear fresh percussion. [Wind source](../src/core/curated/Winds.inc) ·
[Organ source](../src/core/curated/Organ.inc)

**Recorder / Clarinet:** play separated notes around MIDI 48–72 to expose their
short tongue/breath onsets, then hold to hear the pitched body take over. Their
revised onset layers are more present while the sustained spectra remain
different. [Wind source](../src/core/curated/Winds.inc)

**Marimba / Xylophone / Clock Bells:** try Marimba around MIDI 48–72 and the
higher registrations around MIDI 48–67. Marimba's rounded body outlasts its
short wooden knock. Xylophone uses a brighter 4′ body and very brief 2′ strike.
Clock Bells combines 4′ and 2⅔′ ranks a fifth apart, with different decay
lengths. Play single notes first so the two ringing pitches remain easy to hear.
[Bell source](../src/core/curated/Bells.inc)

**Vibraphone:** play MIDI 48–72 and let each strike ring. Its short upper mode
now follows the fourth harmonic, two octaves above its 8′ body. The upper color
fades before the body while the existing gentle tremolo continues.
[Bell source](../src/core/curated/Bells.inc)

**Analog Bass / Sine Bass / Sub Bass:** start around MIDI 36–60. Their 16′
foundations sound an octave below the keys. Analog Bass adds a shorter upper
pulse attack; Sine Bass leaves a pure low body after its click; Sub Bass uses
one centered sine without an upper strike. Compare short repeated notes and
long holds. [Bass source](../src/core/curated/Bass.inc)

**Picked Bass / Rubber Bass:** compare a short strike with a held key around
MIDI 36–60. Picked Bass now leaves a longer decaying body behind its brief pick.
Rubber Bass falls away completely under a held key, with no permanent sustain
bed. [Bass source](../src/core/curated/Bass.inc)

### Rhythmic motion and centered leads

The named tremolo programs now apply a gentle pulse to the note's body while
retaining their existing BBD stereo color. Start without pressure, hold a note
or open chord, and hear the different pacing:

| Program | Pulse rate | Playing suggestion |
| --- | ---: | --- |
| Tremolo Organ | 3.2 Hz | Hold a chord for a steady organ body under the pulse. |
| Tremolo Pad | 3.8 Hz | Wait for both layers to settle before changing the chord. |
| Tremolo Brass | 4.6 Hz | Add pressure afterward to introduce its restrained vibrato. |
| Tremolo Keys | 4.8 Hz | Hold past the strike to hear the body pulse as it decays. |
| Tremolo Poly | 5.4 Hz | Try short chord changes and longer holds. |

[Organ](../src/core/curated/Organ.inc) · [Pads](../src/core/curated/Pads.inc) ·
[Brass](../src/core/curated/Brass.inc) · [Keys](../src/core/curated/Keys.inc) ·
[Polysynth](../src/core/curated/Polysynth.inc)

**Touch Lead / Aftertouch Vibrato Lead / Glide Lead:** their lighter supporting
ranks reduce strong level dips during a held melody. Touch Lead still responds
to pressure; Aftertouch Vibrato Lead introduces vibrato as pressure increases;
Glide Lead keeps its smooth pitch transition. **Glissando Lead** instead moves
in semitone steps, and **Detuned Lead** retains its deliberately broader beating.
[Lead source](../src/core/curated/Leads.inc)

### Play ring sweeps as phrases

In **UFO Landing**, **Gong**, **Temple Bell** and **Ring Sweep Bell**, one ring
carrier acts on the combined voices. The first held key starts its frequency
sweep; overlapping notes continue the same sweep. Release all keys and any
sustain hold before the next attack to start a fresh sweep. Each new note still
has its own rank envelopes. A restarted ring sweep also colors release tails
that are still sounding, because the carrier is shared.
[Ring trigger and mix](../src/core/Engine.cpp)

### Read the footage

Footage is a register choice, not an extra key transposition you must enter.
Relative to an 8′ rank, the [implemented ratios](../src/core/Params.h) are:

| Footage | Sound relative to the played key |
| --- | --- |
| 16′ | One octave below |
| 8′ | At the played pitch |
| 5⅓′ | A harmonic fifth above |
| 4′ | One octave above |
| 2⅔′ | One octave plus a harmonic fifth above |
| 2′ | Two octaves above |

This explains why Piccolo, at 2′, benefits from trying lower keys than its name
may suggest, and why Clock Bells' two ranks form a fifth rather than an octave.

## Programs with the modern tag

The **modern** tag identifies sounds using software effects, modulation or
arpeggiation within the same circuit instrument. In the later 301-address
collection, 24 programs carry this tag. The 18 programs in the [Modern collection source](../src/core/curated/Modern.inc)
are Delay Brass, Plate Pad, Phaser Strings, Drive Lead, Arp Poly, Arp Bells,
Arp Bass, LFO Sweep Pad, LFO Pan Keys, PWM LFO Strings, Stereo Delay Pluck,
Ambient Bells, Ensemble Plus, Space Lead, Random Arp, Drive Bass,
Phased Polysynth and Tremolo LFO Organ. Six further programs carry that tag:
**Rain Pad**, **Random Filter**, **Random Pitch Blips**, **Bubbles**, **Rain**
and **Clockwork**. [Pad source](../src/core/curated/Pads.inc) · [FX source](../src/core/curated/Fx.inc)

Hold two to four notes around MIDI 48–60 for **Arp Poly**; use sparse picked
figures around MIDI 48–72 for **Stereo Delay Pluck**. Give **LFO Sweep Pad**
at least ten seconds. For stepped random motion, hold **Rain Pad** or
**Random Filter**; their changing filter positions come from the Modern layer.

The revoiced original addresses also include programs using these features,
including **Arp Motion** and the other sequence variations. **Studio** exposes
the effects, modulation, arpeggiator and performance controls. There is no
separate engine mode that disables them, centers the authored rank pans or
ignores MIDI expression. The original six SUB functions remain the factory
default; **WAVEFORM SET** in Studio can select the independent Extended waveform
controls. [Circuit and session contract](PARITY_ISOLATION.md)

Each original sequence variation describes its own note rate, direction and
octave span. Fixed filter-motion rates and echo times are separate: changing the
arpeggiator pace does not automatically retime those effects.
[Original sequence recipes](../src/core/OriginalCircuitLibrary.inc)

## Recalling a revised sound

**2026.11.2 identifies a sound-data revision**, separate from the
application's version and saved-session format. Names, order and host program
identities remain stable while the factory voicings change. A session or user
preset restores its stored controls; reload the corresponding factory program
from **Library** to hear the revised recipe. Save a user copy to retain edits.
A delivered factory selection by program number loads the current voicing at
that address. VST3 changes sent only through the Program processing queue remain
subject to a [documented controller dependency](PRESET_REFINEMENT_2026_11_1.md#native-recall-and-host-program-qualification).
Use **Library** to select the revised sound directly; serialized-state recall
remains separate from factory selection.

The original bank now has **revised circuit voicings**. Host indexes **0–221** retain its
names and categories, while indexes **222–522** retain the later collection.
Both address ranges now use the circuit engine. MIDI bank MSB **1** still addresses
the original collection and MSB **0** the later collection; bank LSB selects
a page of 128 and Program Change selects a program on that page. These are
zero-based protocol numbers. [Program contract](../src/core/FactoryPresets.h)

**Old Legacy or Pure sessions intentionally change sound.** Schema 4 normalizes
their engine fields to the circuit engine with normal MIDI and the optional
Studio layer. Stored controls, A/B comparisons and browser Revert cannot restore
the retired engine behaviors. Old projects using those modes can therefore have
different timbre, controller response, effects or tuning. Keep the earlier binary
and a reference bounce if an existing project's exact rendering must be retained.
[Migration details](CIRCUIT_LIBRARY_2026_11_2.md#saved-sessions-and-host-automation)

## Reproduce an audition from source

The capture tool loads the actual factory data. With CMake and a C++20
toolchain, run these commands from the repository root; this build does not
require the plug-in or JUCE. [Build target](../CMakeLists.txt) ·
[Capture tool](../src/tools/preset_capture.cpp)

```sh
cmake -S . -B build-audition -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF
cmake --build build-audition --target cs80_preset_capture --config Release --parallel

./build-audition/cs80_preset_capture --library all --list
./build-audition/cs80_preset_capture --library all --inventory factory-inventory.json

./build-audition/cs80_preset_capture --library all --out audition-keys --wav \
  --preset "Electric Piano I" --preset "Electric Piano II" \
  --preset "Tine Piano" --preset "Analog Piano" \
  --note 60 --velocity 0.85 --hold 4 --tail 1

./build-audition/cs80_preset_capture --library all --out audition-pressure --wav \
  --preset "Pressure Lead" --note 67 --pressure 0.65 --hold 3 --tail 1
```

For a Windows multi-configuration build, use
`build-audition/Release/cs80_preset_capture.exe`. Repeat the keys command with
different output directories and `--velocity 0.3` / `--velocity 0.9` to compare
touch. **Run `--inventory` separately:** inventory, listing and rendering are
exclusive operations; combining them is rejected. Capture commands write `metrics.csv`; `--wav` also writes stereo
floating-point WAVs. The default rate is 48 kHz. Pressure is a fixed level in
these examples, not a performed crescendo.

`--library all` covers every address; `--library original` selects the first
222, and `--library curated` selects the later 301. The default remains `curated`
for earlier capture commands.

The [build workflow](../.github/workflows/build.yml) uses the
[audition script](../scripts/capture-factory-auditions.py) and retains the artifact
name `cs80-curated-preset-audition-<source SHA>`. The revised artifact contains
**68 WAVs covering 56 existing programs**, the complete 523-program inventory,
metrics, source commit, provenance and checksums:

- All 24 keyboard programs at firm velocity 0.85, held for four seconds with
  a one-second release excerpt.
- Five electric-piano/reed/stage comparisons at soft velocity 0.3, with the
  same duration.
- Twenty-two refined instrument examples, held for seven seconds with a two-second
  release excerpt.
- Eight revoiced original addresses, held for four seconds with a one-second
  release excerpt.
- Aftertouch Vibrato Lead, Pressure Lead and Wah Lead, each captured at pressure
  0 and 1 with a five-second hold and one-second release excerpt: six files.
- Tine Piano at MIDI 48, 60 and 84, held for three seconds with a one-second
  release excerpt.

The script preserves the factory levels and applies no normalization. All
excerpts use zero pressure except the three `pressure-full` examples.
**Pressure Lead's `pressure-zero` file is intentionally silent**; its paired
`pressure-full` file supplies the required pressure. Check `capture_status` in
`provenance.json` for `complete`, since a failed capture may also upload a
partial artifact. The source SHA
identifies the bank being heard; a short excerpt does not cover every chord,
register or long release, nor does it qualify a native plugin build.
