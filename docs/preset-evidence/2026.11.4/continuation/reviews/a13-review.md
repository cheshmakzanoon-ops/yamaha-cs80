# A13 — Brass recipe review

Source: `59d489d1a1f0065ae2a0f42a08c74edc43e94159` family recipes and retained final inventory. Read-only review; no production edits. Current tine-only header modification does not affect this family.

## Decision

**No additional definite octave, model-routing, articulation or controller defect found across all 32 recipes.** The recipes deliberately mix acoustic-like brass bodies and circuit synthesizer programs. This review checks consistency with their descriptions; it does not establish acoustic similarity or a listening judgment.

| Program | Source line | Registration I / II | Active body mix I / II | Checked role |
|---|---:|---|---|---|
| Brass Swell I | 30 | 8′ / 8′ | 0.55 / 0.28 | Staggered amp/filter rises; pressure brightness/level and wheel SUB are connected. |
| Brass Swell II | 37 | 16′ / 8′ | 0.64 / 0.39 | 16-foot lower / 8-foot upper split matches description; upper pressure route stronger. |
| Pressure Swell Brass | 46 | 8′ / 8′ | 0.68 / 0.35 | Low base level with .92/.96 pressure-level amounts supports pressure-led swell. |
| Resonant Swell Brass | 55 | 8′ / 8′ | 0.00 / 0.00 | Circuit-only resonant upper rank has shorter bloom and lower held level. |
| Noir Swell Brass | 61 | 8′ / 8′ | 0.00 / 0.00 | Circuit-only dark swell; chorus, initial scoop and pressure SUB enabled intentionally. |
| Sforzando Brass | 69 | 8′ / 8′ | 0.76 / 0.52 | Brief upper rank has zero sustain; lower rank sustains .35 after attack. |
| Brass Stab | 78 | 8′ / 8′ | 0.68 / 0.00 | Both ranks have zero sustain and short releases; no pressure vibrato. |
| Horn Section | 86 | 8′ / 8′ | 0.93 / 0.74 | Both brass bodies have different size/color; no chorus and restrained detuning. |
| Covered Horn | 96 | 8′ / 8′ | 0.98 / 0.55 | Larger/darker body and low mix; pressure level exceeds brightness amounts. |
| Muted Trumpet | 106 | 8′ / 8′ | 0.91 / 0.00 | Smaller brighter brass body plus high-passed resonant circuit edge; zero detune. |
| Trombone Glide | 116 | 16′ / 16′ | 1.00 / 0.87 | Both ranks at 16-foot; .072s glide and negative transient produce upward scoop; no chorus. |
| Fanfare Brass | 126 | 8′ / 4′ | 0.82 / 0.42 | 8-foot root/4-foot crown; crown is shorter and less sustained. |
| Brass Ensemble 16 | 135 | 16′ / 8′ | 0.78 / 0.56 | 16-foot foundation/8-foot top, staggered attacks and shallow chorus match ensemble role. |
| Low Brass | 143 | 16′ / 16′ | 0.96 / 0.82 | Two 16-foot ranks; large bodies and low HP preserve low register; no always-on SUB VCO. |
| Velvet Brass | 153 | 8′ / 8′ | 0.54 / 0.00 | Soft brass body/circuit halo; long envelopes, shallow PWM and chorus are intentional. |
| Bright Brass Poly | 161 | 8′ / 8′ | 0.00 / 0.00 | Circuit-only fast comping; upper transient shorter and lower sustain. |
| Brass Pad Swell | 168 | 8′ / 8′ | 0.52 / 0.00 | Long 1.05/1.65s amp rises and 1.30/1.82s filter rises; no premature struck decay. |
| Fifth Brass | 175 | 8′ / 5⅓′ | 0.66 / 0.00 | 5 1/3-foot supporting rank correctly gives fifth; mix .19 and sustain .44 subordinate it. |
| Wah Brass | 182 | 8′ / 8′ | 0.00 / 0.00 | Pressure SUB VCF .31 drives wah; baseline VCF SUB is zero; wheel pitch route remains. |
| Synth Horns | 189 | 8′ / 8′ | 0.00 / 0.00 | Saw/pulse circuit synthesis intentional; low-rate PWM upper rank is audible. |
| Brass Bite | 195 | 8′ / 8′ | 0.00 / 0.00 | Velocity-brilliance and level emphasize short upper resonant accent. |
| Distant Brass | 202 | 8′ / 8′ | 0.58 / 0.00 | Dark body, later circuit entry and long tail; shallow chorus is intentional distance cue. |
| Brass Crescendo | 210 | 8′ / 8′ | 0.69 / 0.43 | 1.6s lower /2.15s upper amp rise; 2.2s upper filter bloom and strong pressure level. |
| Cuivre Brass | 218 | 8′ / 8′ | 0.89 / 0.00 | Upper narrow circuit edge has stronger after-brilliance/level than lower body. |
| Tuba | 227 | 16′ / 8′ | 1.00 / 0.00 | Only main 16-foot rank audible; largest body 2.2, dark color. Full-body route makes oscillator literals dormant. |
| Brass Octaves | 234 | 8′ / 4′ | 0.66 / 0.43 | 8-foot/4-foot registration and later/quieter upper envelope match octave layering. |
| Swell And Fall | 242 | 8′ / 8′ | 0.54 / 0.00 | Large lower filter initial offset restores dark release while VCA continues longer. |
| Reed Brass | 248 | 8′ / 8′ | 0.79 / 0.46 | Actual oboe body in supporting rank supplies reed color; brass remains primary. |
| Brass Glide Lead | 257 | 8′ / 8′ | 0.76 / 0.00 | .065s glide plus .06-semitone upward scoop; zero detune and low upper mix. |
| Tremolo Brass | 265 | 8′ / 8′ | 0.52 / 0.00 | 4.6Hz SUB VCA and stereo tremolo both enabled; small pressure vibrato remains. |
| Contour Brass | 272 | 8′ / 8′ | 0.68 / 0.00 | Explicit brilliance/level contours are connected to circuit+body signal path. |
| Ring Breath Brass | 279 | 8′ / 8′ | 0.52 / 0.00 | Ring amount .065 is active; zero ring frequency-envelope depth is intentional, not zero ring amplitude. |

