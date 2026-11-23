# A36 — evidence interpretation audit

Read-only review of `docs/preset-evidence/2026.11.4/audio-comparison.json`, its final-bank provenance, continuation a09/a10 findings and source manifests, and `docs/CIRCUIT_LIBRARY_2026_11_4.md`. No DSP, presets, tests, production documentation or Git state changed; no new rendering performed.

## What the complete matched comparison establishes

The 24 recorded candidate source hashes all match commit `59d489d1a1f0065ae2a0f42a08c74edc43e94159` exactly. The baseline is `fa57967c7317828cf769cc7ca97f5857e566520f`. This is a historical development-source comparison, before the current continuation fixes.

| Measure | Verified count |
|---|---:|
| Ordered programs in the base comparison | 523 |
| Musical programs, excluding both Init entries | 521 |
| Changed audio metric vectors at pressure 0 | 519 |
| Additional pressure-only leads changed at pressure 1 | 2 |
| Musical programs with changed spectral metric group | 521 |
| Musical programs with changed relative-envelope metric group | 521 |
| Musical programs with changed stereo metric group | 461 |
| Musical programs classified as level-only | 0 |
| Changed master-volume parameter | 114 |

Electric Leads 136 and Pressure Lead are intentionally silent at pressure 0. Their two pressure-1 captures are needed for the 521 claim. Both Init aggregate metric vectors remain exactly equal; aggregate equality alone is not a sample-parity assertion. The parameter digest changes even for Init because appended fields are included, so it is not an adequate change-coverage test by itself.

All comparisons use velocity approximately 0.8, one category-selected note, 48 kHz, a 3-second hold and 1-second tail. The JSON reports zero nonfinite and over-full-scale sample flags. Its 192,000 frames per render are derived from the requested conditions; observed WAV lengths are not present in those CSVs.

“Changed spectral group” means at least one group measurement clears its CSV rounding tolerance. It does not mean every spectral field changes materially. Absolute centroid shifts range from about 0.00000002 to 2,607.96 Hz; median 24.47 Hz. Absolute whole-clip RMS shifts have median 1.103 dB, maximum 5.332 dB. Absolute body-minus-early-RMS changes have median 0.916 dB across 518 defined ratios. These distributions are descriptive measurements, with no inferred perceptual or realism thresholds. The 3 undefined body/early ratios touch the numeric floor. The spectral window can move to a different part of each sound.

Consequently, the supported statement is: **all 521 musical programs show measured changes beyond simple aggregate level metrics in the documented fixture**. This does not establish that all 521 improved perceptually, by an equal amount, or became accurate reproductions of real instruments. Nonlinearity can also turn level changes into spectral differences; these measurements do not isolate the cause. Independent parameter coverage—not this audio table—establishes reprogramming of existing non-volume controls.

## Continuation evidence has a narrower, meaningful scope

The a09 inventory identifies 17 existing programs / 28 active tine ranks. Detailed complete-preset tests cover three tine programs and two non-tine controls, three registers and six velocities: 90 candidate captures. The measured approximately 3.9–4.0 dB expansion of soft-to-firm body contrast results from removing unintended soft-strike gain; hard-strike endpoints are preserved. Forty-five full-preset parity cases match their audio hashes. These are dynamic-response and selected parity results, not acoustic similarity scores.

The a10 bowed fix tests physical-frequency stability of authored body formants. Its 492-check regression passes, with 174 failures on the preceding model. The 4.10–6.19 dB to 0.003–0.025 dB harmonic-ratio-error reduction compares sliding to a target pitch against playing that target directly; it is not a violin-recording match. The exact 36,115,200-sample comparison covers fixed-pitch bowed output and the other models under specified slides. Its final header hash matches the current `d33f34f5…` header. The a09 earlier `19f9fb23…` header is explicitly a scoped predecessor, preserved as a10's baseline.

The report identifies 51 programs with active bowed ranks. Together with the 17 tine programs these continuation fixes affect the configured model behavior of 68 existing programs (the root's inventory review establishes disjoint sets), without changing factory parameter vectors. This does not mean 68 complete presets each received a detailed listening test.

## Wording and provenance recommendation

The main report already states that these are authored synthesized interpretations and that measurements do not establish audibility thresholds, realism scores or indistinguishability. Its audition roster correctly says **138 WAVs / 74 existing programs / 870 seconds**, alongside a complete 523-program inventory. Do not convert that roster into “all 523 auditioned/listened to,” and do not call generated auditions a human listening assessment. Archive availability and exact contents require the separate current CI artifact verification.

One clarification is advisable before final release: the heading **“Final local bank and matched-audio checks”** and its **281/281 voicing** count refer to the historical `59d489d` stage. Explicitly label that section as the pre-continuation result. The original audio-comparison manifest differs from the current source in `InstrumentBody.h` (real DSP changes) and `curated/Fx.inc` (description correction). Its embedded “final-source” / “native qualification pending” strings remain valid historical provenance, but are not current-release status. Keep those original evidence records intact and explain the boundary in the narrative. Current-source bank reruns, continuation regressions, native qualification and the historical whole-library comparison should retain their distinct source identities.

No other blocking perceptual overclaim was found in the reviewed report. Keep “significantly improved” as a design goal or a claim tied to a specific repaired behavior, rather than asserting a measured universal improvement or an acoustic match across the library.
