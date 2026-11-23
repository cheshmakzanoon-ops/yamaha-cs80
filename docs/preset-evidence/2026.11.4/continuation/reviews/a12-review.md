# Agent 12: Strings recipe review

Base commit: `59d489d1a1f0065ae2a0f42a08c74edc43e94159`. Scope: all28 recipes authored by `buildStrings`, host indexes254–281. The separately authored Modern-category recipes that display Strings are outside this file-specific assignment.

Read-only review; no production modifications. Current tine-only correction does not change the inspected string models. Source inspection establishes routing/recipe consistency, not perceived acoustic similarity.

## Findings

- No additional definite model, octave, controller-route or named-articulation mismatch found. Acoustic-style programs use bowed or plucked bodies; explicitly synthetic programs retain circuit excitation.
- Solo Violin/Cello intentionally have no baseline pitch vibrato: aftertouch and wheel enable it. Their descriptions accurately explain pressure-controlled vibrato. This is not an absent route.
- Bowed model formants currently remain attached to onset spectral weights during bends/vibrato; agent10 owns the investigation. No duplicate prototype work performed.
- Sustained bowed `body*_decay` values are inert under current zero-loss model. They must not count as evidence of audible changes. Do not introduce amplitude decay merely to activate that control.
- Source chains retain overwritten earlier authoring values. Review used the final exported parameter vectors; earlier literals are not the shipped setting.
- Spiccato has unusually high base rank levels after the prior loudness correction. This reduces relative peak velocity contrast, already explicitly reported in the2026.11.4 revision report. Its excitation spectrum remains velocity sensitive.

## Complete checklist

