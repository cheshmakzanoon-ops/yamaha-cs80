# A47 — continuation numeric-claim crosscheck

Final candidate reviewed: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.

This is an independent read-only arithmetic and source-scope review of retained inputs, not a new render, listening session, native build or artifact qualification. No repository file was edited. The reproducible analysis is `a47/crosscheck.py`; exact input SHA-256 values and detailed results are in `a47-summary.json`.

## Finding

The continuation's principal numeric claims recompute correctly. No numeric correction to `docs/CIRCUIT_LIBRARY_2026_11_4.md` is required. Preserve the distinctions below when writing the final release summary.

## Configured affected programs

I reparsed the 523-program final inventory and selected a rank only when its body model is the relevant model and its body mix is positive.

| Model | Programs | Configured active ranks | Original addresses | Curated addresses |
|---|---:|---:|---:|---:|
| Tine | 17 | 28 | 8 | 9 |
| Bowed | 51 | 91 | 27 | 24 |

Their host-index sets have **no overlap**: **68 distinct existing programs** configure one of the corrected models. This establishes configuration reach. It does not establish 68 listening tests, equivalent perceptual benefit in every program, or equal change at every velocity and pitch. Full-strength tine signals and fixed-pitch bowed signals are intended to remain unchanged.

## Electric-piano dynamics

I recomputed soft-to-firm body RMS spans directly from `a05/metrics.csv` and `a09/metrics.csv`, using the 300–800 ms window and velocities 0.15 and 1. The displayed ranges cover MIDI 36, 60 and 84.

| Existing program | Before, dB | After, dB | Actual span increase over the three notes |
|---|---:|---:|---:|
| Electric Piano I | 8.22–9.07 | 12.23–13.01 | 3.9002–4.0052 dB |
| Electric Piano II | 8.28–9.97 | 12.30–13.86 | 3.8922–4.0162 dB |
| Tine Piano | 5.97–6.52 | 9.97–10.41 | 3.8895–3.9950 dB |

The baseline CSV is rounded, so small derived deltas inherit that precision. The candidate CSV contains exactly **90 unique cases: five complete presets × three notes × six velocities**. Within every one of the 15 preset/register groups, peak, early RMS, body RMS and nominal-fundamental measurements increase monotonically. The three named tine programs are the detailed targets; Analog Piano and Reed Piano are controls. These measurements are not a sweep of all 17 tine-enabled programs.

All **45** independently retained baseline-parity case hashes match their candidate counterparts. They cover 15 full-strike endpoints plus 30 additional soft-to-firm control cases, without double-counting overlapping endpoints. Each case contains 48,000 stereo frames, or **96,000 float samples**: **4,320,000 float samples** total. FNV1a64 equality is the recorded audio comparison; source/probe files have separate SHA-256 provenance.

## Direct-model parity scopes

- **A06: 10,137,600 samples.** The harness count recomputes as `(18 × 3 + 1) × 3 rates × 5 notes × 3 colours × 4096 samples`. Tine is tested at full strength; the other 18 enabled models use three velocities. This harness compares binary32 bits. The retained summary reports zero differences; I did not rerun it or manufacture a missing standalone stdout log.
- **A10: 36,115,200 samples.** The count recomputes over Off plus 19 enabled models, three notes, three rates and 1.6 seconds of direct-body output at twice host rate. Bowed uses fixed pitch; the other models slide seven semitones. The actual retained final log reports zero unequal values and zero maximum absolute difference. **This harness uses numeric float comparison, not bitwise comparison.** Its result supports the current prose, “no changed samples”; do not upgrade this particular result to a byte-identical claim.

These are direct-body mono sample positions, unlike the stereo full-preset float counts. Adding the totals would obscure scope and would not create a realism score.

## Bowed harmonic-response errors

Recomputed from `a10/final-bowed-results.log`. Each result is the maximum absolute error among six admitted harmonic ratios, compared with a separately played target note, in three specific octave-slide cases:

| Slide | Body size | Previous maximum | Corrected maximum |
|---|---:|---:|---:|
| MIDI 36 → 48 | 1.8 | 4.096524 dB | 0.025124 dB |
| MIDI 48 → 36 | 1.8 | 4.098064 dB | 0.002855 dB |
| MIDI 72 → 84 | 1.0 | 6.193136 dB | 0.003345 dB |

Thus **4.10–6.19 dB → 0.003–0.025 dB** is correctly rounded. This is the range of three per-case maxima, not a comprehensive physical-instrument error bound or a recording comparison.

## Runtime and storage

The **final** isolated-body raw log contains 11 interleaved pairs in each condition. Medians of the reported pair ratios reproduce **1.1243× moving bowed**, **1.0113× fixed bowed**, **0.9879× fixed organ** and **0.9825× moving organ**. Recomputing ratios from the rounded raw CPU seconds gives the same displayed medians. The older `bowed-results.log` has different timings and must retain its historical label.

The full-Engine CSV has **112 runs / 56 pairs**, or seven pairs across four preset/control cases and two pitch conditions. Recomputed moving medians are **1.0983975× Solo Violin**, **0.9920456× Solo Cello**, **1.0226162× Ensemble Strings**, and **0.9875837× bodies-off control**. Every run records eight active voices, finite metrics, and maximum output peak no greater than **0.5238830447**. All bodies-off pair audio hashes match.

These are noisy shared-machine thread-CPU comparisons. The isolated-body case is not full-Engine cost; the Engine probe excludes initialization and native host wrappers. Fixed Engine storage changes from **478,632 to 491,432 bytes**, exactly **12,800 bytes**. Current report language correctly preserves those limits and does not claim zero runtime cost.

## Release wording boundary

The current revision report correctly distinguishes changed signal behavior from acoustic identity. Keep the 68-program configured reach, five-program detailed dynamics sweep, historical 521-program matched comparison and 74-program audition roster separate. None constitutes universal listening coverage or proof of an indistinguishable real instrument.
