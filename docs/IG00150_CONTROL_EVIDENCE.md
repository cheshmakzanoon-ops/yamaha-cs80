# IG00150 suboscillator control evidence

## Primary source and reproducibility

Original Yamaha IC Guide Book, PDF page 29, printed section 2–1:
https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf

Source SHA-256: `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1`.
Retrieved 2026-10-29 02:09 UTC; 13,608,992 bytes, 164 PDF pages.
Source-inspection run `40804630638`, successful job `123220506353`.
The fixed inspection rendered PDF pages 29 and 30 only.

- `icguide_sub-page29-full-native`: PNG SHA-256 `6fc60f585f712e30656dd110ceb1dc9406238dfc60ecbbe68158274b76f45231`.
- `icguide_sub-page30-full-native`: PNG SHA-256 `1d960e5d5d805381cfa59ac729a734bb97c2611a69afb84fe93cdac93889b8f3`.

These are 1800 × 1272 PDF renders, not original-resolution schematic scans or hardware measurements.

## What the primary diagram establishes

Page 29 identifies IG00150, made by Mitsubishi, function VCO II.
Its functional diagram explicitly places an **exponential converter** after the pin 1 control-voltage input. The oscillator includes a timing-capacitor connection, buffer, comparator pair, flip-flop and waveform converters.

The pin table identifies:

| Pin | Role |
|---|---|
| 1 | Control-voltage input VI |
| 2 | Ground |
| 3 | Frequency-determining capacitor Co |
| 4 | −15 V supply |
| 5 | +15 V supply |
| 6 | Sine output SIO |
| 7 | Sawtooth output SO |
| 8 | Reference-current adjustment |

The independently inspected CS80 SUB schematic page 29 connects VC3 and VC4 through separate 120 kΩ resistors to the common IC5 pin 1 node. This establishes additive electrical contributions at that node, **not additive frequency in hertz**.

## Corrected inference and limits

An earlier inference that the summing resistors themselves disprove a multiplicative speed response is withdrawn. The original IC diagram explicitly includes exponential conversion. If the operative control variable is an affine sum and frequency is exponential in that variable, separate control contributions can produce multiplicative rate factors. This is a qualitative inference, not a calibrated CS80 equation.

These pages provide no numeric control slope, volts-per-octave specification, frequency-versus-input curve, input impedance, complete transistor topology, or measured CS80 aftertouch transfer. They therefore neither validate a particular software multiplier nor justify replacing it with additive hertz. The actual board network, loading, offsets, trimmers and calibration remain necessary for quantitative fidelity.

PDF page 30 is an application circuit for **CS-15, CPA board**, not CS80. Its component values and illustrated approximately 0.1–100 Hz range must not be imported as CS80 specifications.

No production behavior is changed by this evidence note.
