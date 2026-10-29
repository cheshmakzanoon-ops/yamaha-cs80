# Realtime GENERAL / HIGH–LOW output model

2026-10-29. **Realtime module and isolation-only Engine integration implemented and locally verified; exact-commit host/platform CI remains separate evidence.** [`src/core/OutputTapCircuit.h`](../src/core/OutputTapCircuit.h) and [`tests/OutputTapCircuitTests.cpp`](../tests/OutputTapCircuitTests.cpp) implement the bounded next output stage after the PRA wah. The refined standalone suite passes **6,506,427 checks**, with **18 separately compiled mutants rejected**. Standalone counts do not substitute for the separately reported Engine and host/CI checks. The model follows the [public-knowledge mandate](ANALOG_FIDELITY_REQUIREMENT.md): unresolved installed values become named, revisable engineering choices, not a hardware-access implementation gate. The conditional offline API in `src/reference/OutputTaps.h` retains its explicit unknown boundaries and source conflicts.

## Decision and audible scope

In opt-in isolation only, replace the arbitrary HIGH unity / LOW 0.1 multiplier with three independently stateful, loaded, passive output networks. Give LEFT, RIGHT and GENERAL their own 10 µF coupling capacitor. Derive GENERAL **before** those networks from an explicitly idealized active-stage relation. Continue to clock all three networks, but the first Engine integration retains **`global.stereo=true` and inert `P_outputMode` in isolation**: the host hears LEFT/RIGHT, while GENERAL is available only to standalone tests and diagnostics. Preserve the entire legacy-off output arithmetic, including its DC blocker, soft clip, scalar smoothing and mono average.

Do not normalize either new switch endpoint back to the previous level. The chosen source/load profile slightly reduces HIGH; LOW changes from 0.1 to approximately 0.104 of its preamp source at midband; coupling adds a roughly 3 Hz pole rather than a fixed 10 Hz blocker. GENERAL is not defined as the average of already loaded jacks. These are deliberate isolation-only audible changes, not measured voltage calibration.

## Evidence, conflict decision and boundaries

The source graph, originals, URLs, hashes and native crop boxes are recorded in [OUTPUT_LEVEL_SOURCE_REVIEW.md](OUTPUT_LEVEL_SOURCE_REVIEW.md). This design re-read `foldout-output-native.png` and independently reviewed its ideal active summing equations. The foldout, PRA54 board and PN5 switch crop hashes were rechecked and match that record. No new primary-source transcription is claimed for a printed value not already visible there.

**Documented:** three linked changeover poles select unprimed HIGH versus primed LOW at the LEFT/GENERAL/RIGHT jacks; each driver feeds a 10 µF NP series capacitor; the LOW 4.7 kΩ branch and its shunt remain installed in HIGH mode. Headphone HL/HR leave the LEFT/RIGHT drivers before the capacitor and selector. GENERAL is a separate active summing amplifier. **Not documented:** a universal output attenuation in dB, external receiver impedance, source impedance, trim settings, voltage-to-digital scale, or real switch transients.

The implementation uses a named `InferredSymmetricOutputProfile`, not an allegedly recovered installed revision:

| Parameter | Nominal choice | Basis, uncertainty and required alternatives |
|---|---:|---|
| Coupling C, each jack | 10 µF | Documented nominal; ideal capacitor. Sensitivity 5/8/12/20 µF, not measured tolerances. |
| LOW series Ra, all jacks | 4.7 kΩ | Corroborated schematic/board value. |
| LOW shunt Rb, LEFT/RIGHT | 560 Ω | Corroborated schematic/board value. |
| LOW shunt Rb, GENERAL | **560 Ω, inferred conflict selection** | Choose foldout value and repeated attenuator topology for a nominal symmetric instrument. PRA54 **560 kΩ remains a literal alternative**, not silently corrected or dismissed. |
| HIGH series Rh, LEFT/GENERAL | 560 Ω | Corroborated schematic/board value. |
| HIGH series Rh, RIGHT | **560 Ω, inferred conflict selection** | Choose board reading and symmetric three-output construction. Foldout **56 kΩ remains a literal alternative**. |
| Driver Thevenin Rs, each jack | **100 Ω, inferred** | A finite low source impedance makes loading explicit without claiming an HA1452 output-impedance measurement. Sweep 0/100/1000 Ω and larger supported values. |
| Jack load, independently for all three | **100 kΩ resistive, inferred** | A representative high-impedance receiver assumption, not a Yamaha specification. Test open, 10 kΩ, 600 Ω and exact short; neither open nor zero source impedance is implicit. |
| Effective active-feedback ratios | **RfG/RfL = RfG/RfR = 1, inferred** | Nominal symmetric trims; source does not establish installed equality. Test 0.5/1/2 independently. |
| Optional absolute ideal-amplifier fixture | **RfL=RfR=RfG=220 kΩ, inferred effective resistance** | Fits both printed ranges: foldout 220k + trim 0, or board 150k + trim 70k. It does not settle which fixed resistor/trim revision was installed. |
| HIGH/LOW readout transition | **5 ms one-pole, inferred software policy** | Retain existing dezippering timescale, separately from the physical capacitor and hard load selection. No contact-bounce claim. |

