# Playable source-informed factory tones: implementation proposal

2026-10-29; inspected baseline `8bb9b81`. **Design, not delivered audio.** This
document changes no production code, parameter, preset or source classification.
Its proposed nominal model is a public-knowledge reconstruction, not measured
CS-80 equivalence. Exact-commit CI for the baseline is a separate question.

## Decision

Implement the existing hardware selectors' **isolation-only** source path in two
successive, independently testable increments:

1. Seven tones whose 26 matrix cells are all classified, using the common nominal
   converter below. Ship audible matrix-derived controls, not another offline-only
   inventory. The other fifteen selectors retain the explicitly named legacy
   bridge until increment 2, rather than pretending that their missing cells are
   known.
2. All 22 tones, using exactly the same converter plus a separate, versioned
   **25-address inference overlay**. Do this next, without waiting for an
   instrument, serial-number answer, or additional transcription. Publish the
   inferred-address masks and sensitivity renders with the result.

The seven-tone stage is a convenient review boundary, not a reason to stop before
22. Both stages require numerical/behavioral tests, not hardware availability.
Unidentified device laws get named engineering models; unresolved source cells
remain unresolved in `HardwareControl.h`. The governing policy is
[ANALOG_FIDELITY_REQUIREMENT.md](ANALOG_FIDELITY_REQUIREMENT.md).

Keep `factoryPresets()`, `applyHardwareTone()` and all **222 legacy program values,
names, order and default-off audio** unchanged. The first 22 program entries are
two-rank performance studies: they set both ranks, mix and chorus. They are not
the physical rank-local tabs. Improving a tab therefore must not secretly rewrite
those programs or make factory recall select a different source. A browser action
labelled “Source tone — isolation” may invoke the existing rank selector without
adding/reordering host programs. Existing “Hardware Studies” remain legacy studies.

## 1. What is established, and what is not

### Existing implementation seam

- `PluginProcessor::readParameters()` resolves `hardware_source_1/2`: 0–10 are
  rank-local factory tones, 11/12 are the two corresponding memories, 13 is PANEL.
  It substitutes exactly **26** controls. Footage, pan and HP/LP bypass are outside
  that programmer set. APVTS is the stored authority; the effective snapshot is
  private to the audio callback.
- `hardwareFactoryValues` is currently filled by the hand-voiced family recipes
  in `FactoryPresets.cpp`. `Engine::buildSettings()` then receives ordinary Hz,
  seconds and normalized controls; it has no source identity or electrical model.
- The source table has **547 classified / 25 unknown cells**, not 547 recovered
  analog control values. `verifiedColumns` preserves historical fixture order;
  it is **not selector order**. Resolve by `(board,column,output)`.
- Companion unprimed buses are connected in both ranks. Each assembled analog
  bus has one T51/T53 10 kΩ shunt; T52/T54 contribute none locally. Switch rows
  4/5 have no such shunt. This does not change the local metadata or make
  `solveMatrixBus(T52,...)` silently acquire a shunt.
- All waveform-switch cells, noise cells, VCF attack/release and VCA attack,
  decay/release cells are classified. VCF decay has one exclusion. All 22 noise
  branches are open; all VCF-level and initial-level branches are direct diodes.
  “Direct” still includes a diode; “open” is not an unknown value.
- Seven whole columns are classified, but **none has measured audible parity**.
  Known resistors do not establish chip input current, diode drop, loaded voltage,
  filter calibration or envelope curve.

Source basis: [full matrix](PRESET_FULL_MATRIX_AUDIT.md),
[rank-I receivers](PRESET_RECEIVER_PORTS.md),
[rank-II receivers](RANK_II_RECEIVER_PORTS.md),
[BA source graph](PRESET_BA_CUTOFF_BRANCHES.md),
[AL graph](PRESET_AL_RECEIVER_GRAPH.md). Older “no production integration” text
records earlier scope; it is not a hardware-first veto on the explicit model here.

### Canonical selector table and exact unknown mask

`—` means zero excluded cells, not zero model uncertainty. Numbers are output
addresses, not resistor values. Alias `E. Piano` is the current UI spelling of the
matrix's `Electric Piano`; preserve both rather than failing a string match.

| Index | Rank I / address | Unknown outputs | Rank II / address | Unknown outputs |
|---:|---|---|---|---|
|0|String 1 / T51 A|20,25|String 2 / T53 A|20,25|
|1|**String 3 / T51 B**|—|String 4 / T53 B|14,20|
|2|Brass 1 / T51 C|25|Brass 2 / T53 C|20|
|3|**Flute / T51 D**|—|**Brass 3 / T53 D**|—|
|4|E. Piano / T51 E|22,26|Bass / T53 E|23,26|
|5|Clavichord 1 / T51 F|22,25,26|Clavichord 2 / T53 F|22,26|
|6|Harpsichord 1 / T52 A|26|Harpsichord 2 / T54 A|26|
|7|Organ 1 / T52 B|9|**Organ 2 / T54 B**|—|
|8|Guitar 1 / T52 C|22|Guitar 2 / T54 C|22,25,26|
|9|**Funky 1 / T52 D**|—|**Funky 2 / T54 D**|—|
|10|Funky 3 / T52 E|20|**Funky 4 / T54 E**|—|

This table accounts for all 25 exclusions. T52 F/T54 F are not extra tones.
The exact glyph/revision reasons remain authoritative in the full-matrix audit.
In particular T53/B/14 is a readability issue; it must not be described as a
resolved revision. The seven bold names are increment 1's complete columns.

