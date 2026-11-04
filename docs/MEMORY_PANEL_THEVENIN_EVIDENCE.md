# Memory/PANEL local source impedance — declared model, 2026-11-01

**State:** independently derivable control-rate source adapter and test harness,
not an engine audio path. The producer is
`src/core/MemoryPanelSourceEquivalent.h`; the offline oracle is the existing
94-node `src/reference/MemoryPanelNetwork.h` with a default-zero external
current probe. Neither changes factory V1 numerics, legacy audio, or existing
source model selection.

## Source trace and inference boundary

- CS-80 service **BA27/BA28**: 2SC458 selected-endpoint rails, 1S1555
  isolation diodes, and buffered cutoff wipers (NJM4558 IC7/IC9 on memory,
  IC5/IC6 on PANEL); see
  [BA cutoff branch inspection](PRESET_BA_CUTOFF_BRANCHES.md).
- **MBK63** (source scan titled MKB), **S31–S38**: selected M1/M3 or M2/M4
  memory pot banks, PANEL alternatives and joined row buses; see
  [rank II ports](RANK_II_RECEIVER_PORTS.md) and
  [tone-page native crop ledger](FACTORY_TONE_NATIVE_INSPECTION_2026-11-01.md).
- The **50 kΩ linear** potentiometers with **1 Ω** ends, **0 V** reference,
  **1 Ω** ideal follower output and the nominal 2SC458/1S1555 numerical laws
  are **declared engineering inferences**, not measured component parameters.
  Alternate 10 kΩ/100 kΩ pots, different diode curves and forward-active
  drivers remain viable; the source diagrams do not resolve them.
- PANEL cutoff drivers have a different drawn route from memory cutoff
  drivers (BA op-amp output before the T51 primed diode). The present ideal
  follower and diode surrogate is a *network abstraction*, not an assertion
  that their physical PCB nodes are interchangeable.

## Derivation at one operating point

An ordinary 24-row memory source is not a bank of independent voltage dividers.
All pots share a selected supply rail, each unbuffered wiper draws source
current through its top and bottom legs, all isolation diodes are nonlinear,
and the output buses have a 10 kΩ shunt plus receiver loads. A per-output
Thevenin equivalent can therefore only be a **local one-port tangent** with
the other receiver loads held at their reference operating points. It does
not represent the full 24×24 cross-port transfer matrix or a global nonlinear
open-circuit voltage.

For analogue output `i`, define:

- `T_i`, `B_i`: selected pot top/bottom resistances; `a_i=B_i/(T_i+B_i)`;
  `W_i=T_i*B_i/(T_i+B_i)`.
- `r_i`: diode's differential resistance plus its series resistance, plus
  `W_i` for an unbuffered row or the inferred follower output resistance for
  buffered row 7/9. Let `g_i=1/r_i`.
- `G_i=1/10000+G_receiver,i`, including the shunt. `G_receiver,i` is the
  same eight-channel receiver conductance that the circuit engine already
  compiles in `SourceFactoryTones.h`.
- `R_d`: declared selected-emitter output resistance. The shared rail's
  small-signal stiffness is

\[
D=\frac{1}{R_d}+\sum_k\frac{1}{T_k+B_k}
  +\sum_{k\in\mathrm{unbuffered}}
     \frac{a_k^2 g_k G_k}{G_k+g_k}.
\]

The slope of bus voltage against an externally injected current at output
`i`, with all other receivers in place, is

\[
R_{\mathrm{loaded},i}=\frac{1}{G_i+g_i}
  +\begin{cases}
  \displaystyle\frac{\left(a_i g_i/(G_i+g_i)\right)^2}{D},
     &\text{unbuffered}\\[2pt]
  0,&\text{buffered}.
  \end{cases}
\]

The positive second term preserves the selected rail's backreaction. The
ideal follower is powered separately, so its diode output current is not
charged to the selected pot rail; it has no second term in this declared
ideal-feedback model.

### Zero-bias inactive diode network and bounded reverse omission

At a near-zero selected output, the eleven inactive T51–T54 columns and the
two other memory/PANEL banks do **not** have exactly zero small-signal load.
Every physically present isolation diode contributes its reverse/zero-bias
differential conductance to the assembled bus, even though the static
reverse current is negligible under the 2 mV gate. The runtime tangent
enumerates the actual classified factory branches (using labelled median
hypotheses only for unresolved cells), plus two inactive memory diode
branches. Unselected bank wiper positions are not in `ControlFrame`; their
series path is bounded by the 1 Ω end to approximately 50 kΩ / 4 and its
midpoint estimate is an explicit inference, not a measured taper.

The only numerical reverse-bias omission follows an *analytic upper bound*.
For a Shockley branch with `Is`, `nVt`, series resistance `R`, bus potential
`Vb ≥ 0`, and diode current `−Is < I ≤ 0`, its voltage obeys

\[
V_d=-V_b-I R\leq -V_b+I_s R,\qquad
0\leq g_{\rm reverse}\leq\frac{I_s}{nV_t}
 \exp\!\left(\frac{-V_b+I_s R}{nV_t}\right).
\]