This choice explicitly departs from the older source review's hardware-first wording that a source conflict is a gate against any production substitution. The conflict still prevents declaring an installed calibration; it does not prevent this labeled engineering profile. No physical instrument is requested or required.

The driver is a linear Thevenin source, not a rail/current limiter. Headphone input loading, compensation, finite loop gain/bandwidth, muting/startup, distortion, noise and supply interaction remain outside this stage. Rs is a bounded stand-in for the driver boundary, **not** a substitute for a headphone amplifier. No additional soft clip is justified by this design.

## GENERAL derivation and retained digital scale

Read the foldout with ideal op-amps, grounded noninverting inputs and Tr1/Tr2 off. Name A the node after the LI 100k input resistor and before its LEFT 47k input resistor; B is the corresponding RI node. Each also has a 100k branch to the GENERAL virtual ground. With ideal zero-impedance LI/RI sources:

```text
A = (47/194) LI;                    B = (47/194) RI
Lpre = -RfL A / 47k;                Rpre = -RfR B / 47k
Gpre = -RfG (A+B) / 100k
     = 0.47 [(RfG/RfL)Lpre + (RfG/RfR)Rpre]
```

The last relation survives unequal LI/RI source impedances under the same ideal virtual-ground assumptions. It does not survive arbitrary nonideal amplifier behavior, nor does it refer to loaded jack outputs. With the optional 220k effective-feedback fixture, ideal LI→Lpre is **−1.1340206186**, and either input→Gpre is **−0.5329896907**; do not call either unity. Compensation markings and active dynamics are not modeled by this static derivation.

**Recommended Engine boundary:** treat the existing post-volume/expression samples (`mixL * gain`, `mixR * gain`, with the existing `gain = smoothedVolume * 4 * expression`) as normalized ideal LEFT/RIGHT preamp-source units, **not** measured LI/RI volts. Form `Gpre = 0.47 * (Lpre + Rpre)` for the nominal equal-feedback profile, then feed three separate passive networks. This retains the prior stereo source scale without inventing a full upstream voltage calibration, duplicating an active gain, or reversing signal polarity gratuitously. Do not insert the optional absolute LI→driver gains on top of this boundary.

**First-iteration Engine contract:** send only processed LEFT/RIGHT to the host. Isolation already forces stereo and makes `P_outputMode` inert; retain that contract rather than silently enabling GENERAL listening. Always compute GENERAL through its own network for standalone tests and render-thread diagnostics. Host-selectable GENERAL would require a separate deliberate parameter-contract, UI/documentation and test change; it is not part of this integration.

With equal signals, equal tap profiles and equal loads, the modeled GENERAL diagnostic is 0.94 of either stereo output (**−0.537443 dB**), compared with an arithmetic mono average. This is not a newly delivered mono listening mode. Unequal load/source/capacitor profiles invalidate that simple relationship. All three default loads remain connected regardless of which diagnostic is inspected; a future jack-listening selector must not change those electrical loads or histories.

## One-channel passive network and endpoint results

For a real, nonnegative source resistance Rs and an explicit real jack load RL, use the existing branch graph. Let V be the physical capacitor voltage oriented from the source-side plate toward the common tap node. Let U be the ideal preamp-source sample. `Rp` is the resistance from the common node to reference; `m` is selected-jack/common-node gain:

```text
HIGH finite/short: Rp = (Ra+Rb) || (Rh+RL); m = RL/(Rh+RL)
HIGH open:        Rp = Ra+Rb;              m = 1
LOW finite:       Q = Rb || RL; Rp = Ra+Q; m = Q/Rp
LOW short:        Q = 0;       Rp = Ra;    m = 0
LOW open:         Q = Rb;      Rp = Ra+Rb; m = Rb/Rp

T = Rs + Rp; tau = C*T; K = m*Rp/T
iC = (U-V)/T
X = Rp*iC
Yjack = m*X = K*(U-V)
dV/dt = (U-V)/tau
H(s) = K*s*tau/(1+s*tau)
```

Rs=0 is valid. Exact short means exactly zero jack voltage, **not** zero capacitor/load current. Exact open means zero external-load current, **not** removal of the LOW divider. At settled DC, V=U and every downstream voltage/current is zero. Open-HIGH has zero current in Rh; consequently the open-load HIGH/LOW ratio is Rb/(Ra+Rb) independent of Rs and coupling at nonzero transfer. Finite loads generally make even that ratio frequency-dependent because the two poles differ. An ideal 10 µF capacitor alone does not determine a 10 Hz pole.

Calculated midband limits and analog poles, with C=10 µF, Rs=100Ω and Ra=4.7kΩ:

| Tap profile / load | HIGH K | LOW K | HIGH pole Hz | LOW pole Hz | LOW/HIGH, midband dB |
|---|---:|---:|---:|---:|---:|
| Nominal Rh=Rb=560Ω; 100kΩ | 0.974926955 | 0.103956279 | 3.121578 | 2.971037 | −19.442427 |
| Same; open | 0.981343284 | 0.104477612 | 2.969309 | 2.969309 | −19.455954 |
| Same; 10kΩ | 0.920745839 | 0.099488346 | 4.407381 | 2.985852 | −19.327351 |
| Same; 600Ω | 0.467999288 | 0.056910569 | 15.151770 | 3.127028 | −18.301045 |
| Literal RIGHT Rh=56kΩ, Rb=560Ω; 100kΩ | 0.628670732 | 0.103956279 | 3.067498 | 2.971037 | −15.631450 |
| Literal GENERAL Rb=560kΩ, Rh=560Ω; 100kΩ | 0.993267556 | 0.946457545 | 0.186234 | 0.177532 | −0.419302 |

These are calculations of chosen circuits, not instrument measurements. The large literal-conflict effects must remain visible in tests and the uncertainty ledger. With nominal 100k load, changing Rs to 0Ω gives HIGH/LOW K = 0.994431185 / 0.105933807; Rs=1kΩ gives 0.828652340 / 0.089003039. Absolute level is therefore sensitive to an inferred boundary even where the relative attenuation barely moves. Do not conceal that sensitivity by independent endpoint normalization.

## Realtime state, numerical method and controls

The JUCE-free `OutputTapCircuit` provides setup-only `Profile`, `prepare(rate, profile, mode)`, `reset()` / `reset(mode)`, `setMode(mode)` and `process(U)`. `process` returns `Nodes`: all physical capacitor/common/tap/jack voltages and branch/source/load currents, plus the explicitly separate `audibleJackVolts` software readout. `getState()` exposes capacitor voltage, previous source and readout gain; `getProfile()`, `selectedCoefficients()`, `sampleRate()` and `getMode()` provide render-thread-only diagnostics. The default constructor prepares the nominal profile at 96kHz in HIGH mode; Engine integration must explicitly prepare its actual internal rate.

`OriginalOutputCircuit` holds three independent `OutputTapCircuit` instances and an `InferredSymmetricOutputProfile`. It supplies `prepare(rate, profile, mode)`, `reset()` / `reset(mode)`, `setMode(mode)` and `process(leftSource, rightSource)`, returning all three `Nodes` results. Its `leftCircuit()`, `rightCircuit()` and `generalCircuit()` accessors expose read-only diagnostics; it has no listening selector. Sources are sanitized before GENERAL summing, then the resulting GENERAL source passes through the same per-module numerical input guard. The implementation has no dependency on the throwing offline AC solver and no allocation, locking, iteration or per-sample matrix solve.