## Existing limitations and dormant controls

- 26 of 32 programs have active instrument bodies (39 active ranks). Six programs are intentionally circuit-only. They should not be relabeled as acoustically measured brass.
- 29 active sustained ranks carry non-neutral body-decay values. `InstrumentBody::stringLoss()` returns 0 for sustained models, so these do not change audible decay. Do not count such changes as audible improvement; rank amp/filter envelopes still shape articulation.
- In Tuba and Trombone Glide rank I, body mix 1 replaces the oscillator/filter/sine source. Waveform, sine and VCF-output-level literals there are dormant. HP/LP filters, envelope, touch, rank output and body controls remain active.
- `bodySize` distinctions are authored resonance scales. The single brass model and shared contact/air model do not establish realistic trumpet, horn or tuba equivalence.
- Pitch-glide body-weight freezing is already tracked by a03/a10. This family uses brass bodies; a bowed-only correction must not be described as correcting brass glides.

## Routing references

- `CuratedLibrary.inc`: `glide()` sets portamento and glide mode; `ring()` separates frequency-envelope depth from amount.
- `Engine.cpp`: global glide gate at 848, onset glide at 1483, initial transient at 1487, running glide 3072, pitch addition 3136, body pressure 3294 and VCA/body blend 3361.
- `InstrumentBody.h`: brass weights and source-relative formant scale; sustained loss bypass; pressure/color smoothing.

## Non-neutral inactive sustained body-decay settings

- Brass Swell II, rank 1: 1.2×
- Pressure Swell Brass, rank 1: 1.15×
- Sforzando Brass, rank 1: 0.85×
- Sforzando Brass, rank 2: 0.7×
- Brass Stab, rank 1: 0.6×
- Horn Section, rank 1: 1.2×
- Horn Section, rank 2: 1.1×
- Covered Horn, rank 1: 1.3×
- Covered Horn, rank 2: 1.15×
- Muted Trumpet, rank 1: 0.8×
- Trombone Glide, rank 1: 1.2×
- Fanfare Brass, rank 1: 0.85×
- Fanfare Brass, rank 2: 0.75×
- Brass Ensemble 16, rank 1: 1.2×
- Low Brass, rank 1: 1.35×
- Low Brass, rank 2: 1.15×
- Velvet Brass, rank 1: 1.4×
- Brass Pad Swell, rank 1: 1.4×
- Fifth Brass, rank 1: 1.1×
- Distant Brass, rank 1: 1.5×
- Brass Crescendo, rank 1: 1.35×
- Brass Crescendo, rank 2: 1.2×
- Tuba, rank 1: 1.5×
- Brass Octaves, rank 1: 1.1×
- Swell And Fall, rank 1: 1.25×
- Reed Brass, rank 2: 0.9×
- Tremolo Brass, rank 1: 1.3×
- Contour Brass, rank 1: 1.15×
- Ring Breath Brass, rank 1: 1.3×

## Provenance

Brass.inc SHA256: `0eaba12d523d9475451dcb488e6ad0d0e96093bd365e436ce1ed79dc4df82cac`

No source, test, preset, commit or remote ref changed. No extra audio renders were required to assess these routing/literal contracts.
