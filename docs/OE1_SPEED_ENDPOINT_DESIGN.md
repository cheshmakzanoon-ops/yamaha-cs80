# OE1 SPEED end-stop: source audit and offline diagnostic

2026-11-03. Research-only review of main `db24a47170610f81236e8829ef5536af69dfc94d`
and reviewed integration `2ff1b5bb`. No production DSP, host parameter, state schema,
UI, preset, compatibility baseline, or existing test tolerance is changed.

## Decision and existing work

Do not extend `chorus_speed` or introduce a second user-visible Hz control on this
evidence alone. The printed maximum-speed alignment is one operating point, not
a complete physical SPEED law. The existing host parameter is an engineering rate
control; a new independent rate override would not reconstruct PN3 VR3.

The 8 Hz limitation is **already documented**, not newly discovered: see
[BBD_CHORUS_MODEL.md](BBD_CHORUS_MODEL.md),
[BA617_SOURCE_REVIEW.md](BA617_SOURCE_REVIEW.md), and ws4 Step 3.
A read-only search of all 66 available remote refs found 64 readable parameter
tables, all with the same `chorus_speed` definition. The review also deduplicated
33 matching evidence/reference blobs. No replacement OE1 SPEED law was found.
This pass adds the specific PN66/OE55 control trace and a reproducible diagnostic.

## What the adjustment actually requires

[Yamaha Adjustment Procedure](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF),
physical PDF page 23 / printed page 21, step 1:

- TREMOLO/CHORUS section SPEED at maximum; TREMOLO tab on.
- Observe **DO1 on OE1**, adjust **VR1 on OE1**: period **56 ms ±5%**.
- Observe **DO2 on OE1**, adjust **VR2 on OE1**: period **0.55 s ±5%**.
- The row does not separately specify CHORUS, ON/OFF or DEPTH settings. Do not
  import the following step's settings into this one.

These are oscillator-node periods, not the audible beat rate, a BBD clock period,
or an instruction to assign a host parameter range. The period bounds correspond
to DO1 **17.0068–18.7970 Hz** and DO2 **1.73160–1.91388 Hz**. Reciprocal frequency
limits are asymmetric. The next row's **7–12.5 µs** TP1 setting has TREMOLO and
CHORUS off, ON/OFF on, and is a separate OE2 clock/noise adjustment.

