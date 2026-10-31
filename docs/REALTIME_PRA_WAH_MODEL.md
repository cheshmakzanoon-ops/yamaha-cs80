# Realtime PRA wah: circuit-derived nominal model

2026-10-29. **Realtime module integrated into Engine audio in opt-in isolation mode; the default-off branch retains the historical SVF.** Under the [public-knowledge requirement](ANALOG_FIDELITY_REQUIREMENT.md), missing hardware does not block implementing a known feature. The model below separates documented circuitry from a deliberately inferred playable profile. It does not claim measured WI→WO equality, calibrated pedal response, or real IC headroom.

## Documented graph versus inference

The reviewed graph, source images/hashes, unresolved labels and independent AC/MNA verification are in [PRA_WAH_REFERENCE_DESIGN.md](PRA_WAH_REFERENCE_DESIGN.md). Its four audio capacitors are 10µF input coupling, 2.2nF at P, 33nF at Q and 1µF output coupling. Four ideal amplifier constraints, 10k resistors and the **470Ω** feedback return define the instantaneous network. This is not a replacement generic band-pass. The documented order is ring/MODULATION → PRA wah → OE1/OE2 → optical expression; see [routing evidence](OE1_SIGNAL_ROUTING_EVIDENCE.md).

The offline `reference::pra_wah` API retains mandatory explicit boundaries and NaN defaults. A separate production profile supplies the following **engineering choices**, not newly discovered component facts:

| Choice | Nominal | Rationale and sensitivity cases |
|---|---|---|
| Parallel photoresistor branch | 470kΩ | PRA54 candidate near PH3; unresolved trace assignment remains. Compare 100k, 220k, 1MΩ. |
| VR1 effective rheostat | 1kΩ | Small nonzero in-range trim allows broad upper sweep without zero-R degeneration. Compare 0, 12.5k, 25kΩ. |
| VR2 effective feedback | 20kΩ | Explicit gain-setting inference: the un-loaded middle network reaches about 0.5 at its nominal resonance, and this trim provides ×2. Coupling/loading still alter gain; no post-normalization. Compare 0, 10k, 50k, 100kΩ. |
| Source Thevenin resistance | 1kΩ | Finite modest source impedance, not a recovered VR7 output resistance. Compare 0, 5k, 10kΩ. |
| Ropt at heel/toe | 1MΩ / 2kΩ | Logarithmic interpolation produces a broad musical sweep; direction follows existing pedal behavior, not calibrated EI polarity. Compare endpoints by ×0.5/×2 and reversed polarity in sensitivity tests. |
| Optical/control dynamics | 12ms one-pole time constant in log resistance while the resistance falls (brightening); **60ms while it rises (darkening)**, since 2026-10-31 | Declared LDR model ([ws4-circuits Step 4](workstreams/ws4-circuits.md)): CdS-type cells respond faster to rising than to falling illumination. The 12ms keeps the prior timescale for the fast direction. Not lamp/LDR data. Alternatives in the ws4 report. No hysteresis beyond the asymmetry, temperature or noise is invented. |
| FET1/FET2 and downstream load | ideal switch; high-Z receiver | Dry X, processed Y; zero on resistance/no off leakage. Open dry/output load boundaries for circuit computation. Compare finite 100k/10k dry shunts. Output load has no voltage effect with ideal amp. |
| Switching edge | existing 12ms crossfade | Click-control policy, not recovered FET transient. Both paths continue to run. |

At fixed pedal, `Rt=VR1+(Ropt||Rparallel)`, `t1=Rt·2.2nF`, `t2=10k·33nF`. The nominal middle-network resonance is approximately `1/(2π sqrt(t1 t2))`, roughly 330Hz–3.4kHz for this profile; complete loaded peak frequency/gain must be measured from the modeled response, not assumed equal to this approximation. Q is graph-derived and position-dependent. Full voltage-to-digital calibration remains unspecified; signal units remain the existing engine units.

## Stateful discretization

Use trapezoidal capacitor companions at the **engine internal sample rate**, without individual-frequency prewarping. For each capacitor with voltage orientation v and current i in that direction, store previous physical `(v,i)`. For timestep h:

```
g = 2C/h
j = -g*v_previous - i_previous
i = g*v + j
```

After solving, save v and i. This retains capacitor voltage/charge across realtime Ropt changes instead of reinterpreting biquad state. The same discretization can accommodate other resistance changes, but this API treats VR1 and other profile values as setup-only: reconfiguration through `prepare` deliberately resets. Fixed timestep can cache g; no heap allocation, Newton iterations or unbounded loops occur in `process`.

