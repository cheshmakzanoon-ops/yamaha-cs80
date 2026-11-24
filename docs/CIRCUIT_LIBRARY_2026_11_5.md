# Factory voicing refinement — 2026.11.5

Local implementation checks have passed; native qualification is pending. Download links still identify
the preceding qualified build until the native matrix and archive checks pass.

This work starts from `8685e9ed39d6347b67401da7590b7e1e8e95472d` and the
2026.11.4 library. It improves existing program data and the software bodies
those programs already use. It does not add, rename, remove or reorder programs.
The compatibility target is the existing **523 programs**, including the two
unchanged initialization programs, and the complete 200-core/306-host parameter
prefix from the published version.

The sound changes below are supported by rendered Engine tests. They establish
specific dynamics, material spectra, articulation and compatibility behavior.
They do not establish perceptual equivalence to an acoustic recording or a
particular original instrument.

## Why the preceding presets needed another pass

Several real problems remained after the previous body-model work. Quiet
strikes were normalized by their smaller spectral sum, partly cancelling their
intended reduction in energy. Many recipes then added a large fixed amplifier
level, compressing touch further. Struck bodies had their own modal decay
multiplied by another steep decay to silence, which hid the lingering lower
modes. Xylophone and Steel Drum shared inappropriate standard bar profiles.
Some original pure-sine variations changed filters their signal bypassed, and
sequence articulation did not follow the overridden arpeggio gate.

The implementation addresses those mechanisms and programs the existing voices
to use them. It keeps instrumental exceptions: quill keyboards and straight
organ registrations are nearly velocity-independent; pressure-led swells do
not become hammer-like voices; deliberately synthetic ring, PWM, fifth and
motion sounds retain those identities.

## Struck excitation and aftersound

The body normalizer now uses the full-strike spectral bound for touch-sensitive
struck families. The actual soft excitation keeps its lower energy and softer
upper spectrum. The implementation does not rebuild the oscillator bank,
consume extra random draws or add work to the audio sample loop. Existing
full-strike standard profiles remain sample-identical to the immediate
pre-normalization implementation; tine and deliberately touch-independent
families preserve their prior treatment.

[Normalization implementation](../src/core/InstrumentBody.h) ·
[3,439-check direct-model regression](../tests/StruckBodyDynamicsTests.cpp) ·
[Independent baseline sensitivity and measurements](preset-evidence/2026.11.5/reviews/a07.md)

A captured articulation profile adds bounded register-dependent decay and key
damping for struck bodies, and velocity-dependent entrance timing for bow and
breath models. It follows the sounding pitch after footage, stays with the
accepted note, and is applied consistently to live envelope timing updates,
normal release and sustain cancellation. Explicit KEY RELEASE and MASTER
SUSTAIN overrides remain unscaled. Off, zero-onset-mix and policy-masked bodies
have exact neutral behavior.

[Articulation profile](../src/core/BodyArticulation.h) ·
[284-check Engine regression](../tests/BodyArticulationEngineTests.cpp) ·
[Register, lifetime and isolation evidence](preset-evidence/2026.11.5/reviews/a08.md)

Full-body keyboard main ranks retain a small held amplifier pedestal so their
own independent modal losses can produce audible aftersound. The body still
decays while the key is held. Mixed circuit ranks keep zero sustain unless the
existing voice deliberately includes a held bed. Short key release is separate
from held-body decay.

At MIDI 60, using identical 25% and 85% velocity gestures, the current
uncalibrated development captures show:

| Existing voice | Previous soft-to-firm range | Revised range | Previous late body relative to onset | Revised late body |
|---|---:|---:|---:|---:|
| Stage Keys | 5.735 dB | 9.552 dB | −37.341 dB | −28.318 dB |
| Soft Keys | 6.403 dB | 9.496 dB | −38.231 dB | −30.389 dB |
| Honky Keys | 5.767 dB | 9.375 dB | −41.626 dB | −33.843 dB |
| Velocity Keys | 9.080 dB | 10.749 dB | −41.082 dB | −30.883 dB |
| Reed Piano | 5.540 dB | 10.078 dB | −31.289 dB | −22.730 dB |
| Harpsichord I | 0.081 dB | 0.081 dB | −43.947 dB | −29.349 dB |

