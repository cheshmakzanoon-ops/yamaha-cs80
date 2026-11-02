# IG00156 nodal stage (circuit engine)

**Numerical update, 2026-10-31:** the audio solver now uses the
[phase-corrected nodal method](IG00156_PHASE_CORRECTION.md). The complete
registered small-signal grid passes at 2× and 4× without widened limits.
The equations and electrical inferences below are unchanged; the original
trapezoid qualification is retained below as historical evidence. The
[native ADJ/M41 review](evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md)
keeps the HP voltage miss open. Its corrected LP input reading and the
subsequently verified HP mixer shunt are recorded in the dated notes below.

Plan step 1.1b of [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md), 2026-10-31. It
replaces the first-stage realtime filter ([IG00156_REALTIME_MODEL.md](IG00156_REALTIME_MODEL.md),
`Svf::processCircuit`) in the circuit engine (Source Performance and Isolation).
The legacy policy keeps `Svf::process` byte-for-byte.

- Realtime: `src/core/Ig00156Filter.h` (`Ig00156Stage`, one per HP and LP stage).
- Offline truth model: `src/reference/IG00156Network.h` (double precision,
  exact complex response, implicit trapezoid with full Newton for the nonlinear
  case). It extends the conditional graph of `src/reference/IG00156Linear.h`.
- Gate: `tests/Ig00156NodalTests.cpp` (`cs80_ig00156_nodal_tests`) with the
  tolerances pre-registered in `tests/parity/tolerances-v2/ws1-voice.json`
  (commit `b87d560`, before any evaluation).

This is a public-evidence reconstruction with declared inferences. It is not
measured hardware parity.

## Topology and evidence class

Per stage, four capacitor states and ideal amplifiers:

| Element | Class | Source |
| --- | --- | --- |
| HP = 2 AI - LP; two inverting integrators | source-derived | M41 graph, CS20M F-3/F-4 polarity ([evidence](IG00156_MODEL_EVIDENCE.md)) |
| 1.5 nF integrators, 4.7 MOhm feedforward into each summing node | source-derived | M41 native crop |
| 1.2 MOhm in parallel with 180 pF from BP (pin 13) to FB/AI (pins 9/1) | source-derived | M41 native crop |
| 33 kOhm AI-node shunt (both stages) | source-derived | M41 |
| HP input: passive mixing node of the 150 kOhm waveform branches, then 1 uF NP into AI1 | source-derived topology | M41, [oscillator evidence](OSCILLATOR_CIRCUIT_EVIDENCE.md) |
| HP mixing-node Thevenin resistance 75 kOhm | **conditional approximation** | one selected FET waveform through 150 kOhm series, plus a distinct fixed 150 kOhm shunt to ground; off-FET assumed open; separate 150 kOhm NI branch assumed open (M41 follow-up below) |
| LP input: HP pin 15 -> 1 uF NP -> CP2 -> CP7 -> 150 kOhm -> AI1 | source-derived | M41 + harness p44; native endpoint correction below |
| LP series source 150 kOhm | source-derived connection | M41 native crop, 2026-10-31 erratum below |
| Q block: unilateral current Iq = -gq * BP | conditional hypothesis | as in `IG00156Linear.h` |
| Q-block law gq(VQ) | **inferred** | see below |
| Integrator OTAs gm * L * tanh(v / L), L = 2.5 engine units | **inferred** | kept from the first stage; the Q block and the 4.7 MOhm paths are linear |
| Cutoff calibration (180 pF compensation) | **inferred** | see below |
| Stage drive normalized to unity passband gain | **inferred adapter** | no absolute port-voltage calibration exists |

### HP switched-mixer source reading, 2026-11-01

On original M41 printed p41, drawing KEC-4223-65, the two gated waveform
paths are IC9 `SO` pin 5 through `T2`/FET6/150 kOhm, and IC9 `PO` pin 7
through `T3`/FET7/150 kOhm. The two series resistors converge at a common
HP mixer node with its **own fixed 150 kOhm shunt to ground**. With one gate
ideal-on and the other ideal-off, this independently grounded node produces
the nominal 75 kOhm source impedance and half-waveform level used in the
service-point fixture. An inactive FET branch is **not** a grounded source.
The gate impedances, NI source termination and full CP3 voltage transfer
are not fixed by that idealized calculation; the 1.692 Vpp HP miss remains.
The source/crop coordinates and primary-source SHA-256 are in the
[native review](evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md).
This HP input is not the LP `CP7 -> 150 kOhm -> AI1` series input with a
separate 33 kOhm shunt discussed next. Numerical values, test limits and V1
fingerprint are unchanged.