Use double state and coefficients at the Engine internal rate. Store one physical capacitor voltage V and the previous source sample Uprev per channel. Precompute coefficients for both selector positions after setup:

```text
h = 1/fs
b = h/(2*tau+h)              // 0 < b < 1/2 in the supported domain below
Vnext = V + b*(U + Uprev - 2*V)
rawJack = K*(U - Vnext)
V = Vnext; Uprev = U
```

This is trapezoidal integration in the physical capacitor-voltage coordinate. For fixed coefficients its transfer is the bilinear transform of H(s), without frequency prewarping. Compare discrete frequency fd with analog `fa = fs/pi * tan(pi*fd/fs)` when testing exact AC correspondence; separately compare at the **same physical frequency** over increasing rates to demonstrate actual convergence.

**Switch-time convention:** a mode event selects the topology for the next integration interval immediately after the previous sample knot. V and Uprev do not reset. Recompute the old-end derivative using the new topology: that is what the formula above does. A companion-model oracle must likewise rebase the old-end capacitor current at the event, rather than retaining an old-topology current as though a derivative discontinuity were continuous. The instantaneous capacitor voltage is continuous; resistor/node voltages and currents need not be. The first subsequent sample includes normal capacitor evolution, so it is not correct to test equality of V across a nonzero elapsed timestep.

To preserve pleasant automation without claiming a physical intermediate selector, use a separate readout coefficient Ks:

```text
alpha = -expm1(-1/(0.005*fs))
Ks += alpha*(Kselected - Ks)
audibleJack = Ks*(U - Vnext)
```

Initialize Ks at the selected K on prepare/reset. The physical capacitor always follows the selected endpoint load immediately; only the voltage readout morphs. This is a **software dezipper**, not simultaneous contact conduction, and its transient is intentionally not the raw circuit result. It uses the same one physical capacitor, not two incorrectly independent HIGH/LOW shadow histories. Tests must cover both raw and audible outputs. Five milliseconds is one time constant, not a finite settling time. Do not reuse legacy `outputGain` state for this branch, or multiply the resulting LOW attenuation a second time.

### Bounded supported domain and failure policy

The first realtime version supports **real memoryless** Rs and jack loads only. Complex AC impedances accepted by the offline solver are **not** realtime loads; they require additional physical states and a separate design. No reactive value may be silently projected onto its real part.

Implemented inclusive setup domain: internal fs 8kHz–1.536MHz; C 1–100µF; Rs 0–10kΩ; Rh 100Ω–100kΩ; Ra 1kΩ–100kΩ; Rb 100Ω–1MΩ; load kind explicitly open, short, or finite RL 100Ω–1GΩ. GENERAL feedback ratios are independently bounded to 0.25–4. These are numerical/API bounds, not hardware specifications. The load's numeric resistance is ignored for explicit open/short kinds. Defaults are visibly the inference profile; the offline reference does not acquire these defaults.

Throughout these passive bounds, `Rp >= 100 || 1100 = 91.666…Ω`, `tau >= 91.666…µs`, h≤125µs, so `0 < b < 1/2`. The update is a convex combination of V, U and Uprev: coefficients `(1−2b), b, b` are nonnegative and sum to one, even under arbitrarily frequent allowed mode changes. Thus bounded input and initial capacitor voltage imply bounded V. K and Ks remain in [0,1]; for input bound A and zero initial state, `|rawJack|, |audibleJack| <= 2A`. This is a mathematical bound for this declared passive submodel, not for upstream amplifier gain or the whole synth. GENERAL's weighted sum must be included when choosing the input bound.

Both classes validate complete setup atomically before altering live state; invalid `prepare`, `setMode` or explicit-mode `reset` returns false with no mutation. Parameterless `reset()` preserves the selected mode and clears physical/input history while initializing readout gain to that endpoint. Controls and processing are `noexcept`; profile/load changes are setup-only. Nonfinite samples become zero and finite module inputs clamp to ±1,000,000 normalized units, matching the adjacent circuit module's explicit numerical interface policy. That guard is not modeled analog saturation. No automatic capacitor reset conceals an invalid result. Diagnostics are render-thread-only, with no cross-thread access promise.

## Engine integration contract (in progress)

