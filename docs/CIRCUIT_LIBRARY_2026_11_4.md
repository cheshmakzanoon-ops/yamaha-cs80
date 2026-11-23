# Factory voicing refinement — 2026.11.4

This revision reprograms the **521 existing musical programs** and preserves the
two initialization programs. The library still contains **523 programs**, with
the same names, categories, tags, order, host indexes and MIDI addresses. The
work starts from `fa57967c7317828cf769cc7ca97f5857e566520f` and the 2026.11.3
library. It does not add presets or instrument-model choices.

The work addresses specific causes of the earlier generic sound: instrument
bodies bypassed the panel filters, six sustained bodies did not change their
harmonic weights with strike velocity, bass piano attacks had a fixed harmonic
ceiling, and related instruments shared the same body-resonance scale. The
revised recipes use the corrected behavior, with separately programmed attacks,
held bodies, releases, register contours and controller responses.

These remain authored synthesized interpretations. The tests establish signal
behavior, compatibility and numerical limits. They do not establish that a
piano, orchestra, wind instrument or environmental effect is indistinguishable
from a recording of a particular real source.

## Signal path and playing response

Each rank's optional instrument body now follows a separate linear high-pass
and low-pass filter using the rank's effective cutoff, resonance and switch
settings. Filter envelopes, touch and keyboard contour therefore shape its
complete spectrum. The original nonlinear circuit filters continue to process
the oscillator path. Both paths blend before the existing VCA. With the body
off, the original circuit calculations remain unchanged.

This corrects a concrete layering problem: a high-passed upper strike could
previously restore a strong low fundamental through the unfiltered body. In
the integration fixture, the revised high-pass reduces that fundamental by
**26.76 dB**; the low-pass fixture reduces its fourth partial by **15.20 dB**.
The integration suite also covers filter switches, resonance, keyboard contour,
block partitioning, hardware-source isolation and released-voice handoffs at
44.1, 48 and 96 kHz.

[Signal routing](../src/core/Engine.cpp) ·
[Body filter](../src/core/InstrumentBodyFilter.h) ·
[Measured integration checks](preset-evidence/2026.11.4/development/core/cs80_instrument_body_engine_tests.log)

### Struck strings, electric pianos and bars

Low piano notes can now use up to **42 principal partials and six coupled
components**, with a smooth register-dependent partial budget. The prompt
component gives way to a slower aftersound. Different modes develop through the
strike and lose energy at different rates; the bass hammer spectrum is no
longer a transposition of the middle-register spectrum. Principal-mode random
sequences are reserved independently of the admitted partial count, and the
coupled-course transitions use smooth register contours. Fine tuning therefore
no longer reshuffles the audible courses at those onset boundaries; separate
strike seeds still produce different attacks.

Tine and reed electric pianos use distinct harmonic profiles and changing
pickup-excursion weights. Their overtones soften as the modeled vibration
falls, without introducing an unbandlimited sample-domain distortion stage.
Plucked strings and bars also use register-dependent losses, contact bandwidth
and bounded excitation variation. Organ and harpsichord excitations retain
their deliberate velocity independence.

[Body implementation](../src/core/InstrumentBody.h) ·
[Direct-model tests](../tests/InstrumentBodyTests.cpp) ·
[Struck-body development measurements](preset-evidence/2026.11.4/development/struck-body/body_tests.log)

### Bowed strings, winds and brass

The six sustained models now change harmonic balance with playing effort and
develop their upper partials through the onset. Direct-model, equal-RMS tests
measure approximately **4.1–7.1 dB** of soft-to-firm upper-spectrum change.
Pressure changes pass through a short smoother. Restrained, seeded, slow timbre
variation changes spectral color without adding pitch wobble.

Resonant size gives related instruments different spectral shapes at the same
pitch. Bowed low body resonances scale more strongly than the upper response;
the reed and brass profiles also follow the authored size. Clarinet even/odd
balance changes smoothly with equivalent sounding register, with the larger
bass-clarinet body moving that transition downward.

The numerical formants, losses and time constants are authored choices. Size
is a voicing factor, not a measurement of an instrument's dimensions.

[Sustained-model measurements](preset-evidence/2026.11.4/development/sustained-body/body-tests.log) ·
[Unchanged-model parity and direct-model cost](preset-evidence/2026.11.4/development/sustained-body/cost-and-parity.log)

## Programming the existing library

