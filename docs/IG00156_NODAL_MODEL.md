# IG00156 nodal stage (circuit engine)

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
| HP mixing-node Thevenin resistance 75 kOhm | **inferred** | two 150 kOhm branches in parallel; FET on-resistance, NI and the unvalued loading resistors neglected |
| LP input: HP pin 15 -> 1 uF NP -> CP2 -> CP7 -> AI1 | source-derived | M41 + harness p44 |
| LP series source 150 kOhm | **inferred** | see below |
| Q block: unilateral current Iq = -gq * BP | conditional hypothesis | as in `IG00156Linear.h` |
| Q-block law gq(VQ) | **inferred** | see below |
| Integrator OTAs gm * L * tanh(v / L), L = 2.5 engine units | **inferred** | kept from the first stage; the Q block and the 4.7 MOhm paths are linear |
| Cutoff calibration (180 pF compensation) | **inferred** | see below |
| Stage drive normalized to unity passband gain | **inferred adapter** | no absolute port-voltage calibration exists |

### LP series source (declared inference)

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

## Results (all from `cs80_ig00156_nodal_tests`)

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

- The LP series-resistance reading and the HP mixing-node loading resistors.
- A gm-versus-control-current law, finite amplifier effects, Q-block saturation.
- Identification between the Q-block alternatives (needs pin-9 measurements).
- Top-octave accuracy at 2x/4x (oversampling choice, ws2).

### 2026-11-01 evidence recheck: ADJ p11 remains unqualified

The model result **1.69 Vpp under the ADJ p11 HP setup** remains a miss
against **4.3 ± 1 Vpp** (the documented band is 3.3–5.3 Vpp). The LP
reading **1.65 Vpp** remains inside its **2.2 ± 1 Vpp** band. These are
software-model predictions under the declared fixture, not measurements of a
hardware CS-80. They have not been changed to force agreement.

There is a setup inconsistency that warrants native-source inspection before
any adjustment. The HP model's **75 kΩ** source is the parallel value of two
150 kΩ waveform feeds, whereas `serviceSetup` gates off the saw and then
represents the remaining square as `(0 + pulse) / 2`. The actual waveform
switches and passive mixing-node load determine *both* the pulse amplitude
and the Thevenin resistance when the saw is off. Holding the current 75 kΩ
network fixed and merely removing the fixture's factor of two would predict
3.38 Vpp, but that calculation is **only a sensitivity scenario**: with one
branch disconnected the network conductance also changes. It is not grounds
to alter the model, the fixture, or the registered service-point tolerance.

The M41 evidence ledger ([IG00156_MODEL_EVIDENCE.md](IG00156_MODEL_EVIDENCE.md))
calls the LP-side 150 kΩ a **shunt** alongside the 33 kΩ resistor; the realtime
model explicitly uses a **series** 150 kΩ to retain a finite modeled LP Q
under the ideal-output assumption. Alternatives remain (1) the ledger's
150 kΩ shunt plus an unidentified series resistance, (2) a missing loaded
interstage path, or (3) a different effective op-amp output impedance.
These alternatives require the native M41/CP7-to-AI1 crop or pin-level
evidence. No hardware response, source resistor, or drive level has been
fitted from the ADJ discrepancy.

### Numerical limitation of a single cutoff prewarp

The current coefficient law already prewarps all four capacitor histories
at their requested total cutoff. A *constant* per-stage prewarp `p`
maps a sampled frequency `f` to the analog-equivalent frequency
`fs * tan(pi * f / fs) / (pi * p)`. It can cancel bilinear warping at one
chosen frequency, but it cannot cancel the frequency-dependent mismatch
over the gate's entire 20 Hz–20 kHz band. In particular, the upper portion
of the gate includes frequencies substantially above the cutoff at a
sample rate of only 88.2 or 96 kHz. Moving the one anchor cannot by
itself establish the unpassed 8 % / 2 % full-band criteria.

An accurate next candidate is a stable higher-order or matched-z
discretization of the **whole coupled four-state network**, including
the input and 180 pF feedback branches. Any such implementation must be
compared with the unchanged continuous truth model at the registered
rates, cutoffs and Q values, and must satisfy the same nonlinear,
stability, idle-referencing and compatibility gates. This is a
mathematical numerical-method issue, not evidence of an analog component
value; no tolerance or circuit law should be changed to mask it.