- Insert after existing chorus/expression/volume source scaling, before final decimation. Isolation continues to omit the unrelated generic 10Hz blocker/soft clip; the documented coupling network supplies its own DC rejection. Do not add a second DC blocker or limiter in this change.
- Prepare/reset zeros V and Uprev for all three networks and sets Ks directly to the current selector endpoint. Preserve the Engine's existing reset conventions for expression/controllers; no output-specific controller rewrite is needed. An initial nonzero source therefore produces a normal coupling transient rather than a falsely precharged capacitor.
- HIGH/LOW automation changes topology/readout target only. Volume, expression, note changes and voice-idle periods do not reset or freeze any output history. Processing silence drains stored charge, including the unheard GENERAL network.
- Retain isolation's forced-stereo contract: changing stored `P_outputMode` must not change host output, physical network states, electrical loads or selector mode. GENERAL diagnostics must remain independent of that inert parameter. Any future jack-listening support is a separate change.
- Parity-isolation entry/exit already calls Engine reset: include all new output states in that reset. Sample-rate prepare deliberately resets; seamless rate changes are not promised. Preserve existing channel-scoped CC120/123 semantics rather than resetting a shared output capacitor when one channel is silenced.
- Use existing parameters only; no ID/schema migration or new UI load knobs are required. Inference profiles can initially be compile-time/setup objects for tests. Legacy-off code must keep identical arithmetic order, state updates and RNG consumption; both existing pinned raw corpora remain byte-exact acceptance gates.

## Acceptance plan

1. **Source/active matrix:** separately stamp the input/summing resistors and ideal amplifier constraints; test unequal input source impedances and unequal feedback values, single-side drive, correlated/anticorrelated input and the 0.47 relation. Do not make the only oracle another copy of that reduced formula. The delivered standalone test uses the independently stamped ten-node active graph described below.
2. **Static AC:** excite measured sinusoids and compare all physical ports to the existing independently nodal-verified `reference::output_taps` at the warped frequency. Include both modes, open/short/finite real loads, Rs=0, the literal GENERAL560k and RIGHT56k alternatives, and frequencies around each pole as well as 20Hz–20kHz. A shorted jack has zero voltage but nonzero load current; an unloaded HIGH series branch has exactly zero current.
3. **Independent transient oracle:** stamp pre-capacitor/common/HIGH/LOW nodes with resistor and trapezoidal-capacitor companions and explicit ideal-source/short constraints. At mode events preserve capacitor voltage and independently recompute the old-end current for the new topology. Compare all node voltages, source/load/branch currents, charge updates, KCL and KVL under charged-state switching. Add closed-form analog DC-step/decay cases so agreement between two discrete implementations is not mistaken for convergence.
4. **Numerics:** compare fixed-time step/sine/switch trajectories at 48/96/192/384kHz against analytic piecewise constant/linear-input evolution or a much finer independent solver. Verify second-order smooth-segment convergence, distinguish switch/sample alignment, measure unwarped frequency error, and explicitly bound long tails. Exercise all inclusive setup endpoints, adjacent representable invalid values, NaN/infinity, atomic rejection on a charged instance, worst permitted alternating controls and the convex-state bound.
5. **Lifecycle/integration:** prove all three networks run while unheard/idle; GENERAL diagnostics come from their own pre-coupling sum; unequal loads break the average-after-jacks identity; output HIGH/LOW affects all three but not a fabricated headphone path. Verify isolation still forces LEFT/RIGHT listening and `P_outputMode` remains inert for host audio and all three histories. Also verify zero-input startup/reset, live switching without charge reset, current-endpoint Ks initialization, 5ms sample-rate-independent readout slew, repeated toggles, mode resets, block partition invariance, and no double 0.1 multiplier or legacy clip/DC block in isolation.
6. **Sensitivity and mutations:** retain literal-source alternatives as first-class fixtures. Reject mutants that remove the always-connected LOW divider in HIGH, omit C or source impedance, change each resistor, swap selector contacts, treat finite/short as open, average already loaded jacks, replace 0.47 with 0.5, reset/freeze charge on switching, introduce two independent endpoint capacitors, normalize HIGH/LOW independently, or add the old 0.1 attenuation after the network.
7. **Release gates:** all core suites, new module/integration suites, module allocation check, address/undefined-behavior checks with their actual supported configuration, both legacy raw-compatibility corpora, host/UI state smoke and multi-platform CI. These are software/circuit-model evidence, not measured hardware equivalence. Validator repair and unrelated lifetime investigations are outside this output task.