Each of the 300 musical recipes in the later collection has individually
revised sound controls. The original bank's 221 musical programs inherit those
recipes and receive instrument-aware finish processing. All fourteen finish
styles distinguish the source instrument and the roles of its two ranks;
struck “Slow” variations retain a prompt onset. The two Init programs retain
their prior parameter values, with neutral size defaults appended.

| Family | Existing recipes | Main programming work |
|---|---:|---|
| Keys | 24 | Separate hammer/tine/quill strike and body; touch color; damping; register balance |
| Winds | 14 | Pitched tone, air and chiff separated; instrument size and register; breath releases |
| Strings | 28 | Solo/section distinction; bow onset; cello resonance; short and slow articulations |
| Brass | 32 | Tongue, bloom and release; horn/trumpet/low-brass distinctions; pressure response |
| Organ | 14 | Flute/principal/reed/electronic registration; pipe entrance; separate octave percussion |
| Plucked | 16 | Finger/pick edge, string resonance, course separation and damping |
| Bells | 22 | Metal/wood decay differences, restored modal bandwidth and short mallet components |
| Pads | 30 | Independent rank blooms and releases, clearer low register and controlled motion |
| Polysynth | 24 | Transient versus sustain balance, comping articulation, velocity and keyboard contour |
| Bass | 24 | String body and pick/finger layers; distinct pure, synthetic, fretless and plucked roles |
| Leads | 30 | Individual articulation, register balance, expression and preserved glide/trill identities |
| FX | 24 | Separate impact, rumble, surge, spray and air; less mechanical nature-effect repetition |
| Modern | 18 | Refined body/filter/effect balance with intended arpeggiator and modulation rates preserved |

The original tone-study audit found that numeric MIX settings alone did not
ensure the named row was strongest. Two string studies' supporting ranks had
been 6–7 dB stronger in the tested sustained window. Their revised balances
put the named row ahead, and all 22 tone studies pass the measured dominance
check. Wide original variations also received measured stereo corrections. Those
measurements retain their development-stage source hashes; they precede the
final piano onset and level corrections.

The final level review found ten quiet programs across seven underlying
recipes. Their rank balances were corrected without changing the existing
RMS or peak acceptance limits. Swell retains its measured pressure contrast;
Spiccato retains a detached stroke. Raising the base rank level reduces the
relative peak velocity contrast of Spiccato, Sforzando and Metallic Keys, while
their spectral touch settings remain unchanged. The exact comparisons are
retained in the [independent level review](preset-evidence/2026.11.4/reviews/40-level-gate-review.json).

[Original-bank recipes](../src/core/OriginalCircuitLibrary.inc) ·
[Original-bank measurements](preset-evidence/2026.11.4/development/originals/behavior-summary.json) ·
[Complete structural coverage](preset-evidence/2026.11.4/parameter-coverage.json)

The coverage checker compares actual binary32 values, preserves the original
198 parameter IDs, checks every ordered identity and tag, requires a changed
existing sound control for every musical program, rejects duplicate non-volume
vectors and preserves both Init vectors exactly. Parameter differences alone
are explicitly not treated as proof of audible improvement.

## Saved projects and program identities

The first **198 core parameter IDs and 304 host automation positions** are
preserved. The positions below are zero-based. `body1_size` and `body2_size` append at core positions 198/199 and
host positions 304/305. The totals are **200 core and 306 host parameters**.
The new AU parameters use version hint 17; preceding parameters retain their
published hints and positions.

Older body-aware states keep their eight body controls and default both sizes
to 1. Older states without bodies restore the off defaults. Model, partial
decay and resonant size take effect on the next accepted note. Mix and
brightness remain live. State, user presets, A/B comparison, undo and factory
selection include the appended controls.

Stored parameter values do not freeze old DSP. Existing body-enabled patches
use the revised excitation and filtering code, so their audio can change.
Reload a factory program from **Library** to hear its newly programmed recipe.
The body controls are available through the host parameter list; the custom
Studio and standalone interfaces still lack dedicated body-control widgets.

[Parameter definitions](../src/core/Params.h) ·
[Host layout](../src/plugin/PluginProcessor.cpp) ·
[Compatibility regressions](../tests/InstrumentBodyHostTests.h)

## Listening and qualification

### Continuation: strike dynamics and bowed-body motion

The continuation after `59d489d` corrects two additional signal-model defects
without adding programs, changing any factory parameter vector, extending the
model choices or moving host automation positions.