Let `R=10000`, `r=470`, source input U, source resistance Rs, passive real dry shunt Gd. Input capacitor orientation is from its source-side plate to X. P and Q capacitor orientations are node to ground. Output capacitor orientation is D to C. Compute the four histories jI,jP,jQ,jO and conductances gI,gP,gQ,gO:

```
aP = 1/(1+Rt*gP); bP = -Rt*jP*aP
P = -aP*N + bP
B = (1-2*aP)*N + 2*bP                  // B=2P-A, A=-N

aQ = 1/(1+R*gQ); bQ = -R*jQ*aQ
bN = 1-2*aP; b0 = 2*bP
cN = (2*aQ-1)*bN
c0 = (2*aQ-1)*b0 + 2*bQ
C = cN*N + c0                          // Q=aQ*B+bQ; C=2Q-B

dN = 2/R + (1-cN)/r
nX = (1/R)/dN; n0 = (c0/r)/dN
N = nX*X + n0

yI = gI/(1+Rs*gI); jSource = jI/(1+Rs*gI)
X = (yI*U + jSource + n0/R)/(yI + (2-nX)/R + Gd)
D = (gO*C-jO)/(gO+2/R)
Y = -(VR2/R)*D
```

Recover `iI=yI*(U-X)+jSource`, `vI=U-Rs*iI-X`; P/Q have their solved node voltages; output has `vO=D-C`. Update all four capacitor histories. Positive component values and nonnegative source/load conductances yield `aP,aQ` in (0,1], `cN` in [-1,1], `dN >= 2/R`, `nX <= 1/2`, and a strictly positive input denominator. This removes instantaneous feedback analytically, rather than delaying a loop by one sample. Positive instantaneous denominators do **not** by themselves prove a universal energy/gain bound under arbitrary time-varying control; the bounded dynamic fixtures below are separate numerical evidence. Use double state/arithmetic; float I/O conversion only at the engine boundary.

For constant coefficients this is the bilinear transform of the declared continuous linear graph. The AC comparison must use the exact warped analog frequency `f_a = fs/π * tan(π*f_d/fs)`; agreement there verifies discretization, while comparison at the **same physical frequency** across rising sample rates quantifies convergence/warping rather than hiding it.

## Implemented API and Engine lifecycle

`src/core/PraWahCircuit.h` implements an explicit bounded `Profile`, `prepare(internalRate, profile, pedal)`, `reset()` / `reset(pedal)`, `setPedal(normalizedTravel)`, `setOpticalResistance(ohms)`, and `process(sample)` returning all nine named node voltages, including dry X and wet Y. Read-only capacitor voltage/current, optical resistance, target, rate and profile diagnostics support verification. There is no reference solver dependency in production. No additional host/core parameters are needed.

`prepare` validates once, returns false without changing existing state on invalid setup, and otherwise resets. Supported internal rates are 8kHz–1.536MHz; Ropt/heel/toe 100Ω–10MΩ; Rparallel 10kΩ–10MΩ; VR1 0–25kΩ; VR2 0–100kΩ; source 0–100kΩ; dry load 0–1mS; optical time constant 0–1s. Reversed heel/toe endpoints are allowed explicitly, not silently reordered. The inference profile is bounded; this is not an unrestricted circuit simulator.

For the log-resistance state `l`, each sample uses `l += (1-exp(-1/(tau*fs))) * (target-l)`, implemented with `-expm1` for precision. Tau zero selects the target immediately on the next processed sample. Twelve milliseconds means **one exponential time constant**, not time to reach the endpoint. Both control setters only select the target; `reset` selects that target immediately and zeros capacitor state. Nonfinite setter controls and out-of-range direct resistances are ignored, finite pedal travel is clamped [0,1]. `reset(nonfinitePedal)` ignores the invalid pedal but still performs the explicitly requested reset using the existing target. Nonfinite audio input is treated as zero and finite input is clamped to ±1,000,000 engine units as an explicit numerical interface guard, **not analog rail/saturation modeling**. No automatic capacitor reset conceals instability. Setup, control and sample methods are `noexcept`; processing uses bounded stack storage and scalar arithmetic.

Only isolation mode uses this module. Default-off continues through the existing SVF branch with unchanged arithmetic, RNG consumption and legacy-filter reset behavior. Existing foot mode selects dry/wet; physical expression controls the inferred optical target. Both stereo instances receive the same control trajectory; isolation currently centers voices before this stage. The circuit runs every internal sample even while EXP/dry is selected, expression is zero or voices are idle; switching cannot resurrect a stale capacitor tail. No additional generic SVF or post-normalization is applied in isolation.