## Executed standalone verification

The refined standalone checkpoint of `tests/OutputTapCircuitTests.cpp` passes **6,506,427 checks, zero failures** in the local GNU strict C++20 build, native/FMA-enabled GNU 14 and Clang 19.1.7 builds, and AddressSanitizer + UndefinedBehaviorSanitizer with **`ASAN_OPTIONS=detect_leaks=0`**. Both native builds also pass the **2,080,580-check `--mutation-probe` subset**. Both the earlier **5,456,427-check** checkpoint and final **6,506,427-check** full suite were reproduced by an independent reviewer. The subsequent refinement replaces a cross-kernel triple-versus-scalar bitwise comparison with independent stamped three-channel/readout oracles at unchanged transient tolerances; it does not change the physical module or weaken exact same-path rollback/partition tests. Leak detection was disabled: no leak-clean claim is made. Exact-commit cross-platform and host results remain separate release evidence; none is inferred from these local module runs.

Executed compilation profiles: strict GNU `-std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror`; native full suite and probe `-std=c++20 -O3 -march=native -ffp-contract=fast -Wall -Wextra -Wpedantic -Werror`; sanitizer full suite `-std=c++20 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined`, all with `-Isrc tests/OutputTapCircuitTests.cpp` and a separate output executable. No fast-math relaxation is used.

Standalone source checkpoint SHA-256: header `5c440161fcacd75c29288b7402159bb102119e4d393cc66b2c253124db986c2a`; refined tests `ec311e98fddbf5374ae9b553800b78e1edb61e4b837583d40a37f63c8e12393a`. These identify the local module evidence, not a released commit.

| Evidence | Executed scope / result |
|---|---|
| Independent transient oracle | Four physical nodes stamped separately with capacitor companions, explicit zero-R source and short constraints, and rebased switch current. 72 fixtures at 8/48/192/1536kHz, 600 samples each, include rapid switching while charged and silence. Maximum absolute physical-port error **1.5099×10⁻¹⁴**. |
| Measured sinusoidal AC | **140 fixtures**, 11 physical ports, both modes, open/short/600Ω/10kΩ/100kΩ loads, source and capacitor sensitivity, and both literal resistor conflicts. Frequencies include half/twice each modeled pole, 20Hz, 1kHz and 20kHz. Maximum complex-port error against the independent offline AC reference **2.56964×10⁻¹²**. The 48kHz fixtures and one 192kHz source fixture use the declared bilinear-frequency mapping; capacitor sensitivities scale the offline solver frequency to preserve `s*C`. |
| GENERAL active relation | **1,000** separately stamped ten-node ideal-active fixtures with unequal sources/feedbacks, single-side/correlated/anticorrelated inputs. Maximum absolute reduced-relation residual **4.44089×10⁻¹⁶**. |
| Continuous-time convergence | Fixed and switched piecewise-linear/step/decay trajectories and 900Hz sine trajectories compared at common physical times with independent analytic continuous-state solutions. Across 48→96→192→384kHz, capacitor RMS error decreases by approximately four per doubling. Switched sine errors: **1.42206×10⁻⁵, 3.55452×10⁻⁶, 8.88592×10⁻⁷, 2.22146×10⁻⁷**. |
| Unwarped frequency error | Same-physical-frequency 3Hz/1kHz/12kHz comparisons improve under the same rate refinement; 12kHz complex-jack errors are **5.44250×10⁻⁵, 1.31726×10⁻⁵, 3.26755×10⁻⁶, 8.15310×10⁻⁷**. These are numerical convergence results, not hardware residuals. |
| Bounds and long tails | All resistor/C/source bound corners, explicit load kinds and both rate endpoints exercised under every-sample HIGH/LOW toggles. Maximum observed `b=0.405405…`, matching the proven bound; raw/readout fixture peak **1.26016×10⁶** for input bound 10⁶, below the 2A bound. Nominal and longest supported open-load charging/decay tails are explicitly run to **27 time constants**; no freeze/reset hides decay. |
| Controls, state and three-channel behavior | Charged-instance setup rejection, inclusive/adjacent invalid bounds, invalid enums, NaN/infinity guards, source clamping, reset endpoints, exact open/short identities, 5ms readout time constant, separate GENERAL coupling/loading and standalone block partition invariance. All three outputs are compared to separate independent transient stamps; independent high-frequency AC limits and exponential readout evolution verify their software outputs. Exact rollback/partition comparisons use the same uninlined runtime call site so compiler specialization/FMA differences do not masquerade as changed state. |
| Allocation probe | C++ allocation hooks observe zero heap allocations during **5,000** three-channel processing/control steps. This is module-local, not a whole-plugin hard-realtime guarantee. |