**2026-11-02 four-branch clarification:** Enlarged native M41 p41 pixels
resolve a **fourth 150 kOhm resistor from the mixer to NI terminal 43**, in
addition to the two gated waveform resistors and the permanently grounded
150 kOhm. Yamaha M11/12 harness p44 connects NI(43) to SUB NO1(39), which
Yamaha SUB schematic p29 traces to an IC3/2 buffer. The original 75 kOhm
half-pulse calculation is therefore the **NI-open boundary**, *not* a fully
loaded HP source model. If NO1 were ideally silent and zero impedance, the
effective resistance would instead be 50 kOhm and pulse ratio 1/3. Actual
NO1 voltage and loaded output impedance are unknown. `PW(40)` via 680 kOhm
and `PWM(41)` via 100 kOhm lead to IC9 WSC pulse-width control (PWI/PWO),
**not additional direct mixer shunts**. See the source image hashes and
[NI topology correction](evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md),
plus the [pre-registered, model-only endpoint comparison](evidence/continuation-2026-10-31/ADJ_NOISE_LOAD_PREREG_20261102.md).
No production model value or service band changes with this qualification.

**CP3 termination boundary, 2026-11-02:** M41 + M44 route IC11 LP pin 15
to `CP8(56)`, then the same-board `CP3(29)` probe; from CP3, a **1 µF/50 V
nonpolar capacitor and 100 kOhm resistor** connect in series to IG00151 VCA
IC4 `+IN` pin 3. Its `LI` control pin 2 instead connects to `LP1(27)` via
100 kOhm and has a separate 0.01 µF ground capacitor. The ideal-stage
service fixture probes CP3/CP8 at the IC11 output before that VCA input,
**not** at CP4, and omits undocumented finite IC11/IC4 impedances. See the
[M41 CP3 zoom and SHA-256](evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md).

**Follow-up model-only qualification:** A source-hash-verified, preregistered
NI admittance sweep and successive *surrogate* VR7-cutoff/VR8-Q grids are
documented in the [native ADJ evidence record](evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md).
Even though the synthetic Q=9 case can mathematically yield about 4.1 Vpp
at CP3, Q=9 is outside Yamaha's documented Q≈0.5–5 control endpoints,
with about 11 Vpp simulated at CP2 under an **unvalidated large-signal
linear assumption**. The original frozen HP CP3 calculation remains
**1.692 Vpp MISS**. These synthetic results are not hardware calibration,
have no source-derived VR7/VR8 potentiometer law, and make **no production
DSP or tolerance changes**.

### LP series source: original inference and 2026-10-31 erratum

**2026-10-31 erratum:** independent reinspection of the native M41 crop
resolves the previously conflicting resistor reading. IC11 AI1 has a
**33 kOhm shunt to ground** and a **150 kOhm series connection to CP7,
terminal 53**. The 150 kOhm lower lead ends at the labelled CP7 terminal
circle, not at ground. In the 2070 × 770 `m-filter-detail` crop the endpoint
is near `(645, 700)`, with the AI1 junction near `(640, 423)`.

Source: printed p41, drawing KEC-4223-65; native box
`[1180,1080,3250,1850]`; source SHA-256
`1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`;
crop SHA-256
`5dfe074bc3868f22fe662cbe55d718dbb9479db0d05f36a4089a8aae445cbcc7`.
The crop was obtained in
[run 41224106605, job 124604536319](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536319).
See the [corrected native review](evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md).

The older ledger and the first native-review text mistakenly called the
150 kOhm a second ground shunt. Their `33k || 150k = 27.049k` description is
withdrawn for this node. The existing series-source numerical model already
uses the visible connection, so this erratum changes evidence classification
only: no source, V1 or circuit-engine numeric values, tolerance, or test is
changed. The HP fixture's 1.69 Vpp discrepancy remains open; this resistor
reading does not explain or fit it.

**Historical reasoning, before that endpoint was verified:**

