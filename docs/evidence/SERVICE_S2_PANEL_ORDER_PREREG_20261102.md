# Original service S2 panel order — preregistration, 2026-11-02

## Acquired public primary source

The original Yamaha service-manual drawing, printed page 2, is available as
[02 - Panel Layout](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/02%20-%20Panel%20Layout.jpg).
The acquired image's SHA-256 is
`18dcbb9d267ba2a8b8e9afce3f21b5d854185849504850a1b71a0c57c22e0835`.
Actual pixels were inspected; this is not a transcription inferred only from
repository summaries. This resolves the historical retrieval block recorded in
the 2026-10-31 [panel audit](../PANEL_CONFORMANCE.md), not all panel discrepancies.

S2's PN2 order, left to right, is PITCH, DETUNE, ring attack/decay/depth/speed/
modulation, sub function/speed/VCO/VCF/VCA, FEET I/II, the two tone-selector rows,
MIX, BRILLIANCE, RESONANCE, TOUCH RESPONSE (initial bend; after speed/VCO/VCF),
KEYBOARD CONTROL (brilliance low/high; level low/high), then VOLUME. Current
`CentreRow.cpp` places KEYBOARD CONTROL before TOUCH RESPONSE: a concrete
source-established ordering defect. Other broad sections already follow S2.

## New test contract, before production changes

An independent fixed list of 26 parameter controls checks actual editor child
bounds at 1600×1000 and 1120×700. Require each control to exist with positive
dimensions, and each adjacent ordered pair's right edge to precede or equal the
next left edge. These 102 checks do not derive their expected order from the
production list or layout coordinates. Missing controls fail explicitly. The
tone-selector gap is preserved; this narrow check is not a full tone-tab audit.

Native Actions must first record the baseline failure. Expected defect:
after-touch VCF incorrectly lies to the right of keyboard brilliance LOW,
at both sizes. Do not edit production placement before obtaining that evidence.
Then move only the TOUCH RESPONSE and KEYBOARD CONTROL groups, preserving their
internal order, parameter IDs, values, range, serialization and automation.
The unchanged original layout/overlap tests and image limits remain mandatory.
New actual native screenshot candidates/diffs must be reviewed before any
intentional golden update. No image threshold is relaxed.

## Other source observations, not claims of completed implementation

- S2 independently labels both sustain and portamento LONG above SHORT. This
  supports the previously registered visual-panel estimate against the
  contradictory owner printed-page-12 prose; an erratum remains an inference.
- Owner-manual part 2, [CS80E_2.pdf](https://www.therogoffs.com/cs80/CS80E_2.pdf),
  SHA-256 `12956f39b67e66ce78a03d23754f1729200acfa97b5ef26dce71a158506c4fa9`,
  printed pages 15–23 were directly inspected. Programmer levels, modulation
  depths, cutoff/resonance and time controls increase upward. IL's upward
  negative printed voltage corresponds to increased unsigned excursion depth,
  not to a new signed host parameter. Printed 23 explicitly puts programmer
  touch response zero below maximum. No programmer-row inversion is indicated.
- S2 confirms the lid at upper left and rank I above rank II on its right.
  Exact screen proportions remain software-scaled adaptations, not a metrology
  match. The 61-key graphical keybed and software deck do not reproduce every
  physical dimension or power/jack location. Colour fidelity is not established
  by the monochrome S2 drawing.

Base is the separately frozen, production-unchanged owner-direction test series
`e7e6e7a677a9c60f4dc1319bbd98fe65c3cf424d`, src
`dbbefcc5ca0eeac5b741a90b916c0e79336246fd`. Native RED and implementation remain
pending at this registration. No physical CS-80 is required or claimed.
