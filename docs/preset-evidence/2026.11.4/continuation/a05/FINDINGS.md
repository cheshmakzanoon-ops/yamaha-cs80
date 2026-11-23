# Agent 05: full-preset strike dynamics

Baseline: `59d489d1a1f0065ae2a0f42a08c74edc43e94159`. 90 one-second renders: five existing programs, MIDI 36/60/84 and velocity .15/.3/.5/.7/.9/1. No effects or factory controls overridden.

Every measured complete-preset peak, 25–250 ms RMS, 300–800 ms RMS and nominal fundamental rose monotonically. Thus the direct-model normalization issue does not cause a whole-preset velocity reversal in this sweep. It compresses dynamics, especially tine programs.

| Program | Early soft-to-hard range, dB | Late soft-to-hard range, dB | Late .9→1 step, dB |
|---|---:|---:|---:|
| Analog Piano | 7.82–8.35 | 7.89–8.16 | 0.56–0.58 |
| Electric Piano I | 8.47–9.30 | 8.22–9.07 | 0.18–0.26 |
| Electric Piano II | 8.68–10.55 | 8.28–9.97 | 0.22–0.51 |
| Tine Piano | 7.36–7.68 | 5.97–6.52 | 0.10–0.18 |
| Reed Piano | 7.95–8.77 | 7.89–8.77 | 0.59–0.70 |

A strike-independent normalization bound that retains the velocity-1 endpoint is reasonable: it removes reciprocal attenuation of firmer strikes. It will reduce soft/mid output compared with this baseline, so the complete-bank RMS floor and phrase/stress headroom need checking at their existing settings. Do not compensate by a global gain increase before examining those gates.

Suggested acceptance: all 90 peaks and early/body RMS remain monotonic; hard-strike outputs remain unchanged within arithmetic tolerance; the tine fundamental no longer drops at the direct-body layer when only velocity rises; timbral soft/firm distinction survives; existing loudness/peak gates remain intact. The exact .9→1 increment is descriptive, not an arbitrary pass threshold.

The fundamental/spectral metric uses a Hann-window DFT at nominal equal-temperament harmonics. Chorus, detuning and piano inharmonicity can affect it; broadband stereo RMS is the reliable complete-preset level measure. This is not a perceptual or acoustic accuracy rating.

Files: metrics.csv, summary.json, provenance.json and probe.cpp. Baseline capture and library copied before DSP editing.
