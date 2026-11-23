# Agent 16 — Bells recipe review

Scope: all 22 recipes in `src/core/curated/Bells.inc`, host indexes 458–479, preserved at base `59d489d1a1f0065ae2a0f42a08c74edc43e94159`. Final inventory used rather than overwritten intermediate helper values. No production files changed.

## Result

No definite accidental routing, octave, sustain, or ring-control error was found. Two concrete acoustic-model limitations remain: Xylophone uses the marimba mode profile; Steel Drum uses vibraphone modes plus an octave layer rather than a coupled steelpan model. These prevent claiming exact named-instrument equivalence. Simple cutoff/decay edits cannot relocate existing modes.

## Complete checklist

| Host index | Program | Models I / II | Feet I / II | VCA decay I / II (s) | VCA release I / II (s) | Review |
|---|---|---|---|---|---|---|
| 458 | Ring Bell I | Bell / Bell | 8′ / 4′ | 5.5 / 0.31 | 3.8 / 0.17 | Fixed 307 Hz carrier is active; short octave strike and long bell body match description. |
| 459 | Ring Bell II | Bell / Bell | 8′ / 2⅔′ | 3.65 / 0.23 | 2.4 / 0.13 | 191 Hz carrier; upper2⅔′ rank is3×main, the described twelfth; darker/faster than I. |
| 460 | Tubular Bells | Bell / Bell | 16′ / 2⅔′ | 8.4 / 0.49 | 6.1 / 0.31 | 16′ body and2⅔′ upper strike; long independent losses, small2.5% ring blend. Generic bar model is not a measured tube model. |
| 461 | Glockenspiel | Celesta / Bell | 4′ / 2′ | 4.3 / 0.13 | 2.8 / 0.075 | 4′/2′ high registration and hard celesta/metal attack; zero sustain, no ring carrier. |
| 462 | Celeste Bells | Celesta / Celesta | 8′ / 4′ | 4.1 / 0.28 | 2.6 / 0.17 | Soft celesta bodies, shallow chorus; separate faster upper accent; zero sustain. |
| 463 | Music Box | Celesta / Tine | 4′ / 2′ | 2.65 / 0.095 | 1.35 / 0.055 | Celesta main plus brief tine upper rank; tine correction affects only the quieter upper component. No ring carrier. |
| 464 | Gong | Bell / Bell | 16′ / 4′ | 10.5 / 1.35 | 7.3 / 0.85 | 16′ bell body, residual noisy upper circuit, slow onset and real carrier sweep; long tail. No nonlinear gong coupling model. |
| 465 | Temple Bell | Bell / Bell | 16′ / 5⅓′ | 8.7 / 0.78 | 6.2 / 0.47 | 16′ main and5⅓′ upper; slow metal modes, restrained carrier movement, long release. |
| 466 | Marimba | Marimba / Marimba | 8′ / 2′ | 2.7 / 0.1 | 1.25 / 0.065 | Marimba ratios1:4:10, fast upper losses; f2 strike is fourth harmonic of8′ main; no carrier. |
| 467 | Vibraphone | Vibes / Off | 8′ / 2′ | 6.4 / 1.2 | 3.8 / 0.65 | Vibes metal modes and circuit sine fourth-harmonic strike; actual5.2 Hz SUB amplitude modulation; no carrier. |
| 468 | Xylophone | Marimba / Marimba | 4′ / 2′ | 1.45 / 0.075 | 0.6 / 0.04 | Marimba modal profile1:4:10 with bright/short loss setting; expected xylophone-like1:3:6 profile absent (model limitation). |
| 469 | Crystal Bell | Bell / Celesta | 4′ / 2′ | 5.2 / 0.41 | 3.6 / 0.25 | High4′/2′ metal/celesta ranks, bright modal decay with deliberate ring color. |
| 470 | Bell Pad | Bell / Celesta | 8′ / 4′ | 5.5 / 0.44 | 5.4 / 2.7 | Only this Bells recipe retains nonzero sustain; residual sine/circuit bed supports claimed pad after struck modes die. |
| 471 | Ring Sweep Bell | Bell / Bell | 8′ / 2⅔′ | 5.9 / 0.25 | 4.1 / 0.15 | Actual carrier-depth 0.34 sweep and ring-amount 0.43; upper twelfth strike; no confused depth/amount. |
| 472 | Steel Drum | Vibes / Vibes | 8′ / 4′ | 2.65 / 0.19 | 1.15 / 0.115 | Vibes1:4:9.86 main plus separate octave rank; short damping and2.5% ring. Not a dedicated steelpan1:2:(3or4)coupled-modal model. |
| 473 | Chimes | Bell / Celesta | 4′ / 2′ | 8.1 / 1.32 | 5.8 / 0.84 | 4′/2′ octave metal courses, long unequal losses, only2.5%fixedcarrier. |
| 474 | Dark Bell | Bell / Bell | 16′ / 8′ | 6.8 / 0.42 | 4.7 / 0.26 | 16′/8′ low body/upperstrike, dark brightness, long tail; ring color intentional. |
| 475 | Bell Choir | Bell / Bell | 8′ / 5⅓′ | 6.3 / 3.1 | 4.7 / 2.1 | 8′/5⅓′ yields true3:2 fifth; unequal decay and light chorus; low ring amount. |
| 476 | Metallic Keys | Reed / Bell | 8′ / 4′ | 2.65 / 0.2 | 1.3 / 0.12 | Reed main plus bell upper and residual saw; explicit synthetic name/description consistent. |
| 477 | Agogo | Bell / Bell | 4′ / 2⅔′ | 1.3 / 0.115 | 0.55 / 0.07 | 4′/2⅔′ pitched hollow clang, short modal losses and pulse resonance; authored bell approximation. |
| 478 | Clock Bells | Bell / Bell | 4′ / 2⅔′ | 4.9 / 2.85 | 3.3 / 1.9 | 4′/2⅔′ ranks are2f0 and 3f0, therefore true3:2 fifth, correctly described. |
| 479 | Ring Mallet | Marimba / Bell | 8′ / 2⅔′ | 2.7 / 0.17 | 1.35 / 0.1 | Marimba main plus bell twelfth attack, quick upper losses and audible ring amount 0.22. |

