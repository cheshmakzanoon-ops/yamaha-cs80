# Factory voicing continuation — 2026.11.7

Revision **2026.11.7** retains the complete 2026.11.6 factory programming and
corrects four behaviors in the instrument-body renderer: upper piano partials
that rang too long, noise texture that weakened at higher internal sample
rates, body resonances that moved with standard string bends, and insufficient
harmonic coverage in low choir notes. **No preset values or parameter definitions
change in this increment.**

The catalogue remains **523 programs**: 301 in the later collection and 222
at the original addresses. The cumulative 2026.11.4-to-2026.11.6 programming
changes to **all 521 musical programs** are retained. **Init Patch** and
**Init Circuit**, names, categories, ordering and program addresses remain
unchanged. A shared mechanism repair affects the programs and saved patches
that use that mechanism; it does not mean every program changes audio again
in 2026.11.7.

**This is a source candidate awaiting native build and delivery qualification.**
The frozen binary source is
[`ee0dabcacb4cdff93f8803844f60348e0ceda6ed`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/ee0dabcacb4cdff93f8803844f60348e0ceda6ed),
tree `17115913cbb29157bd00539485bcc5f38b57219c`.
The local evidence below is scoped to the implemented changes. It does not
qualify a downloadable plugin, a completed audition archive or every host.
The [current online download table](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads) identifies accepted
artifacts and their exact source. Existing archives retain their original
revision and binary identity. The preceding
[native qualification audit](preset-evidence/2026.11.7/reviews/a03.md) records
why results from the earlier unfinished candidates cannot be combined into
qualification for this source.

The reviewed sound source `c5549f7d` has the same core/plugin sound code and
factory recipes as this frozen source; the intervening repairs concern offline
tool and audit-harness storage. The
[source-continuity review](preset-evidence/2026.11.7/reviews/a40.md) preserves
that evidence boundary. Later online documentation does not change the binary
source or the guides frozen inside its archives.

These remain authored synthesized instruments. The changes repair specific
rendering behaviors; they do not establish equivalence to a recorded acoustic
instrument or physical CS-80. Expected listening effects below are inferences
from the implementation and measured PCM, not listening-panel findings.

## Retained full-bank work

The [2026.11.5 report](CIRCUIT_LIBRARY_2026_11_5.md) documents the retained
touch response, independent partial decay and aftersound, register-dependent
losses, family-specific programming, choir variants and revised circuit,
motion and sequence programs. The
[2026.11.6 report](CIRCUIT_LIBRARY_2026_11_6.md) adds stable piano course tuning,
fixed-frequency wind/brass responses during pitch motion, and nine distinct
bell, comb, tine and plucked materials. Its material choices, parameter
contract and corrected recipes remain in this candidate.

The authenticated inventory comparison preserves all **106,692 parameter
values** (523 × 204), every complete program record and the parameter metadata.
The four new mechanisms are structurally available to **256 existing programs**;
their actual effect depends on pitch, register and rendering rate. This is
separate from the retained 521-program revision of the published bank.
[Complete identity and coverage evidence](preset-evidence/2026.11.7/reviews/a12.md)

Relative to published `.4`, **493 musical programs** change controls beyond the
declared conservative level/pan exclusions; another **28** change active
additive rank-VCA baselines that contribute to touch response. The matched
default captures have changed audio-metric vectors for **519 musical programs**.
The other two, **Pressure Lead** and **Electric Leads 136**, have changed
waveforms at both tested pressure levels. This gives cumulative signal-change
evidence for **521/521**, without claiming every note or gesture changes or
that the result is perceptually preferable. Both Init programs preserve their
published controls and match the earlier stereo output in the twelve retained
comparison cases.
[Cumulative accounting](preset-evidence/2026.11.7/reviews/a33.md) ·
[Init comparisons](preset-evidence/2026.11.7/reviews/a27.md)

The narrower `.6 → .7` comparison has **zero recipe edits** and **67 changed
default metric vectors**: 54 standard-string programs, ten piano programs and
three low-Oo programs. Its capture uses **48 kHz**, velocity **0.8**, zero
pressure, a **three-second hold and one-second tail**, with no output
normalization. MIDI notes are **48 for Bass, 67 for Winds/Leads, 72 for Bells
and 60 for the other categories**. These short single-note measurements are
supplemented by selected long holds, chords and controller gestures; they do
not complete every entrance. Fourteen programs have an active mapped attack
beyond the bank capture's three-second hold.
[Measurement scope](preset-evidence/2026.11.7/reviews/a33.md) ·
[Long entrances and sustained headroom](preset-evidence/2026.11.7/reviews/a34.md)

