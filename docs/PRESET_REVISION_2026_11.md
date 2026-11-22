# Circuit library revision 2026.11

This report records the original full-bank redesign. The later
[2026.11.1 refinement report](PRESET_REFINEMENT_2026_11_1.md) documents twelve
further sound refinements and the current publication status. Historical
measurements below retain their original conditions and provenance.

## Result and publication status

The revision replaces the voicing of **all 300 musical Circuit programs** while
preserving their names, categories, order and host addresses. Init Circuit is
unchanged, giving 301 Circuit programs alongside 222 frozen Legacy programs.
An independent baseline comparison found **19–61 changed non-volume parameters
per musical program**, with no exact duplicate non-volume voicings. This is
sound-data authoring followed by separate level calibration.
[Identity results](preset-evidence/2026.11/identity/summary.json) ·
[Per-program counts](preset-evidence/2026.11/identity/program-change-counts.csv)

**Local integration is verified**, including the final Stereo Delay Pluck
correction, all-program audit, 301-vector nominal data and native checks below.
Platform CI and downloadable bundles remain **pending**. The publication update
will identify the actual source commit and run links; the revision label alone
does not identify a build. Historical comparison starts at
`61f45feeae1f01038476eb9c99105534950db4de`.
[Evidence provenance](preset-evidence/2026.11/provenance.json)

## Authored scope and compatibility

The implementation separates the bank into 13 authoring files under
[`src/core/curated/`](../src/core/curated/), included by the existing ordered
[library builder](../src/core/CuratedLibrary.inc). The Modern file contributes
18 programs; six programs in other files also carry the `modern` tag. All 24
retain their instrument categories and are included in the 300 musical programs.

| Authoring file | Programs | Authoring file | Programs |
| --- | ---: | --- | ---: |
| Brass | 32 | Strings | 28 |
| Pads | 30 | Polysynth | 24 |
| Keys | 24 | Organ | 14 |
| Winds | 14 | Plucked | 16 |
| Bass | 24 | Leads | 30 |
| Bells | 22 | Fx | 24 |
| Modern | 18 | Init, in main builder | 1 |

The independent identity audit compared **all 190 float bit patterns** and the
metadata of every Legacy record, extending the existing historical digest's
139-parameter coverage. All 222 records and the complete Init record match the
baseline. Legacy's historical FNV-1a remains `0x3628dd9acc109b2a`; its complete
canonical-record SHA-256 is
`169e95eb438806e71b76100e0e8c1ffcaea496063871536399f52ea68f9683b8`.
The 190 ordered core parameter IDs, 296 host parameters and state schema 3 are
unchanged. All 49,152 tested bank/program combinations preserve their assigned
index or unassigned result. [Identity evidence](preset-evidence/2026.11/identity/summary.json)

| Addressing, zero-based | Preserved mapping |
| --- | --- |
| Host programs | Legacy 0–221; Circuit 222–522 |
| MIDI bank MSB 0 | Circuit: LSB 0/1 use programs 0–127; LSB 2 uses 0–44 |
| MIDI bank MSB 1 | Legacy: LSB 0 uses 0–127; LSB 1 uses 0–93 |
| Other tested pages/MSB 2 | Unassigned |

Saved sessions restore their serialized parameter values. Restoring their
factory-name/index metadata does not reload the revised bank. Explicit factory
selection or a later MIDI Program Change loads the new values at the stable
address; a host saving only a program number therefore selects the new sound.
The [state restoration](../src/plugin/PluginProcessor.cpp) and
[library metadata code](../src/plugin/PluginProcessorLibrary.cpp) retain that
distinction.

Fresh instances now load **Brass Swell I**, Circuit program 0/host 222, with its
matching factory identity. This also fixes startup Library navigation: a Legacy
identity previously selected the Legacy browser scope even with the Circuit
engine active. Explicit Legacy recall still opens the Legacy section.
[Startup/identity tests](../tests/PresetIdentityTests.h) ·
[Program-library host tests](../tests/ProgramLibraryHostTests.h)

## How the voicings changed

The two ranks now have explicit roles: sustained body plus breath/pick/hammer,
staggered ensemble sections, or complementary registers. A noise source and
oscillator within one rank share its VCA envelope, so an independent disappearing
breath requires another rank. Pan Flute, Sine Bass, struck keys, mallets and
Percussive Organ use that separation. Short articulations reach silence while a
key remains held; slow swells retain sufficient attack time to develop.
[Behavior checks](../tests/PresetVoicingTests.cpp)