## Sounding and numerical check

All 22 short C4 captures at velocity 0.8, 48 kHz, 0.3 s hold plus 0.2 s tail were finite, non-silent and free of full-scale samples. Peaks ranged from -26.54 to -20.62 dBFS. This is a brief sounding check, not complete long-tail/phrase/headroom qualification and not an acoustic similarity test. Root owns full-bank qualification. `a16/provenance.json` binds the actual tine-fixed executable; later source edits do not alter that captured binary.

## Primary-source basis for model limitations

- Beaton & Scavone, JASA 149(6), 2021, 3758–3768, DOI 10.1121/10.0005062, introduction: common tuned ratios 1:4:10 for marimba/vibraphone and 1:3:6 for xylophone. https://caml.music.mcgill.ca/lib/exe/fetch.php?media=publications:beaton_idiophonebartuning_jasa_2021.pdf . Current `makeBarModes()` has only the marimba profile for Xylophone; both ranks select it.
- Touzé et al., “Identification of mode couplings in nonlinear vibrations of the steelpan”, Applied Acoustics, 2014: measurements describe second mode 2× fundamental and third 3× or 4×, with nonlinear energy exchange contributing to character. https://www.sciencedirect.com/science/article/pii/S0003682X14002151 . Primary abstract was available in search; publisher full-page retrieval returned 403. Current Steel Drum retains a brief separate octave but not that coupled modal system.

## Other routing observations

- Marimba and Vibes models have genuinely different losses: marimba fundamental 0.65 relative loss and upper 3i+2i² versus vibes 0.09 and 0.9i+0.6i², before register/partial-decay scaling. Wood/metal distinction therefore goes beyond just VCA duration.
- Music Box main body is Celesta, not Tine. The tine correction affects its0.13 mixed upper rank with 0.92 body mix and short 0.095 s VCA decay. It preserves that model’s full-strike endpoint.
- At body mix 1, oscillator-wave/sine/VCF-output-level settings are dormant; panel filters, rank envelopes and touch remain active. Such dormant literals must not be credited as audible improvement.
- Bell Pad has nonzero sustain because it deliberately combines decaying body modes with residual sustained circuit/sine signal. Other 21 Bells recipes have zero sustain in both ranks.
- Gong/TempleBell/Agogo/Tube/Steel names refer to broad authored spectral approximations. No tests in this review establish calibrated shell/tube/plate behavior or perceptual indistinguishability.