## 2. Proposed code and data contract

Names below are proposed interfaces, not existing APIs.

```text
HardwareControl.h                  immutable classified source facts
FactoryToneModel.h/.cpp             canonical mapping, nominal model, overlay
FactoryToneControls                typed receiver/effective controls, provenance
Engine::setControlFrame(frame)     one render-thread transaction
```

`ControlFrame` contains the sanitized effective PANEL/MEMORY `ParamValues`, two
source selections, and a fixed model version. The plugin captures it once per
block. The core CLI uses the same frame; it must not impersonate a tone merely by
calling `applyHardwareTone()` and losing its address. The legacy one-argument
`setParams(values)` remains available and has exactly its existing meaning.

`FactoryToneControls` distinguishes at least:

- `BusVolts[26]`, with separate known-cell and inferred-cell masks;
- oscillator `sawEnabled`, `pulseEnabled`, `pulseThresholdVolts`, PWM rate/depth;
- filter `effectiveCutoffCv[voice][side]`, `q[voice][side]`;
- filter targets in **envelope volts** and stage durations in seconds;
- amplitude target as a dimensionless ratio and separate linear VCA gains;
- initial/aftertouch receiver drives, not an unconditional velocity multiplier;
- model version, source address, boundary-law IDs and bridge-path mask.

Do not put volts into `CP_hpfCutoff` (Hz), ohms into `CP_fA` (seconds), or blindly
copy `V/10` into `CP_fIl/CP_fAl`. Current envelope helpers have their own legacy
offset convention. Feed the existing stateful envelope primitives through explicit
typed targets in the new path. Keep physical pin volts distinct from the
`nominalFilterCutoff()` effective-CV domain.

**Common envelope-state coordinate:** retain the primitive's existing stored
coordinate `z` for every source, without rescaling it on a tab change. In the new
isolation adapter define `TP1_model_volts=(5/3)*z`; convert a factory target
voltage to the primitive with `z_target=(3/5)*Vtarget`. PANEL/MEMORY/legacy-bridge
targets and their old readouts (`z` LP, `0.5*z` HP) remain unchanged. Thus moving
between those sources and a factory tone changes drive/readout rules, not the
meaning or numeric value of the stored `z`. The factory readout alone feeds
`TP1_model_volts` through the loaded graph. This 5/3 scale is a declared adapter
between the old ±3 coordinate and nominal ±5 V envelope range, not a measured
octaves-to-volts law. Keep its inverse in one place and test round trips; never
overwrite `z` with a number newly interpreted as volts. Amplitude state retains
its existing dimensionless coordinate. A new source's algebraic filter/gain
readout may change immediately even though stored envelope/audio states do not;
settled-DC control integration does not promise continuous cutoff-pin voltage.

The proposed oscillator boundary is shared with the
[oscillator reconstruction](OSCILLATOR_REALTIME_PROPOSAL.md):
`WscControls{pulseThresholdVolts,sawEnabled,pulseEnabled}`. A PW bus voltage is
not automatically the internal comparator threshold. A threshold-based oscillator
may produce a duty shifted by reset/coupling; do not force measured duty back to
the panel's nominal proxy after rendering.

### Activation and source ownership

Use `parity_mode && source < 11 && toneAvailableInModelVersion` as the only
activation condition. No new host parameter is necessary for these increments.
The existing 244 IDs, order, ranges, defaults and AU hints stay unchanged.
Default-off selects the existing lookup and old arithmetic, not a numerically
similar translation through the new model.

While a factory source is selected, its 26 controls come from the source model.
Changing the stored PANEL sliders or either unselected memory does not change
the tone, and returning to PANEL/memory reveals the stored values unchanged.
Footage and original global controls remain live; the existing isolation mask
continues to force filter enables/pan and suppress modern features. A rank-I
selection does not rewrite rank II, mix, detune, chorus, output, expression,
footswitch state or physical setup. Actual shared-bus electrical coupling is a
separate modeled effect, not permission to mutate another parameter.

Session restore uses the current schema rules: absent/malformed isolation is off;
absent selectors are PANEL. Factory recall remains PANEL, memories retained;
user-preset/session restore retains existing selector policy. Record model version
in capture/diagnostic metadata. Previously saved **isolated factory selections**
will intentionally use the newer reconstruction after an upgrade; do not promise
old isolated audio, or silently introduce an unautomated second enable flag to
pretend otherwise. Default-off legacy sessions remain the compatibility target.

## 3. Nominal converter ready to implement

Use one global, versioned `NominalFactoryModelV1`, shared by every tone. The
following choices make missing laws explicit and permit audio now. They are
**engineering inference**, not recovered Yamaha device parameters.

### 3.1 Selected source and assembled bus

Initial nominal boundary: selected-column source 10.0 V, zero source impedance;
ideal conducting isolation diodes; inactive factory/PANEL/memory sources isolated;
ideal common reference; uncharacterized static chip-input currents explicitly
`G=I0=0`. These are deliberate ideal limits, not claims that BA delivers 10 V,
that a 1S1555 has no drop, or that unselected memory outputs are physically open.
BA's actual emitter follower, shared-column loading and diode-isolated memory
followers remain in the uncertainty ledger. Do not add fictitious grounded BA
loads or permanently connected memory-pot loads.

For a classified analog branch, reduce the source branch and **one assembled
10 kΩ shunt** to its source-side equivalent before attaching downstream loads:

```text
resistor R:  E = 10 * 10000/(R+10000) V;  Rth = R || 10000 ohms
open:       E = 0 V;                    Rth = 10000 ohms
direct:     E = 10 V;                   Rth = 0 ohms
```

Then connect the actual known passive receiver graph. These E values are
**open-circuit equivalents**, not already loaded bus voltages. Do not feed a
loaded-divider result through a second shunt or second receiver load. Open
branches still retain their actual shunt and can acquire voltage from other
connected sources. Rows 4/5 instead use a separately declared ideal switch model:
direct asserts the corresponding wave, open deasserts it; no artificial 10 kΩ.
This is the logical limiting model of the M-card switch path, not a recovered
switch threshold, leakage or click waveform.

Unknown source cells never enter these cases. Increment 2 must first resolve
them through the explicitly separate overlay in section 4.

### 3.2 All 26 output roles and converter destinations

`u(V)=clamp(V/10 V,0,1)` is used only where the declared nominal law calls for it.
The clamps define an engineering operating range; record saturation in traces
rather than treating it as a measured rail limit.

|Out|Source destination / model operation|Existing setting or replacement boundary|
|---:|---|---|
|1|SUB VC1/VC2, not the global six-function SUB oscillator; inferred PWM-rate law `0.05*400^u` Hz|Rank PWM rate; 0.05–20 Hz are existing software endpoints, not recovered Yamaha anchors|
|2|SUB D1/D2 → processed P1/P2 → M PWM41; infer linear depth until SUB gain is derived|PWM depth, separately tagged inferred|
|3|M PW40; 680 kΩ path versus processed PWM41's 100 kΩ path at WSC input/feedback network|Voltage-domain comparator adapter; do not conflate depth bus2 with PWM41|
|4|M G1 → pulse gate|`pulseEnabled`|
|5|M G2 → saw gate|`sawEnabled`|
|6|SUB C1/C2 → NO1/NO2 → M NI43|Inferred linear noise VCA; all selected matrix branches open, so nominal factory noise drive is zero, not a claim of noiseless circuitry|
|7|R51/52 FH → eight HP pin-4 branches, plus BA alternate injection branches|Loaded cutoff adapter below|
|8|R51/52 QH → eight 220 kΩ branches, each sharing another 220 kΩ from master RES|32-node two-rank Q adapter below|
|9|R51/52 FL → eight LP pin-4 branches, plus BA alternate injection branches|Loaded cutoff adapter below|
|10|R51/52 QL → eight 220 kΩ branches plus shared RES|Q adapter; no fixed `RESH = RESL*0.35` recipe|
|11|M IL58 → IG00152 IL16|Inferred linear target `−5*u(V)` envelope V|
|12|M AL59 → IG00152 AL14; filter SL grounded|Inferred linear target `+5*u(V)` envelope V; sustain target 0 V|
|13|M AF62 → IG00152 AT9|`A(V)=1 s * 10^(−3u(V))`|
|14|M DF61 → IG00152 1DT10|`D(V)=10 s * 10^(−3u(V))`|
|15|SUB RF/RF′ → processed RFO/RFO′ → M RF60 → IG00152 2DT11|Release selector followed by `R(V)=10 s * 10^(−3u(V))`|
|16|M LP1 → 100 kΩ → IG00151 IC4 LI2, 10 nF to reference|Inferred linear filter-path gain, separate from envelope/LC gain|
|17|M LP2 → 100 kΩ → IG00151 IC3 LI2, 56 nF to reference|Inferred linear post-filter sine gain|
|18|M AA67 → IG00159 AT9|Same nominal A range; separate chip/law identity|
|19|M DA66 → IG00159 1DT10|Same nominal D range; preserve source `1A` lettering conflicts|
|20|M SL64 → IG00159 SL15|Inferred sustain ratio `u(V)`; amplitude initial target zero|
|21|SUB RA/RA′ → RAO/RAO′ → M RA65 → IG00159 2DT11|Release selector then nominal R law|
|22|R51/52 LV → 560 kΩ branch into each LC node → IC5 LI2|Loaded level graph; not raw bus22 used as audio gain|
|23|TRG1 IBI/IBII, then per-voice initial brilliance|Inferred monotone velocity-controlled receiver drive; preserve rank-specific wiring|
|24|TRG1 ILI for I; TRG4 ILII for II|Initial level drive into TI, not blanket velocity loudness|
|25|TRG2 ABI/ABII|After-brilliance drive; pressure zero removes this contribution|
|26|TRG2 ALI for I; TRG5 ALII for II|After-level drive into TA; do not mirror TRG2 onto rank II|

These timing ranges are directly stated on IC42 and IC52; their **exponential
interpolation is not**. Both state that higher timing voltage shortens time.
Endpoints describe nominal control ranges, not an assertion about a loaded
instrument's actual stage duration, trim position, or current software completion
threshold. Keep the existing inferred envelope shape/live-target architecture
initially; test coefficient and completion behavior separately. Do not call a
passed endpoint-function test a measured 1 ms acoustic attack.

Release uses the already documented idealized selector, not a raw preset wire
to the EG: when sustain is enabled use `min(individualCV, sustainCV)` in the
common nominal timing domain, equivalently `max(individualTime, sustainTime)`.
Keep the existing footswitch truth table and live retiming. Use the current
global sustain-time adapter initially, label it behavioral, and do not clamp or
reset a stored envelope value when the selector changes.

