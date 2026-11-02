# Blank Yamaha CS-80 ADJ HP / noise bench-evidence checklist

**Status: ALL ITEMS UNOBSERVED. No bench run, adjustment, or measurement
performed by this software project.** This template is for an **owner-
authorized qualified synthesizer service technician** to decide which
observations are safe and warranted. It is **not** powered-circuit access,
calibration, disconnection, or probe instructions for a non-technician.
Missing data must be marked **NOT ACQUIRED**, never reported as 0 V.

Source basis: original [Yamaha 36-page ADJ PDF](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF),
SHA-256 `eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba`;
printed pp1–2 (PSU), p10 (M-board Figure 7), **p11 (HP/LP)**,
**p17 (separate NOISE alignment)**; original
[M41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg),
[M44](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg),
and [SUB29](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/29%20-%20SUB%20Circuit%20Diagram.jpg).
The detailed source classes, decision tree and **synthetic-only** software
predictions appear in
[ADJ_HP_PHYSICAL_SETUP_DECISION_TREE_20261102.md](../../docs/evidence/continuation-2026-10-31/ADJ_HP_PHYSICAL_SETUP_DECISION_TREE_20261102.md).

## A. Authority, identity and non-interference — no values filled in

- [ ] Owner's authorization and qualified technician's signed report ID
  documented. Otherwise stop the evidence request.
- [ ] Instrument serial, channel I/II, active physical M-board ID,
  board/revision/date and visually identified test terminals documented.
  Do **not** assume M11/M12 channel-I wiring identifies a channel-II board.
- [ ] De-energized inspection/photo provenance and any history of trimmer
  changes recorded, if independently safe and authorized.
- [ ] Yamaha printed p2 **PSU connector warning** acknowledged in
  technician's own safety plan. No energized harness unplugging,
  improvised short/termination or power-rail modification requested.
- [ ] Scope and DVM manufacturer/model/calibration, coupling/reference,
  effective analog bandwidth/filter, sample rate, capture length,
  probe attenuation, input **R/C**, input loading and source waveform
  clipping documented per acquisition. No 50 Ω termination assumed.
- [ ] Temperature, warmup, voice allocation, simultaneous-probe
  disturbance and noise acquisition-window effects recorded, or
  individually marked **NOT ACQUIRED**.

## B. Yamaha printed p11 HP: PANEL C1 / CP3 acceptance node

The following are **printed conditions**, not a claim anyone set them:
FEET **2′**, BRILLIANCE **center**, master **RESONANCE max**,
TONE SELECTOR **PANEL**, VCO **pulse ON**, VCF **RESH HIGH, LPF HIGH,
RESL HIGH**, and HPF set to **+4.0 V at FH** with DVM on R51 (CH I)
or R52 (CH II). C1 depressed; oscilloscope at **CP3 on active M board**.
Printed sequence: **VR7 first for CP3 maximum**, **VR8 second to
CP3 4.3 ± 1 Vpp**. CP2 is *not* the acceptance terminal.

- [ ] Technician confirms the above actual panel conditions, active
  voice, FH DVM observation and the **CP3 (29)/CP8 (56)** wiring and
  distinguishes adjacent **CP4 (VCA out)**. Variants separately noted.
- [ ] Paired same-event **CP2 (46)/CP7 (53)** and **CP3/CP8** AC
  waveforms under as-found alignment captured with probe/loading
  metadata, or each specifically marked **NOT ACQUIRED**.
- [ ] Technician's actual **VR7 CP3-amplitude maximum** and control
  reference documented; paired CP2/CP3 captured at that state where safe.
  **Vfc4** observation only if safely instrumented, never translated
  into a fitted cutoff law.
- [ ] Technician's actual subsequent **VR8** state documented with
  paired CP2/CP3 shape, distortion and CP3 Vpp if safe; **Qadj6** only
  if safely instrumented. No trimmer angle is labeled **Q=9**.
- [ ] NO1(39)/NI(43), VCO NOISE control **as found during p11**, and
  shared connected-board count documented if safely observable, or
  marked **NOT ACQUIRED**. No source impedance inferred from high-Z
  voltages alone.
- [ ] Chassis/M-board supply observations separately documented if
  technician judges them warranted; compare to Yamaha's separate
  **p1 PSU calibration** without transferring rail targets to CP3.
- [ ] Master volume, expression and output settings documented only as
  contextual state; **p11 does not specify them**, and CP3 is pre-VCA.

## C. Separate Yamaha printed p17 NOISE GENERATOR procedure

Do not blend this with HP p11. The original p17 separately specifies
TONE SELECTOR **PANEL**, VCO **NOISE 10**, scope **SUB NO1 (CH I)**
or **NO2 (CH II)**, and SUB **VR1 / VR2** to the printed
**3.0 ± 0.1 Vpp** noise-output target. Its Vpp instrument time
window/bandwidth is not numerically specified in the excerpt.

- [ ] If independently authorized, technician's **separate p17**
  checkpoint is identified, with NO1 or **NO2 correctly named by
  channel** and control/probe/capture bandwidth/window supplied.
- [ ] State whether actual p17 source alignment was checked by the
  technician; do not derive a p11 NO1 value from that target.
- [ ] No p17 measurement or adjustment is silently attributed to
  M-board HP VR7/VR8. In JSON importer v1, **NO2/p17 checkpoint is
  unsupported**; do not relabel NO2 as NO1.

## D. Separate printed p11 LP control and release

- [ ] If separately warranted, record LP condition **HPF LOW, RESH LOW,
  RESL HIGH, LPF → +3.3 V at FL**, then **VR9 CP3 maximum** and
  **VR10 CP3 2.2 ± 1 Vpp**. Preserve independent CP2/CP3 traces;
  never use this LP state as an HP observation.
- [ ] Report raw waveform references and actual artifact SHA-256,
  technician method, exact node and acquisition metadata; annotate
  **NOT ACQUIRED** nodes and deviations. Do not fill missing entries
  from the repo's linear model, previously published restorer anecdote,
  or a synthetic Q sweep.
- [ ] Technician and owner review the report within their own authority.
  The software team performs **source/model comparison only**, with no
  physical intervention, no new factory pass gate and no V1 edit.

## E. Interpretation locks

| If an authorized observation is eventually supplied | Permissible distinction | What must **not** be inferred |
| --- | --- | --- |
| CP2 and CP3 differ in the same event | Localizes a stage/interstage relationship | CP2 can replace Yamaha CP3 |
| CP3 response changes with probe/cable load | Measurement disturbance or load-sensitive node | A universal probe correction factor |
| p11 NO1/NI spectra or RMS are observed | Noise-path contribution is nonzero/conditional | Exact NO1 output impedance or p17 target at p11 setting |
| p17 NO1/NO2 Vpp documented | Independent manufacturer noise-output setup observation | HP p11 source calibration or CP3 gate |
| CP3 changes after real VR7→VR8 sequence | Physical calibration dependence | Physical Q=9 or fitted gm/Q mapping |
| Rail/headroom or CP2 compression observed | Investigate actual nonlinear/electrical conditions | The linear ~11 Vpp CP2 stress test was hardware-validated |

**Freeze:** production DSP unchanged; frozen HP source-factory CP3
**1.692 Vpp MISS** remains; printed HP CP3 band **3.3–5.3 Vpp**
unaltered; V1 source fingerprint `e5f877a025fc52c6` and
**25 unresolved T51–T54 tone revisions** remain untouched.
