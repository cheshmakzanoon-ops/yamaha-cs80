# CP3 input capacitor versus inferred IG00151 interstage coupling

**2026-11-02 — source-topology investigation and fixed-control shadow trial.** The independent test finds a **source-visible missing input-capacitor state** in the aggregate software route, but it does **not** demonstrate a cause or correction for the remaining original musical-click score failures. Its preregistered high-frequency sensitivity and DC discriminator both hold. The original score gates and all production audio are unchanged and remain **FAIL** where previously red. This evidence does not claim measured hardware voltages, subjective audibility, or a V2-ready component implementation.

## Corrected Yamaha topology: two capacitors, not one misplaced part

The independently enlarged original [Yamaha M41 p41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg), image SHA-256 `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` in the preexisting source ledger, reveals:

1. **Before voice IC4:** IG00156 IC11 LP15 → CP8/CP3(29) → 1 µF/50 V NP → 100 kΩ → IG00151 **IC4 +IN3**. The separate LP1(27) branch uses 100 kΩ and 0.01 µF toward IC4 LI2.
2. **Between VCA stages:** IC3+IC4 outputs join at CP4(28), with a 27 kΩ shunt, then CP4 → a **different** 1 µF/50 V NP → 100 kΩ → IG00151 **IC5 +IN3**. IC5 is labeled the **SUB-OSC VCA** on M41; its LI drive separately connects LC31 through 100 kΩ/0.01 µF.
3. **Further downstream:** IC5 OUT8 → CP5(32), 27 kΩ shunt → 100 kΩ → IC6 +IN3 → Tr8 output → 4.7 µF/50 V NP and 1 kΩ → **O(33)**, the buffered audio harness terminal independently corroborated in `docs/OE1_SIGNAL_ROUTING_EVIDENCE.md`. The crop glyph alone could be mistaken for D(33).

This corrects an initially tempting but unsound conclusion that the existing `Ig00151Vca` output capacitor is necessarily misplaced. In frozen `src/core/Vca.h`, a **declared-inference** 1 µF·100 kΩ output HP stage follows one aggregate amplitude-controlled VCA. It may stand in for the actual CP4→IC5 interstage coupling. It does **not** explicitly preserve the distinct CP3→IC4 **input** capacitor, IC3/4 and IC5 individual gain controls, or further downstream loaded states. **Neither chip input impedance nor effective loaded time constant is established merely by the drawn 100 kΩ series resistor**; the ideal-source 0.1 s hypothesis below is explicitly provisional. Exact KAS→IG00159 gate-pin continuity also remains unverified.

The original `Ig00156Stage` network has a source-derived 1 µF **HP input** capacitor and another 1 µF **HP→LP interstage** capacitor. Neither of those is the separate LP/CP3→IC4 capacitor at issue here. The established full-state continuity tests rule out the tested instantaneous audio-state reset, not every possible stage-law mismatch. The service ADJ p11 HP-amplitude model discrepancy (predicted approximately 1.692 Vpp versus a source band of 3.3–5.3 Vpp) is still open and is not evidence that a CP3 waveform edge is a note-gate defect.

## Preregistration and minimal intervention

- Original numerical/source hypothesis locked **before trial** as `4743032`, `tests/parity/tolerances-v2/click-cp3-input-coupling-screen.json`.
- The independent M41 two-capacitor correction was committed **before any result** as `a126e9b`, `tests/parity/tolerances-v2/click-cp3-coupling-topology-addendum.json`; it alters no numeric prediction or test control.
- The standalone probe uses the **actual unchanged `Ig00151Vca::process`** with LI=1 and EI=0 for both paths. The augmented path inserts only an extra, independently implemented ideal **pre-VCA** 1 µF/100 kΩ high-pass recurrence before the same existing primitive; it retains the present inferred output/interstage coupling in both. Both models are warmed for one second at the native sample rate while the source oscillator/nominal DC input runs; gated scenarios hold the amplitude envelope at zero before an exactly 4 ms C1 half-cos onset.
- Fixed native rates: 44,100 / 48,000 / 96,000 Hz; 8 ms metric window. A 220 Hz 0.10 FS sine and 1760 Hz 0.20 FS sine are mathematically continuous. Additional controls are a constant DC input, grounded audio, held pitched tones with **no gate event**, and two deliberately introduced one-sample digital impulses (+0.20 FS left at +1 sample and right at +6 ms).
- The score reference is the unchanged, independently frozen **original Funky 1+2 60 ms pre-note-off maxJump**: 0.037371031939983368 / 0.037026423960924149 / 0.029931630939245224 at the three rates. It is used as a *numeric specificity probe* on synthetic negative controls, never as a replacement source score, a new source signal, or a physical Yamaha calibration.

The 42 raw rows were independently rendered twice with exactly the same digest: SHA-256 `a35352bc083fad0cb78479108d7f4e79a4a9e241e43bb2bd73d0fa49215fd620`. Complete raw data: [`click-cp3-input-coupling-screen-20261102.csv`](click-cp3-input-coupling-screen-20261102.csv). Standalone source: [`ClickCp3CouplingScreen.cpp`](../../tests/diagnostics/ClickCp3CouplingScreen.cpp). Both runs completed with all **3/3** predetermined 1760 Hz ±5% ratio checks and all **3/3** DC-placement controls satisfied; the smooth-score prediction was observed at two of three native rates.