For PW/PWM, the first bridge may retain the current nominal duty proxy
`d0=0.5+0.4*u(PW)` and depth scale, with
`theta=A_nominal*(1−2*d0)` after the oscillator's documented sign convention.
This is an explicitly inferred panel-domain bridge, **not** the physical
680 kΩ/100 kΩ transfer. Replacing it with that loaded WSC summing adapter is the
next oscillator/control refinement; it must not delay the seven/all-22 delivery.
Report this bridge in every tone's metadata. Rates, depth and threshold get one
common law, never a new per-tone hand setting.

### 3.3 Reuse loaded reference equations without misusing their boundaries

**Cutoff:** build source equivalents for rows7/9 and use the R51/52 and M41
network from `LoadedControlRank.h`: all eight voices, FH/FL 39 kΩ branches,
separate HP/LP shunts, TP1 paths 100 kΩ/47 kΩ, and distinct trim branches.
Use explicit grade B as a **nominal fixture**, ideal ±15 V rails and centered
100 kΩ trim equivalents (0 V / 25 kΩ); test all nine HP/LP grade combinations.
Do not copy one voice's isolated voltage into eight voices or use cutoff shunts
on Q nodes. Rank-II passive use is supported by the common R51–52 schematic and
actual rank-II harness; missing upstream equivalence remains an inference.

Every reference input must be filled, not left as NaN or guessed by a helper:

|Cutoff boundary|Nominal v1 voltage / source impedance|
|---|---|
|FH, FL|Selected row7/9 source equivalents, including finite Rth|
|KC, TF|0 V / 0 Ω per voice; existing key/keyboard effective transports retained separately|
|IB|`Vbus23*velocity` / 0 Ω per voice, from the inferred TRG adapter|
|AB|`Vbus25*pressure` / 0 Ω per voice, from the inferred TRG adapter|
|TP1|`(5/3)*z` envelope-model volts / 0 Ω per voice|
|SUB, BR|0 V / 0 Ω common boundaries; existing effective transports retained separately|
|HP/LP trim|0 V / 25 kΩ each, from the declared midpoint pot model|
|HP/LP chip currents|Explicit `G=I0=0` for every receiver|

`keyVolts` remains the engine's separate Hz/V key input in the published cutoff
equation, not an additional invented R39 KC voltage. Do not inject key tracking
twice. Zero/stiff unused boundaries retain their physical summing resistors;
they are a nominal boundary assumption, not deletion of those branches.

The nominal model still needs a physical-pin-to-effective-CV convention. Choose
and expose an affine adapter per filter side:

```text
Veff_s = offset_s + slope_s * (Vpin_s − Vpin_s_at_reference_setup)
fc     = 125 Hz/V * keyVolts * 2^(Veff_s / 1 V)
```

For v1, choose the inferred slope so that changing the corresponding **loaded
FH/FL bus** by 1 V at the reference operating point changes effective CV by 1 V.
Derive that conversion from the nominal passive graph's nonzero derivative,
holding its other reference inputs fixed; do not equate pin voltage with bus
voltage. This slope is an explicit domain-interpretation hypothesis, not a new
Yamaha specification. Compare half/double slopes in sensitivity captures.

Set each offset by a **model alignment exercise** reproducing the documented
service setup: PANEL, square, FEET 2′, C1, centered brilliance, stated resonance
settings, FH=4.0 V for HP, FL=3.3 V for LP with HP/RESH low. Choose the offset
giving the maximum modeled response at that excitation, then freeze it for every
tone. Do not fit a different offset per patch or quietly substitute MIDI60 for
the printed C1. Use the key-voltage/footage relationship to record the chosen
key interpretation. The 4.3±1 and 2.2±1 Vpp CP3 targets are a separate audio-gain
alignment problem, **not values to impose on final plugin output**. Source
alignment constrains the model; it does not physically calibrate it.

Existing global brilliance/SUB/keyboard transports can retain their present
effective-domain mappings in this bounded increment, declared as such. Do not
also inject those same contributions through R39. Factory TP1 and touch paths
use their own receiver derivatives; remove the old generic HP envelope factor
and generic per-tone touch multiplier on that active path. Thus global behavior
is not silently lost while its full voltage-domain transport remains a later
refinement.

**Resonance:** use `ResonanceControl.h`'s 32-node graph with one shared master
RES source and four rank/side sources. For the first bridge, the existing master
resonance parameter `r` supplies the explicitly ideal source `10*(1−r)` V.
Map resulting VQ by `Q=1/(0.2+0.18*clamp(VQ,0,10))`. Yamaha supplies Q=5 at0 V
and Q=0.5 at10 V; linear damping between them is inferred. Do not apply the
old `0.8*rankRes+0.6*masterRes` combination afterward. If the other rank is
PANEL/MEMORY, derive its Q-source voltage through the explicit inverse bridge
`10*(1-rankRes)` V from its normalized resonance control, initially with zero
source impedance; do not omit its eight receiver ports. This is a nominal input
bridge, not a claim that the graph reproduces the old normalized Q mixer.