Envelope settings follow the implemented response. Filter initial and attack
levels map to negative and positive octave offsets; decay/release values describe
approximately the time to reduce the remaining span by 60 dB. A nominal
one-second zero-sustain decay is already roughly 30 dB down halfway through.
Longer body decays and much shorter strike decays consequently replace similar
envelopes on both ranks. Sine bypasses both filters, allowing a stable foundation
under a separately filtered edge. [Engine](../src/core/Engine.cpp) ·
[Authoring helpers](../src/core/CuratedLibrary.inc)

Footage is also structural: 16′, 8′, 5⅓′, 4′, 2⅔′ and 2′ correspond to frequency
multipliers 0.5, 1, 1.5, 2, 3 and 4. Octave Pad now uses 16′/8′ instead of the
baseline's two-octave 16′/4′ combination. Percussive Organ has a sustained 8′
body and disappearing 4′ percussion. Piccolo, Ocarina and Whistle no longer
carry the unintended prominent lower octave detected in the baseline.
[Pad measurements](preset-evidence/2026.11/families/pads-polysynth.json) ·
[Wind/organ measurements](preset-evidence/2026.11/families/winds-organ.json) ·
[Baseline failures](preset-evidence/2026.11/behavior/original.log)

Velocity is not an unconditional gain multiplier: rank gain adds static level,
initial-touch level times velocity, and aftertouch level times pressure.
Rebalancing static and initial-touch contributions makes strike response useful.
All 117 velocity-tagged programs have an active level route; targeted renders
also preserve deliberate near-even harpsichords and velocity-independent organs.
All 77 aftertouch-tagged programs received a static routing screen. Pressure
Lead alone has zero pre-pressure gain and intentionally requires aftertouch.
Pressure brightens both high-pass and low-pass filters, so reviews measured
absolute fundamental energy as well as its percentage of the spectrum.
[Velocity screen](preset-evidence/2026.11/controls/velocity-static.json) ·
[Pressure screen](preset-evidence/2026.11/controls/pressure-static.json)

Original SUB random produces fresh noise every sample; changing its speed does
not create held random steps. Rain Pad and the appropriate FX use modern LFO
waveform 5 for actual sample-and-hold, with explicit `modern` tags and Pure CS-80
limitations. Rain Pad uses 8 Hz steps; Random Filter uses 7.5 Hz. The Modern
layer also supplies deliberately programmed delay, ensemble and arp behavior.
[Pad analysis](preset-evidence/2026.11/families/pads-polysynth.json) ·
[FX gate analysis](preset-evidence/2026.11/families/fx-gates.json) ·
[Modern analysis](preset-evidence/2026.11/families/modern.json)

## Representative measured results

These focused reviews use unnormalized 48 kHz stereo renders. Several precede
master calibration: relative levels and intervals describe the stated captures;
their absolute dBFS values are not final loudness specifications. MIDI 60 means
C4 here. Static pressure endpoints are not performed controller crescendos.

| Example and conditions | Observation | Evidence |
| --- | --- | --- |
| Brass Swell I; MIDI 60, velocity .8, pressure 0 | RMS rises 5.7 dB from 50–150 ms to 450–700 ms; the 20 ms RMS maximum occurs at 570 ms. | [Brass](preset-evidence/2026.11/families/brass.json) |
| Solo Violin; MIDI 55/69/84, velocity .8 | Full pressure adds 1.73–1.97 dB held RMS and approximately ±15.6 cents vibrato; absolute fundamental energy increases at every checked note. | [Strings](preset-evidence/2026.11/families/strings.json) |
| Octave Pad; MIDI 60, velocity .8, isolated ranks, 1.2–1.95 s | 130.681 and 262.242 Hz, or 1205.8 cents including authored detune/modulation, support the octave registration. | [Pads](preset-evidence/2026.11/families/pads-polysynth.json) |
| Guitar I; MIDI 60, velocity .9 | Late body at 1.0–1.4 s changes from −50.9 to −19.1 dB relative to its 5–105 ms attack, supporting a longer pitched decay. | [Keys/plucked](preset-evidence/2026.11/families/keys-plucked.json) |
| Picked Bass; MIDI 36, velocity .8 | Fundamental share rises from 10.86% to 73.20% in the 0.4–0.9 s body window; the held-note fall increases from 8.40 to 35.60 dB by two seconds. | [Bass/leads](preset-evidence/2026.11/families/bass-leads.json) |
| Clock Bells; MIDI 60, velocity .9, isolated ranks | Approximately 523/785 Hz establish the 4′/2⅔′ fifth; unequal decay keeps the upper mode subordinate later. | [Bell ranks](preset-evidence/2026.11/families/bells-ranks.csv) |
| Clockwork; MIDI 60, full velocity/pressure, four-second hold | Closed gate interiors sit 57.30 dB below active interiors; median tick interval is 166.59 ms, consistent with 6 Hz. | [FX gates](preset-evidence/2026.11/families/fx-gates.json) |