The nine retained Materials and four Variants select compatible models in all
**18 specialized programs / 34 active specialized ranks**. Their spectra,
contact layers and losses are authored profiles. Separate ranks do not
exchange energy through a solved shared resonator, and these profiles do not
add complete coupled hammer/string, soundboard, membrane or shell mechanics.
[Material and variant review](preset-evidence/2026.11.7/reviews/a41.md)

Deliberately electronic sounds keep their circuit, ring, PWM and motion
identities. The changes below operate inside the optional body path; they do
not introduce a second engine or turn every preset into an acoustic imitation.

## Smoother upper-piano decay

The piano's first six principal partials already received an additional prompt
loss, alongside their separately slower second course. The seventh and higher
principal partials escaped that loss. In low notes, this produced a strong
late rise of the seventh partial relative to the sixth for a reason unrelated
to the authored string geometry.

The correction extends the prompt loss to all principal piano partials,
capping the extra amount at the sixth partial's value. The lower six keep
their previous coefficients; the separate course, its stable tuning,
stiffness and strike variation remain intact. This should make low piano
tails settle with less isolated upper ringing. Hold **Stage Keys** or
**Soft Keys** around MIDI 24–36 to hear the affected region. The correction
also acts during the attack, so complete attack PCM is not claimed unchanged.

The rendered regression passes **220 checks** across C1/C2, 44.1/48/96 kHz
host rates, different touch settings and seeds, and decay endpoints. Its
pooled early-to-late seventh/sixth ratio rises at most **1.87 dB** after
correction in the decay-1 cases, compared with **15.14 dB** before it. Restoring
only the old damping condition produces **44 failures**, one for every
tested tail case. Existing piano course, body and struck-dynamics tests
also pass in the local report. This checks the defect without demanding that
every neighboring partial decay monotonically through course beating.

The repair adds no sympathetic string coupling, damper noise or measured
piano fit. Full-preset pedal, filter and release auditions remain distinct
from this body-level evidence.

[Implementation and measured results](preset-evidence/2026.11.7/reviews/a04.md) ·
[Piano regression](../tests/PianoPartialDecayTests.cpp)

## Consistent breath, contact and flutter across rates

The body texture uses fresh random samples filtered at fixed frequencies.
Previously, increasing internal sample rate reduced the admitted noise power.
Higher-quality or offline rendering could therefore weaken a wind's breath
or a string's contact texture while leaving its pitched body almost unchanged.

Two independently calculated gains now preserve the authored noise-band and
slow-flutter strength relative to **96 kHz internal rate**, the normal 48 kHz
host/2× reference. The gains are prepared when the sample rate is set.
Random draws, filter frequencies, envelopes and modal amplitudes retain their
existing behavior. For this correction alone, the 96 kHz internal reference
stream is preserved exactly; that is not a claim that the other changes in
this revision leave every reference-rate sound unchanged.

The new public-API regression passes **157 checks**; removing just these
gains causes **52 failures**. Across the tested 44.1/48/96 kHz host and
2×/4×/8× body-rate matrix, the maximum steady-flute texture deviation from
the reference is approximately **0.03 dB**, compared with **8.85 dB** before correction.
Pooled short-contact tests also cover piano, tine, clavichord, steel and
marimba at soft and firm velocities, with a maximum corrected deviation
below **0.076 dB**. Flutter is measured separately and uses a wider tolerance
appropriate to its slower variation. The body-rate matrix is a probe matrix;
the Engine does not select every listed factor at every host rate.

For playing and export checks, compare a held **Flute** or **Solo Violin**
note and the onset of a struck voice. The correction targets texture energy.
Complete waveforms, anti-alias filtering and all quality-mode behavior are
not asserted to be identical.

An independent full-Engine comparison covers flute, bowed, clarinet and brass
at 44.1/48/96 kHz. The tested HQ/realtime nonharmonic-band difference falls from
as much as **5.89 dB** to at most **0.059 dB**. Wider harmonic exclusions and
separate subbands confirm the result. Every offline capture exactly equals its
corresponding explicit HQ capture, and the 48 kHz realtime reference fixtures
retain their earlier PCM. [Engine integration evidence](preset-evidence/2026.11.7/verification/a10/reporta10.md)