**Level:** use `LevelControl.h`'s 16-node graph (AM180 kΩ, LV560 kΩ,
TI/TA/KB270 kΩ) with shared AM and per-key KB, not two independent master
sources. Initial model TRG outputs are `Vdepth*velocity` or `Vdepth*pressure`,
explicitly zero-source-impedance, linear and unmeasured. Set omitted physical
modulation boundaries explicitly to zero while preserving their existing
effective-domain transports once, not twice. Interpret IC5 LI through a common
linear gain with zero offset; choose its unit scale from the nominal LV=10 V,
other drives=0 operating point. This normalizes engine units, not volts at CP5.
Likewise IC3/4 gain is `VLI/10 V` in v1. Keep the existing final engine unit-scale
constant; no per-tone loudness normalization, compressor, or hidden gain fitting.
The source describes LI as linear versus EI as exponential but does not supply
these absolute slopes or offsets. Saturation/headroom are separately inferred.

In mixed-source operation, use `10*vLevel`, `10*initLevel` and `10*afterLevel`
as the other rank's explicit nominal LV/TI-depth/TA-depth source proxies, with
ideal source impedance before the TRG velocity/pressure multiplication. Consume
new network readouts only for factory-selected ranks; PANEL/MEMORY keeps its
existing audio adapter in these increments. The other rank's represented ports
are therefore a declared boundary approximation, not a claim that the entire
mixed-source instrument has been solved. A later finite-impedance, fully shared
model must upgrade both adapters together rather than deleting those ports.

**Dynamics boundary:** the first converter is settled-DC/quasi-static for control
nodes, while the existing audio/envelope states remain continuous. This is an
explicit omission of the sourced 10 nF cutoff/LC and 10/56 nF LI transients,
not evidence that those capacitors vanish. A later state-space integration should
reuse `LoadedControlTransient.h` as an oracle, carry prior charge across source
changes, and add the level/LI capacitors with their real loading. Do not execute
the throwing, unfactored offline solvers inside the sample loop. Compile fixed
linear maps/coefficient sets outside rendering; apply small fixed-size maps on
the render thread. Reference solves remain independently executable oracles.

The first audio release must list retained bridges and omitted dynamics. It can
honestly claim **matrix-derived source-tone reconstruction**, not a solved
complete analog control system. Finite BA/diode/chip loading, full source
selection, touch laws, optical/effect behavior and capacitor dynamics can then
be refined without rewriting the source matrix or the preset bank.

## 4. Explicit inference overlay for the remaining 25 cells

Do not promote a legible crossed-out/revision-marked value into `verifiedOutputs`.
Do not make an unknown disappear by returning zero or copying the previous tone.
Define `InferredCell{address, policyId, nominalDrive, nominalSourceOhms,
uncertaintyDomain, reason}` separately. Queries of `findBranch()` must still
return `unknown` for all 25 addresses after increment 2.

One deterministic, implementable initial policy is **same-rank / same-output
median nominal excitation**, not neighbor-tone copying:

1. For each accepted cell in that rank/output compute `x=0` for open, `x=1` for
   direct, or `x=10000/(10000+R)` for resistor. This is a dimensionless
   ideal-source coordinate, not a recovered voltage measurement.
2. Exclude every unknown. Sort the accepted coordinates; use the middle value
   (mean of the middle pair for even count).
3. Supply the missing cell's inferred source equivalent as `E=10*x V`,
   `Rth=10000*(1−x) ohms`. This is a consistent ideal-divider hypothesis,
   **not an inferred physical resistor entered in the evidence table**.
4. Tag every use, even when the median happens to be 0 or 1. Zero here is an
   inferred excitation, never proof that the original branch is open.

This policy deliberately avoids treating disputed revisions as settled. It is
not claimed to be the most likely sound of each unknown tone. It is a transparent
starting reconstruction which uses the observed same-control distribution and
is easy to replace. Visible revision candidates remain separate alternative
hypotheses for comparison, not additional votes in that median.

Computed from the current accepted table (rounded display only; calculate from
source values rather than these decimals):

|Rank|Output|Accepted cells|Median x|
|---|---:|---:|---:|
|I|9|10|0.68077618|
|I|20|9|0.5|
|I|22|8|1|
|I|25|8|0|
|I|26|8|1|
|II|14|10|0.50199800|
|II|20|8|0.19638044|
|II|22|9|1|
|II|23|10|0.3125|
|II|25|9|0|
|II|26|7|1|

Full uncertainty for a missing passive branch is at least the nominal
`x in [0,1]` family, not merely the spread of known cells. This is a model-family
range, not a statistical confidence interval or proof of all revision topologies.
Render each tone's nominal overlay and single-address endpoint alternatives;
also render joint low/high cases and known visible resistor/topology candidates
when independently described in the audit. Do not claim those few corners bound
all nonlinear audio metrics. Particularly scrutinize Organ 1's LPF, String 4's
decay, sustain/level cells, and pressure responses affected by rows25/26.

Numeric-law uncertainty also applies to the seven classified tones. Sweep ideal
versus finite BA loading, nonzero diode drop/receiver current, grade/trim state,
cutoff slope, monotone timing alternatives, LI gain and TRG curves in a separate
research fixture. Name chosen values and ranges as engineering assumptions; no
probabilistic “authenticity score” follows from 547/572.

## 5. Selection, events and state continuity: no reset surprises

The existing **isolation-mode transition** deliberately resets voices/controllers
and effect histories; retain and communicate that established behavior. Select
isolation before playing. A source-tab change while already isolated is different:

- Apply the new selected controls once at the next block boundary, without
  `prepare()`, `reset()`, `allNotesOff()`, oscillator restart, note allocation,
  lost pressure, lost foot state, or an extra RNG draw.
- Preserve oscillator/filter/audio states and every active envelope's stage,
  accumulated value and elapsed time at the zero-duration event. Update target
  and timing drive using existing live-target/retiming rules. A newly short stage
  may complete on a subsequent tick under that rule; not inside the selector.