The tine model formerly normalized each strike against its own spectral sum.
As firm strikes acquired stronger upper harmonics, that division reduced the
fundamental. It now uses a fixed full-strike headroom bound. The full-strength
signal is unchanged, while softer strikes no longer receive unintended extra
gain. This affects 17 existing programs, including the electric-piano family
and the supporting tine ranks in Kalimba, Music Box and LFO Pan Keys.

In matched complete-preset measurements at MIDI 36, 60 and 84, the 300–800 ms
body RMS span from velocity 0.15 to 1 increased as follows:

| Existing program | Before correction | After correction |
|---|---:|---:|
| Electric Piano I | 8.22–9.07 dB | 12.23–13.01 dB |
| Electric Piano II | 8.28–9.97 dB | 12.30–13.86 dB |
| Tine Piano | 5.97–6.52 dB | 9.97–10.41 dB |

All 90 velocity/register captures retain monotonic peak and RMS response.
Forty-five full-preset comparisons preserve identical float-audio hashes for
hard-strike endpoints and unchanged acoustic-piano/reed controls. The new
direct regression passes 486 checks and fails 162 against the earlier model.
No factory master-level compensation was applied.

The bowed body now keeps its authored resonance frequencies stationary when
the string pitch moves. Vibrato, ribbon bends and slides move harmonics through
that response, rather than transposing the body along with the string. The
existing gain ramps preserve continuity, and a common scale retains the
previous absolute-mode-sum headroom bound. This affects the 51 existing
programs with active bowed ranks. Wind and brass bodies keep their prior
behavior; this correction is specific to bowed strings.

The new bowed regression passes 492 checks and fails 174 against the preceding
model. In its focused octave-slide comparisons, the maximum harmonic-ratio
error against a directly played target changes from 4.10–6.19 dB to
0.003–0.025 dB. This measures the correction to the authored body response,
not similarity to a recorded violin. A separate 36,115,200-sample comparison
finds no changed samples for fixed-pitch bowed excitation or the other models
during seven-semitone slides. Fixed Engine storage grows by 12,800 bytes on
the measured Linux build; no allocation or work is added to the per-sample
modal-rotation loop. Control-boundary work increases for moving bowed notes.

[Full-preset dynamics and affected programs](preset-evidence/2026.11.4/continuation/a09/FINDINGS.md) ·
[Tine regression and parity](preset-evidence/2026.11.4/continuation/a06/summary.json) ·
[Bowed response, parity and scoped runtime measurements](preset-evidence/2026.11.4/continuation/a10/FINDINGS.md)

