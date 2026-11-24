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
Independent review bounded the tine's four least useful electrical modes. The
standard tine caller now renders 12 electrical modes plus three mechanical
modes, while the reusable pickup helper and reed retain 16. The omitted
coefficient sum is at most 0.00009164 of the fundamental over the reviewed
operating domain. Paired body benchmarks measured an 18.2% fixed-control and
13.3% modulated-control cost reduction against the initial 16-mode tine.

Seven existing curated electric-piano recipes were then calibrated for these
pickups, reaching eight inherited original programs as well. Resting geometries
now distinguish the existing EP variants. Tine Piano's separate third-harmonic
strike is shorter and quieter, allowing the decaying magnetic response to
supply its held color. All existing preset-voicing, keyboard-body and modern
recipe tests pass with their previous assertions. This is not a claim that the
chorus in Electric Piano II has uniform gain at every register.

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
channel power without introducing stereo-sum cancellation. The new contact's effect is concentrated near onset. A separate finite-contact
bandwidth correction reduces excess upper piano energy above C4, smoothly
reaching its full authored weight at C5. Every piano note at or below C4 and
every non-piano body's output remain unchanged by this narrower correction.
Played-strike and full-strike contact states are cached at note-on; there is no
new per-sample contact calculation or normalization that removes soft strikes.

The reference review includes additional held-out notes, all ten existing piano
programs, and full-pressure chords. With consistent 15% or 20% onset detection,
all 24 measured fundamental-share errors and all 24 upper-band errors move
toward the recorded references. The raw 10% threshold has one exception caused
by a recorded A4 pp room-noise transient preceding the musical onset; its raw
result and onset sensitivity analysis are retained. These are specific spectral
comparisons, not a general acoustic-accuracy score. An earlier, broader piano
register adjustment was rejected because additional held-out notes regressed.

Room/microphone response, instrument differences and the unknown correspondence
between recorded pp/mf/ff and MIDI velocity limit direct comparisons. Numerical
differences and passing engineering checks do not establish listening preference
or exact acoustic realism.

## Existing-program layer corrections

Several sustained voices combined a fixed-phase body and a free-running circuit
at the same pitch. Their relative phases could cancel much of a repeated note's
foundation, producing large level changes despite equal playing gestures.

Ten existing organ recipes now give each same-pitch rank a coherent source,
with measured level and brightness adjustments where needed. Two original
hardware studies inherit the corrected organ programming. Footage, independent
upper ranks, tremulants, effects and playing controls are retained. Across the
reviewed registers, repeated-note level span is at most 1.042 dB, including
intentional modulation. The tests exercise actual successive gates in one
running engine, rather than resetting its phase for each render.

Ocarina's principal rank now uses its complete modeled air-column body, with
its existing program volume reduced to 0.625. The separate air rank remains.
An independent review applied pressure after every note-on and measured 180
gates across three registers, two velocities and three pressure levels.
Repeated-note level span fell from 11.053–15.346 dB to 0.142–0.170 dB. Average
single-note energy stays within −0.468 to +0.553 dB, with positive velocity and
pressure response. A simultaneously started eight-note chord is 4.720 dB
quieter in held RMS; this calibration preserves repeated-note average level,
not every possible phrase's loudness.

Crystal Strings, and its inherited Glass Strings and Frozen Strings 29, use a
stronger principal bowed layer with calibrated bandwidth and gain. Their
separate two-foot upper glint rank remains exactly programmed. At the nominal
reviewed touch gesture, the three-register repeated-note span falls from
4.557–6.996 dB to 1.622–2.939 dB. All 27 reviewed touch/register conditions stay
at or below 2.939 dB. The high main layer becomes smoother: the largest measured
centroid reduction is 24.0% in a soft high Glass Strings note. This is a real
voicing tradeoff, not timbral equivalence. Existing string and preset-voicing
tests, plus a regression that fails on all nine original program/register
cases, pass with the correction.

## Family review and headroom

The review covers existing family descriptions, final parameter vectors,
inheritance and representative expressive behavior. It preserves deliberately
electronic voices instead of giving every program the same acoustic treatment.
No parameter changes are justified merely to make a review produce a diff.

Completed extended tests include 63 pads and 25 polysynths. Twelve-second held
eight-note chords at full velocity and pressure remain finite and unclipped in
the measured cases. The highest pad peak is −1.045 dBFS (Slow Pads 56); the
highest polysynth peak is −0.738 dBFS (Bright Poly). Cathedral Pad and Golden
Pads 66 retain their existing 0.650 level ceilings. These bounded tests do not
promise that every possible combination of user edits, notes and effects is
unclippable.

Fresh reviews also retain the distinct brass, bass, bell, plucked and mallet
materials where no causal defect was established. The recorded evidence
separates unchanged programming, accepted corrections and rejected experiments.
Private probes that originally sent pressure before ordinary note-on are
explicitly relabeled; final pressure qualifications send it after every new
note-on, matching the engine's existing per-voice event policy.

## Native test scheduling

All CTests remain globally exclusive by default. On Linux plugin builds only,
108 explicitly enumerated headless host-automation witnesses may share two
workers when the optional system MTS-ESP library is absent. These tests have
separate output directories and sample-indexed event schedules. GUI, persistence,
timing-sensitive and all unlisted tests remain exclusive. Test commands,
assertions, timeouts and the complete native evidence stream remain required.

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