The ledger reads a 150 kOhm resistor at the LP AI node as a shunt (parallel
27.049 kOhm with the 33 kOhm). With an ideal op-amp at HP pin 15 and only the
1 uF capacitor in series, the LP AI node would be held by a stiff source at
audio frequencies and the LP stage would have no damping at all
(Q = Gtotal / (2 (Gf + gq)) grows without bound as the source conductance
grows). A finite series resistance is therefore required for the documented
LP resonance to exist. The model reads the 150 kOhm as that series element,
which mirrors the 150 kOhm waveform resistors at the HP input.
Alternatives: (a) 150 kOhm shunt as the ledger states plus an unidentified
series resistance; (b) a stiff source, rejected because it removes all LP
damping. A focused native crop of the CP7 -> AI1 path would resolve it.

### Q-block law (declared inference)

The documented endpoints are Q = 5 at VQ = 0 V and Q = 0.5 at VQ = 10 V
(service p42). The selected law makes the Q-block conductance affine in VQ,
as an OTA whose gm is proportional to a control current I0 + VQ / Rq:

`gq(VQ) = gq(0) + (gq(10) - gq(0)) VQ / 10`, with `gq(0) = Gtotal / 10 - Gf`
and `gq(10) = Gtotal - Gf` (the Qadj trim sets gq(0)).

In the algebraic limit Q = Gtotal / (2 (Gf + gq)), so this law is exactly
`1/Q = 0.2 + 0.18 VQ`, which is what `provisionalFilterQ` and
`source_factory::qFromVolts` already compute. The stage therefore takes the
existing low-frequency Q and derives gq = Gtotal / (2 Q) - Gf. The test checks
this identity at every 0.5 V step.

Alternatives listed for later discrimination: exponential gq(VQ); linear Q in
VQ; a bilateral conductance Iq = gq (AI - BP); a saturating Q OTA.

### Cutoff calibration (declared inference)

In the ideal graph the 180 pF branch feeds BP back into OP1 and lowers the
natural frequency by about `1 / sqrt(1 + 2 w Cfb / Gtotal)`: -2.3 % at 1 kHz,
-4.4 % at 2 kHz, -20 % at 12 kHz (uncompensated alternative, reported by the
test). The first evaluation of the pre-registered anchor gate failed for that
reason. The selected interpretation is that the documented control law
(guide p34, service p42) describes the network's cutoff, as a technician
would measure it, so gm is raised to compensate: `ig00156IntegratorRate`
solves the first-order model `W^2 = w^2 / (1 + 2 w / a)` in closed form,
a = Gtotal / Cfb. Above 20 kHz the compensation ratio is held at its 20 kHz
value (the first-order model fails near the 33-39 kHz AI-node corner, and an
unheld ratio made the stage reach 216 on the +/-8 stability fixture at 0.45 fs).
An exact cubic-factorization Newton was tried and rejected: it has no stable
root for overdamped settings above about 5 kHz.

As in the first stage, the documented law is the gm part and the 4.7 MOhm
feedforward adds 22.575 Hz.

## Discretization

Trapezoidal companion model. Every capacitor's companion coefficient uses the
same prewarp p = tan(theta) / theta, theta = pi * fTotal / fs: the bilinear
transform warped to pin the requested cutoff exactly. The two linear states
(input coupling and 180 pF branch) are eliminated per sample through a cached
2 x 2 inverse; one scalar Newton solve on BP remains, with the first stage's
cached-slope start, 1e-7 tolerance and at most five evaluations. Coefficients
are recomputed only when cutoff, Q or rate change (exact-argument memo, as in
7b55d34). Per-capacitor prewarps (180 pF row at 15 kHz, fs/5, fs/4 or its own
corner) were evaluated and are all worse (core error 0.13-1.0 at 2x).

Antiderivative antialiasing was evaluated and **not added**: driving the pair
with a pure sine at 1, 2.5 and 5 engine units at 88.2/96/192 kHz, the
non-harmonic energy the in-loop nonlinearity creates is at the -88 to -93 dB
measurement floor, independent of level. The OTA currents are integrated, so
only very high harmonics can fold into the audio band. ADAA (a log-cosh per
OTA per Newton step) cannot measurably improve the -70 dB target.

## Initial trapezoid results (historical, before phase correction)