The 10 kΩ assembled shunt bounds the de-embedded local source resistance by
10 kΩ. Consequently, deleting a conductance `g` changes it by no more than
`(10 kΩ)² g`. The implementation skips one negligible reverse tangent only
if its *computed upper bound* is at most
`0.002 Ω / (13 × (10 kΩ)²)`; the thirteen possible inactive branches then
contribute at most **0.002 Ω total** error. For the nominal surrogate this
criterion is reached at approximately **0.5 V** reverse bias, while alternate
diode parameters recompute it dynamically. This is **not** a fitted diode
threshold or an allowance to weaken the preregistered **2 Ω + 0.2%** source
tangent gate.

Remove **only** the receiver loading. The 10 kΩ assembled shunt is part of
the actual upstream source and must remain:

\[
R_{\mathrm{th},i}=
 \left(1/R_{\mathrm{loaded},i}-G_{\mathrm{receiver},i}\right)^{-1},\qquad
E_{\mathrm{th},i}=V_{\mathrm{loaded},i}
 \left(1+R_{\mathrm{th},i}G_{\mathrm{receiver},i}\right).
\]

Thus `E_th` is a **linearised open-circuit intercept** about the original
loaded DC operating point, not the voltage after disconnecting a resistor
from a nonlinear diode circuit. Outputs 4/5 remain logical waveform switches;
they must never be interpreted as analogue Thevenin sources.

## Test-first contract

The pre-registered source of limits is
`tests/parity/tolerances-v2/memory-panel-thevenin-v1.json`, with the old
2 mV selected-bus tolerance preserved. `tests/MemoryPanelTheveninTests.cpp`
independently injects `+50 nA` and `-50 nA` at each analogue port of the
full Newton network, estimates `dV/dI` by a central difference, de-embeds the
known receiver conductance, and compares the result to the scalar adapter.

It covers both ranks, all three selected memory/PANEL sources, and every one
of their 24 analogue outputs (144 port comparisons). It also checks source
voltage reconstruction, pulse/saw identity, the selected-rail correction,
and three deliberate local mutations: omit the shunt, leave the receiver in
the source, and delete rail backreaction. All acceptances are about **model
consistency**; none compare a hardware CS-80 or claim a recovered device law.

The JSON acceptance was committed alone as `0edaf30`, before the independent
test ran. Isolated optimized C++20 execution then passed **144/144**
current-injection ports (two ranks × selectors 11–13 × 24 analogue outputs):
maximum loaded-bus deviation **0.000183972 V** against the original 2 mV
gate; local source-impedance error **0.236753 Ω** (max relative
**6.87×10⁻⁵**) against 2 Ω + 0.2%; recovered receiver-loaded voltage
**1.11022×10⁻¹⁶ V** against 1 nV; and **3/3** explicit topology mutants
detected (omit 10 kΩ, omit receiver de-embedding, omit positive rail term).
The preceding `MemoryPanelTests.cpp` rebuilt with the offline probe extension
also passed unchanged: maximum bus difference **0.000283 V**, inactive
current **2.52×10⁻⁹ A**, Newton residual **6.54×10⁻¹⁵ A**.

The subsequent *inert* `src/core/MemoryPanelToneAdapter.h` maps all 26
electrical outputs into named `source_factory::ToneControls` laws, including
two exact waveform switches, and calls the existing 8-voice cutoff/Q/LC
receiver functions. Its separately preregistered acceptance file
`tests/parity/tolerances-v2/memory-panel-tone-adapter-v1.json` was committed
alone as `d27ea2f`, before execution. The isolated source test passed
**144/144 analogue output copies** and **138/138 role-law comparisons**:
source-voltage and source-resistance copies exact to **0 V / 0 Ω**;
largest scalar law difference **8.88178×10⁻¹⁶** against 1×10⁻¹²;
maximum zero-drive full-network bus difference **0.000183972 V** against
2 mV; receiver reconstruction error **1.11022×10⁻¹⁶ V** against 1 nV.
All **3/3** adapter mutations were rejected: HP/LP swap (4.05099 V wrong
HP bus), zero HP source impedance (0.0315365 V wrong bus), and inverted
PULSE/SAW identities (transaction rejected).

These figures compare **two mathematical models and source laws**, not a
Yamaha CS-80 hardware measurement, audio-loop integration, or proof of the
full shared-rail cross-port dynamics.

## Required audio-integration decision

The adapter is suitable as an independent input to the future memory/PANEL
`ToneControls` bridge. The audio engine must still decide how to represent
off-diagonal rail coupling when touch and filter EG currents move at the same
time. A diagonal one-port model can preserve each row's local response but
cannot substitute for a fully joint solve under simultaneous multi-row loads.
Before claiming complete memory-programmer fidelity, either implement a
joint multiport update or explicitly record the omitted coupling with a
preregistered dynamic cross-port gate. See
[integration sequence](MEMORY_PANEL_CIRCUIT_AUDIO_HANDOFF.md).

Suggested dated `docs/HARDWARE_COVERAGE.md` entry **when the standalone
Thevenin test is committed** (research-only, not an audio-feature closure):