| Program | Rank I/II footage | Body model/mix I; II | Amp attack I/II (s) | Sustain I/II | Release I/II (s) | Assessment |
|---|---|---|---|---|---|---|
| Ensemble Strings | 8′/8′ | Bowed 0.74; Bowed 0.48 | 0.14/0.26 | 0.88/0.8 | 0.75/1.05 | Bowed/circuit hybrid with staggered section entrances and restrained chorus; consistent. |
| Slow Strings | 8′/8′ | Bowed 0.78; Bowed 0.42 | 0.85/1.15 | 0.9/0.84 | 1.7/2.1 | 0.85/1.15 s amplitude entrances; 0.9/1.25 s filter blooms and long releases match name. |
| Violins | 8′/4′ | Bowed 0.84; Bowed 0.44 | 0.06/0.1 | 0.9/0.64 | 0.38/0.32 | 8-foot principal and quieter4-foot sheen; brighter bowed sizes and rapid articulation consistent. |
| Cellos | 16′/8′ | Bowed 0.94; Bowed 0.45 | 0.11/0.07 | 0.93/0.48 | 0.55/0.32 | 16-foot main,8-foot edge; larger bodies2.05/1.90; low HP retains fundamental. |
| Octave Strings | 8′/4′ | Bowed 0.58; Bowed 0.4 | 0.16/0.24 | 0.88/0.72 | 0.9/0.85 | 8/4-foot layers; secondrank later and quieter, consistent. |
| PWM Strings | 8′/8′ | Circuit 0; Circuit 0 | 0.11/0.23 | 0.9/0.75 | 0.6/0.88 | Intentionally circuit-only; distinct slow pulsewidth rates0.17/0.31; no acoustic model required. |
| Thin Strings | 8′/8′ | Circuit 0; Circuit 0 | 0.22/0.32 | 0.82/0.68 | 1/0.92 | Intentionally high-passed circuit section; distinct delayed upper envelope. |
| Dark Strings | 8′/8′ | Bowed 0.84; Bowed 0.45 | 0.36/0.5 | 0.91/0.84 | 1.4/1.7 | Bowed sizes1.60/1.35, reduced cutoff/color and slower articulation support dark label. |
| Tremolo Strings | 8′/8′ | Bowed 0.65; Circuit 0 | 0.08/0.055 | 0.9/0.6 | 0.5/0.2 | Final SUB is sine6.5Hz amplitude modulation, not earlier overwritten square; deliberate tremolo approximation. |
| Solo Violin | 8′/8′ | Bowed 0.98; Circuit 0 | 0.024/0.003 | 0.9/0 | 0.145/0.035 | Dry8-foot bowed98%, noise-only short rankII; pressure/wheel vibrato; no ensemble chorus. |
| Solo Cello | 16′/8′ | Bowed 0.99; Circuit 0 | 0.045/0.008 | 0.94/0 | 0.21/0.055 | Dry16-foot bowed99%, size2.10; noise-only short rankII, no octave tone; pressure/wheel vibrato. |
| Pizzicato Strings | 8′/4′ | Nylon 0.96; Steel 0.54 | 0.0012/0.001 | 0/0 | 0.21/0.045 | Nylon/steel plucked bodies; zero sustain bothranks and short releases; synthesis proxy for bowed-string pizzicato. |
| Swell Strings | 8′/8′ | Bowed 0.82; Bowed 0.36 | 0.28/0.42 | 0.9/0.84 | 1.2/1.5 | Initial-touch destinations disabled; strong aftertouch level/brightness for held-pressure swell. |
| Chamber Strings | 8′/8′ | Bowed 0.92; Bowed 0.75 | 0.08/0.105 | 0.88/0.82 | 0.5/0.42 | Dry two bowed ranks; sizes1.08/1.24, close1.4cent detune and staggered entrance. |
| Wide Strings | 8′/8′ | Bowed 0.58; Bowed 0.47 | 0.25/0.32 | 0.88/0.8 | 1.25/1.25 | Two hybrid sections with oppositepan,5.8cent detune, shallow chorus; deliberate ensemble width. |
| Crystal Strings | 8′/2′ | Bowed 0.64; Circuit 0 | 0.14/0.56 | 0.88/0.36 | 0.75/1.2 | 8-foot bowed main and quiet2-foot circuit glint, consistent with stated two-octave sheen. |
| Fifth Strings | 8′/5⅓′ | Bowed 0.74; Bowed 0.46 | 0.14/0.26 | 0.88/0.7 | 0.75/0.85 | f513 upper footage is3:2 relative to8-foot; subordinate mix0.20 preserves named fifth. |
| Analog Orchestra | 16′/4′ | Circuit 0; Circuit 0 | 0.25/0.31 | 0.9/0.72 | 1.2/1.15 | Intentionally circuit16/4-foot layers spanning twooctaves; no acoustic body claim. |
| Spiccato Strings | 8′/8′ | Bowed 1; Bowed 0.55 | 0.0065/0.003 | 0/0 | 0.065/0.04 | Bothranks zero sustain;0.30/0.11s decay and0.065/0.04s release. Raised baselevels reduce relative velocity amplitude range (already documented). |
| Bowed Swell Strings | 8′/8′ | Bowed 0.86; Bowed 0.56 | 0.55/0.8 | 0.9/0.82 | 1.6/2 | Amplitude rise precedes1.1/1.5s filter blooms; pressure intensity remains active. |
| Synth Strings Saw | 8′/8′ | Circuit 0; Circuit 0 | 0.14/0.27 | 0.92/0.84 | 1.1/1.35 | Intentionally circuit saws; separate sustained envelopes and chorus. |
| String Machine | 8′/8′ | Circuit 0; Circuit 0 | 0.032/0.055 | 0.985/0.97 | 0.52/0.65 | Intentionally circuit saw/ensemble approximation; fixed initialtouch and steady sustain consistent. |
| Contour Strings | 8′/8′ | Bowed 0.76; Bowed 0.48 | 0.18/0.3 | 0.88/0.8 | 0.95/1.2 | Keyboard-contoured hybrid bow sections, restrained treble-level reduction and darker lowrange. |
| Ring Strings | 8′/8′ | Bowed 0.58; Circuit 0 | 0.28/0.45 | 0.9/0.84 | 1.2/1.5 | 0.8Hz lowdepth ringcomponent behaves as slowlevel shimmer; no fixedmetallic pitch expected. |
| Velvet Strings | 8′/8′ | Bowed 0.84; Bowed 0.44 | 0.28/0.42 | 0.92/0.83 | 1.7/2.1 | Dark bowed/circuit layers with long1.70/2.10s releases and lowdepth chorus. |
| Tutti Strings | 16′/8′ | Bowed 0.84; Bowed 0.64 | 0.07/0.095 | 0.93/0.86 | 0.58/0.74 | 16-foot large mainbody plus8-foot brightersection, faster attacks and moderate chorus. |
| Marcato Strings | 8′/8′ | Bowed 0.9; Bowed 0.55 | 0.014/0.004 | 0.6/0 | 0.18/0.055 | Sustained bowed rankI plus zero-sustain short upper accent; correct articulation distinction. |
| Air Strings | 8′/8′ | Bowed 0.9; Circuit 0 | 0.12/0.024 | 0.88/0.09 | 0.8/0.18 | Bowed principal plus short filterednoise rankII with9% sustain; noise is strongly subordinate in measured fixture. |

## Scoped upper-noise masking check

Rendered Solo Violin, Solo Cello and Air Strings at MIDI60, velocity0.9, pressure0,48kHz,1.5s hold plus0.2s release. Rank captures set MIX to0 or1. The early window is0–200ms. Estimated final blend uses each recipe’s actual MIX. These are separate-rank diagnostics; they do not assert sample-exact additive reconstruction through downstream processing.

| Program | Isolated rankII−rankI early RMS (dB) | Estimated difference with factory mix (dB) | Conclusion |
|---|---:|---:|---|
| Solo Violin | -22.78 | -42.88 | Upper noise is subordinate; no broad onset masking indicated in this fixture. |
| Solo Cello | -16.77 | -35.86 | Upper noise is subordinate; no broad onset masking indicated in this fixture. |
| Air Strings | -25.88 | -44.04 | Upper noise is subordinate; no broad onset masking indicated in this fixture. |

Solo noise ranks decay below−169dBFS in the final held window; Air Strings retains a weak noise bed at−74.66dBFS when isolated. This does not establish audibility, ideal bow realism or balance outside this note/velocity. No listening claim is made.

## Evidence

- `a12/rank1/metrics.csv` and `a12/rank2/metrics.csv` contain actual capture results.
- `src/core/curated/Strings.inc`, `src/core/InstrumentBody.h`, `src/core/Engine.cpp`, and the final inventory are the reviewed sources.