Engine `reset`, including prepare and mode changes, follows the existing controller convention: expression becomes **1**, capacitor voltages/currents become zero, and both current and target optical resistance start immediately at the nominal **2kΩ toe**. The foot-mode crossfade starts at the selected EXP/EXP-WAH endpoint. This is an explicit startup/reset policy, not inferred physical lamp behavior. If the host subsequently restores a non-toe physical pedal value, even before the first rendered sample, it is a normal control event: optical resistance follows the **12ms log-R slew**, rather than snapping to the restored pedal. Downstream expression gain uses that restored value immediately under the existing gain law. The module's standalone `reset(pedal)` can instead select a specified optical position immediately; Engine deliberately uses the reset-toe convention. Continuous sample-rate changes without reset are not a promised host lifecycle.

`Engine::getPraWahState(channel)` exposes render-thread-only read-only snapshots of capacitor voltage/current, circuit rate, optical resistance/target and switch mix for integration tests. These are reconstruction-model states, not measured instrument quantities. Invalid channel indices return an empty snapshot.

Dry X rather than raw U intentionally includes the documented input coupling and modeled source loading. FET/off-branch/parasitic loading remains idealized. Crossfade of X/Y is an interpolation policy, not a circuit-accurate intermediate switching topology. Analog compensation, op-amp bandwidth/rails, FET distortion, optical noise/history, tolerances and supply behavior remain unmodeled, not hardware blockers.

## Verification and release checks

1. Compare all nine node phasors (or diagnostic snapshots) against the existing independently MNA-verified AC oracle over pedal/resistance/trim/source/load sweeps. Excite measured steady-state sinusoids, account for bilinear warping explicitly, and include both coupling corners and resonance. Include unwarped frequency error/convergence across internal rates.
2. Independent transient nodal/companion assembly (or independently implemented continuous-state integration at refined timestep) checks the analytic feedback reduction, capacitor orientations and state updates. Smaller h must converge on the same physical-time impulse/step/pedal trajectory, not merely produce finite samples.
3. Dynamic full pedal sweeps, fast reversals, changing resistance while charged, DC/impulse/silence, long tails and repeated foot switching remain finite and bounded for the declared profile domain. Check held capacitor voltages before and after a target change; no reset occurs. Do not assert arbitrary gain bounds without measuring the graph.
4. Startup, prepare/reset, EXP always-running histories, wet enable after a dry interval, stereo identity, partition invariance and mode changes. Tests prove dry path uses X and wet Y, no hidden makeup factor and no generic SVF in isolation.
5. Reject mutants for 470Ω→470kΩ, capacitor values/orientations, feedback inversion, omitted coupling, state reset on pedal change, frozen bypass state, hidden normalization, and isolation/legacy dispatch.
6. Run all core suites and sanitizers; both pinned legacy-off raw corpora must remain byte-exact. Host smoke and multi-platform CI cover existing pedal/foot parameters with no schema changes.

### Executed standalone verification

`tests/PraWahCircuitTests.cpp` currently passes **8,758,418 checks** in a local optimized GNU C++20 build. These are algorithm/fixture checks, not hardware acceptance statistics:

