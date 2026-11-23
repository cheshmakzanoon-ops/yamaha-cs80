# Agent 23 — Modern bank source and routing review

Reviewed all 18 recipes in `src/core/curated/Modern.inc`, host indices 504–521, against the final exported 200-parameter inventory, arpeggiator and LFO execution, and actual classic/modern effects. Source commit: `c4fe30987a5242305be60bd6cfddb144facd6643`.

**Finding:** No definite named-identity, wrong-model, inactive featured-route, or unsafe authored feedback defect found in these 18 recipes. No production files edited. This is a source/inventory/math review; it does not establish perceptual realism, listening preference, or exact acoustic-reference matching.

| Index | Existing preset | Checklist / verified behavior |
|---:|---|---|
| 504 | Delay Brass | Two brass bodies and pressure expression; tape echo ≈375 ms at 19% wet, then shallow plate at 6%. Short dry releases leave echo distinct. |
| 505 | Plate Pad | Circuit pulse and octave saw with deliberately staggered entrances; modern plate is active at 22.5% wet. Original chorus is off. |
| 506 | Phaser Strings | Bowed bodies, later upper entrance, modest detune; active classic six-stage phaser at 29%. **Receives stationary bowed-formant correction.** |
| 507 | Drive Lead | Circuit pulse and short saw accent, 18 ms glide, pressure vibrato; active modern drive at 36%, stage 2 oversampling. |
| 508 | Arp Poly | Up arpeggio at 8 notes/sec over two octaves; 58% gate = 72.5 ms. Active 1 Hz triangle filter motion and 4′ upper rank. |
| 509 | Arp Bells | Up/down at 4 notes/sec over two octaves; 145 ms gates. Bell bodies, short 2⅔′ third-harmonic accent, ≈375 ms tape echo. |
| 510 | Arp Bass | Up arpeggio at 6 notes/sec over one octave; 96.7 ms gates. 16′/8′ bass ranks; 1.5 Hz filter cycle spans four arpeggio steps. |
| 511 | LFO Sweep Pad | Active 0.10 Hz triangle filter sweep and 0.07 Hz smooth-random pan motion. Shallow original chorus; these LFOs do not modulate pitch. |
| 512 | LFO Pan Keys | Tine bodies, high-passed short octave strike; active 1 Hz triangle pan at depth .72 (±.504 engine pan modulation), shallow plate. **Receives corrected tine strike dynamics.** |
| 513 | PWM LFO Strings | Both pulse ranks receive the .22 Hz triangle PWM LFO; depth .56 gives ±.1008 width modulation around .53/.60 biases. Native PWM off; original chorus off. |
| 514 | Stereo Delay Pluck | Steel bodies and a short high-passed octave pick; active stereo cross-feedback tape echo ≈250 ms at 16% wet. |
| 515 | Ambient Bells | Bell bodies, 5⅓′ fifth above 8′ main, long main/short upper strike decay. ≈560 ms tape echo then modern plate; struck source remains distinct from effects tails. |
| 516 | Ensemble Plus | Two bowed ranks with staggered entrances, shallow original chorus plus active three-tap-per-channel modern ensemble at 21% wet. **Receives stationary bowed-formant correction.** |
| 517 | Space Lead | Circuit lead with 90 ms glide and pressure vibrato; ≈400 ms tape echo at 19% wet followed by shallow plate. Dry releases remain short. |
| 518 | Random Arp | Random arpeggio at 6 notes/sec over two octaves; 45% gate = 75 ms. Active 3 Hz sample-and-hold filter modulation, short releases. |
| 519 | Drive Bass | 16′ pulse/sine low rank plus brief 8′ saw rasp, zero detune and low voice variation. Active modern drive at 24.5%, stage 2 oversampling. |
| 520 | Phased Polysynth | Circuit saw main and sharper pulse accent; active classic six-stage phaser at 32% plus 7% modern plate. Original chorus off. |
| 521 | Tremolo LFO Organ | Organ bodies with 8′/4′ registration and sustained gate behavior; active 4 Hz triangle amplitude LFO at depth .8 (±.2 amplitude CV). Velocity touch routes are zero; upper pressure-level route remains active. |

## Shared implementation and disclosure checks

- All named arpeggiators are enabled and connected to the engine's rate/direction/octave controls. They run in notes per second, using interval `1 / rate`, not host-beat transport. Up/down excludes duplicated end points. The source header and `docs/PRESET_GUIDE.md` disclose free-running rates and distinguish echo times from arpeggio pace.
- All named LFO destinations match `Engine.cpp`: filter ×1.6, amplitude ×.25, pan ×.7, PWM ×.18. Pan clamps to [-1, 1]; pulse width clamps to [.05, .95]. The authored PWM biases and excursion remain comfortably inside those limits.
- Modern models override classic type within their respective serial FX slot as intended. Plate, tape, drive, and ensemble models are selected by the actual modern model IDs; the two named phaser programs intentionally retain classic six-stage phasers.
- Tape delays use `time = .06 * 20^A` seconds and `tail = .3 * (8/.3)^B`; nominal feedback is `min(.95, 10^(-3*time/tail))`. Computed nominal gains are .188 (Delay Brass), .122 (Arp Bells), .191 (Stereo Delay Pluck), .176 (Ambient Bells), and .158 (Space Lead), all well below the .95 cap. These are control-law calculations, not rendered true-peak guarantees.
- Body-model dispatch is correct for the eight physical-model Modern programs. The new corrections affect only LFO Pan Keys (tine), Phaser Strings (bowed), and Ensemble Plus (bowed) within this set; the other bodies retain their intended model.
- Sustained brass, bowed, and organ partial-decay fields are not audible decay controls in the current sustained branch. This is already documented and is not counted as an audible preset improvement here. At 100% body mix, oscillator mix controls can likewise be dormant while the body and downstream filtering/envelopes remain active.

## Evidence

`/workspace/scratch/dde4ad85ed5b/a23/review-evidence.json` records source SHA-256 hashes, source commit, host-index scope, and the five calculated tape-delay settings. Sources: `Modern.inc`, `CuratedLibrary.inc`, `Engine.cpp`, `FxModels.h`, `FxSlot.h`, `ModulationLfo.h`, `docs/PRESET_GUIDE.md`, and `docs/preset-evidence/2026.11.4/development/final-bank/inventory.json`.

No forced parameter changes recommended from this review. Final bank rendering and cross-platform CI remain separate qualification gates.
