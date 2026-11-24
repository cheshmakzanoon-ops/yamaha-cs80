# Factory voicing continuation — 2026.11.6

Revision **2026.11.6** retains the full-bank 2026.11.5 programming and addresses
remaining problems in piano tuning, moving wind/brass tone color, and nine
existing percussion and plucked voices. The catalogue remains **523 programs**:
301 in the later collection and 222 at the original addresses. The cumulative
work revises **all 521 musical programs** relative to the published 2026.11.4
bank; **Init Patch** and **Init Circuit** retain their initialization settings.
Names, categories, order and program addresses remain stable. No programs are
added.

This continuation starts from
`c76ad288e02fbeca48f39652bf4fcdd99837f793`, the unfinished 2026.11.5 candidate.
The currently published downloads still identify 2026.11.4. Local results below
are scoped implementation evidence; native builds, host tests, validators,
package integrity and the actual audition archive must qualify their own
committed source before the download table changes. The
[current download table](../README.md#downloads) is the authoritative delivery
record. Earlier evidence retains its original source identity.

These are authored synthesized instruments. The changes target identifiable
faults and missing distinctions; neither spectral tests nor a preset's name
establish that it is indistinguishable from a recorded instrument or a physical
CS-80. No blind listening-panel result is claimed.

## Cumulative full-bank improvements

The [2026.11.5 report](CIRCUIT_LIBRARY_2026_11_5.md) records the retained work:
stronger soft-to-firm response, independent partial decay and aftersound,
register-dependent damping, more specific keyboard, bowed, wind and bass
programming, distinct xylophone/steelpan variants, voiced choir resonances, and
individualized circuit, motion and sequence programs. Its later pad-headroom
correction is also retained. Deliberately electronic ring, PWM, fifth and
motion programs keep those identities; organ and quill-keyboard touch behavior
remains appropriate to their intended mechanisms.

Revision 2026.11.6 builds on that complete bank. It does not present the nine
new material selections as the entire scope of the user's preset revision.

## Stable piano strings with varied strikes

Previously, each repeated piano note generated a new detuning for each partial
of its secondary course, alternating above and below the main string. That
changed late tuning with the strike and prevented the partials from sharing a
coherent course offset. The secondary course now has one stable authored
**+1.15-cent** displacement, introduced by the existing smooth bass transition.
Hammer phase and contact remain variable. Existing stiffness, partial losses,
register curves and course strength remain in place.

The direct rendered geometry test passes **212 checks**, while the same test
rejects the preceding source with **75 failures**. Maximum measured
strike-seed frequency spread falls from **0.56449 cents to 0.000028 cents**.
The minimum attack waveform difference between seeds remains **5.31% relative
RMS** in the tested cases. This demonstrates stable string tuning with changing
excitation, not a complete acoustic piano: sympathetic string coupling and a
measured instrument fit are outside this change.

[Implementation](../src/core/InstrumentBody.h) ·
[Geometry test](../tests/PianoCourseGeometryTests.cpp) ·
[Scoped baseline/candidate measurements and sources](preset-evidence/2026.11.6/reviews/a05.md)

## Wind and brass color during pitch changes

Flute, clarinet, oboe, bassoon and brass now use the existing bounded response
correction during live pitch changes. Their harmonics move through resonances
that remain fixed in Hz rather than carrying the complete onset spectrum to a
new pitch. Returning to the accepted note restores its original response. The
note keeps its original excitation and onset profile.

The rendered regression compares moving-pitch notes with independently started
destination notes at **44.1, 48 and 96 kHz**, three body sizes and downward/upward
intervals. The integrated correction passes **957 checks**; the preceding source
fails **549** of the same checks. Worst normalized harmonic error is
**0.202533 dB**, against the test's **0.23 dB** bound, compared with
**14.025267 dB** before correction. These are comparisons between synthesized
gestures. The numerical body profiles remain authored choices.

The change uses existing storage and control-rate processing, with no new
allocation or per-sample transcendental calculation. This is not a complete
plugin/DAW performance benchmark.

[Regression](../tests/WindBodyFormantTests.cpp) ·
[Measured results and acoustics references](preset-evidence/2026.11.6/reviews/root-wind.md)

## Nine existing voices with distinct materials

A separate Material selector specializes the existing Model without changing
its saved range. Each of these existing programs uses its compatible material
in both ranks. Main body and short contact now share the intended resonator;
unrelated clavichord, celesta, oscillator or fixed ring accents have been
removed from the affected recipes.

| Existing program | New material behavior | Registration and playing consequence |
| --- | --- | --- |
| Tubular Bells | Twelve tube modes, weak lowest modes and a bright octave nominal suggest a virtual strike pitch. | Both ranks use 8′; the contact shares the tube tuning. |
| Gong | Twenty irregular plate modes combine an early low thud with delayed upper development. | Both ranks use 16′; leave space for the bloom and long aftersound. |
| Temple Bell | Twelve shell modes include stable close pairs with slowly beating tails. | Both ranks use 16′; its aftersound differs from the gong's developing upper energy. |
| Agogo | Eight compact shell modes, hard contact and faster material losses. | Both ranks use 4′ for a short high shell strike. |
| Music Box | Five sparse comb-tooth modes replace the celesta bar spacing. | Both ranks use 4′; the brief contact clears before the simple ringing tail. |
| Kalimba | Four supported-tine modes replace electric-piano pickup harmonics. | Both ranks now use 8′; previously the primary was 4′ and the secondary 2′. |
| Banjo | Membrane and bridge resonances, near-bridge pluck notches and rapid upper losses. | Both ranks use 8′; repeated picked notes clear quickly. |
| Koto | A sharp plectrum spectrum, low body/air clusters and a broader wooden response. | Both ranks use 8′; restrained initial settling and live bends retain body color. |
| Harp | Broad finger contact, pluck-position notches and longer lower aftersound. | Both ranks now share 8′ pitch; the previous upper octave glint is removed. |

The modal profiles occupy existing fixed storage. The new Gong uses a bounded
linear approximation with delayed mode entrances; it does not solve nonlinear
mode coupling. Tube frequency ratios draw on published measurements, while
amplitudes and losses remain authored. Banjo, Koto and Harp use literature to
inform their mechanisms without fitting a particular recorded instrument. The
compatible **Steel** model ID for Koto is a state-contract choice, not a claim
about its physical string material.

[Modal sources, implementation and direct checks](preset-evidence/2026.11.6/reviews/a07.md) ·
[Plucked material sources and checks](preset-evidence/2026.11.6/reviews/a09.md) ·
[Bell recipes](../src/core/curated/Bells.inc) ·
[Plucked recipes](../src/core/curated/Plucked.inc)

### What the local tests establish

The modal material regression passes **794 checks**, including partial ratios,
temporal development, touch, rates and neutral unsupported selections. In the
measured registers, the gong's upper/body contrast develops **23.57–24.54 dB**
between early and later windows. That measures an authored bloom, not a real
gong match. The previous variant suite still passes **1,132 checks**.

The direct plucked-material suite passes **666 checks**. It compares normalized
harmonic shape so changing gain alone cannot satisfy the material distinction.
Generic Nylon, Steel and Bass streams remain sample-identical to the previous
source in **108 separately captured comparisons**. The new plucked materials
also use fixed-frequency body responses during bends; disabling that correction
produces **27 failures** in its negative control.

The four integrated Harp/Koto/Banjo/Kalimba recipes pass **124 full-Engine
checks**, the existing **149-check** bass/plucked suite, and their unchanged
bank phrase, stress and audibility gates. Across MIDI 48, 60 and 72, their
soft-to-firm differences are **12.59–13.58 dB**, **12.84–13.88 dB**,
**12.70–13.80 dB** and **8.83–9.87 dB**, respectively. Thirty-six paired
baseline/candidate renders are finite. Kalimba's large baseline spectral change
includes its deliberate octave correction; a separate material-only comparison
isolates its resonator at the same pitch.

[Integrated plucked measurements and preserved original limits](preset-evidence/2026.11.6/reviews/a11.md)

These scoped results do not replace the complete-bank audit or native
qualification. Bell recipe measurements and complete integration results are
tracked against their exact candidate inputs under
[preset-evidence/2026.11.6](preset-evidence/2026.11.6/).

## Saved sessions and automation

The two new parameters, `body1_material` and `body2_material`, append at core
indexes **202/203** and host indexes **308/309**. Totals become **204 core
parameters** and **310 host positions**, with **14 body controls**. Each Material
has ten choices: Standard plus nine profiles. Standard is neutral, and an
incompatible model/material pair uses Standard. Material selection is captured
on the next accepted note; it does not retune a body already ringing.

All **202 preceding core parameter definitions** and **308 host positions**
remain in their original order. An independent metadata comparison includes
IDs, names, ranges, units, defaults, skews and 257 normalization probes per
parameter. Model and Variant retain their previous automation ranges. The
socket-free state-index suite passes **80 fixtures** with the expanded IDs;
full processor/GUI recall remains a native qualification gate.

Old states without Material fields load both as Standard. Missing Variant fields
also use Standard, and missing Resonant size fields use 1×. Full session,
user-preset, A/B and factory recall include the new controls. Keeping an old
state's control values does not freeze its audio: the piano tuning and live
wind/brass fixes affect existing body-enabled notes independently of the new
material defaults. Save a reference bounce and retain the earlier binary when
an arrangement's exact previous rendering must be preserved.

[Parameter/state contract and scoped results](preset-evidence/2026.11.6/reviews/a08.md) ·
[Control definitions](../src/core/Params.h)

## Selecting the revised sound

Open **Library → FACTORY LIBRARY**, select the program and press **LOAD** or
**RELOAD**, then play fresh notes. An older project can display a factory name
while retaining its earlier stored recipe. Browser **REVERT** restores the
sound active when the browser opened; it does not mean “latest factory recipe.”

Hardware tone tabs retain the original electrical CS-80 tone recipes. For
example, the panel **E. PIANO** tab and the enhanced browser **Tone I: E. Piano**
are distinct selections. Tab tooltips point to the corresponding revised
browser programs. Bodies remain available on PANEL sources and suppressed on
hardware-tone and memory paths. This preserves one circuit instrument with
stable original source behavior.

[Playing guide](PRESET_GUIDE.md) ·
[Installation and build identification](INSTALL_AND_QUICKSTART.md)

## Audition and release evidence

The configured audition roster preserves all prior **171 requests** and adds
**54** matched material examples: the nine affected voices at MIDI **48/60/72**
and velocities **0.25/0.85**, with zero pressure, a 3.5-second hold and one-second
release. The resulting configuration is **225 WAVs**, **89 existing programs**,
**46 folders**, and **1,276.2 seconds** of requested audio. Exports remain
48 kHz stereo float with factory levels preserved and no normalization.

The helper's **20 tests** pass, including preservation of all 204 inventory
values and source/executable/checksum safeguards. This verifies the capture
configuration; it is not evidence that the actual archive has been produced.
A delivered archive must have complete capture status, the committed source and
executable hashes, the 523-program inventory, individual metrics and checksums.
[Audition roster verification](preset-evidence/2026.11.6/reviews/a13.md)

The preceding Apple Silicon AU GUI run aborted at seed **0x3c99748** with a
graphics-path assertion. The new workflow adds that exact seed as a required
independent full-GUI validation lane on both Mac architectures, while preserving
the random and previous fixed-seed lanes. A failed GUI lane still blocks
packaging. Adding a regression lane is not itself a crash fix or a successful
reproduction. New builds must pass their own full native and validator matrix
before replacing the published downloads.

[Exact-seed validation change](preset-evidence/2026.11.6/reviews/a12.md) ·
[Build workflow](../.github/workflows/build.yml) ·
[Current download record](../README.md#downloads)
