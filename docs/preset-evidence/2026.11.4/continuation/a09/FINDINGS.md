# Full-preset verification of tine strike normalization

Source baseline: `59d489d1a1f0065ae2a0f42a08c74edc43e94159`. Candidate InstrumentBody header SHA256: `19f9fb23b61ab98fa843c0cf126cb6342ef33e3c62a089fc4c63ae451f470852`. No preset parameter values changed in this correction.

## Results

**All 90 candidate captures retain monotonically increasing peak, early RMS, body RMS and nominal fundamental with velocity.** The correction expands the three tine programs’ soft-to-firm body-level contrast by approximately 3.9–4.0 dB. It does so by removing unintended gain from soft strikes, while preserving the full-strike signal.

| Existing program | Prior body dynamic span (.15–1) | Corrected body dynamic span (.15–1) | Corrected top velocity step (.9–1) |
|---|---:|---:|---:|
| Analog Piano | 7.89–8.16 dB | 7.89–8.16 dB | 0.56–0.58 dB |
| Electric Piano I | 8.22–9.07 dB | 12.23–13.01 dB | 0.83–0.90 dB |
| Electric Piano II | 8.28–9.97 dB | 12.30–13.86 dB | 0.87–1.15 dB |
| Reed Piano | 7.89–8.77 dB | 7.89–8.77 dB | 0.59–0.70 dB |
| Tine Piano | 5.97–6.52 dB | 9.97–10.41 dB | 0.75–0.81 dB |

Ranges cover MIDI notes 36, 60 and 84. Each existing program uses its unchanged complete factory vector, with a fresh Engine, 48 kHz stereo float output, 128-frame blocks, a one-second held note and no pressure. The six velocities are .15, .3, .5, .7, .9 and 1. Early RMS spans 25–250 ms; body RMS spans 300–800 ms.

**All 45 parity comparisons have matching float-audio FNV1a64 hashes:** full-strike audio for all five programs at all three registers, plus Analog Piano and Reed Piano at every tested velocity/register. Each comparison covers 96,000 float samples, totaling 4,320,000 samples. The source, static libraries and probe binaries also have SHA256 provenance records.

The baseline metrics supplied by agent 5 use rounded CSV precision; tiny reported metric deltas on unchanged controls are rounding differences. Matching parity hashes independently establish unchanged output for those cases.

## Scope of affected existing programs

The full 523-program inventory contains **17 programs with 28 active tine ranks**: eight original-address programs and nine curated programs. This is configuration-based effect coverage; the detailed velocity sweep above exercises three of those programs, with two non-tine controls.

| Host index | Existing name | Address collection | Active tine ranks |
|---:|---|---|---|
| 8 | Tone I: E. Piano | original | 1, 2 |
| 39 | Bright Keys | original | 1, 2 |
| 140 | Amber Keys 97 | original | 1, 2 |
| 147 | Electric Keys 104 | original | 1, 2 |
| 148 | Midnight Keys 105 | original | 1, 2 |
| 149 | Slow Keys 106 | original | 1 |
| 152 | Wide Keys 109 | original | 1, 2 |
| 154 | Amber Keys 111 | original | 1 |
| 336 | Electric Piano I | curated | 1, 2 |
| 337 | Electric Piano II | curated | 1, 2 |
| 338 | Tine Piano | curated | 1 |
| 351 | Chorus Keys | curated | 1, 2 |
| 357 | Tremolo Keys | curated | 1, 2 |
| 358 | Contour Keys | curated | 1 |
| 400 | Kalimba | curated | 1 |
| 463 | Music Box | curated | 2 |
| 512 | LFO Pan Keys | curated | 1, 2 |

## Acceptance and limitations

Accept the normalization correction subject to the root’s unchanged complete-bank RMS and peak gates and native platform qualification. Do not compensate with a blanket master-gain increase: softer playing becoming quieter is the intended repair, and hard-strike levels remain unchanged.

These measurements establish improved dynamic response and preservation of selected signals. They do not establish acoustic-instrument similarity or perceptual indistinguishability. Nominal-frequency harmonic DFT results are affected by detuning, chorus and inharmonicity; stereo RMS is the stronger complete-preset level measure.

Reproduce by compiling `probe.cpp` against the retained baseline/candidate core archives, running the baseline probe with `--parity` and the candidate probe without arguments, then running `compare.py`. Complete commands and exact inputs are recorded in `provenance.json`.
