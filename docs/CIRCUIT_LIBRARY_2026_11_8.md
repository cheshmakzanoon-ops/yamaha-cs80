# Existing-program sound continuation — 2026.11.8

## Status

This source continues the 2026.11.7 work from `f8b650171426f2f675798aaa6d25eff7e400671b`.
The published downloads still identify the qualified 2026.11.4 source until the
new native builds, validators and installation archives complete qualification.
The bank retains 523 programs: 222 original addresses and 301 curated addresses,
including the two Init entries. All 204 parameter identities/ranges remain intact.

The cumulative earlier programming changed all 521 musical programs relative to
2026.11.4. This continuation targets remaining synthesis and voicing defects;
the following mechanism changes are not a claim that every program was retuned
again or that synthesized instruments are acoustically identical to recordings.

## Integrated mechanisms

### Piano contact and early radiation

A finite nonnegative contact pulse replaces the piano body's generic noise
burst. Harder strikes have shorter contact; the register varies the contact
duration continuously. Its cumulative impulse controls onset and the existing
partial-growth clock. The same pulse drives four quiet, positively damped
soundboard resonances whose frequencies follow the existing body-size control.
The string tuning, course geometry and held partial-loss laws remain intact.

The string bank receives an onset envelope rather than a coupled nonlinear
hammer/string solution. Four authored resonances do not represent a measured
soundboard. The implementation separates the roles discussed in
[Bank and Sujbert, DAFx-02](https://www.dafx.de/papers/DAFX02_Bank_Sujbert_piano_synthesis.pdf),
while its numerical contact and radiation profiles remain voicing choices.

### Electric-piano pickup response

Standard tine and reed bodies now distinguish magnetic flux conversion from
capacitive gap conversion. Closed-form signed Fourier coefficients provide
bandlimited harmonic banks. Resting geometry follows the existing body-size
control, while brightness, strike and decaying excursion shape harmonic balance.
The tine retains its short mechanical bending components. Kalimba retains its
separate material path. Fundamental pitch, existing parameter ranges and the
velocity-independent excitation bound remain intact.

These are compact spatial proxies, not the measured field/finite-element model
in [Pfeifle, DAFx-17](https://www.dafx.de/paper-archive/2017/papers/DAFx17_paper_79.pdf).
The independent review is examining the useful harmonic count and runtime cost
before the final source is frozen.

### Key release and pedal ownership

Accepted piano releases smoothly increase upper-mode losses with a finite-pad
spatial weighting. Tine/reed release instead accelerates pickup excursion loss,
so the pickup response supplies the changing harmonic balance. The rank envelope
continues to control amplitude; its fundamental loss and phase are preserved.
CC64-held notes keep their held response until the engine's existing pedal
ownership policy releases them. Live release, key-release and sustain timing
retime future damping without restarting the contact or restoring lost energy.

The added piano damper contribution fades out smoothly in the highest register;
the authored software amplitude release remains available there. Non-keyboard
bodies, disabled bodies and Kalimba keep their existing release behavior.

## Initial verification and independent review

The starting 523-program audit passed: maximum musical phrase peak -6.2 dBFS,
maximum eight-voice stress peak -1.5 dBFS, and loudest-window RMS -29.9 to -18.0
dBFS. All 523 nominal 48 kHz captures were finite and below full scale. Two
pressure-required Leads are intentionally silent at zero pressure.

The integrated contact tests and existing piano tuning/decay/rate/body tests
passed. The combined pickup/damper/body/articulation CTests also passed.
Independent release probes cover both MPE zones, overlapping manager/member
pedals, CC121, CC123, CC120, outgoing-voice handoff and live timing. These are
scoped local results; current-source full-bank and native acceptance remain
separate gates.

Recorded-reference review uses nine public piano recordings from the
[University of Iowa Electronic Music Studios](https://theremin.music.uiowa.edu/mis.html).
Their original files are analysis references, not shipped instrument samples.
The review verified their hashes, aligned the actual musical onsets and measured
channel power without introducing stereo-sum cancellation. The new contact's
effect is concentrated near onset; it does not repair the stronger preexisting
register-dependent spectral mismatch. That mismatch is being investigated in
a separate piano-only correction with additional validation notes.

Room/microphone response, instrument differences and the unknown correspondence
between recorded pp/mf/ff and MIDI velocity limit direct comparisons. Numerical
differences and passing engineering checks do not establish listening preference
or exact acoustic realism.

## Catalogue and session compatibility

The curated catalogue guard now requires exactly 301 programs and freezes its
names/categories/order, complementing the existing 222-program original digest.
Sound values can change without inserting or readdressing a program. Existing
projects retain their saved parameter vectors; use the factory browser's
LOAD / RELOAD action to load revised factory programming. The body renderer is
shared by saved sessions, so exact earlier rendered audio requires the earlier
binary or an audio render of that session.

## Remaining acceptance

Finish family programming and the independent reviews, bind final source and
inventory identities, check complete-bank levels and selected expressive/chord
phrases, and qualify all four native installation targets and their validator
lanes. Download links will identify the actual accepted artifact IDs and source.