**Eighteen separately compiled mutations are rejected**, not merely compared algebraically: removing the always-connected LOW divider; omitting C or Rs; changing Rh/Ra/Rb; swapping selector contacts; treating finite or short loads as open; resetting charge on a switch; freezing the first switched interval; introducing two independent endpoint capacitors; normalizing endpoints; adding the old LOW 0.1 multiplier; replacing GENERAL's 0.47 by 0.5; averaging already loaded jacks; discarding the previous input on switching; and freezing idle charge. Each compiled mutant runs the dedicated `--mutation-probe` subset, not the full slow AC/convergence suite. All 18 were recompiled and rejected again after the three-channel test refinement. Every entry in local `/tmp/output-tap-mutants/summary.json` is `KILLED`; no compile failure is counted as a detected behavioral mutation. Local logs are `/tmp/output-tap-full.log`, `/tmp/output-tap-native-full.log`, `/tmp/output-tap-clang19-full.log`, `/tmp/output-tap-sanitizer.log` and `/tmp/output-tap-mutants-run.log`; these temporary paths are not published hardware data.

The standalone suite does **not** establish Engine dispatch, host state restoration, parameter inertness in the actual plugin, pinned legacy byte compatibility, whole-plugin allocation behavior or remote CI status. Those remain integration/release gates, with evidence to be added only after they execute.

## Next action / blocker statement

There is **no external-data, hardware-access or authorization blocker**. The module and isolation Engine dispatch are implemented. Engine tests preserve forced LEFT/RIGHT stereo and inert `P_outputMode`; GENERAL is independently clocked diagnostics only. Host mono-bus downmix is unchanged. Exact-commit plugin/platform validation remains a separate release gate. Source/trim/load uncertainty stays visible regardless of software test results.

## Executed Engine integration verification

`tests/OutputTapIntegrationTests.cpp` passes **184,745 checks** against independent physical-node old-knot rebasing and next-interval solves. It checks all three histories/ports, GENERAL source relation, immediate load selection without charge reset, 5ms readout tracking, rate endpoints, reset/reprepare/mode transitions, idle drain, channel panic, partition invariance and inert mono selection. Maximum host-sample error is zero after float conversion; maximum capacitor-voltage error is **9.08e-16**. A separate reviewer reproduced these results.

`tests/PraWahIntegrationTests.cpp` now passes **226,798 checks**, adding independently stamped output coupling/loading after expression and before decimation. Different prior wah histories correctly retain different output charge: each stream is checked against its own history instead of requiring false equality. Exact PRA-state continuity remains checked. Four temporary Engine mutants are rejected: transparent output, expression moved after coupling, extra LOW attenuation and output reset on every parameter refresh. A binary64-only oracle spot-check also passes; this is not MSVC execution evidence.

The final local Release and ASan/UBSan configurations each pass **29/29 CTest suites** (63.27s and 283.37s respectively; leak detection disabled locally). Both legacy raw corpora match the baseline as recorded below. Exact-commit host/platform outcomes remain separate evidence. No host parameter or state schema is changed.

Sixteen separately compiled Engine integration mutants were rejected: host output bypass, wrong legacy dispatch, wrong rate/reset endpoint, selector charge reset, double LOW attenuation, extra generic DC block, pre-gain placement, idle/LOW/zero-expression freezes, GENERAL freeze/post-loaded sum/0.5 average/host listening, and re-enabled mono. Both local legacy-off raw corpora remain byte-identical to the pinned baseline (normal SHA-256 `4a3483f77c9df9895386b98eaa169358d19b463c21283231f994c5d0f6c523de`; tails `fe447882e3ea3560eefb7b5ae4a474b151efe5ced775b5ab36f2609c5fde0660`).