- Change waveform gates/thresholds and gains as controls, not note retriggers.
  No seamless/click-free switching claim follows; a physical switch can change
  signal routing. Any later finite switch/capacitor transient is an explicit
  model, not a crossfade which silently replaces the instrument's behavior.
- Repeated same-selection frames are idempotent. Selecting an unselected memory
  or moving inactive panel controls must not recompile or disturb a factory
  state. New source compile failure keeps the previous valid compiled frame,
  sets diagnostic status, and does not throw, zero all voices or publish NaNs.
- Future charged-node reconfiguration carries old node voltages into the new
  topology; it must not call a constructor that silently initializes to DC.

APVTS source and scalar reads are not a globally atomic host transaction today.
Use one captured, validated frame and an epoch for compiled settings so that no
render uses half an old topology and half a new coefficient set. If construction
requires worker work, publish fixed-capacity complete frames atomically and define
the activation block; do not block the audio thread or let a late older result
overwrite a newer selection. Fast finite lookup/compiled maps are preferable.

## 6. Concrete implementation sequence and acceptance

### Increment 1: seven-tone source route

1. Add canonical address table, typed model/controls and complete-column guard.
   Keep all current source data and legacy bank bytes unchanged.
2. Implement the explicit nominal boundary/converter above. Generate fixed
   coefficient tables from literal source data with a model-version/hash check;
   no hand-edited per-tone Hz, seconds, Q or gain table.
3. Add core frame and plugin isolation binding, using the existing two selector
   IDs. Seven available tones use the new path; other selections report
   `legacy_bridge`, never “source complete.” Preserve PANEL/memory/global policy.
4. Deliver captures and software tests below. This is an audible increment, not
   a requirement to finish the eventual transistor-level model first.

### Increment 2: all 22, immediately next

1. Add the 25-address overlay and manifest; verify the seven old results remain
   unchanged, the new fifteen are deterministic, and all 572 original source
   classifications remain unchanged.
2. Enable every factory selector in isolation. Add per-tone uncertainty display
   and source/receiver/model diagnostics; remove the fifteen legacy tone bridges.
   Retain the separately listed PW/global/dynamic engineering bridges honestly.
3. Run nominal/alternative renders for every tone and two-rank combinations,
   including mixed factory/PANEL/memory operation. Publish no fidelity percentage.

### Required tests before each increment is called delivered

- **Source identity:** all 22 indices/names/aliases map to the exact addresses;
  original fixture order is irrelevant; all 25 exclusions and 547 classifications
  retained; 750 Ω, 510 Ω and 390 Ω keep their units. Both companion pairs have
  exactly one assembled analog shunt, no switch shunts, no extra unused columns.
- **Converter oracle:** independent literal nodal assembly versus source
  equivalents; DC KCL and source/receiver current budgets; no double shunt/load;
  open/direct/resistor limits; finite/invalid inputs and receiver sign tests.
  Independently test 16 cutoff pins/rank, all 32 Q nodes and 16 LC nodes.
- **Nominal anchors:** timing monotonicity and endpoint functions for both EGs;
  IL≤0, AL≥0, filter sustain zero; Q endpoints and shared RES; independent HP/LP
  envelope paths; global sustain changes both releases without creating a held
  filter target. Document exactly which analog law remains inferred.
- **Routing:** all 26 controls and both ranks, all source choices, unselected
  edits, physical setup preservation, repeated automation, invalid restored
  selectors, editor closed/open, no hidden program/mix/chorus/footage overwrite.
- **Continuity:** source changes during Attack/Decay/Sustain/Release and active
  tail reuse, both directions between factory/PANEL/memory; exact zero-frame
  state snapshots; no phase/filter reset or controller loss; idle stays idle.
  Mode-toggle reset remains separately expected.
- **Audio/numerical:** all 22 at 44.1/48/96/192 kHz, keyboard extremes and eight
  held voices; finite output, documented peak/RMS/DC, releases settle, pressure
  and velocity routing, no clipping/normalization in captures. Compare identical
  events under varied block partitions; timing events stay at the same samples.
  Do not impose a nonzero steady sustain on naturally percussive source tones.
- **Mutations:** wrong selector order, doubled companion shunt, Ω→kΩ error,
  unknown→open substitution, reversed timing polarity, old Q mixer applied twice,
  shared RES duplicated, HP envelope copied from LP, reset on tab selection,
  isolation gate omitted. Each must fail a specific independent test.
- **Compatibility:** same-compiler standard and noisy-tail raw float corpora for
  all 222 legacy programs at all three established rates against pinned baseline
  `d56911a7c5f6953cd244457226b126cc567a538b`; default-off hardware-tab dynamic
  sequences also compared against pre-increment code. No parameter reorder.
  Run core, sanitizers, host smoke and existing validators on the exact commit;
  do not substitute this design review for those results or repair unrelated
  validator behavior as part of this tone change.

### Candidate evidence, not a fabricated reference corpus

Extend the existing candidate-capture tool with a new fixture version, leaving
existing fixtures unchanged. Record core frame, both source IDs/addresses, model
version, source-table hash, unknown mask/overlay policy, all electrical boundaries,
retained bridges, initial-state policy, rates/latency and exact event samples.
Export receiver/effective-control traces separately from raw audio. Include dry
single-rank auditions without modifying the saved patch, plus representative
paired performance. Label the audition's mix/effect settings explicitly.

