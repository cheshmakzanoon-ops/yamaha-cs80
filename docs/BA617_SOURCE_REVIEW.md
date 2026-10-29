# BA617 and OE2 clock: bounded source review

Review date: 2026-10-29. Research-only supplement; no production DSP calibration is claimed.

## Installed CS-80 adjustment evidence

Primary source: Yamaha **CS-80 Polyphonic Synthesizer — Adjustment Procedure**, manufacturer-authored scan in [Adjustment Procedures and updates](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF). Cover and physical pages23–24 (printed21–22) were visually checked.

Printed21, TREMOLO/CHORUS, step2 specifies:

- TREMOLO and CHORUS tabs **off**, ON/OFF tab **ON**.
- Oscilloscope at **TP1, OE2**; amplifier/speaker at output jack.
- Adjust **VR1, OE2** over **7–12.5 µs**, selecting minimum audible output noise.
- The square-wave annotation spans successive rising edges: this is a **period**, not a half-cycle width.

Thus the corresponding TP1 trim interval is **80–142.857 kHz**, calculated as the reciprocal of that period. It is neither a single factory frequency nor the full range while modulation is active. Printed22/Figure14 locates OE2 TP1, VR1 and IC1 consistently with the detailed board evidence. No separate TP2 target is specified in this adjustment step.

The same table separately specifies OE1 DO1/DO2 oscillator periods of 56 ms and 0.55 s (each ±5%) at maximum SPEED with TREMOLO on. Its VCA-gain row specifies O1/O2/O3/O4 amplitudes of 0.65/0.58/0.58/0.65 Vpp under the preceding PANEL, 8-foot, sine-selected B4-key setup; these are not general output levels or BA617 clock periods. Preserve the printed physical key label rather than silently mapping it to MIDI. No unprinted amplitude tolerance is supplied.

## What this closes—and what it does not

This is materially stronger installed timing evidence than the MN3001 device limits. It closes the absence of a documented **TP1 clock-trim interval and switch conditions** in the earlier OE2 source pass. It does not establish the actual VR1 setting of a particular instrument, clock modulation endpoints, control-voltage waveform, full duty-cycle specification, or audible noise level. The schematic has TP2, but this procedure does not assign it an independent timing target.

The quantitative BA617 control equation, input limits and timing-law tolerances remain **not established**. Functional pin identities are now verified from the Yamaha IC Guide Book below; a standalone Rohm semiconductor specification was not obtained. Do not calculate them from100 pF alone, substitute BA611/near-name parts, or treat the above period interval as the whole chorus modulation range. The [existing OE2 ledger](OE2_CIRCUIT_EVIDENCE.md) remains the source for pin-numbered external wiring; this pass inspected its native `oe57-clock.png` crop.

## Other leads, not adopted as CS-80 calibration

[Yamaha PAS System Electone service-guide scan](https://www.evilturtle.nl/_downloads/84374b33b93df6c157a18c68f5c373e6), physical121, section4/Figures180–181, has searchable text naming BA617 and another application's pin1/pin2 adjustment procedure. The drawing was not visually verified in this pass (web screenshot fetch failed). It therefore supplies only a lead, **not** a verified pin-map or equation for this report, and its Electone frequency targets must not be transferred to OE2.

An original standalone Rohm BA617 datasheet was not obtained in this bounded search. An already available Yamaha IC Guide Book subsequently supplied the exact part entry; the unsuccessful archive lookup is not used as evidence.

## Verified functional pins and clock connectivity

The [Yamaha IC Guide Book](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf), physical page 133 / printed 8-1, identifies BA617 as a voltage-controlled oscillator, made by **R. ohm**, Yamaha part **iG00141**. Its seven-pin table gives:

| Pin | Printed name | Functional role |
|---|---|---|
| 1 | CV1 | First control-voltage input |
| 2 | CV2 | Second control-voltage input |
| 3 | C | Frequency-setting capacitor connection |
| 4 | VDD | Negative DC supply |
| 5 | TIN | Trigger-pulse input |
| 6 | Out | Output |
| 7 | GND | Positive DC supply or ground |

These roles match the detailed OE57 external wiring. Pin5 has no external connection drawn on OE57; that does not characterize unused-input bias or its internal behavior. Page134 is a **CS-40M** application, not CS-80 calibration. Neither page supplies a general frequency equation, control-input limits or timing tolerances.

Guide physical page126 explicitly depicts CS80 OE2 and the two cascaded TC4049 inversions: TP1 feeds MN3001 CP1/CP1′, TP2 feeds CP2/CP2′. Independent detailed OE57 tracing confirms TP1(64) on the MN3001 pins2/6 net and TP2(57) on pins12/10. TP1 is therefore an actual BBD clock net, not merely an assumed upstream oscillator output. The simplified guide repeats input I(33); retain the already established detailed OE57/58 resolution **I(53)**.

Combining the installed alignment interval with the manufacturer nominal 256-clock-period convention yields **256T = 1.792–3.2 ms** per chain in this specific alignment setup. This is a source-derived nominal timing inference, not measured pin-to-pin delay, sampling-aperture characterization, full-board group delay or modulated operating range. Do not choose an arbitrary midpoint and call it factory calibration.


## Reproducibility

Local evidence directory: `artifacts/ba617-source-20261029/`. The PDF has36 physical pages, letter-sized612×792 point pages, and no embedded JavaScript according to `pdfinfo`. PNGs are whole-page Poppler renders, **not native scan pixels**: `pdftoppm -scale-to 1800 -png` for physical23/24; `-scale-to 1400` for cover.

| File | SHA-256 |
|---|---|
| `cs80-adjustment.pdf` | `eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba` |
| `cs80-adjustment-cover.png` | `fe08761b5574a1113a7c16c0b49f172238e6d02630882ec80391ad95dadd4b5b` |
| `cs80-adjustment-physical23.png` | `0415b12e9d80ee1c272f8f0d32ee524897ce9d8abcc928991cdbf82545b8a484` |
| `cs80-adjustment-physical24.png` | `fbb8a5f7f2dc6152cbfc030565a6f158c8d4e4478f41759e6bf133b99d340a1f` |

Independent review corroborated the full-period annotation, switch conditions, direct TP1/CP1 wiring, all three guide pages and their pin roles, PDF/render hashes and the bounded nominal timing inference. Installed trim, BA617 transfer law and modulated clock endpoints remain open. This document changes no production DSP.


### Additional guide provenance

Source PDF SHA-256: `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1` (the same already acquired guide used by the oscillator evidence). Full-page Poppler PNG renders use `-scale-to 1800`; no crops or image enhancement. Physical page numbers are one-based.

| Render | SHA-256 |
|---|---|
| `yamaha-guide-physical126.png` | `02b76d1e02a88ef9be4af6a24871c053c589450d160cded123e518601b1679ad` |
| `yamaha-guide-physical133.png` | `95deeefe9a2da1fbabd40918d3baef2f7eb1e4a6d4c5bb4d1178fcf2e75336a4` |
| `yamaha-guide-physical134.png` | `31357308ec202fc9c63a742b0ac7cc941b73d9b4323aa449f45a8d8b30c86b90` |