The bowed correction follows the qualitative body-filtering relationship in
[UNSW's violin articulation research](https://www.phys.unsw.edu.au/jw/violinarticulation.html).
The model's formant positions and widths remain authored approximations.
These continuation measurements are development evidence. The current README
download record identifies the separately qualified native source and files.

The audition script produces **138 unnormalized stereo float WAVs** of **74
existing programs**, totaling **870 seconds**, together with the complete
523-program, 200-parameter inventory. It includes soft/firm piano examples at
MIDI 36, 60 and 84, sustained-effort comparisons with matched C4 foundations,
clarinet register comparisons, pressure comparisons, and examples from every
instrument-body model. Complete-preset comparisons do not isolate size alone.

[Listening guide](PRESET_GUIDE.md) ·
[Audition producer](../scripts/capture-factory-auditions.py) ·
[Audition boundary tests](../scripts/test-capture-factory-auditions.py)

Development evidence and release qualification are separate. The initial
engine run passed **194/194 tests**. The final direct-model suite, expanded to
cover fractional-note piano boundaries, passes **2,077 checks** and generates
**41,062,350 samples**. The earlier engine-integration suite passed **18,156
checks**. Subsequent factory calibration, affected-suite reruns and native
artifact qualification are recorded with their own exact inputs and source
identities. A development capture is never used as evidence that a downloadable
installer contains those bytes.

The final piano-course source also passed its affected model and integration
reruns. Two LFO tests were rerun with fresh capture directories, and the Mac
workflow contract test was updated for the stronger AU payload check. These
follow-ups have separate source manifests and are counted separately.
The final body/filter headers also passed the scoped ASan/UBSan probe; that
local probe did not qualify leak scanning or native host wrappers.

[Piano principal-mode boundary regression](preset-evidence/2026.11.4/reviews/34-piano-register-fix.json) ·
[Coupled-course boundary regression](preset-evidence/2026.11.4/reviews/37-piano-course-boundaries.json) ·
[Affected-suite follow-ups](preset-evidence/2026.11.4/development/final-affected/summary.json) ·
[Final header sanitizer probe](preset-evidence/2026.11.4/reviews/47-final-sanitizers.json)

Use the [current download qualification](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads)
to identify the accepted native source and archives. The configured audition
roster and local development results do not themselves establish their availability.

[Engine test record](preset-evidence/2026.11.4/development/core/summary.json) ·
[Actual worker assignments](preset-evidence/2026.11.4/work-log.json)

### Final local bank and matched-audio checks

After two calibration passes per bank and a rebuild between passes, all four
bank suites passed. The complete 523-program phrase audit measures a maximum
phrase peak of **−4.6 dBFS**, loudest-50 ms RMS values of **−29.8 to −18.0 dBFS**,
and a maximum eight-voice stress peak of **−1.5 dBFS**. The unchanged acceptance
limits remain −30 to −12 dBFS for that RMS window, at most −1 dBFS for the
phrase peak, and below 0 dBFS for the stress peak. Voicing, character and
precision checks passed **281/281**, **128/128** and **285/285** respectively.

A separate matched 48 kHz capture compares all 523 programs with the preceding
revision, using three-second holds and one-second tails. Substituting explicit
pressure-one captures for the two pressure-only leads, all **521 musical
programs** have changed normalized spectral and relative envelope measurements
above CSV rounding tolerances. None is classified as a level-only change.
Both Init metric vectors remain exactly equal. These measurements establish
changed signal behavior; they are not audibility thresholds or a realism score.
The four-second whole-clip RMS in this comparison is a different measure from
the phrase audit's loudest-50 ms RMS, and short holds do not fully assess slow
pads or long releases.

[Final bank results](preset-evidence/2026.11.4/development/final-bank/audit-summary.json) ·
[Complete matched comparison](preset-evidence/2026.11.4/audio-comparison.json) ·
[Capture provenance](preset-evidence/2026.11.4/development/final-bank/matched-capture-provenance.json)

### Development runtime cost

Seven interleaved thread-CPU measurements per case compared identical control
vectors on the preceding and development Engines at 48 kHz Standard/2×
operation. The retained source hashes identify this stage, before the final
piano onset-stream and course-boundary corrections and final level calibration.
Those later piano corrections add no audio-rate work.
Median development/baseline ratios for eight active voices were 1.033 for low piano,
1.005 for middle piano, 1.024 for tine, 1.123 for bowed strings and 1.023 for
brass. The bodies-off control measured 1.014. Individual pairs varied on the
shared machine; these figures are development estimates, not realtime deadline
guarantees for a user's computer or host.

The allocation probe recorded no instrumented C++ allocations or frees during
note/controller delivery, rendering, handoffs, reassignment or panic across
Off and all 19 models. Dynamically linked library internals and host wrappers
are outside that probe. Fixed Engine storage increased from 386,968 to 478,632
bytes. A separately reported 16-voice case uses two eight-voice Engines and is
explicitly a proxy, not native processor scheduling evidence.

[Raw trials, allocation controls and reproducible harness](preset-evidence/2026.11.4/reviews/20-runtime-cost.json)

## Acoustics references and limits

The implementation uses primary acoustics references to guide which behaviors
matter, without claiming to reproduce their complete models or measured
instruments:

- [Bank and Sujbert, DAFx 2002](https://home.mit.bme.hu/~bank/publist/dafx02.pdf):
  piano attack, beating and differing prompt/aftersound decay informed the
  separately evolving string components.
- [Pfeifle, DAFx 2017](https://www.dafx.de/paper-archive/2017/papers/DAFx17_paper_79.pdf):
  electric-piano pickup behavior informed the distinct, excursion-dependent
  tine and reed harmonic approximations.
- [Woodhouse, signature modes and formants](https://euphonics.org/5-3-signature-modes-and-formants/):
  body-resonance differences informed the separate bowed size profiles.
- [UNSW clarinet acoustics](https://www.phys.unsw.edu.au/jw/clarinetacoustics.html):
  register and harmonic-balance behavior informed the smooth clarinet profile.

The reference material does not validate the authored numerical settings or
the finished presets. Acoustic matching would require identified reference
instruments, matched performances and perceptual comparisons in addition to
the reproducible engineering checks retained here.