> **2026-11-01, row 46 memory/PANEL source research:** 26 panel outputs mapped
> to existing APVTS controls; 24 analogue selected-bus tangents include pot
> wiper loading, diode differential resistance, 10 kΩ shunt and shared-rail
> backreaction, checked against current-injection Newton truth. Topology from
> service BA27/BA28, MBK63 and S31–S38; numerical pot/diode/transistor values
> remain declared inferences. **Not yet wired into circuit-engine audio.**

After an actual audio integration lands, add a *separate* dated entry with
its audio SHA and mandatory V2 re-pin; do not silently upgrade the research
entry's status.

## Exact future engine/processor patch boundary

The runtime wiring should be a separate intentional-audio commit, only after
the outstanding oscillator/CPU and retrigger changes have released Engine.cpp:

1. **Route and authority** — `PluginProcessor.cpp` already supplies the
   selected bank's `cached` effective `ParamValues` and two selector numbers
   in `setControlFrame({cached, cachedHardwareSources})` for *both* eight-voice
   engines. Do not change APVTS, `Params.h`, serialization, the four memory
   bank ownership rules, or the extension-voice dispatch. In
   `source_factory::Route` add a separate `source_memory_panel` route, selected
   only for selectors 11–13 with `usesSourceModels(policy)` true; Legacy must
   still receive `panel_or_memory`. Do not redirect factory selectors 0–10.
2. **Transactional compiler** — in `Engine::setControlFrame` compile the
   selected source per rank using
   `control_bridge::positions(frame.effectiveValues, rank)` and
   `source_equivalent::compile(rank, selector, positions)`, only on changed
   selector or changed 26-control wiper state. Cache the result alongside its
   positions and selector, and publish both ranks together only after every
   nonfinite/invalid input is rejected. The existing inactive rank's
   `bridgeBoundary()` must keep servicing its shared Q/LC ports. No Newton,
   diode or rail solve may be added inside the per-sample loop.
3. **Bridge all output laws** — in an isolated new helper, map the 24 analogue
   port `SourceEquivalent`s and two exact waveform switches to a
   `source_factory::ToneControls` instance. Use the named `unit`,
   `attackSeconds`, `decayReleaseSeconds`, `compileCutoffSide`, `loadedQBus`,
   `loadedLevelBus` and existing 26-row `Role` assignments. Preserve the
   source's impedance in rows 7/8/9/10/22 and the shared receiver graphs;
   `busVolts` alone are not acceptable source inputs. Give provenance a
   distinct memory/PANEL identity and mark the inferred device/slider laws,
   rather than masquerading as a verified factory cell. Never reorder or
   refactor the production factory `compileTone()` floating-point operations.
4. **Engine consumers** — mark the new compiled route `sourceActive` so the
   existing `buildSettings`, `filterInitialTarget`, `filterAttackTarget`,
   `updateSourceReadouts`, per-voice WSC/filter/LC consumers, and the mixed
   32-Q/16-LC receiver network consume one coherent rank source. Keep the
   current zero-frame selector continuity (no forced oscillator, VCF or EG
   reset). `getSourceToneInfo()` must expose memory/PANEL as its *own* route,
   not as `source_complete`/`source_inferred` factory cells.
5. **Truth and mutation before sound** — add preregistered full-engine gates
   comparing the rank's selected zero-drive 24 buses and both logical gates,
   plus positive/negative touch, EG and resonance response in both ranks and
   under 16-voice ownership. Mutation-check a swapped M1/M2 bank, erased
   output impedance and unavailable rail-coupling term; retain explicit
   uncertainty about off-diagonal 24×24 response for simultaneous driven
   buses. Legacy-default, legacy-tails and all V1 factory bytes must remain
   identical before the audio SHA is pushed; re-pin only the V2 gate in a
   separate full-SHA commit, then run `--strict-v2`.

The standalone diagonal adapter is intentionally only the *third* building
block. Until a separately preregistered engine gate is run, this patch plan
does **not** constitute integrated audio or a passing V2 measurement.

### Preregistered circuit-engine RED audio gate

Commit `55cce4d` separately froze
`tests/parity/tolerances-v2/memory-panel-engine-audio-v1.json` **before**
any waveform rerouting. The pending
`tests/MemoryPanelEngineAudioTests.cpp` provides a deliberately RED check
against the current source-inactive memory/PANEL engine behavior. It requires
the ordinary Legacy API and memory selector frames to produce byte-identical
PCM, the circuit/PURE route to enable actual memory electrical sources for
both ranks, exact 8-voice HP/LP and shared Q/LC taps from independently
compiled `ToneControls`, 2 mV zero-drive source buses, sample-zero state
continuity, and observable selected-wiper recompilation. Four deliberately
wrong alternatives are asserted: no memory source route, 0 Ω HP source,
rank-I/rank-II bank swap, and inverted PULSE/SAW contacts. The tests must
pass *after* an explicitly reviewed Engine audio commit and a mandatory
full-SHA V2 audio gate re-pin; none of the RED gates is counted as closed here.
