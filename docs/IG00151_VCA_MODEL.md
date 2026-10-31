# IG00151 voice VCA (circuit engine)

Plan step 1.4. Implemented by ws1-voice in `src/core/Vca.h` (`Ig00151Vca`,
`constantPowerPan`) and wired in the circuit-engine rank chain of
`src/core/Engine.cpp` (the `circuitVca` branch). Legacy keeps its plain
`levelCv * envelope` multiply and linear pan, byte-identical
(`scripts/check-compat-gates.sh`). Nothing here is a measurement of a CS-80.

## What the model does

Each rank's voice signal (the filtered oscillators plus the sine) passes the
IG00151 VCA before the rank pan:

```
LI   = LC through the 10 nF level node (one-pole, tau 0.542 ms)
gain = max(0, LI) x 2^EI x envelope
out  = gain x (input + offset) + feedthrough x gain
next stage = out through a 1 uF / 100 k coupling (tau 0.1 s)
```

where LC is the linear level drive (channel LEVEL, touch, keyboard level
contour, matrix level), EI the exponential input (the SUB oscillator's VCA
modulation) and the envelope the IG00159 amplitude envelope.

## Laws and evidence classes

| Law | Class | Source / alternative |
| --- | --- | --- |
| Distinct exponential (EI1) and linear (LI2) control inputs | source-derived | service manual p52 |
| LC31 -> IC5 LI2 with a 10 nF pin capacitor; five branches 180 k / 560 k / 3 x 270 k | source-derived | M41, R39 |
| LI node time constant 0.542 ms (ideal-source Thevenin of the five branches x 10 nF) | declared inference | the branch sources' impedances are not documented |
| SUB VCA modulation on EI1 ("VCA for SUB-OSC"), +/- 1 octave at full depth | declared inference | alternative: linear on LI (the legacy law) |
| Gain = LI x 2^EI x envelope | declared inference | OTA multiplier topology |
| Offset 1.5e-3 + CV feedthrough 1e-3 (engine units): pedestal -52 dB at full LI | declared inference | typical OTA magnitudes; no data-sheet values for the IG00151 die |
| Next-stage AC coupling 1 uF / 100 k (0.1 s) | declared inference | by analogy with IC5's documented input coupling |
| Constant-power rank pan 0.5 sqrt(1 -/+ pan), exactly 0.5 per side at centre | engineering (modern rank pan) | the original's left/right routing is not reconstructed |

The AC coupling was added after ws4's ring-modulator gate ("carrier feedthrough
is independent of the signal") failed against the DC pedestal; with it that
gate passes on the merged tree.

## Gate and results

`cs80_ig00151_vca_tests`, limits pre-registered in
`tests/parity/tolerances-v2/ws1-voice.json` (`ig00151_vca`, commit `0143066`):

| Check | Limit | Result |
| --- | --- | --- |
| Linear law (gain proportional to settled LI) | 1e-6 relative | pass |
| Exponential law (2^delta EI) | 1e-6 relative | pass |
| LI time constant at 88.2/96/192 kHz | 2 % of 0.5419 ms | 0.5419 ms |
| Pedestal at full LI | <= -50 dB | -52.04 dB |
| Pan power constant, 0.5 at centre | 1e-6 | pass |
| SUB tremolo exponential in the circuit engine, additive in Legacy | ratio 2^(2d) | pass |

Mutation checks (2026-10-31, each run against the merged tree and restored):

| Mutation | Failing checks |
| --- | --- |
| LI node removed (LI = LC instantly) | 3 |
| Next-stage AC coupling removed | 1 |
| SUB moved back to the linear input (EI = 0) | 1 |

## Click scores

The step-1.4 criterion (steal and retrigger click scores <= 1 in
`cs80_engine_audit --suite phrase`) and its current values are recorded in
[STATUS.md](STATUS.md) with the engine audit. ws1 traced its one outlier
(retrigger 1.26 on "Funky 3+4" at 44.1 kHz) to two same-pitch voices whose
waveform edges coincide when the retrigger lands on an idle voice beside a
bright release tail: an allocation and phase effect, not a VCA discontinuity.

## Intentional circuit-engine sound changes

SUB tremolo now acts in decibels (exponential input); LC changes are smoothed
by the 0.54 ms LI node; an envelope-shaped -52 dB pedestal follows each note
and is AC-coupled away; off-centre rank pans are constant-power. Historical
renders: use the commits before the ws1 merge (`ea391d7`).
