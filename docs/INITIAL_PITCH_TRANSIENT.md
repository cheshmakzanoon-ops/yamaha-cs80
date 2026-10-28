# Nominal initial pitch transient

This increment changes only the initial **pitch-bend** onset approximation. It does
not impose a common decay on Initial Brilliance or Initial Level.

## Source-derived nominal pole

See [the touch circuit evidence](INITIAL_TOUCH_CIRCUIT_EVIDENCE.md), including
native TWS page17 and TRG4 page23 scans. The buffered b output drives a 1µF
coupling capacitor. Under the ideal-opamp, nonconducting-clamp assumption, the
capacitor sees a 100kΩ shunt in parallel with the 100kΩ virtual-ground input.
The resulting conditional pole is:

- R = 100kΩ || 100kΩ = 50kΩ
- tau = 1µF × 50kΩ = 50ms
- A positive b step gives a negative C onset, then exponential recovery.

TRG4 places the PB depth control downstream of C. Consequently the normalized
onset state must continue evolving even when PB depth is zero; moving the control
scales the current state rather than creating a new onset.

## Implementation boundary

The previous arbitrary 75ms decay is replaced by the conditional nominal 50ms
positive-step response. Each accepted note-on starts normalized state c=-velocity.
At each internal sample, c is multiplied by exp(-1/(0.050*internalRate)).
The coefficient is computed in construction/preparation, not in the sample loop.
The current PB control multiplies c only at the pitch destination.

The first host-frame diagnostic observes two internal updates, so its elapsed
time is exactly 1/hostRate. Tests compare elapsed host-frame counts, not an
ambiguous pre-update sample.

Double state/coefficient arithmetic limits accumulated rounding. A normalized
magnitude below 1e-15 snaps to zero to avoid denormals. The previous 1e-4 threshold
stopped updating and left a permanent tuning residue; it is removed.

**This is not a full TWS high-pass or transistor-level model.** It deliberately
retains the existing reset-to-velocity MIDI note-on/retrigger approximation.
A physical high-pass responds to changes in b, and actual FET acquisition,
previous capacitor state, trigger timing, clamp conduction, leakage, opamp limits
and loading can change retrigger/release behavior. MIDI velocity-to-voltage and
PB voltage-to-frequency scaling also remain provisional. No exact lifetime of
the b sample/hold signal is asserted. The held Initial Brilliance/Level paths
are untouched.

## Compatibility and verification

No parameter ID, count or state format changes. Existing patches with PB enabled
recover toward nominal pitch faster than before (50ms rather than75ms nominal
decay). PB edits during an onset now act on the still-evolving upstream state.
This audible change is intentional and is not claimed to reproduce every
calibrated or aged instrument.

New numerical regressions cover:
- e^-1, e^-2 and e^-3 at50/100/150ms across44.1,48,96,192kHz;
- monotonic recovery from below, both ranks, and no frozen residual;
- zero-depth audio identity and late enabling/changing PB without state restart;
- staggered polyphonic onset age and velocity independence;
- preservation of the existing held initial-brilliance and level behavior.

These tests verify this declared approximation, not measured hardware equivalence.
Compilation and exact-revision CI must establish passing results.