| Gate (pre-registered) | Result |
| --- | --- |
| Realtime equals the truth trapezoid, 100 grid points x 3 configurations | worst 1.9e-7 of output peak (limit 2e-6): **met** |
| Small signal vs continuous truth, core band (0.5..2 fc, <= fs/8), 3 % | met for cutoffs <= 4 kHz at every rate and for the whole grid at 384 kHz; **not met** at 12 kHz cutoffs for 88.2/96/192 kHz (4.8/4.0/3.5 %) |
| Small signal, full band 20 Hz..min(20 kHz, fs/5): 8 % below 192 kHz, 2 % at >= 192 kHz | met for cutoffs <= 1 kHz and at 384 kHz; **not met** at 4/12 kHz cutoffs (11.3/14.0 % at 88.2 kHz, 2.8/3.5 % at 192 kHz) |
| Large signal vs nonlinear truth at 16x, 1.0 and 2.5 units, Q 5 and 1 | worst 0.13 % RMS (limit 3 %): **met** |
| Q anchors at 200 Hz | HP 5.023 / 0.502, LP 5.026 / 0.501 (limit 1 %): **met** |
| Cutoff anchors 0.25 V/5 V, 0.5 V/5 V, 0.25 V/6 V (law + 22.575 Hz) | HP +0.25 / +0.46 %, LP +0.29 / +0.53 % (limit 1 %): **met** (uncompensated alternative: -2.3 / -4.4 %, LP -2.7 / -5.1 %) |
| ADJ p11 service points, physical gains | HP setup 1.69 Vpp vs documented 4.3 +/- 1: **miss**; LP setup 1.65 Vpp vs 2.2 +/- 1: within band. Registered as reported, never fitted. |
| Stability, +/-8 noise, 88.2-384 kHz, 0..0.45 fs, Q 0.35-9 | each stage < 200, cascade finite and not growing: **met** |

The small-signal misses are the bilinear warp of the top octave at 2x and 4x
for resonances above about 4 kHz; the first-stage TPT filter has the same
limitation. The test asserts the registered limits on the sub-grids where they
hold and prints the full-grid verdict, so the gate and its limits are not
relaxed. At 8x (384 kHz) the whole grid passes. This is direct input to the
Standard/HQ oversampling choice (ws2).

The ADJ p11 HP/LP ratio is not explained by any stage gain: both setups put a
Q = 5 resonance on the fundamental, so their ratio is independent of the
gains, but the documented readings differ by 1.95x. The setup conditions in
`SourceFactoryTones.h` (both sides Q 5, LP HIGH) may not be the manual's.

Model predictions (not gates): the 180 pF branch lowers resonance at high
cutoff (Q setting 5: HP 5.02 at 200 Hz, 3.52 at 12 kHz; LP 5.03 -> 3.12), and
rolls the HP passband off by 1 dB at 20 kHz. Physical passband gains are
HP 0.600 and LP 0.353; the audio path normalizes both to 1.

## Mutation checks

| Mutation (temporary, in a scratch copy) | Detected by |
| --- | --- |
| 180 pF branch removed (Cfb x 1e-6) | equivalence (300 failures), nonlinear, direct lock-in |
| Input coupling removed (Cin x 1e9) | equivalence, DC-blocking check |
| OTA made linear | nonlinear truth, resonance compression |
| Q law without -Gf | equivalence, nonlinear, direct lock-in |
| Cutoff calibration removed | cutoff anchors (6 failures) |

The DC check originally drove an HP -> LP pair, which blocks DC through the
HP response alone; the coupling mutation exposed that, and the check now
drives single stages.

## Engine integration

`Engine::Rank` holds `hpStage`/`lpStage`; the circuit-engine branch of the rank
chain calls `set(cutoff, Q, rate)` and `process()`. `RankAudioState` reports the
stages' BP/LP companion histories in its `hpf`/`lpf` fields for circuit
policies (same zero/equality probes) and the complete states in the appended
`hpCircuit`/`lpCircuit`. Resets follow the existing policy (cold assignment,
reset, policy transitions).

Intentional circuit-engine sound changes: frequency-dependent resonance
(less at high cutoff), 1 uF coupling corners (about 1.5 Hz HP, 0.9 Hz LP),
HP passband roll-off near 20 kHz, the OTA limit now acting on the gm part only
(the 4.7 MOhm path stays linear), and the calibrated cutoff law.

## Still open

- The HP mixing-node loading resistors and the complete interstage loading
  model. The LP 150 kOhm series connection is resolved by the dated erratum.
- A gm-versus-control-current law, finite amplifier effects, Q-block saturation.
- Identification between the Q-block alternatives (needs pin-9 measurements).
- Top-octave accuracy at 2x/4x (oversampling choice, ws2).