Range uses the complete captured RMS. Late body is the last half-second of a
three-second held note; onset is the first 200 ms. These are relative measures,
not a claim that making a preset louder makes it better. The final full-bank
level audit is a separate gate. The keyboard regression also measures idle
chorus noise separately when testing key damping, so the existing BBD noise
floor cannot be mistaken for a sustained note.

[Keyboard response test](../tests/KeyboardResponseTests.cpp) ·
[All three-register measurements](preset-evidence/2026.11.5/development/keyboard-response-comparison.json) ·
[Frozen baseline test failures](preset-evidence/2026.11.5/development/keyboard-response-baseline.log)

## Material-specific resonators

Separate saved variants preserve the published model selector's 0–19 range.
Xylophone uses the Marimba slot with a wooden-bar variant whose prominent upper
mode is near three times the fundamental. Steel
Drum uses the Vibraphone slot with an octave/twelfth shell variant. Their
contact, touch and loss profiles are distinct. The existing standard profiles
remain unchanged by the variant implementation.

The Xylophone recipe uses matched-register bar ranks rather than an unrelated
octave course. Steel Drum removes the residual pulse and fixed ring carrier.
Natural chime, tube and clock voices also avoid unrelated fixed carriers;
explicitly electronic ring voices retain them. Vibraphone now damps when its
key is released, while the existing pedal controls can retain ringing.

[Material implementation and sources](preset-evidence/2026.11.5/reviews/a10.md) ·
[Direct variant checks](../tests/InstrumentBodyVariantTests.cpp) ·
[All 23 mallet-category measurements](preset-evidence/2026.11.5/reviews/a12.md)

## Voiced choir resonances

Choir Pad and Dark Choir now use separate Ah and Oo variants of the existing
Organ source. A harmonic voiced source passes through fixed vowel resonances;
the harmonic frequencies move through those resonances during vibrato and
slides. The two variants have distinct spectral weights, a shaped entrance,
breath and pressure response. Standard organ registrations retain their
steady strike behavior.

This implementation reuses the existing 24-harmonic sustained-body arrays and
bounded summation. It adds no extra voice storage or per-sample transcendental
calculations. The direct regression covers 895 checks; 594 reference renders
covering Standard, Xylophone and Steelpan profiles stayed sample-identical to
the preceding implementation. A moving-pitch harmonic-ratio comparison differed
from independently prepared notes by at most 0.041 dB in the measured cases.
That establishes the authored resonance behavior, not human-vocal equivalence.

[Vocal implementation and measurements](preset-evidence/2026.11.5/reviews/a16.md) ·
[Direct vocal regression](../tests/VocalBodyTests.cpp)

## Instrument programming

The working family reports distinguish implemented recipe changes from their
measured results. Original-bank programs inherit the chosen revised recipes
and retain their own instrument-aware finishes and program identities.

| Family | Programming evidence |
|---|---|
| Keys | [Keyboard response comparison](preset-evidence/2026.11.5/development/keyboard-response-comparison.json) |
| Bells and mallets | [Material, touch and damper report](preset-evidence/2026.11.5/reviews/a12.md) |
| Winds and strings | [42-program breath, bow and register report](preset-evidence/2026.11.5/reviews/a13.md) |
| Brass and organ | [46-program breath, registration and keying report](preset-evidence/2026.11.5/reviews/a14.md) |
| Bass and plucked strings | [40-program touch, aftersound and damping report](preset-evidence/2026.11.5/reviews/a15.md) |
| Pads and polysynth | [54-program entrance, expression and chord-release report](preset-evidence/2026.11.5/reviews/a17.md) |
| FX | [24-program environmental contour, cadence and pressure report](preset-evidence/2026.11.5/reviews/a18.md) |
| Modern programs | [18-program effect, motion and struck-aftersound report](preset-evidence/2026.11.5/reviews/a19.md) |
| Leads | [30-program strike, pressure, interval and glide report](preset-evidence/2026.11.5/reviews/a20.md) |

Wind, Rain, Surf and Thunder no longer share a generic periodic amplitude
shape. Wind uses independent smooth random gusts; Surf separates its surge,
spray and draining foam; Thunder separates a broadband crack and an irregular
low roll. Rain's measured maximum two-millisecond modulation jump falls from
1.1204 to 0.0407 dB. Clockwork retriggers short decaying ticks, retaining its
deliberate clock rate. Named mechanical, alarm, siren and helicopter effects
keep their intentional periodic character.