Reviews also corrected two attack layers that existed in isolation but were too
quiet at their authored mix. Pan Flute's stronger, slightly longer noise rank
raises weighted chiff/body RMS from −26.99 to −15.59 dB over 4–20 ms at MIDI 67,
velocity .8. Nylon Pluck replaces its ineffective noise-only finger transient
with a filtered pulse edge and trace of air: weighted onset/body changes from
−49.89 to −18.29 dB over 0–10 ms at MIDI 60, velocity .9. Their sustained-body
roles remain intact. [Pan Flute](preset-evidence/2026.11/families/winds-organ.json) ·
[Nylon refinement](preset-evidence/2026.11/families/nylon-refinement.json)

Metallic Keys, Agogo and seven FX received targeted source/filter/VCA balancing
where legal maximum master volume could not reach the existing loudness gate.
Their envelopes, characteristic ring/noise behavior and velocity relationships
were retained. No acceptance threshold was relaxed.
[Bells audit](preset-evidence/2026.11/families/bells-final-audit.csv) ·
[FX audit](preset-evidence/2026.11/families/fx-final-audit.csv)

## Calibration, headroom and longer tails

Calibration ran twice, **rebuilding the core between passes**, because the output
driver is mildly nonlinear. The first generated table must be compiled before
the second measurement can refine it. The targets are loudest 50 ms RMS of
−18 dBFS, or −22 dBFS for single-line categories, constrained by phrase/stress
peaks of −3/−1.5 dBFS and volume 0.05–1.00. These targets are separate from the
unchanged audit acceptance gates. [Pass 1](preset-evidence/2026.11/calibration/pass-1-levels.inc) ·
[Intermediate rebuild](preset-evidence/2026.11/calibration/rebuild-between-passes.log) ·
[Pass 2](preset-evidence/2026.11/calibration/pass-2-levels.inc) ·
[Audit implementation](../tests/LibraryAuditTests.cpp)

The completed final integrated audit passes all 301 Circuit and 222 Legacy
program checks: maximum phrase peak **−5.0 dBFS**, loudest-window RMS
**−30.0…−18.0 dBFS**, maximum eight-note stress peak **−1.5 dBFS**. At 44.1 kHz,
the category phrase uses velocity .9, pressure rising around .3–.6 seconds,
note-off around 1.2 seconds and approximately 1.9 seconds total. Stress holds
MIDI 36/43/48/55/60/64/67/72 for approximately one second at full velocity and
pressure. [Full audit](preset-evidence/2026.11/final/library-audit.log)

Separate nominal measurements cover all 301 final program vectors at 48 kHz,
velocity .8 and pressure 0. Holds are at least three seconds, extended for slow
attacks, followed by one-second releases; notes and durations are recorded per
row. The collection retains 300 unchanged measured rows and recaptures Stereo
Delay Pluck after its two-control correction. All 301 row digests match the final
inventory. Together they represent 1,216.75 seconds of audio, with zero nonfinite
and over-full-scale samples. Pressure Lead's zero output is intentional under
these conditions.
[Nominal metrics](preset-evidence/2026.11/final/nominal.csv) ·
[Collection provenance](preset-evidence/2026.11/final/nominal-provenance.json)

One-second stress cannot cover a slow bloom. Six risk-selected programs therefore
received **84 seconds** of longer four-note chords, maximum velocity/pressure,
natural release overlap, 48 kHz and the factory 2× policy. Each reached eight
active voices and ended with zero. Deep Space, Evolving Pad, Plate Pad, Ambient
Bells, Bell Pad and Ensemble Plus all remained finite and below full scale.
The largest peak, Plate Pad during overlap, was **−6.487 dBFS**, leaving at least
6.487 dB observed sample headroom in this set. Final release windows extended
7.85–9.85 seconds; these are explicit sequence results, not universal guarantees.
[Long-tail verification](preset-evidence/2026.11/long-tails/verification.json) ·
[Metrics](preset-evidence/2026.11/long-tails/metrics.csv)

## Stereo correction and regression sensitivity