- 48 measured sinusoidal fixtures, all nine nodes, 48/88.2/192kHz, 8Hz–18kHz, optical/trim/loading variations. The warped AC oracle is the existing independently MNA-verified `reference::pra_wah`; maximum measured complex error was **2.285×10⁻¹⁰**.
- Unwarped comparisons at 332Hz, 3.4kHz and 12kHz improve with 48→96→192→384kHz refinement. No resonance prewarping hides the error.
- Separate **10-node transient Gaussian-elimination assembly**, including the physical source-side capacitor node, compares all nodes and capacitor states under randomized controls/profiles at 8k/88.2k/192k/1.536MHz. KCL and trapezoidal charge continuity are also checked directly.
- The same continuous 900Hz input envelope and 20Hz pedal trajectory, observed at common physical times, converge toward a 1.536MHz reference: nine-node RMS errors at 48/96/192/384kHz are **9.2260×10⁻⁴, 2.3002×10⁻⁴, 5.6830×10⁻⁵, 1.3531×10⁻⁵**. This checks moving-coefficient convergence, independently of the AC comparison.
- Startup/reset, charged-state control changes, invalid-input/setup nonmutation, log-R time constant, zero smoothing, DC rejection, decay, dry/wet output selection with continuously clocked histories and dynamic reversals pass. Tested sensitivity trajectories have peak wet magnitude **1.698** for bounded input; nominal dynamic fixture peak **0.211** for input bounded by 0.5. These are fixture outcomes, not a universal gain/stability proof or real-instrument voltage limits.
- Atomic rejection is checked against a charged, mid-slew, nondefault model, using distinct candidate profile/rate/pedal values. Each field's inclusive endpoints, nearest representable out-of-range values, ±infinity and NaN are covered. Diagnostics, capacitor voltage/current and later output/control trajectories remain bit-identical after rejection. Valid reprepare discards old state; finite extreme pedals clamp even with reversed polarity; direct resistance endpoints, explicit reset with invalid pedal and symmetric input guards are covered.
- C++ allocation hooks observe zero allocations while processing 5,000 samples/control changes. This is a module-local test, not a whole-plugin hard-realtime guarantee.
- The same suite passes AddressSanitizer/UndefinedBehaviorSanitizer locally with leak detection disabled; no leak-clean claim is made.
- Eleven independently compiled mutations are rejected: 470Ω return changed to 470kΩ, each of the four capacitor values changed, output inversion removed, dry loading omitted, pedal setter clearing charge, hidden fixed output normalization, frozen optical state and capacitor-history current sign reversed. Bypass-freeze/legacy-dispatch mutations belong to the separate Engine integration suite; the standalone module deliberately has no bypass/freeze method.
- Seven additional independently compiled rollback mutations are rejected: a failed `prepare` prematurely replacing the profile, rate, optical target, capacitor/optical reset state, smoothing coefficient, endpoint-log cache or capacitor-conductance cache. All **18 mutants** fail the final expanded suite; compilation failure is not counted as mutation detection.

`tests/PraWahIntegrationTests.cpp` checks Engine dispatch, physical-pedal and foot-mode trajectories, reset/reprepare/mode changes, always-clocked histories, render partitioning and surrounding-stage routing against a separately assembled nodal circuit. Envelope, chorus and decimator primitives are shared intentionally: this establishes routing, not independent validation of their internal models. Optical dynamics use an independent closed-form exponential, and nominal circuit values are pinned in the oracle rather than read back from the production profile. Before publishing a verified checkpoint, the full core suites, pinned legacy-off raw corpora, sanitizers and applicable host CI remain required gates. Hardware comparison is optional future refinement, not permission to fabricate evidence or pause implementation.

### Integration verification checkpoint

The focused Engine suite passes **164,808 checks**. Twelve separately compiled integration mutants are rejected, covering frozen bypass state, raw dry path, incorrect wet node, misplaced/missing expression, wrong internal rate, stale reset pedal, wrong legacy dispatch, hidden makeup, charge reset and ring/chorus ordering. The final local Release and ASan/UBSan configurations each pass **27/27 CTest suites** (58.35s and 245.45s respectively; sanitizer leak detection disabled). Both default-off raw corpora retain the pinned baseline hashes (`4a3483f77c9df9895386b98eaa169358d19b463c21283231f994c5d0f6c523de` normal; `fe447882e3ea3560eefb7b5ae4a474b151efe5ced775b5ab36f2609c5fde0660` tails). These hashes are same-compiler software compatibility evidence.

The old cross-pedal exact RMS ratio is no longer physically appropriate: dry X depends on optical network loading. The parity suite instead proves source-amplitude linearity at a fixed pedal using the original tolerance, while the independent integration oracle checks downstream expression gain. Host tests compare consumed-versus-pending restoration at identical pedal values, both selectors, reprepare, ignored CC11 and regular/irregular render partitions. The host source passes a JUCE 8.0.6 syntax check; actual plugin/UI execution requires exact-commit CI. No parameter ID or state schema changes.

### Cross-platform rollback-fixture correction

At commit `a494118`, [macOS ARM job 123770931775](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40966112571/job/123770931775) failed 86 exact continuation comparisons in the standalone rejection fixture. The host smoke and Engine integration suites passed; the job remained failed and its validator stages were not run. The fixture had independently warmed two inlined instances before testing rejection. Different optimizer specialization/contraction is a plausible explanation, not a reproduced or proven ARM diagnosis.

The corrected fixture copies one charged state, checks every byte of the same object before/after rejection (including private caches), and runs both continuations through a shared non-inlined runtime-dispatched kernel. Exact equality remains required; no tolerance or production DSP is changed. All **8,758,418 checks** pass with GNU and Clang19 optimized FMA-enabled builds and ASan/UBSan locally (leak detection off), and all 18 source mutants remain rejected. ARM verification requires the new exact-commit CI run; passing x86 checks alone does not close that result.
