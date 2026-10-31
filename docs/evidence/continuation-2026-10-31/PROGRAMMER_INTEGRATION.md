# Programmer audio integration: registered design

2026-10-31. Acceptance limits are registered in
`tests/parity/tolerances-v2/programmer-integration.json` before evaluating the
integration. The electrical model and its alternatives were already registered
in ws6; this step connects that model to the circuit-engine rank chain.

The plugin's APVTS parameters are still the only mutable authority. A
`ControlFrame` may carry explicit electrical positions for its selected
memory/PANEL bank. Legacy ignores them. Existing frames without positions and
`Engine::setParams` preserve their physical-value API. Plugin, audit and
benchmark source-policy frames opt into the electrical coordinate explicitly.
No parameter IDs or saved-memory format are changed.

## Declared coordinate, not a fitted calibration

Each row maps by channel enum and the already declared nominal receiver law.
Rows1/2/3 are PWM rate/depth/PW,4/5 pulse/saw,6 noise,7/8 HP cutoff/Q,
9/10 LP cutoff/Q,11/12 filter IL/AL,13/14/15 filter A/D/R,16/17 filter/sine
level,18/19/20/21 amplitude A/D/S/R,22 overall level,23/24 initial
brilliance/level,25/26 aftertouch brilliance/level. The source evidence and
receiver-law qualifications are in `SourceFactoryTones.h`, `MemoryPanels.h`,
`PRESET_BA_CUTOFF_BRANCHES.md`, `PRESET_AL_RECEIVER_GRAPH.md` and
`RANK_II_RECEIVER_PORTS.md` (BA27/28, MBK63, PN1, M41 and R39).

The map uses inverse nominal voltage laws for seconds and resonance; PWM
rate is logarithmic. Cutoff uses the existing key-voltage reference and
frozen affine service-alignment coordinate. Linear level/depth controls use
their declared fraction. Fractions are saturated to the physical pot ends.
The mapping is a **declared interface inference**: alternatives are actual
measured shaft/wiper coordinates or a different mechanical taper. It is not
selected to preserve a preset waveform or force a service voltage to pass.

The selected50kΩ linear pots,1Ω endpoints and ideal cutoff followers remain
the pre-existing declared electrical inference. Alternatives10kΩ/100kΩ are
retained in the model documentation. Loaded bus voltages can therefore change
timing, level and resonance relative to nominal parameter labels. Those
changes must be audited and reported, not removed through per-program gains.

## Source boundary

Compile the shared rail and every selected bus at its original operating
point. A per-row Thevenin equivalent includes the pot, diode differential
resistance, assembled10kΩ shunt and shared-rail differential impedance, with
the receiver conductance removed exactly once. It feeds the existing cutoff,
Q, LC, EG and waveform adapters. The equivalent is a local linearization;
off-diagonal dynamic rail coupling and large-injection diode changes remain
declared limitations, as in the refined factory boundary.

Unchanged positions reuse the compiled network. Selection and all positions
are validated and compiled before `setControlFrame` publishes either rank.
The four memories use existing rank/bank ownership: rankI M1/M3 and rankII
M2/M4. Memory/PANEL hardware is available under Pure CS-80.