`PARITY_HARNESS.md`'s candidate export does not constitute a hardware corpus.
Do not mark these renders `hardware`, loosen its ingestion protections or train
against its synthetic stand-ins to claim physical agreement. Public recordings
can inform listening if their patch/effect/recording uncertainty is retained.

## 7. Additional primary inspection for this proposal

Existing local native ledgers supply matrix and harness evidence; they were not
reclassified here. New work visually checked the original IC52 scan, the complete
IC42 envelope text, M41 PW/gate region, and adjustment printed p11. No new
resistance cell or hardware measurement was recovered.

|Input|Identity / SHA-256|
|---|---|
|[Yamaha IC52](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/52%20-%20M%20Circuit%20ICs.jpg)|5100×3300; `387d44e9f891c8af9bcef83e3894077df4f198d1676d4580041c828ba1c7fb51`|
|[Yamaha IC42](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg)|5100×3300; `5698fdfd19600f6b0b8b8f9dfdfc70723248b4b33e8daad39b9f9c0ee11cc24d`|
|[Yamaha M41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg)|5100×3300; `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`|
|[Yamaha adjustment procedure](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF)|PDF `eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba`; printed11/PDF13|

Native PNG crops use Pillow `Image.open(...).crop(box).save(...)`, no resizing;
coordinates `[left,top,right,bottom]`. Temporary evidence is under
`/tmp/cs80-factory-proposal/`, not proposed repository payloads.

|Crop|Box / rendering|PNG SHA-256|
|---|---|---|
|IC52 VCA/LI|`[950,580,1610,1800]`|`cb5aae7b9bb55f28e19003571ec2f7776b0896498d0ab83cbefd0084f268b1bc`|
|IC52 VCA-EG timing|`[1610,580,2890,2860]`|`08becb1769cb0742729eabe2674c98c6f2b6eadbf57904522aa1ddeeee38f635`|
|IC42 release text|`[3000,0,5100,850]`|`b82598661a3dc80ef6d9e5805b623adbb7499bd27f0e99487a259eb93e9b9007`|
|M41 WSC/gates|`[350,1050,1800,2650]`|`957d92f7a87f3eb45ec7d7034d32077639830dad76d3d736f338235d4e2c7596`|
|Adjustment p11|`pdftoppm -f 13 -l 13 -scale-to 2400 -singlefile -png`|`c568ee1720d9f17e9b526b94e5af8789fbcedfadf1fd7bb1f6a24259222e253c`|

IC42's existing lower envelope crop and its native provenance remain in
[ENVELOPE_TARGET_TRANSITION_REVIEW.md](ENVELOPE_TARGET_TRANSITION_REVIEW.md).
The downloaded IC52 retains its printed Vee `+15V` annotation conflict; use the
actual M41 supply graph, not that isolated typo, for an electrical model.
The VCA-EG crop covers the timing text; do not treat its truncated right edge as
a new complete SL-pin transcription.

No hardware/access blocker was found for this proposal. The web reader failed
to display the two JPEG URLs; direct primary-source retrieval supplied IC52 and
already-present local IC42 supplied native pixels. Missing chip laws and the
25 source exclusions are explicit model uncertainties with implementable choices
above, not reasons to pause. At proposal drafting, independent model review, implementation and exact-commit
tests were still pending. Section 8 records the subsequent standalone converter
and independent numerical/source review; audio integration and its exact-commit
acceptance tests are still required before an audible-delivery claim.

## 8. Standalone converter implemented (2026-10-29; integration still pending)

`src/core/SourceFactoryTones.h` now implements the seven complete-column nominal
converter as fixed-capacity, allocation-free, non-throwing arithmetic. This is a
**standalone model milestone, not yet an audible factory-tone delivery**. The
first-increment activation and continuity work in sections 2/5/6 remains the
integration contract; these numerical results do not substitute for Engine,
plugin, host, capture, compatibility or exact-commit CI results.

The public namespace is `cs80::source_factory`:

- `address(rank,selector)`, `addressByName`, `unknownMask`, and `route` preserve
  canonical board/column identity, both Electric Piano spellings, and explicit
  `legacy_bridge` status for the fifteen incomplete columns. Isolation is a
  required argument of `route`; `compileTone` alone does not authorize activation.
- `compileTone` derives typed targets, source equivalents and fixed maps from
  immutable source cells. Failed construction leaves its output unchanged.
  All 26 output roles have named boundary-law IDs; masks distinguish classified
  cells from inferred device laws. No missing-cell overlay is enabled in V1.
- `evaluateCutoff` consumes eight voice envelope coordinates/velocity/pressure
  values and yields all sixteen physical pins and separate effective-CV values.
  It preserves finite-source back-coupling across the common FH/FL buses rather
  than copying one voice's isolated solve eight times.
- `evaluateShared` represents all thirty-two Q ports and sixteen LC ports;
  the single RES, single AM and per-key KB sources are explicit ideal boundaries.
  `bridgeBoundary` supplies the documented mixed PANEL/memory input proxies;
  its output does not replace that rank's legacy audio adapter.
- `envelopeVoltsFromState` / `envelopeStateFromVolts` define the common 5/3
  readout and 3/5 inverse. Evaluators never write the supplied envelope state.
  Out-of-range finite voice readouts are limited to the named nominal ranges
  (z ±3, velocity/pressure 0–1) and report saturation; nonfinite input rejects
  the complete readout without publishing partial output.

