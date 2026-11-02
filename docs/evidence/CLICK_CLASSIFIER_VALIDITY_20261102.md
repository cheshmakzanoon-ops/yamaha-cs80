# Preregistered click-score classifier validity — 2026-11-02

**Research outcome: the original maximum-sample-jump score responds to
injected digital sample discontinuities, but it does not independently
identify a click rather than an ordinary continuous pitched signal.**
This is a diagnostic validity experiment, **not** a new acceptance gate,
perceptual listening verdict, or score-limit waiver. All source-level
Bass/Funky/Clav score failures remain open.

## Frozen, genuinely pre-results contract

- Preregistered `tests/parity/tolerances-v2/click-classifier-validity.json`
  as `REGISTERED` in isolated local commit **`fd192a7` before** implementing,
  compiling or evaluating the synthetic controls.
- Input production `src/` Git tree:
  `7dd159d491012fd47b46c0aa651e86ea959ca289`.
- Original `src/tools/engine_audit.cpp` is included by a standalone
  test-only probe to reuse its source factory/tone setup, 64-host-frame MIDI
  rounding, complete preceding phrase, 8 ms event score and unchanged
  denominator (60 ms prior for note-off, earlier playing/chord window for
  note-on). The original **score >1** alert condition remains unchanged.
- Exactly **11 preregistered fixtures × 8 controls = 88** rows. Fixtures:
  original physical-PANEL Bass first strike/retrigger at 48/96 kHz plus its
  44.1 kHz retrigger, Bass *electrical programmer* retrigger at 48/96 kHz as
  a distinct coordinate, Funky 1 + Funky 2 tabs note-off at all three rates,
  and Clavichord 1 + Clavichord 2 tabs 96 kHz four-voice steal.

Every synthetic control begins from an independently rendered, unmodified
**omitted-event** PCM reference. Original source event PCM is included as
an **unlabeled observational** row, not a known click-free negative.
No musical synthesis parameters or realtime code are changed; each
synthetic intervention modifies a *copy of final rendered PCM only*.

Strictly labelled controls, fixed before results:

| Strict class | Applied to each fixture | Independent definition |
| --- | --- | --- |
| Positive: known digital discontinuity | Four single-sample cases | Add +0.05 or +0.20 full-scale to **exactly one** L-channel sample at event +1 host frame, or R-channel sample at event +6 ms (rounded); no other sample changes |
| Negative: no single-sample impulse | Two C1-continuous pitched cases | Add a 220 Hz, 0.10 full-scale sinusoid or 1,760 Hz, 0.20 full-scale sinusoid to left PCM, with 4 ms half-cosine rise: value and first derivative continuous at onset |

These negative labels mean **mathematically no synthetic sample impulse**;
they do *not* establish that every audio waveform is perceptually click-free
or agrees with hardware. High-pitched, high-amplitude attacks are legitimate
continuous mathematical signals, even if they have a large derivative.
The chosen absolute amplitudes can be large compared with a quiet factory
preset's prior-chord reference; that difference is part of the validity test,
not a gain-matched hardware audition.

The other **11 omitted-event controls** (the target MIDI event is absent)
and **11 real source-event renders** are observations **outside the strict
two-class confusion table**. A missing MIDI event does not certify that
the background source waveform is perceptually click-free, and real source
note attacks have no independently measured hardware/perceptual label.
The preregistration calls the omission a negative **for target MIDI-event
causality**; it is conservatively kept *unlabelled for one-sample-click
classification* rather than retroactively treated as verified click-free
audio. Both categories and all 11 omissions are reported without exclusion.

## Preregistered result: strictly labelled 2 × 2 table

| Actual injected PCM class | Original score alerts (>1) | Original score does not alert (≤1) | Total |
| --- | ---: | ---: | ---: |
| **Positive: one-sample discontinuity** | **42** | **2** | 44 |
| **Negative: C1-smooth pitched addition, no one-sample glitch** | **15** | **7** | 22 |

