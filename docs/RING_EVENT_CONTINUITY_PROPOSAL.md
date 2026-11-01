# Global ring state at key events: preregistered causal review

2026-11-01. **Source review and proposed numerical contract only. No new gate
has been evaluated and no production audio has been changed by this proposal.**
The exact acceptance contract is
`tests/parity/tolerances-v2/ring-event-continuity.json`.

## The two omitted states

The published 72-field rank-continuity gate covers rank oscillators, filters,
envelopes and VCA state. It does not observe the global ring oscillator or its
separate frequency-envelope generator. In production
`23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162`, `Engine::startVoice` sets
`ringPhase=0`, `ringEnv=0` and `ringAdsr.reset()` whenever `ringKeyDown` is false,
then enters Attack. The last released key clears `ringKeyDown`, even while
released voices can still contribute audio. `mixBus` advances the carrier and
envelope continuously, including while MODULATION is off.

These are two separate possible discontinuities. Replacing carrier phase
changes the ring multiplier immediately. Replacing the frequency-envelope
charge changes its frequency control; it does not by itself replace phase.
Neither observation identifies the maximum derivative in a complete phrase.

## Inspected source and declared inference

The [Yamaha PRA service schematic, printed p53](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/53%20-%20PRA%20Circuit%20Diagram.jpg)
shows TR(19) connected to IC6 IG00159 G2, with a 100pF path to G1. IC9 VI receives
VC1(4) through 100k and VC2(3) through 180k and has its own timing capacitor at C.
No external TR wire reaches IC9 in this drawing. The reciprocal
[PRA p54 board/wiring page](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/54%20-%20PRA%20Circuit%20Board%20%26%20Wiring.jpg)
identifies IC9 as IG00150 and reconciles the ambiguous handwritten pin 7 as
SO, its sawtooth output; it is unconnected, not a reset input. The same page
connects EO(18) to DEPTH VR5 terminal 3, VC2(3) to VR5 terminal 2,
VC1(4) to SPEED VR6 terminal 2, and TR(19) to KAS TR9(35).

The same page separates the ring envelope's stored state from its trigger:
IC5 OUT8 feeds IC6 BI2 with a 0.22uF capacitor; buffered IC6 OUT3/EO returns to
IC5 -IN4 through 100k. LC drives the opposing +IN3 path through 100k and its
470k trim network. The trigger reaches the sequence gates, and no direct
trigger-operated capacitor discharge branch is traced. This matches the
IG00159 state architecture already reviewed in
[ENVELOPE_TARGET_TRANSITION_REVIEW.md](ENVELOPE_TARGET_TRANSITION_REVIEW.md),
using the [Yamaha IC guide](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf)
PDF 41 / printed 2-7. The recovered PDF hash and the independently rendered
page match that earlier ledger exactly. G1/G2 enter sequence selection, which
drives LC/TC; BI2 and buffered BO3 form a separate state/feedback path.

**Declared inference:** preserve carrier phase and the actual double-precision
envelope-charge state at zero elapsed time. A first held key still selects Attack, using the
existing [inferred RC law](ENVELOPE_RC_MODEL.md) from the retained value.
Legato gating and last-key Release retain their existing selection behavior.
The oscillator's phase derivative may change when its frequency control changes.
No new component value, time constant, fade or envelope shape is inferred here.

Alternatives remain explicit: an untraced synchronization path, trigger charge
injection, a finite transient discharge, or internal sequence/comparator details
could refine the physical model. Their magnitude and behavior are not measured.
The proposal is consistency with the inspected external topology and the already
declared capacitor model, not a claim of measured hardware equivalence.

The owner-manual attribution for AD triggering/legato is pp13-14,45 in the
[existing reference ledger](HARDWARE_REFERENCES.md). This review has not independently
recovered those scanned pages; phase continuity is supported by the inspected
PRA drawing, not by inventing an owner-manual statement about phase.

## Preregistered witness and preservation boundaries

