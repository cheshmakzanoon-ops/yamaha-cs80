# Circuit preset guide

The **2026.11.1 Circuit library** contains 300 revoiced musical programs and
**Init Circuit**. The redesign gives sounds distinct attacks, sustained bodies,
releases and touch responses. Instrument names describe synthesized interpretations: a violin-like
bow, a plucked-string decay or a bell's metallic color. They do not identify
samples or establish a match to an acoustic instrument or a physical CS-80.
The [2026.11.1 refinement report](PRESET_REFINEMENT_2026_11_1.md) documents the latest
12-program detail and stereo improvements; the [original revision report](PRESET_REVISION_2026_11.md)
documents the full-bank redesign.

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
key and drag downward along it. See the [playing and mode instructions](INSTALL_AND_QUICKSTART.md#choose-the-mode-before-playing).

**Pressure Lead is intentionally silent without pressure.** Hold a key and apply
polyphonic or channel aftertouch, or drag down on the on-screen key. A harder
strike alone will not produce its sound. Pressure brings up its level and
brightness; easing off fades it again. [Lead voicings](../src/core/curated/Leads.inc)

## Find a family

Counts below include Modern-tagged programs in their normal musical categories.
They total **301**, not 301 plus a separate Modern bank. The
[library builder](../src/core/CuratedLibrary.inc) defines their order.

| Family | Programs | Useful starting points |
| --- | ---: | --- |
| [Brass](../src/core/curated/Brass.inc) | 33 | Brass Swell I for held chords; Brass Stab for short accents. |
| [Strings](../src/core/curated/Strings.inc) | 31 | Solo Violin for melody; Chamber Strings for a smaller section. |
| [Pads](../src/core/curated/Pads.inc) | 32 | Slow Pad for overlapping chords; Sweep Pad for a long opening. |
| [Polysynth](../src/core/curated/Polysynth.inc) | 25 | Velocity Poly for touch dynamics; Poly Pluck for decaying chords. |
| [Keys](../src/core/curated/Keys.inc) | 25 | Electric Piano I for soft-to-firm playing; Harpsichord II for coupled registers. |
| [Organ](../src/core/curated/Organ.inc) | 15 | Flute Organ 8+4 for steady chords; Percussive Organ for separated notes. |
| [Winds](../src/core/curated/Winds.inc) | 14 | Clarinet for hollow reed lines; Pan Flute for a distinct breath onset. |
| [Plucked](../src/core/curated/Plucked.inc) | 17 | Guitar I for picked phrases; Harp for ringing broken chords. |
| [Bass](../src/core/curated/Bass.inc) | 25 | Analog Bass for body and bite; Sub Bass for a plain low foundation. |
| [Leads](../src/core/curated/Leads.inc) | 32 | Touch Lead for expression; Ribbon Glide Lead for connected slides. |
| [Bells](../src/core/curated/Bells.inc) | 23 | Marimba for rounded mallets; Clock Bells for paired bell tones. |
| [FX](../src/core/curated/Fx.inc) | 24 | Wind for sustained air; Random Filter for stepped motion. |
| [Sequences](../src/core/curated/Modern.inc) | 4 | Arp Poly, Arp Bells, Arp Bass and Random Arp. |
| [Init](../src/core/CuratedLibrary.inc) | 1 | Init Circuit: one saw rank, open filter and no modulation. |

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

**Solo Violin / Solo Cello:** begin around MIDI 60–84 for violin and 48–72 for
cello. Each uses one dry bowed rank, with pressure introducing restrained
vibrato. Cello's 16′ registration sounds an octave below the played key.
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
before the body; release early to damp the decay. [Key source](../src/core/curated/Keys.inc)

**Guitar I / Guitar II / Nylon Pluck / Steel Pluck:** start around MIDI 48–72.
I gives a warm body and short pick; II is brighter and rings longer. Nylon Pluck
has a mellow finger transient and a centered, unchorused body. Steel Pluck
adds a faster-fading octave course. **Harp** pairs a long 8′ body with a shorter
4′ glint: spread the notes of a chord and vary key release for damping.
[Plucked source](../src/core/curated/Plucked.inc)

**Flute / Pan Flute:** try MIDI 60–84. Flute keeps a separate air layer during
the held note; pressure can bring it forward. Pan Flute's short breath attack
dies away while the pitch continues. Similarly, **Percussive Organ** has an
8′ sustained body and a disappearing 4′ strike, whereas **Flute Organ 8+4**
keeps both stops sounding. Try the organs around MIDI 48–72 and release between
notes to hear fresh percussion. [Wind source](../src/core/curated/Winds.inc) ·
[Organ source](../src/core/curated/Organ.inc)

**Marimba / Xylophone / Clock Bells:** try Marimba around MIDI 48–72 and the
higher registrations around MIDI 48–67. Marimba's rounded body outlasts its
short wooden knock. Xylophone uses a brighter 4′ body and very brief 2′ strike.
Clock Bells combines 4′ and 2⅔′ ranks a fifth apart, with different decay
lengths. Play single notes first so the two ringing pitches remain easy to hear.
[Bell source](../src/core/curated/Bells.inc)

**Analog Bass / Sine Bass / Sub Bass:** start around MIDI 36–60. Their 16′
foundations sound an octave below the keys. Analog Bass adds a shorter upper
pulse attack; Sine Bass leaves a pure low body after its click; Sub Bass uses
one centered sine without an upper strike. Compare short repeated notes and
long holds. [Bass source](../src/core/curated/Bass.inc)

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

## The 24 Modern programs

The **modern** tag identifies sounds using software effects, modulation or
arpeggiation. The 18 programs in the [Modern collection source](../src/core/curated/Modern.inc)
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

**Pure CS-80 disables these additions.** The underlying voice remains, but the
named echo, arpeggio or Modern modulation will change or disappear. Pure also
centers both rank pans: the authored stereo spread in Noir Swell Brass, Brass
Ensemble 16, Neo-Noir Pad, Arctic Pad and Synthwave Chords is available in normal
Circuit mode, and is deliberately removed in Pure. Pure also
ignores MIDI pitch bend and the modulation wheel; pressure and ribbon remain
available. Factory recall preserves the current Pure setting. Select the mode
before playing, because a mode change clears sounding notes and effect history.
[Mode contract](PARITY_ISOLATION.md)

## Recalling a revised sound

**2026.11 and 2026.11.1 identify sound-data revisions**, separate from the
application's version and saved-session format. The redesign changes factory voicings while retaining
their names, order and host program identities. Existing sessions keep their
serialized sound: reload the corresponding Circuit factory program to hear
its revised voicing. Save a user copy if you want to retain an edited version.
If a host restores only a program number, or sends Program Change after restoring
the saved state, that selection loads the current factory voicing at that number.

The **222-program Legacy library is frozen**: its original values and order
remain intact. Host indexes 0–221 retain those Legacy identities; Circuit
programs follow from index 222. Legacy recall selects the older engine policy,
subject to the current Pure setting. The preset redesign does not redefine the
DSP algorithms or engine policies. [Program contract](../src/core/FactoryPresets.h) ·
[Revision report](PRESET_REVISION_2026_11.md)

## Reproduce an audition from source

The capture tool loads the actual Circuit factory bank. With CMake and a C++20
toolchain, run these commands from the repository root; this build does not
require the plug-in or JUCE. [Build target](../CMakeLists.txt) ·
[Capture tool](../src/tools/preset_capture.cpp)

```sh
cmake -S . -B build-audition -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF
cmake --build build-audition --target cs80_preset_capture --config Release --parallel

./build-audition/cs80_preset_capture --list
./build-audition/cs80_preset_capture --inventory circuit-inventory.json

./build-audition/cs80_preset_capture --out audition-keys --wav \
  --preset "Electric Piano I" --preset "Electric Piano II" \
  --note 60 --velocity 0.8 --hold 3 --tail 2

./build-audition/cs80_preset_capture --out audition-pressure --wav \
  --preset "Pressure Lead" --note 67 --pressure 0.65 --hold 3 --tail 1
```

For a Windows multi-configuration build, use
`build-audition/Release/cs80_preset_capture.exe`. Repeat the keys command with
different output directories and `--velocity 0.3` / `--velocity 0.9` to compare
touch. **Run `--inventory` separately:** inventory, listing and rendering are
exclusive operations; combining them is rejected. Capture commands write `metrics.csv`; `--wav` also writes stereo
floating-point WAVs. The default rate is 48 kHz. Pressure is a fixed level in
these examples, not a performed crescendo.

The [build workflow](../.github/workflows/build.yml) defines an audition artifact
named `cs80-curated-preset-audition-<source SHA>`. A completed capture contains
**24 WAVs**, the full inventory, metrics, source commit, provenance and checksums.
The original 12 examples remain, alongside the refined Flute, Breathy Flute,
Harp, Kalimba, Sine Bass, Upright Pluck, Noir Swell Brass, Brass Ensemble 16,
Neo-Noir Pad, Arctic Pad, Synthwave Chords and Ether Pad. Most excerpts hold one
note for three seconds with a one-second release excerpt. The seven examples
under `slow/` hold for five seconds with a two-second release excerpt, allowing
the pad and ensemble layers to develop. The files retain their factory levels;
no normalization is applied. Pressure Lead uses pressure 0.65; the other
examples use zero pressure. Check `capture_status` in `provenance.json`
for `complete`, since a failed capture may also upload a partial artifact.
The source SHA identifies the bank being heard; a short excerpt does not cover
every chord, register or long release. Consult the revision report for the
recorded verification results and build availability.