[Correction, scope and measurements](preset-evidence/2026.11.7/reviews/a05.md) ·
[Noise-rate regression](../tests/BodyNoiseRateTests.cpp)

## String bends through a fixed body

Standard **nylon, steel, bass, clavichord and harpsichord** bodies already had
resonances expressed in Hz at note onset. Their live-pitch path previously
kept each partial's original gain while changing its frequency, carrying
the body spectrum along with the bend.

These five models now use the existing bounded live response correction.
The harmonics move through the same fixed body resonances. Pluck position,
contact, stiffness, decay and onset history stay with the accepted note;
returning to its starting pitch restores the original response without
retriggering it. The existing Banjo, Koto and Harp material responses, and
the earlier wind/brass and bowed/vocal corrections, remain in place.

Try a slow ribbon bend on **Nylon Pluck**, **Steel Pluck** or a body-enabled
bass. The expected difference is the harmonic balance during the motion,
not a new pitch destination or a restarted envelope. Large bends can invoke
the existing common headroom bound and reduce overall level.

The rendered regression passes **5,388 checks**, comparing 3,093 normalized
partial ratios across three rates, three sizes, three onset registers and
four bend intervals. Worst error against the fixed-body prediction is
**0.0071 dB**. The old source instead follows the transposed-body prediction
and fails **2,535 checks**. An isolated reference-rate comparison found
**zero mismatches in 8,847,360 unbent sample pairs**. That preservation applies
to the exact onset pitch; drift, detuning or a slightly different supplied
frequency can legitimately engage the corrected live response.

These measurements establish the implemented response during a bend, not
agreement of its authored resonance profile with a recorded instrument.

[String correction and evidence](preset-evidence/2026.11.7/reviews/a06.md) ·
[Bend regression](../tests/StringBodyFormantTests.cpp)

## More vowel detail in low choir notes

The **Choir ah** and **Choir oo** variants formerly used 24 harmonic modes
at every pitch. In low registers that spectrum could stop before reaching
the upper vowel regions. Their onset budget now grows with lower pitch and
smaller Resonant size, from **24 up to 48 harmonics**, aiming to cover an
upper region of **4,400 Hz divided by Resonant size**. The highest admitted
partial fades across fractional budget changes, and the breath random stream
resumes from its original 24-mode position so adding a harmonic does not
reseed the texture.

The existing oscillator-bank ceiling remains 48. The deepest notes can still
reach that ceiling before spanning the intended upper region. The budget is
captured at note onset; a later deep downward bend does not allocate new modes.
Fixed-Hz vowel response during motion and the host Nyquist fade continue to
apply to the modes present. This is improved harmonic coverage, not an
unlimited vocal model.

Compare **Choir Pad** and **Dark Choir** on low held notes, allowing their
envelopes to settle. Their ah/oo distinction, rank registration, body sizes
and preset levels remain authored as before. The added harmonics affect
normalization as well as upper detail, so unchanged preset values do not mean
identical low-register PCM. The new rendered regression passes **1,342 checks**;
the same test against the preceding implementation fails **92 checks**. Added
upper harmonic groups exceed nearby noise by **40.56–55.29 dB** in its captures.
All 21 measured standard-model and high-choir fingerprints remain identical.
The tested worst fixed-tract error among added harmonics is **0.446 dB**.
At default-size C2, 48 modes reach **3,139.5 Hz**: they restore the 2,800 Hz
region while still falling short of the 3,500 Hz region. Full 4,400 Hz coverage
begins near F♯2. These are authored spectrum checks, not a recorded-singer fit.

The actual 16′ Oo foundation in **Dark Choir**, **Blade Choir** and
**Midnight Pads 55** reaches a stricter low-register limit: at played MIDI 36,
its approximately **32.70 Hz** fundamental puts mode 48 near **1.570 kHz**,
below both upper vowel centers at approximately **2.105 and 2.807 kHz** after
the saved size scaling. Its 8′ partner supplies a higher spectrum but does
not remove the foundation's cap. The
[all-seven-vocal-program review](preset-evidence/2026.11.7/reviews/a35.md)
accounts for every Ah/Oo recipe and its sounding rank register.

[Harmonic budget and response storage](../src/core/InstrumentBody.h) ·
[Choir recipes](../src/core/curated/Pads.inc) ·
[Rendered choir evidence](preset-evidence/2026.11.7/reviews/a07.md)

## Saved sessions and factory recall