- Alert prevalence among injected impulses: **42/44 = 95.45%** overall.
  However, the three already-alerted Funky omission baselines contribute
  **12/44 impulse rows that would alert whether or not the impulse occurred**.
  Restricting to the eight fixture baselines whose omitted-event score was
  originally ≤1, the synthetic impulse alert rate was **30/32 = 93.75%**.
  Even this is a score-threshold sensitivity, not a subjective hearing result.
- Both missed positive cases were **Clavichord tabs, 96 kHz**, where the
  omitted-event reference score was `0.00249765`: a **single +0.05 full-scale
  sample** in L at event +1 scored `0.381650`; the analogous R at +6 ms
  scored `0.379649`. Both remained below the unmodified alert threshold.
  The stronger +0.20 single-sample controls crossed it.
- Alert prevalence among mathematically continuous pitched additions:
  **15/22 = 68.18%**, despite zero introduced one-sample discontinuities:
  **10/11** for the 1,760 Hz attack and **5/11** for the 220 Hz attack.
  Among the eight originally non-alerted omitted-event fixtures, these
  smooth attacks still alerted in **9/16** cases. The metric therefore
  confounds a steep legitimate musical waveform with a digital one-sample
  corruption; it is not a specific click classifier.
- Of the **11 unlabelled omitted-event controls**, **3** already scored >1
  despite *no target MIDI event*—all three Funky tone-tab note-off rates.
  Of the **11 unlabelled real source-event renders**, **8** scored >1.
  These observations are deliberately not added to false-positive or
  true-positive counts because neither category has perceptual ground truth.

All 88 cases preserved the pre-event PCM **bit for bit** and retained the
original numerical reference jump. All 44 intentional digital positives
changed **exactly one** channel sample, verified by exhaustive bit support.
Unmodified omission controls changed zero PCM samples; all smooth
additions began with precisely unchanged PCM at the event instant.

Four preregistered synthetic mutation oracles were **rejected as required**:
corrupted pre-event PCM, a two-sample supposed positive, a nonzero first
sample in a nominally C1-smooth onset, and a modified frozen denominator.
The whole 88-row experiment succeeded twice independently; SHA-256 of both
byte-for-byte identical outputs:

`ba30d2d0fe1b0121b3f68d929e66b0485b0353b1109e6fc185ed3e06279f748e`

## Method reproduction and interpretation boundary

The full output is `docs/evidence/click-classifier-validity-20261102.csv`;
the standalone probe is
`tests/diagnostics/ClickClassifierValidityProbe.cpp`. Build the frozen
`cs80_core`, then run from the repository root:

```bash
g++ -std=c++20 -O3 -DNDEBUG -Isrc -Itests \
  tests/diagnostics/ClickClassifierValidityProbe.cpp build-core/libcs80_core.a \
  -o build-core/click-classifier-validity-probe
./build-core/click-classifier-validity-probe \
  --prereg tests/parity/tolerances-v2/click-classifier-validity.json \
  --out /path/to/new-empty/click-classifier-validity.csv
```

The probe does not register a new CTest, modify any original audit output
schema, alter the audio engine, or force a PR build. Its synthetic positives
deliberately resemble *digital single-sample corruption*, not a measured
CS-80 hardware fault. Its smooth negatives have analytically continuous
onsets, not proof of subjective inaudibility. No actual listener or physical
hardware recording provided a ground-truth click verdict here.

### Owner decision

**Do not promote the original score >1 into a click-specific causal
classification or a release pass based on this experiment.** It is useful
as a sensitive general sample-slope alert, but the two known missed +0.05
digital glitches, the high alert rate on C1-smooth pitched attacks, the
event-absent Funky alerts and the already-alerted-baseline ceiling show
insufficient discrimination. The existing frozen full-engine score gate
remains **FAIL/OPEN**, not waived or redefined.

Any replacement causal/perceptual classifier would need a **new, separately
preregistered** independent hardware or human listening ground truth,
balanced known positive/negative fixtures, sample-rate and sound-level
stratification, and mutation controls. No threshold or inferred fade was
retrofitted to these results.
