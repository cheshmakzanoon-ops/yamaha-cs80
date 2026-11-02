# Owner-manual lever direction — preregistration, 2026-11-02

Base: published host-fix `b91bb20978fff2c2bc0f5a3a745b939b1b23a6d3`.
This test-only checkpoint changes no production source, parameter range, state
format, DSP, golden image or existing tolerance. Run its new physical-direction
checks in the native Actions JUCE harness **before** implementing a UI reversal.

## Public primary source and exact pixels

The original Yamaha owner's manual is publicly mirrored at
[The Rogoffs' CS-80 archive](https://www.therogoffs.com/cs80/),
[Part 1 PDF](https://www.therogoffs.com/cs80/CS80E_1.pdf).
Downloaded bytes SHA-256:
`a9985aade98a495c3f204fd7170a9e19a805c241a4a2031d2ee9f8b778b4a3d4`.
PDF pages 10-18 correspond to printed pages 6-14. I inspected rasterized original
pixels, not an OCR-only inference. The manufacturer download index also lists
`CS80E.zip`, but that endpoint returned HTTP 403; no official-ZIP byte identity
is claimed. The public mirror is explicitly the acquired source.

`pdftoppm -f 11 -l 18 -scale-to 1800 -png CS80E_1.pdf owner-global`
produced the reviewed per-control pages under the shared engineering lock.

Printed page 6 explicitly distinguishes levers whose maximum is toward the
player from those whose maximum is away. Therefore a blanket inversion of all
levers is wrong. On this editor's top-down panel, toward the player is downward.

| Control | Increasing physical value | Primary evidence |
| --- | --- | --- |
| MIX (existing value = rank I share) | upward | printed 7, rank I at top / rank II bottom |
| FEET I/II (enumeration 16,8,5⅓,4,2⅔,2) | downward | printed 8, six labelled positions |
| DETUNE II | downward | printed 8, negative above / positive below |
| Global brilliance, resonance | downward | printed 8, explicit prose and panel illustration |
| Keyboard brilliance and level LOW/HIGH | downward | printed 9, curves/lever diagrams and level prose |
| Sub function (sine through EXT) | downward | printed 9, six printed waveform positions |
| Sub speed and VCO/VCF/VCA depths | downward | printed 9-10, explicit prose and diagrams |
| Initial bend, after-touch speed/VCO/VCF amounts | downward | printed 10, no-effect-up statement and paired diagrams |
| Ring speed, depth and modulation | downward | printed 14, explicit prose |
| Ring attack/decay times | downward | printed 14, slow/fast envelope examples; interpretation of the time-labelled controls |
| Sustain duration | upward | printed 11, panel photograph and labelled diagrams |
| Portamento duration | upward (documented estimate) | printed 11-12 panel labels and diagrams; contradictory prose retained below |

### Source conflicts and scope boundaries

Printed 12's portamento paragraph says short at the upper end and long at the
lower end, while its own diagrams and the preceding panel photograph label
long at the top and short at the bottom. We retain the existing top=long mapping
as the best supported **visual-panel estimate**, and infer a likely prose
erratum; this is not a proved erratum or an instrument measurement. No general
rule is transferred from this choice to another control.

DETUNE II is physically centred and signed according to printed 8, but the
existing plugin parameter is positive-only 0..50 cents. This test fixes only
the monotonic gesture direction; it does not pretend the parameter now has a
signed centre. A compatibility-safe signed source-mode model is a separate
circuit/state workstream. No numeric detune interval is specified on that page.

Programmer-row throws remain unchanged by this focused scope. The LPF positive
control checks preservation, not a newly established hardware direction.

## Registered UI checks

An independent fixed inventory contains 23 downward-increasing controls, three
upward-increasing globals and one unchanged programmer control. For each, test
actual JUCE slider endpoint coordinates, five value/geometry round trips,
physical Up/Down arrow direction and exactly one balanced host gesture per
arrow. These expectations are not generated from a production direction table.
The original image limits remain unchanged. Any intentional new golden must be
reviewed from actual native candidate/diff images, never generated as a fake UI.

Native baseline failure, implementation, pointer/keyboard regression checks,
reviewed screenshots and clean final-source platform requalification remain
pending at this preregistration. No physical instrument or complete panel
conformance is claimed.