Modern effect programs retain their established arpeggiator directions, rates,
octave spans and explicit LFO selections. Their dry articulation now leaves
space for the existing echoes and reverberation. The same-engine old-recipe
control isolates these changes from the new body implementation: Phaser
Strings' soft-to-firm contrast grows from 3.196 to 8.936 dB, PWM LFO Strings'
from 2.985 to 8.637 dB, and Ensemble Plus's from 2.482 to 8.159 dB in the
reported C4 gesture. The organ remains intentionally velocity-independent.

## Original finishes and sequences

Pure bypass-sine finishes now act through amplitude entrance, release and
touch, with descriptions matching those audible changes. Filtered circuit
voices receive a coherent opening-envelope contour. Inactive ranks are skipped.

All 22 numbered sequence variations scale dry attack, decay, accent and release
against their actual note spacing: the existing 58% gate, or 45% for random
mode. Bell main resonance and effect timing remain separate from the dry gate.
The original Init treatment is explicitly preserved.

[Implementation](../src/core/OriginalCircuitLibrary.inc) ·
[63-check render suite](../tests/OriginalArticulationTests.cpp) ·
[Baseline/current results](preset-evidence/2026.11.5/reviews/a11.md)

## Saved sessions and automation

The two new next-note variant parameters append at core indexes 200/201 and
host indexes 306/307. They use AU version hint 18. The 200 published core
parameter definitions and all 306 published host positions retain their
identity. Existing normalized body-model automation keeps its old meaning.
Missing variant values in older sessions, user presets and inactive A/B slots
default to Standard. A sounding note retains its captured variant; the next
accepted note uses the edited selection.

[Host migration and next-note tests](../tests/InstrumentBodyHostTests.h) ·
[Published prefix and state-index tests](../tests/ParameterStateChildIndexTests.cpp) ·
[Initial host-contract review](preset-evidence/2026.11.5/reviews/a09.md)

## Qualification status

The final structural check confirms all **523 names, categories, ordered
addresses and the old 200-parameter prefix**, all **521 musical programs with
changed existing sound controls**, two unchanged Init programs, and no duplicate
non-volume sound vectors. **272 programs use an instrument body**; 251 retain
circuit-only excitation, including the two Init programs.
[Final inventory](preset-evidence/2026.11.5/inventory.json) ·
[Coverage record](preset-evidence/2026.11.5/parameter-coverage.json) ·
[Independent exhaustive address review](preset-evidence/2026.11.5/reviews/a22.md)

The complete level audit passes all **301 curated plus 222 original** programs.
Its loudest-window RMS spans **−29.9 to −18.0 dBFS**, maximum phrase peak is
**−4.5 dBFS**, and maximum eight-note stress peak is **−1.5 dBFS** (rounded).
The first integrated audit found 17 voices below the existing −30 dBFS floor;
their corrected body, rank and playing-effort balances pass without changing
the acceptance thresholds. The initial failure log is retained. Eight affected
family/cross-bank suites pass again after the fixes; the preceding complete
18-suite integration run is retained separately.
[Final bank audit](preset-evidence/2026.11.5/development/final-library-audit.log) ·
[Initial findings](preset-evidence/2026.11.5/development/initial-integrated-library-audit-failed.log) ·
[Corrected regression results](preset-evidence/2026.11.5/development/corrected-focused-ctest.log) ·
[Wind corrections](preset-evidence/2026.11.5/reviews/a30.md) ·
[Brass/string corrections](preset-evidence/2026.11.5/reviews/a31.md) ·
[Bass/metallic-key corrections](preset-evidence/2026.11.5/reviews/a32.md)

All 523 programs have also been captured at exactly the preceding bank's
note, sample rate, velocity, pressure, hold and release conditions. The capture
executable stayed unchanged across all 40 condition groups. Those local
development renders and their metrics are separate from the clean-source
171-clip native CI audition archive and do not establish listening approval.
[Matched-condition metrics](preset-evidence/2026.11.5/development/all-program-metrics.csv) ·
[Capture conditions and executable hash](preset-evidence/2026.11.5/development/matched-capture-manifest.json)

Local implementation checks are recorded under
[preset-evidence/2026.11.5](preset-evidence/2026.11.5/). The requested 60 distinct
agent assignments are being carried out in batches of three workers; the
[work log](preset-evidence/2026.11.5/work-log.json) records actual assignments.
Native Windows x64, macOS arm64, macOS x86-64 and Linux x86-64 qualification,
validator evidence, archive hashes and final download publication are pending.
