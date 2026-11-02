# M41 pin and LC-path erratum

2026-11-02. **Corrected public-source transcription, not a new circuit law
or physical observation. No DSP or calibration values change.**

## Corrections

1. **IC11 LP output is pin 10**, not pin 15. M41 labels pin 15 **HP**,
   pin 13 **BP** and pin 10 **LP**. The connected pin-10 upper node runs
   right and down to **CP8(56)**; M44 then connects CP8 to CP3(29).
   The incorrect number propagated into the native-filter review, nodal
   model's CP3-termination paragraph and optional measurement specification.
   Their references now say pin 10. The existing nodal computation already
   reads the second stage's LP state (`z[7]`), so this is not a software
   output-selector correction.
2. **LC31 goes directly to IC5 LI pin 2**, with a **0.01 µF (10 nF)**
   shunt. The recent VCA coupling review mistakenly put a 100 kΩ resistor
   in this control wire. That resistor is in the distinct **CP4→1 µF NP
   →100 kΩ→IC5 +IN pin 3 audio path**. The older
   [level-control design](../../LEVEL_CONTROL_REFERENCE_DESIGN.md) and
   existing model already distinguish the LC wire correctly. Do not add
   a fictitious series resistor or retune the LI time constant from the
   corrected paragraph.

The separate CP3→IC4 and CP4→IC5 1 µF/100 kΩ audio branches remain
source-visible. Their loaded poles and the collapsed software VCA's
control law are still engineering estimates, not identified die behavior.

## Primary pixels and reproducibility

Original [Yamaha M circuit drawing, printed p41, KEC-4223-65](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg),
5100×3300 JPEG, source SHA-256 recorded by the archived job:
`1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`.
The exact archived crop payloads were freshly retrieved from
[run 41224106605, job 124604536319](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536319),
whose source checkout is `0281af873389a13452ac5dca2ef9b90dcec07c72`.

| Crop | Original-pixel box | Decoded size | Fresh decoded PNG SHA-256 |
| --- | --- | --- | --- |
| `m-filter-detail` | `[1180,1080,3250,1850]` | 2070×770, unresized | `5dfe074bc3868f22fe662cbe55d718dbb9479db0d05f36a4089a8aae445cbcc7` |
| `m-r0-c1` | `[2486,0,5100,1714]` | 1800×1180, resized overview | `09a2234a43801864ce6c633f73bed7cd2aec1ebd1a87274cfbbec91761cd2311` |

Payload chunk indexes were checked contiguous before decoding. Fresh PNG
hashes and dimensions match the archived declarations. The whole original
JPEG was not reconstructed in this review; its identity is the archived
source manifest, not a claimed fresh whole-image hash. No downloaded
source scans are added to the repository.

An independent qualification agent inspected both decoded PNGs, followed
the pin-10 node to CP8 and the LC31 wire across overlapping views to LI2,
and separately confirmed that the 100 kΩ belongs to +IN3. This agreement
is visual source review, not a measurement of an installed instrument.

The public-only reconstruction scope requires no owner hardware. The
original **HP CP3 1.692 Vpp MISS** and Yamaha **4.3 ±1 Vpp** criterion,
the source-cell classification and V1 fingerprint `e5f877a025fc52c6`
remain unchanged by these corrections.
