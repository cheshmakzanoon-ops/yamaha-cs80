# A17 — complete Pads recipe review

Source base: `59d489d1a1f0065ae2a0f42a08c74edc43e94159`. Reviewed all 30 curated Pads, host indexes 282–311, using the final exported inventory and source routing. No production edits.

## Outcome

No additional definite octave, envelope, controller-routing or named-motion defect found. All 30 entries use circuit excitation (both body-model selectors are Off), so the current tine/bowed body fixes do not apply to these pad recipes. Each main rank sustains; the differently timed upper ranks retain their intended roles. These checks establish recipe coherence, not subjective audio quality or acoustic realism.

## Complete checklist

| Host index | Existing program | Amplifier attack I / II (s) | Release I / II (s) | Verified role / routing |
|---:|---|---:|---:|---|
| 282 | Slow Pad | 0.95 / 1.68 | 4.1 / 4.7 | Warm saw precedes PWM upper rank; pressure preferentially brightens upper layer. |
| 283 | Ring Drift Pad | 0.43 / 1.32 | 3.2 / 4.2 | 0.55 Hz ring carrier, nonzero depth and amount; separate pulse halo remains audible. |
| 284 | Choir Pad | 0.12 / 0.21 | 1.65 / 1.85 | Two active resonant pulse bands; explicitly synthesized vowel, not sampled voices. |
| 285 | Dark Choir | 0.26 / 0.53 | 2.6 / 2.15 | 16-foot lower vowel and 8-foot nasal upper band; unequal attacks and modest detune. |
| 286 | Sweep Pad | 0.5 / 0.98 | 3.7 / 4.2 | Filter attacks 2.75/3.85 s support two slow crests; amplifier speaks earlier. |
| 287 | Breathing Pad | 0.24 / 0.64 | 2.35 / 2.75 | 0.7 Hz SUB VCA depth 0.38 plus shallow filter motion; separate 0.107 Hz PWM. |
| 288 | Glass Pad | 0.42 / 1.05 | 3.6 / 2.7 | 8-foot body with quieter 2-foot sine/pulse glint and lower upper sustain. |
| 289 | Warm Pad | 0.28 / 0.67 | 3.2 / 2.7 | Low-pass saw/sine foundation, quiet pulse, compact chorus and modest pressure brightness. |
| 290 | Wide Pad | 0.32 / 0.78 | 3.8 / 3.4 | Explicit left/right pan, distinct PWM rates, 9.3-cent detune and active chorus. |
| 291 | Octave Pad | 0.52 / 0.95 | 3.9 / 2.3 | 16-foot and 8-foot ranks form the claimed octave; upper release shorter. |
| 292 | Fifth Pad | 0.48 / 1.05 | 3.1 / 2.7 | 8-foot and 5 1/3-foot ranks form a 3:2 fifth; zero inter-rank detune. |
| 293 | Evolving Pad | 0.48 / 1.22 | 4 / 4.9 | 0.7 Hz rising SUB filter motion plus independently slow upper PWM. |
| 294 | Noise Pad | 0.39 / 1.6 | 2.9 / 4.2 | Pitched saw and isolated band-passed noise; noise enters/releases later. |
| 295 | Pressure Pad | 0.18 / 0.44 | 2.6 / 2.9 | Nonzero quiet base bed; pressure level 0.90/0.96 and brightness 0.24/0.40 work. |
| 296 | Sine Pad | 0.62 / 1.45 | 4.2 / 3.6 | Sine-only 8/4-foot octave; filters bypassed naturally, pressure level only, chorus/noise off. |
| 297 | Hollow Pad | 0.31 / 0.56 | 2.9 / 2.45 | Near-square pulse pairs with shallow independent PWM and distinct filtering. |
| 298 | Resonant Pad | 0.38 / 0.84 | 3.5 / 2.9 | Upper high-resonance filter has later crest and shorter decay; lower saw steadier. |
| 299 | Pulse Pad | 0.32 / 0.66 | 2.7 / 3 | Pulse-only ranks, independent PWM depth/rate and explicit stereo spread. |
| 300 | Tremolo Pad | 0.26 / 0.51 | 2.2 / 2.55 | 3.8 Hz SUB VCA motion is real; BBD tremolo is additional stereo modulation. |
| 301 | Neo-Noir Pad | 0.62 / 1.17 | 6 / 4.1 | Dark lower body, later upper glow, explicit stereo pan and faint slow ring amount. |
| 302 | Rain Pad | 0.29 / 0.96 | 2.65 / 3.7 | LFO waveform5 is sample-and-hold, rate8 Hz, destinationfilter and nonzero depth. |
| 303 | Cathedral Pad | 0.84 / 2.1 | 6.4 / 5.2 | 16-foot main plus 4-foot sine is two octaves; long releases, no reverb effect. |
| 304 | Shimmer Pad | 0.48 / 1.46 | 3.1 / 3.4 | 8-foot body and late 2-foot pulse; open upper LP preserves its high registration. |
| 305 | Low Drone | 1.3 / 1.9 | 5.2 / 3.5 | Both16-foot; chorus off, tiny drift/detune, low main HP and nonzero held sustain. |
| 306 | Ether Pad | 0.45 / 1.8 | 3.6 / 5.4 | Pure sine main with separately filtered noise rank; late noise envelope/pressure route. |
| 307 | Velvet Pad | 0.46 / 0.76 | 3.4 / 2.3 | Two filtered saw ranks; upper release2.3 s versus lower3.4 s. |
| 308 | Analog Haze | 0.58 / 1.12 | 3.7 / 4.1 | Broader detune/PWM/dispersion than restrained pads, all explicitly described. |
| 309 | Bowed Glass | 0.78 / 1.24 | 3.5 / 4.6 | Sine-only8/4-foot ranks, fixed217 Hz ring sidebands; not the bowed-string model. |
| 310 | Night Pad | 0.84 / 1.52 | 5.1 / 3.9 | Dark main LP680 Hz and upper1280 Hz; slow entrances, lower long tail. |
| 311 | Arctic Pad | 0.35 / 0.92 | 3.4 / 3.2 | 8/4-foot saw/pulse bands, distinct HPs and explicit pan spread under chorus. |

## Pressure Pad rendered check

Matched fresh complete-preset captures: MIDI60, velocity0.8, 48kHz, three-second hold plus one-second release. Only pressure differs (0 versus1). No WAV normalization or recipe adjustment.

| Metric | Pressure0 | Pressure1 | Difference |
|---|---:|---:|---:|
| Peak dBFS | -29.382 | -13.703 | +15.678 dB |
| Whole-clip RMS dBFS | -39.204 | -24.843 | +14.361 dB |
| Body-window RMS dBFS | -37.265 | -23.541 | +13.723 dB |

Both captures are finite and have zero samples above full scale. Pressure Pad deliberately keeps a soft low-pressure bed; it should not be described as a silent pressure-only lead. The rendered contrast supports the current description.

## Relevant implementation references

- `src/core/CuratedLibrary.inc`: fluent helpers write the rank/global controls directly.
- `src/core/ModulationLfo.h`: waveform5 is sample-and-hold.
- `src/core/Engine.cpp`: modern filter LFO and SUB VCF enter brilliance; SUB VCA enters the circuit exponential gain; sine bypasses HP/LP; nonzero ring amount remains active even when ring frequency-envelope depth is zero.
- `src/core/curated/Pads.inc`: thirty stable program definitions.

No additional sound change recommended by this focused review. Broader listening comparisons and root full-bank headroom qualification remain separate.