## Recorded results

| Native rate | C1 1760 Hz score, existing model | With additional CP3 input cap | New/old maxJump ratio | Held 1760 Hz score, **no event** | Constant-DC gate: new/old peak |
|---:|---:|---:|---:|---:|---:|
| 44,100 Hz | **1.338306** | **1.338155** | 0.9998875 | **1.338321** | 0.253726% |
| 48,000 Hz | **1.241504** | **1.241384** | 0.9999036 | **1.241490** | 0.253728% |
| 96,000 Hz | 0.769218 | 0.769180 | 0.9999511 | 0.769226 | 0.253728% |

In all six pitched C1-gate paths, the first gate sample has **exactly zero PCM step** relative to the preceding sample. The artificial attack is continuously differentiable at onset by construction. Yet at 44.1/48 kHz the unchanged original-reference adjacent-sample score exceeds one in **both** capacitor variants, and the separately generated held sine with **no event at all** also exceeds one. Those are labeled *known synthetic smooth/held controls*, not claims about the real Yamaha sound or an independent subjective listening evaluation. This directly demonstrates a specificity limitation of maxJump normalized to a different source phrase window. It coheres with the separately preregistered 88-case click-classifier validity study: an alert can be an ordinary large pitched derivative, and a click cannot be inferred from an alert alone.

The source-visible extra input capacitor changes the measured synthetic pitched-attack derivative by at most about **0.012%**, while dramatically rejecting a precharged **DC** signal during the same smoothly changing envelope: the new DC peak is only **0.2537%** of the existing model's DC peak. That DC control verifies genuine noncommutativity of input coupling and time-varying VCA multiplication and prevents interpreting this as a no-op. It also highlights why treating the two physically separate capacitors as a single one cannot be justified across every envelope/bias condition. The 220 Hz pitched negative similarly retains 99.982–99.994% of the original 8 ms derivative.

The **grounded audio x=0** controls are bit-identical through both paths, including the original inferred control-dependent pedestal/coupling; the test would reject a cap intervention that accidentally changed VCA control law. The 4 ms C1 gate has zero-time PCM continuity; artificial +0.20 FS single-sample corruption is detected in exactly one sample at each independently specified left/right location, while one-ULP pre-event corruption and an intentionally defective two-sample control are also discriminated by test oracles. No Engine source or full-source render is changed.

## What may be a model gap, and what this trial cannot settle

**Source-supported structural gap:** the Yamaha source-visible CP3 capacitor is a distinct state before voice IC4. The frozen aggregate software path lacks a named capacitor at that location; its one modeled post-amplitude coupling could approximate a different, also source-visible physical CP4→IC5 stage. This is a candidate for a separately reviewed V2 topology reconstruction, **not a verified high-frequency click bug or permission to remove the inferred output stage**.

**No proven IG00156 attack-equation defect in this work:** the existing phase-corrected four-node solver has independent numerical/source-derived gates and the documented 44.1/48/96 internal-rate coverage. Its known uncertain physical source resistances, noise NI branch, active-element laws, and service HP gain discrepancy remain real model-fidelity questions, not grounds to retune filter gain or attack smoothing to make a score green.

The exact frozen-source **`cs80_ig00156_nodal_tests`** and independent **`cs80_ig00156_phase_correction_tests`** were built and rerun unchanged: **2/2 passed**, 33.56 s and 1.34 s respectively, with no registered limit changes. From the preceding isolated CP3 path trial, the original **`cs80_click_event_continuation_tests`**, **`cs80_click_bank_causality_tests`**, **`cs80_ig00151_vca_tests`** and **`cs80_vca_tail_modulation_tests`** had also passed. These software-equation checks do not prove unique physical circuit fidelity or replace the original red click suite.

The shadow study does **not** route an extra capacitor into `Engine`, test a physical IC4/IC5 multi-stage netlist, distinguish capacitor load impedances, test the unknown KAS gate pulse, or perform hardware/audio audition. The strong idealized high-frequency insensitivity means the mere absence of that capacitor is **not sufficient evidence** for a causal correction to Bass/Funky 8 ms scores. A future production candidate would require its own source-anchored loaded netlist, independent response oracles, exact-state and MIDI controls, full compatibility/performance review, original full-bank audit, and hardware/listening uncertainty stated honestly. No fade, phase reset, arbitrary release extension, `src/**` edit, original threshold change, PR or release was made.

## Reproduction

```sh
g++ -std=c++20 -O3 -DNDEBUG -Isrc \
  tests/diagnostics/ClickCp3CouplingScreen.cpp -o build-core/ClickCp3CouplingScreen
build-core/ClickCp3CouplingScreen \
  --prereg tests/parity/tolerances-v2/click-cp3-input-coupling-screen.json \
  --topology tests/parity/tolerances-v2/click-cp3-coupling-topology-addendum.json \
  --out build-core/click-cp3-input-coupling-replay.csv
sha256sum build-core/click-cp3-input-coupling-replay.csv
```
