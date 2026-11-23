# Agent 19 — Bass recipe review

Scope: all 24 curated Bass recipes, host indexes 404–427, starting main `59d489d`. Read-only production audit; current capture executable was copied and SHA256-bound before rendering. This is routing and measured signal evidence, not an acoustic-match or subjective listening claim.

## Finding

No additional definite octave, articulation, controller-routing or low-end phase defect found. All programs retain the 16-foot rank-I foundation. All have zero rank detune, centered authored pan and chorus disabled. Most use restrained dispersion 0.08/drift0.025; Sine Bass, Sub Bass and Pedal Bass explicitly disable both. These are not blanket claims that every multivoice performance is phase-identical.

## Measured checks

- 24 complete preset captures: MIDI48 (16-foot root C2), velocity0.85, 48kHz, 0.45s hold plus0.15s release.
- All finite, all below fullscale. Peak range **−27.620 to−13.428 dBFS**.
- Stereo correlation **0.999999938–1.0**; worst mono-fold power loss versus mean stereo power **0.000153dB** in this fixture. No meaningful cancellation found.
- Pedal Bass at velocities0.15 and0.85 produces **byte-identical WAVs**, confirming the full tested patch’s intended velocity neutrality.
- These short, single-note captures do not replace the parent’s long held, chord, repeated-note or library-wide headroom qualification.

## Recipe checklist

| Host | Existing program | Source line | Checked final behavior |
|---:|---|---:|---|
| 404 | Analog Bass | 23 | 16′ saw/sine foundation; separate 8′ pulse settles to 0.09 sustain; upper mix 0.19. |
| 405 | Resonance Bass | 30 | 16′ sine-supported body with more resonant 8′ sweep; upper sustain 0.055 and mix 0.17. |
| 406 | Sine Bass | 37 | Pure 16′ sine body at full sustain; high-passed 8′ click has zero sustain and 22 ms release; zero drift/dispersion. |
| 407 | Sub Bass | 45 | Only rank I audible, pure 16′ sine; 15 ms onset, full sustain, no drift/dispersion or aftertouch. |
| 408 | Pluck Bass | 51 | 95% bass body plus short steel pick; both VCA sustains zero, upper nominal HPF420Hz and mix0.18. |
| 409 | Fretless Glide | 60 | Both ranks16′; bass bodies with differing partial losses; 46 ms portamento and pressure/wheel vibrato; zero sustain. |
| 410 | Funk Bass | 70 | Bass/clav bodies blended with resonant circuit; both sustain0; strong initial touch and brief upper snap. |
| 411 | Brass Bass | 79 | 16′ circuit foundation plus8′ brass body, distinct bloom times and active aftertouch; deliberately hybrid. |
| 412 | Octave Bass | 87 | 16′/8′ saw registrations correctly form octave; upper sustain0.53 with0.25 mix and zero detune. |
| 413 | Pulse Bass | 94 | Both ranks16′ pulses with independent shallow PWM; sine on mainrank stabilizes lowbody; no pitch SUB. |
| 414 | Round Bass | 102 | Half bass body blended with sustaining sine/saw; same-register upper edge; low resonance. |
| 415 | Velocity Bass | 111 | Low static VCA levels, strong initial level/brilliance, short upper accent; named velocity role active. |
| 416 | Rubber Bass | 118 | Positive initialbend creates upward scoop; both ranks zero sustain; bass body is a small blend, no continuous pitch SUB. |
| 417 | Dark Bass | 126 | 16′/16′ covered saw/sine with partial bass body; low resonance, slowish onset and short release. |
| 418 | Bright Bass | 135 | 16′ foundation with brighter 8′ saw attack, upper HPF235Hz nominal, restrained sustain/mix. |
| 419 | Picked Bass | 142 | Full bass main body and short steel/circuit pick; both zero sustain, upper release26ms and HPF580Hz nominal. |
| 420 | Upright Pluck | 152 | Full low bass body with larger resonant size; same-register nylon/circuit-noise transient; both zero sustain. |
| 421 | Fifth Bass | 162 | 16′/5⅓′ gives upper twelfth (3:1), exactly as description; upper mix0.17 and HPF210Hz nominal. |
| 422 | Pedal Bass | 169 | Pure16′/8′ sine stops, initial/aftertouch disabled, no drift/dispersion; measured velocity independence. |
| 423 | Squelch Bass | 176 | Sine-supported16′ body and strongly resonant8′ sweep; short upper envelope, no unplanned detune/chorus. |
| 424 | Ring Bass | 183 | 35Hz ring amount0.13 remains active despite frequency-envelope depth0; predominantly dry centered bass remains. |
| 425 | Sub Wobble Bass | 190 | 3Hz SUB routes to filter only, not pitch; bypass sine remains unaffected by filter wobble. |
| 426 | Pressure Bass | 199 | Nonzero immediate main bed; upper pressure brilliance0.56 and level0.76 active; not a silent pressure-only patch. |
| 427 | Contour Bass | 207 | Keyboard brightness/level contours active, independent rank decay and short upper attack; stable zero detune. |

## Remaining model limitations and inactive controls

- **Fretless Glide uses the plucked-bass spectral model.** Its gliding pitch is correctly active, but the plucked body’s resonance weighting still transposes with the captured spectrum. The parent’s bowed-only stationary-formant correction must not be described as correcting Fretless Glide. No measured pickup, fingerboard or body-resonance fit is established.
- **Upright Pluck** uses the larger-sized generic plucked-bass body plus a nylon/noise onset, not a calibrated upright-bass mechanical model. The description correctly calls it an authored string spectrum.
- **Brass Bass rankII** has non-neutral partial decay1.05, which is inactive for its sustained brass body. Its VCA/filter envelopes and aftertouch remain active; this parameter alone must not count as audible refinement.
- At body mix1, the main-rank oscillator/sine/VCF-output-level settings are dormant in Fretless Glide, Picked Bass and Upright Pluck. HP/LP filters, touch, body brightness and VCA articulation remain active.
- Ten of the24 recipes use body models; none uses tine or bowed bodies, so the two current body corrections should not be described as directly revoicing these24 recipes. They retain the preceding2026.11.4 recipe overhaul.

## Evidence

`a19/summary.json` contains per-capture mono/stereo analysis and the Pedal Bass identity result. `a19/all/metrics.csv`, `a19/soft-pedal/metrics.csv`, WAVs, capture logs and `a19/provenance.json` retain the exact source/executable identity. No tracked files, presets, tests, commits or remote refs changed.