The contract tests Source Performance and PURE CS-80 independently at 44.1/48/96kHz
and all three quality selections. A fixed 77Hz fixture exercises cold first-key,
legato, partial release, last release and a re-key over an active released tail.
Its preconditions require nonzero carrier, nonzero ring-envelope charge before
re-key, positive rank amplitude envelopes and nonzero preceding audio. A missing
witness fails rather than silently skipping the assertion.

Phase, float envelope output, actual double charge and envelope telemetry have
separate zero-bit bounds. The re-key fixture must contain a double charge that
cannot be represented exactly by the float diagnostic. `Adsr::noteOn` currently
rounds its double accumulator through that float, so merely removing the
explicit `ringAdsr.reset()` is insufficient. A Source-preserving path must retain
the double charge and initialize its attack origin from that same value while
existing/default callers keep their arithmetic. The independent reference
restores the saved double directly after ordinary stage initialization, without
calling the new preservation method. The rank72 gate's float diagnostics do
not establish exact retention of hidden double state.

A separate bounded subsection registers the same numerical invariant for both
rank amplitude envelopes. Eight assigned voices, one release, and a replacement
over that active tail provide positive, non-float-representable charge in each
rank while seven other keys remain held. This excludes a first-key global ring
trigger from the rank fixture. A primitive-level check also covers held and
released re-entry. Source ring and rank callers can opt into one preserving
path; default/Legacy callers retain their existing rounding and reset behavior.
The rank numerical correction is asserted separately and is not presented as
an explanation of an audible click score.

An independent retained-state replay and untouched-control PCM comparison
guard the observer and event timing. First-key/legato AD stage semantics remain
asserted independently. MODULATION=0 must render exactly the same PCM as a
test-only historical-reset replay. Legacy keeps its historical phase/EG reset;
explicit engine reset keeps its cold zero state. The 72-field rank gate and
every existing phrase, frequency-law and compatibility criterion stay unchanged.

For the original electrical-programmer Laser Sweep phrase, four same-prefix
replays intervene only at the 6.25s target: historical behavior, retained phase
only, retained EG only, and both. An omitted-target control distinguishes a
new-note contribution from ongoing audio. The capture must retain the original
8ms window, chord denominator, peak sample and ratio. No particular score
improvement is a prerequisite or a justification for a model change.

The Clavichord 96kHz `click_steal` exceedance remains a separate investigation.
The metric's event name does not establish that the selected circuit still has
a sounding tail; that requires a pre-event voice/charge trace. A neutral ring
amount also excludes a ring-state change as a cause of that row's output.

## Native-source provenance

The p53 source is 5100x3300, 986,593 bytes, SHA-256
`a691e710b59deecace01a62b57587e6b939e1084bfd71003efe71929bbe42c61`.
Six original native tiles were recovered from
[Actions run 40807476007](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40807476007),
job 123229758333, source commit
`c87b57c8b3b7b1ff79c8a4b4e168926a7a1f05de`; their dimensions and hashes were
validated before unresized reassembly. The separate source-evidence receipt
preserves the six tile bytes and manifest under
`docs/evidence/qualification-2026-11-01/ring-trigger-source-review/`.

| Inspected focus crop | Source-native box | SHA-256 |
| --- | --- | --- |
| Ring envelope | `[1740,1540,2630,2430]` | `98de2009fcafc9044b84bee956f11cb3b7e892669bbce564cdc68d20791d61a4` |
| Carrier | `[2870,2150,3690,2640]` | `e83f291ba6b9cc5b25136ab2ff38e7ac18f299fb1c04ebef7227bf5c3c353004` |

These focus crops are extracted from the validated native-tile reassembly;
they are not original Actions crop identifiers. No hardware measurement was made.

Root must publish the contract before evaluating the witness or editing
production behavior. Any resulting Source audio correction follows the separate
audio-commit/V2-pin/strict-compatibility sequence; the current full audit remains
an immutable pre-change baseline.