Eight targeted stereo programs were checked at MIDI 60, velocity .8, pressure 0,
48 kHz. Stereo Delay Pluck exposed early repeat/body cancellation. Reducing delay
mix **.28→.18** and repeat control B **.58→.42** preserves its 250.013 ms timing.
In the matched .50–.75 s window, fundamental mono loss improves
**10.1427→1.9967 dB**; the worst active 250 ms window moves to .625 s and measures
2.0852 dB. First-echo combined RMS at .25–.50 s falls only **.932 dB**; full-held
RMS changes −.044 dB. Mono uses `(L+R)/2` against averaged stereo power, retaining
channel imbalance; the fundamental band spans ±10% around MIDI 60 with Hann
windows. The integrated final bank, audit and nominal data include this correction.
[Matched comparison](preset-evidence/2026.11/stereo/comparison.json) ·
[Attack/echo analysis](preset-evidence/2026.11/stereo/echo-and-attack.json)

The behavior suite runs identical checks against three immutable libraries at
44.1 kHz. It measures articulation, sustained body, register, velocity, pressure
and onset contribution rather than asserting the current parameter values.

| Bank | Passed | Failed |
| --- | ---: | ---: |
| Original baseline | 116/136 | 20 |
| Quiet intermediate draft | 134/136 | 2 |
| Reviewed corrected bank | **136/136** | **0** |

The quiet draft fails precisely the Pan Flute and Nylon mix-level onset gates.
Weighted isolated ranks must also reconstruct the unmodified onset within 1%
RMS, preventing a diagnostic mismatch from masquerading as an audible-layer
defect. The corrected captures reconstruct within approximately −142.5 and
−144.3 dB relative RMS error. The suite generates 45.05 seconds of audio per run; the final integrated rerun
also passes all 136 checks. [Results and negative controls](preset-evidence/2026.11/behavior/summary.json) ·
[Final log](preset-evidence/2026.11/behavior/final.log)

## Host, capture tool and release verification

The final native Linux build passes **6,800 recall checks across five modes**:
81 identity checks, 2,355 modern/session/preset/A–B checks, 2,784 user-preset file
recall checks, 193 refresh checks and 1,387 revert checks. Four headless
panel-golden checks also pass at **1600×1000 and 1120×700**: both report
**0.0000% pixels changed under the comparator threshold**. This is not a
pixel-identical claim; worst 8×8 block means are .73 and .00.
[Identity](preset-evidence/2026.11/host/native-identity.log) ·
[Modern recall](preset-evidence/2026.11/host/native-modern-recall.log) ·
[File recall](preset-evidence/2026.11/host/native-file-recall.log) ·
[Refresh](preset-evidence/2026.11/host/native-refresh.log) ·
[Revert](preset-evidence/2026.11/host/native-revert.log) ·
[Goldens](preset-evidence/2026.11/host/panel-golden-tests.log)

The capture CLI passes **nine tests**, including raw float RIFF metrics,
determinism, direct-DFT comparison, note-off boundaries, invalid arguments,
Unicode paths and conflicting output-mode preservation. Six before/after
captures at 44.1/48/96 kHz and MIDI 0/127 are byte-identical after portability
fixes. Supported Windows wide-path handling was source-reviewed; native Windows
and macOS execution awaits CI. [CLI tests](preset-evidence/2026.11/capture/tests.log) ·
[Focused comparisons](preset-evidence/2026.11/capture/focused-checks.json)

| Publication deliverable | Verified status / link |
| --- | --- |
| Final integrated bank and local checks | **Passed; source commit and platform CI pending** |
| Windows x64 VST3 / Standalone | **Pending CI and artifact verification** |
| macOS Apple Silicon VST3 / AU / Standalone | **Pending CI and artifact verification** |
| macOS Intel VST3 / AU / Standalone | **Pending CI and artifact verification** |
| Linux x86-64 VST3 / Standalone | **Pending CI and artifact verification** |
| Twelve-excerpt audition artifact | **Pending complete capture and checksums** |

Review assignments and their completion records are maintained separately.
The [evidence index](preset-evidence/2026.11/README.md) explains
snapshot conditions and reproduction. Raw audio and binaries are excluded from
this compact bundle. The [preset guide](PRESET_GUIDE.md) supplies capture commands
and playing suggestions. These results establish software behavior on bounded
fixtures; they do not claim a listening-panel verdict, acoustic-instrument
equivalence, calibrated hardware matching, or qualification of every DAW,
register, controller history and playback system.
