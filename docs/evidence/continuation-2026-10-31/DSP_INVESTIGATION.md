# DSP continuation: preregistered questions

2026-10-31. Starting revision `db24a47170610f81236e8829ef5536af69dfc94d`.
No candidate audio change has been evaluated at this checkpoint.

## Retrigger and release

The unchanged phrase audit reports Bass Stack's retrigger score as 1.51 at
44.1 kHz and paired Funky 1 + Funky 2 note-off as 1.65 at 48 kHz. A score is a
ratio of maximum sample-to-sample steps over a window; it does not identify a
physical discontinuity by itself.

The investigation will compare the amplitude and filter envelopes, filter
capacitor voltages, IG00151 LI node and output-coupling state at the actual
assignment/release. Counterfactual probes must isolate mechanisms; they are
not proposed tonal changes. A fix must preserve free-running phase and the
capacitor states, rather than masking an output step with a fade.

One source-review hypothesis is that a never-used PANEL/MEMORY voice warms
its filters with an untriggered filter envelope at zero, then `startVoice`
replaces that capacitor level with IL. In the selected EG topology, released
TR selects IL (see `ENVELOPE_TARGET_TRANSITION_REVIEW.md`,
`ENVELOPE_RC_MODEL.md` and `ACTIVE_TAIL_CONTINUITY.md`); the existing numerical
RC law is a declared inference. The hypothesis and alternatives (VCA pedestal,
amplitude-envelope restart, and coincident ordinary waveform edges) remain
unproven until traced.

The new continuity and phrase limits are in
`tests/parity/tolerances-v2/completion-dsp.json`. The existing score calculation
and its windows will not be changed. Any surviving score above one will be
reported, including a case proved to be a normal waveform edge.

## Filter numerical accuracy

`Ig00156Stage` already prewarps every capacitor at the requested cutoff.
The outstanding error is the curvature of the bilinear frequency map over
a broad band, not the absence of a prewarp. The current nodal test prints
whole-grid misses and asserts only the already-passing subgrids. Completion
requires asserting the existing limits over the entire registered grid.

No hardware-network or Q law will be adjusted to compensate for a numerical
frequency-warp error. A candidate correction must keep the continuous truth
model and use an independent derivation of the new discretization. The
limits in `ws1-voice.json` remain unchanged.

The ADJ p11 HP discrepancy is an evidence question. M41's LP 150 kOhm
interpretation (shunt in the inspected ledger, series in the current declared
model), HP input-switch loading and the actual adjustment setup need direct
inspection. Component or adapter values will not be selected to fit the
reported voltage.