The header's canonical-address FNV-1a source fingerprint is
`a95212770767733c`, checked against a literal in both a compile-time assertion
and independent tests. It includes board, column, output, branch classification,
printed unit and printed source-value coordinate (integer millionths), not just
converted ohms. Source fixture array reordering is therefore immaterial. This
is a deterministic source/model drift check, not a cryptographic authenticity
claim. `HardwareControl.h` remains unchanged: 547 classified and 25 unknown.

### Frozen shared cutoff alignment and oscillator boundary

Further independent primary inspection of the already cited adjustment PDF
resolves printed **C1 as the lowest C**, with **K1–K8 = 125.0 mV ±0.1%** on printed
p3/PDF5. Under the retained MIDI60/.5 V interface convention, that is MIDI36,
not MIDI60. FEET 2′ gives a 4× oscillator ratio while the key input remains
0.125 V; the A440 square excitation is 261.6255653 Hz. The PDF5 render (`pdftoppm -f 5 -l 5 -scale-to 1600 -png`)
SHA-256 is `ad7c08b72ea60a5c826835f497758daaab81bd571ba31d29e10bd0e9b0233194`;
the original PDF hash in section 7 is unchanged.

The common alignment exercise uses a unit ±1, 50% square and the settled
peak-to-peak response of the **ideal continuous linear SVF HP→LP pair**:

```text
Hhp(s) = s² / (s² + wh*s/Qh + wh²)
Hlp(s) = wl² / (s² + wl*s/Ql + wl²)
```

This is an engineering alignment hypothesis, not a Yamaha device law or a fit
to measured CP3 amplitude. PANEL cutoff HIGH/LOW are explicit stiff 10/0 V
boundaries in this exercise. Service master RES is HIGH, represented by 0 V.
For HP alignment both rank resonances are HIGH, hence Qh=Ql=5, and LPF is HIGH.
For LP alignment HPF/RESH are LOW, RESL HIGH: the shared-RES graph gives Qh=10/11
and Ql=5, **not Qh=.5**. The 4.0/3.3 V service reference buses remain fixed.
With `a=fcHP/f0` and `b=fcLP/f0` at their respective alignment reference setups,
maximize square peak-to-peak in these two coupled conditions:

```text
HP exercise: fcHP/f0=a;    fcLP/f0=b*2^6.7; Qh=5;     Ql=5
LP exercise: fcHP/f0=a/16; fcLP/f0=b;       Qh=10/11; Ql=5
```

A converged Fourier-series search (8,192→32,768→131,072 time points) was
independently checked using the exact four-state continuous periodic square
solution and derivative-root extrema. The latter gave reference effective-CV
offsets HP **4.05640724586** and LP **4.07460544278**; V1 freezes rounded values
**4.05640725 / 4.07460544** for every tone. A 1,001-point logarithmic ratio scan
from .01 to 100 found its largest sampled peak in the fundamental region; the resulting
reference cutoff ratios are approximately .9936652495 / 1.0062787580. No
per-tone fit, plugin-output normalization or CP3 gain calibration is applied.
The test suite pins these declared anchors; it does not call them measurements.

The effective slope is independently derived as the inverse passive derivative
`1 / (dVpin/dVloadedBus)`, not the derivative with respect to unloaded source E.
Production uses grade B with centered trims. The optional nine grade-pair fixtures
recompute their own affine slope/reference (a **recalibrated conditional fixture**).
They verify passive graph arithmetic; an uncompensated grade-sensitivity study
must instead freeze the grade-B calibration while varying the passive graph.

The retained factory PW bridge now explicitly shares the oscillator proposal's
nominal half-ramp span **1.525 V**, yielding
`pulseThresholdVolts=1.525*(1−2*dutyProxy)`. It is still the disclosed panel-domain
proxy, not the recovered 680 kΩ/100 kΩ WSC transfer. Cross-namespace oscillator
control structures must be copied by named fields, not positional aggregates.

### Standalone checks and limits

`tests/SourceFactoryToneTests.cpp` currently passes **60,959 checks** with strict
C++20 warnings and independently with AddressSanitizer/UndefinedBehaviorSanitizer
(leak detection disabled). Checks include literal source-resistor + single-shunt
18-node cutoff assembly, all nine grade pairs, unequal drives at all eight voices,
comparison with the separately implemented 16-pin/32-Q/16-LC reference solvers,
independent KCL/current budgets, timing/Q/polarity anchors, source identity,
unknown/failure guards, finite-input/readout contracts and typed provenance.
Independent reviewer checks additionally compared **76,680** node/bus values,
with maximum discrepancy about `5.33e-15` under the selected nominal boundaries.

Eight deliberately incorrect mutations were rejected: doubled assembled shunt,
Ω→kΩ conversion, unknown→open conversion, reversed timing polarity, old Q mixer
applied again, HP envelope resistor copied from LP, omission of the eighth common
bus load, and wrong selector ordering (the last fails the fingerprint assertion
at compile time). Engine-only reset/isolation/continuity mutations and the
legacy raw-float corpus remain integration acceptance work; they are not claimed
by this standalone test result.

Remaining declared boundaries include ideal BA/diodes and receiver currents,
linear TRG/LI/noise/PWM laws, exponential timing interpolation, physical-to-effective
cutoff interpretation, retained global transports, mixed-source proxies, and
settled-DC omission of capacitor/switch transients. None requires a hardware
permission gate; each is a documented refinement target. The fifteen-tone
inference overlay remains the separately reviewable next increment.