This increment keeps **204 core parameters**, **310 host positions** and
**14 body controls**. It adds no parameter or material, and changes no saved
ID, range, default or automation order. The two Material controls were
appended in **2026.11.6**; the two Variant controls were appended in
**2026.11.5**. Those historical additions remain separate from this rendering
revision. Existing missing-field defaults still apply: Material and Variant
use Standard, Resonant size uses 1×, and patches without body fields keep
their off defaults.

Saved projects retain their stored controls. Body-enabled patches can still
sound different because piano damping, texture compensation, string bends
and low choir coverage are engine behaviors. Preserve an earlier binary and
reference bounce if an arrangement must retain its exact previous rendering.

For a first comparison, use a new empty project and a fresh instrument instance.
To use the cumulative factory programming, release all keys and the sustain
pedal, open **Library → FACTORY LIBRARY**, choose the program, press **LOAD** or
**RELOAD**, then play fresh notes.
An old project can display the same factory name while restoring an earlier
recipe. Browser **REVERT** restores the sound active when the browser opened.
Factory loading applies the sound parameters and PANEL sources, while retaining
physical setup, MSEG shapes and modulation routing. Existing host automation
can write later parameter values; LOAD is not a complete session reset.
Hardware tone tabs retain their electrical recipes and suppress optional
bodies, as do the existing memory paths.

[Parameter definitions](../src/core/Params.h) ·
[Program identities and revision](../src/core/FactoryPresets.h) ·
[Current compatibility review](preset-evidence/2026.11.7/reviews/a42.md) ·
[Loading behavior](preset-evidence/2026.11.7/reviews/a43.md) ·
[Playing and recall guide](PRESET_GUIDE.md#recalling-a-revised-sound)

## Processing and delivery scope

The piano correction adds no sample-loop work. The noise correction adds one
multiplication to the additive texture; its gain calculations run in prepare.
The five string models reuse the existing 16-sample response-update path.
Low choir notes can run more oscillators, and the two response histories now
cover 48 entries instead of 24. The bank remains fixed-size, but these changes
are not a claim of unchanged memory use or CPU cost. A whole-plugin deadline
or performance limit requires its own measurement.

The paired local body benchmark observes the expected extra work for low choir
notes: the isolated Ah MIDI-36 case grows from 24 to 48 modes and takes about
**1.85×** the earlier body processing time. It does not reproduce a large
high-register regression where the mode count stays 24. The body object grows
from **6,096 to 6,496 bytes**; 960 timed runs record zero C++ allocations.
Continuous string bends also perform the new response calculation. These are
body measurements on a shared host, not whole-plugin CPU percentages or a DAW
deadline guarantee. [Method and complete cost evidence](preset-evidence/2026.11.7/reviews/a11.md)

The complete-Engine comparison supplies context for that isolated body cost.
On the stated shared Linux host, with **48 kHz / 2×**, **256-frame blocks** and
eight low notes, median paired `.7/.6` CPU ratios are **1.153 for Choir Pad
(Ah)** and **1.167 for Dark Choir (Oo)**. The Engine's inline extent grows by
**12,800 bytes**. These observations exclude the plugin wrapper, driver and DAW;
they establish neither a universal maximum nor a callback deadline. The
unchanged high-register control and shared-host variability also limit the
precision of small overhead estimates.
[Engine ownership, allocation and cost evidence](preset-evidence/2026.11.7/reviews/a37.md)

The configured audition roster retains all **225 requests** introduced in
2026.11.6 and appends **13 existing-program clips** for low-piano tails and
low choir registers. It now requests **238 WAVs from 92 programs**, totaling
**1,361.2 seconds**, with the complete 523-program, 204-value inventory.
[Exact roster and preservation checks](preset-evidence/2026.11.7/reviews/a09.md).
A configured roster or local regression
pass does not establish that a complete archive exists. Accepted delivery
must bind the actual native tests, all **15 required pluginval lanes**, package
payloads and audition files to one committed source and its hashes. Until
that work finishes, this report describes the candidate rather than replacing
the README's accepted downloads.

[Installation and build identification](INSTALL_AND_QUICKSTART.md) ·
[Audition reproduction](PRESET_GUIDE.md#reproduce-an-audition-from-source) ·
[Native qualification audit](preset-evidence/2026.11.7/reviews/a03.md) ·
[Delivery contract](preset-evidence/2026.11.7/reviews/a30.md) ·
[Sound-claim and all-family review](preset-evidence/2026.11.7/reviews/a54.md)