The [owner's manual](https://www.therogoffs.com/cs80/CS80E_1.pdf), physical page 17 /
printed page 13, describes maximum SPEED at full clockwise rotation, a slower
CHORUS range, a moderate-to-fast TREMOLO range, and greater speed variation in
TREMOLO. It gives no quantitative intermediate-position curve or minimum rate.

## Control routing recovered from the drawings

**Supplement:** the overall foldout grounds SPEED terminal 1, unlike the detailed
Panel66 drawing below. See [the source-conflict note](OE1_SPEED_SOURCE_CONFLICT.md);
this panel trace does not establish which wiring is installed in a given unit.

Primary drawings for serial numbers 1001 onward:
[Panel 3 p66](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/66%20-%20Panel%203%20Circuit%20Diagram.jpg),
[OE1 p55](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/55%20-%20OE1%20Circuit%20Diagram.jpg),
and reciprocal [OE1 wiring p56](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/56%20-%20OE1%20Circuit%20Board%20%26%20Wiring.jpg).

1. PN3 **VR3 B10k** is labelled SPEED. Terminal 3 references VR1 terminal 1,
   which connects to BA +10.6 V; wiper terminal 2 goes to **OE1 SP(28)**.
   Terminal 1 has no external wire drawn. The separately drawn **C** terminal
   references VR1 terminal 3 and VR4B terminal 1; the latter also references EP.
   Preserve the separate C marking rather than silently identifying it with
   wiper 2 or resistor terminal 1. The drawing supports a variable series
   resistance into a loaded network, not a grounded three-terminal divider.
   The older `RELEASE_CONTROL_ROUTING.md` VR1/VR3 cross-reference wording needs
   reconciliation with this literal trace before reuse.
2. SP feeds two branches on OE1: **22k / 100k** with midpoint **T(30)**, and
   **8.2k / 1.2k** with midpoint **CH(29)**. The lower ends join the E reference.
   Unloaded divider fractions alone are not installed oscillator rates: SP
   sees both branches and the selected midpoint sees the C/oscillator load.
3. Panel SW5 TREMOLO common **T1 → C(18)** selects **M1 → T(30)** or
   **B1 → SW6 T1**. SW6 CHORUS then selects **M1 → CH(29)** or an undrawn/open
   B1 connection. Thus selecting SW5's M throw makes the downstream SW6 choice
   irrelevant to C. This is cascaded selection, not independent summing.
4. SW5 T2/T3 feed DEPTH VR4A/VR4B terminal 3. Their M throws choose
   **DO1′(16)/DO2′(20)**; their B throws go through SW6, whose M throws choose
   **DO1(17)/DO2(19)**. OE55 shows each primed signal behind **150k series**
   from the same oscillator output, not a third/fourth oscillator. Loaded
   DEPTH amplitude is not established by the 150k alone.
5. OE56 identifies IC3/IC5 as **IG00150**. OE55 routes common C through separate
   revised **180k** branches to their pin-1 control inputs, with 330k branches
   to the negative rail and independent VR1/VR2 reference-current trims.
   The OE56 PCB printing still shows 100k in these branches; retain that
   revision conflict rather than treating all supplied sheets as identical.
   The [Yamaha IC Guide Book](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf),
   physical page 29 / printed 2-1, identifies pin 1 as the control input and
   explicitly shows an **exponential converter**. It identifies pin 6, used
   by the DO outputs, as the sine output. This closes the basic waveform-type
   lead; distortion, amplitude, offsets, loading and phase at startup remain
   unmeasured. Numeric slope, input impedance and a calibrated installed
   frequency-versus-voltage equation are still absent.

The switch-contact labels above are literal. If M corresponds to the engaged
position, SW5/TREMOLO takes precedence even with both tabs engaged. No complete
mechanical-switch/transient model is claimed. This conflicts with treating the
existing independent both-on clock-plus-VCA law as recovered physical behavior;
that law remains a software inference and must not be promoted by this diagnostic.

A related bound is visible on OE55: the 47k/47k IC4 stage inverts **LO1 → LO3**,
and IC6 similarly inverts **LO2 → LO4**. The [OE2 p57](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/57%20-%20OE2%20Circuit%20Diagram.jpg)
control inputs continue L1→IC7/O1, L2→IC9/O2, L3→IC8/O3 and L4→IC10/O4.
The current software's crossed right-channel assignment and constant coherent
wet mono sum are declared assumptions, not those literal phase pairs. Absolute
DO-to-LO/VCA gain signs and full loaded transfer still require analysis. This
review does not silently change the production mapping or its regression oracle.

A further independent trace of OE57 shows **IC5 pin 2 → IC7 and IC10**, and
**IC6 pin 13 → IC8 and IC9**, through the respective 1 µF / 100k input paths.
The upper/lower reconstruction branches therefore cross into both OE1 output
sums; the existing evidence/model grouping IC7+IC9 as one BBD branch and
IC8+IC10 as the other is not the printed routing. Absolute transfer, trim
settings and gain/sign remain unresolved. This is another explicit reason not
to treat the existing rate fixture as verification of physical stereo behavior.

## Current implementation and the diagnostic

Both reviewed revisions define core index 27, `chorus_speed`, as 0.05–8 Hz,
default 0.55, skew 0.4. Engine settings pass that rate to BbdChorus; DO1 advances
at speed/sampleRate and DO2 at speed × (0.056/0.55)/sampleRate. The latter ratio
is a documented extrapolation away from the one calibrated operating point.
At maximum host rate this gives **125 ms** and **1.227678571 s**, outside both
printed period intervals.

`tests/diagnostics/Oe1SpeedEndpointProbe.cpp` is standalone and deliberately not
registered as a new passing CTest/CI gate. From the repository root:

```sh
c++ -std=c++20 -O2 -Isrc tests/diagnostics/Oe1SpeedEndpointProbe.cpp -o /tmp/oe1-speed-probe
/tmp/oe1-speed-probe --host-endpoint
/tmp/oe1-speed-probe --service-fixture
```

- `--host-endpoint` reads the actual parameter maximum, checks the printed
  period bounds, and **exits 1** for the current code. An already-known source
  mismatch must not become a passing hardware claim.
- `--service-fixture` directly supplies 1/0.056 Hz to the existing core class,
  bypassing host limits. It tests only whether the existing software can run
  at this point. It is not a physical SPEED implementation or a playable host
  control, and does not validate Engine/APVTS/automation/state migration.
- Paired deterministic renders differing only in DEPTH isolate the existing
  software VCA trajectory. A literal closed-time rate oracle checks it at
  88.2/96/192 kHz for two seconds each. The 2e-6 software error limit was fixed
  before evaluation; it is not a hardware waveform tolerance. The oracle
  deliberately retains the existing declared branch assignment rather than
  mixing a new physical-routing claim into a rate diagnostic.

Results with GCC 14.2.0 were identical on both reviewed revisions:

| Probe | Result |
|---|---|
| Host endpoint | FAIL; DO1 0.125 s, DO2 1.227678571 s; worst service-rate residual 0.252205 |
| Direct-core fixture | PASS; DO1 0.0559999983 s, DO2 0.549999983 s; worst service-rate residual 6.14024e-7 |
| Own-rate software trajectory | Worst error 8.19448e-8; finite output at all three rates |
| Mutation: DO2/DO1 ratio changed to 1 | Rejected, exit 1; period and trajectory failures |
| Mutation: actual DO1 phase increment halved | Rejected, exit 1; trajectory failure despite unchanged metadata-derived periods |
| Mutation: paired unmodulated render replaced with NaNs | Rejected, exit 1; nonfinite base/reference/residual checks prevent a NaN from hiding in `std::max` |

No existing threshold, fixture or gate was weakened. The fixture demonstrates
an isolated capability; it does **not** close the host-accessible end-stop gap.

## Compatibility-safe next implementation boundary

1. Keep the shipped ID, index, range, default, skew, normalized automation
   interpretation and old-session sound frozen. Merely increasing the range
   or multiplying every existing value changes saved automation/sessions.
2. First build an offline OE1 control-network model with explicit resistance,
   trim and load boundaries, contact selections, and independent DO1/DO2
   calibration. Fit only what primary numeric evidence or hardware measurements
   constrains. One end-stop cannot identify a taper, minimum rate or tracking law.
3. Test the physically selected contacts, LO phase pairs and shared downstream
   modulation routes together. Do not fix the rate by layering another control
   over an unresolved mode model.
4. Once quantitatively defensible, propose a versioned, explicitly opt-in model
   interpretation with old behavior as the restore default. Define appended
   parameter/state/UI contracts and migration tests *then*, before production
   changes. No new parameter or schema revision is proposed by this patch.
5. Retain separate source-point, software-compatibility and hardware-measurement
   gates. Hardware parity still requires measurements, not just diagram agreement.

## Reproduction and provenance

All board JPG originals are 5100×3300. Hashes below identify the inspected source
files. Crop boxes use native pixels, top-left origin. No contrast/sharpness edit
or resampling was applied to the PNG evidence crops; Panel66 crops are rotated
90° clockwise for reading.

| Source | SHA-256 |
|---|---|
| Adjustment PDF | `eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba` |
| OE55 | `524aaba53696004178b00f653ee2b69c736d957c589bb7641f67274859c76ef4` |
| OE56 | `79f58431e27434dacc6c55b664bc41cf4f5102bf00b84ed475ae17cc39308bfe` |
| OE57 | `9d3060b05899526469cbff8e65e5abbde116230fb4951580d9abd25902db72a9` |
| Panel66 | `d24815efb81591c6a194020490627d68c6dc83de94f8c39b436fb0eba7bbd64c` |
| Owner manual part 1 | `a9985aade98a495c3f204fd7170a9e19a805c241a4a2031d2ee9f8b778b4a3d4` |
| Yamaha IC Guide | `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1` |

Native crop boxes: OE55 oscillators `(2700,300,4700,3100)`; OE55 divider/reference
`(2100,1700,2900,3000)`; Panel66 knobs `(2850,1900,4600,3100)` and tabs
`(2850,400,4700,1800)`; OE57 controls `(2550,400,4750,2950)` and audio crossover
`(2700,850,4050,2350)`. The full OE56 sheet
was inspected for reciprocal harness/part identities. Adjustment and IC-guide
pages were Poppler whole-page renders, not native schematic crop pixels.
An independent reader corroborated the PN66 terminal assignments/cascade,
primed-output resistors, OE55 LO inversion pairs, and OE57 audio crossover. Board-revision differences,
real contact behavior and a particular instrument's trims remain unverified.
