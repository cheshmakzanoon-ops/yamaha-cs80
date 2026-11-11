# Controller state when activating sixteen voices

## Reproduced gap

The dormant second instrument receives neither ordinary MIDI controllers nor MPE negotiation. Previously, switching from eight to sixteen voices reset that instrument and restored only the expression pedal. Its newly allocated notes could consequently use different zones and bend ranges, lose member bend/pressure/timbre, or ignore an already-held manager sustain pedal.

A direct native processor fixture on published `a16ff4b4` reproduced 160 failures in its initial 236 checks. An independent scratch witness confirmed disagreement in lower/upper zone membership for an upper-zone configuration, missing partial RPN data and missing ninth-note expression in both Legacy and Circuit. This is a change to the previously documented activation limitation, not a compatibility-only optimization.

## Activation contract

After applying the matching control frame and resetting the extension, the processor copies the primary's current retained controller/protocol state exactly once, before new MIDI and UI events in that callback:

- The complete MPE zone negotiator, including zone sizes, ranges, selectors, pending Data Entry MSB and NRPN suspension.
- Per-member raw bend, pressure and timbre with their channel roles.
- Per-channel sustain state; global/lower and upper master bend targets; pitch-wheel, mod-wheel, channel-pressure matrix, expression and ribbon targets.

The source is the primary's current state, after any applicable lifecycle reset; activation itself does not reset the primary. An earlier prepare, policy reset or effective MPE mode edge must not be undone by restoring stale dormant state. Reactivation replaces old extension state rather than merging it. The already-active extension is not resynchronized each block.

No voices, held keys, note queues, allocator state, oscillator/filter/FX state, or modulation-envelope history are copied. Processor-owned CC88 velocity prefixes and bank/program selection already have one shared owner and are not duplicated. New notes continue through ordinary routing/allocation. Existing MIDI-learn consumption still takes precedence.

The new instrument starts its reset controller smoothers at the copied targets. It does not inherit the primary's in-flight dezipper or optical-cell history. There is no bit-identical transition-audio claim. Source inspection shows fixed-size assignments without new loops over samples or heap allocation; measured CPU/deadline and full real-time guarantees are not claimed.

## Regression scope

The final fixture tests first activation and reactivation, both engine policies, zero-frame and rendered callbacks, lower and upper zones, saved RPN selectors and Data Entry MSB, NRPN suspension, CC121, member expression, both manager bends after settling, manager sustain/release, and five retained matrix-controller sources. It verifies that eight primary held notes are not copied into the extension and that prepare/MPE/policy invalidation wins. The older member-onset fixture is updated to the explicitly changed activation policy.

An initial direct frequency comparison failed eight checks because the primary's controller smoother was still moving while the reset extension had snapped to the same target. The final pitch witness renders 4096 frames before comparing exact oscillator frequencies; no tolerance was relaxed and no production code changed to hide that expected lifecycle difference.

The expanded final 376-check fixture separately failed 224 checks against the retained exact `6fc5354d` control build and passed all 376 against the correction. That older control is not mislabeled as current main; the initial 236/160 result above used `a16ff4b4`. The final affected-target build completed 18 steps, and all 21 selected tests passed in 81.60 seconds. This includes six MPE/controller/release core tests, both new and existing native lifecycle tests, allocation 204/0, undo 317/0, the mirror regression and generator, and all six actual-module VST3 rate/mode registrations. An independent run of the same 21-test selection on `5abcd281` also passed, in 79.62 seconds, including handoff 376/0 and allocation 204/0. This does not implement the remaining MPE manager/member pressure/timbre combination, full MCM note termination, or live held-note bend-range retargeting. No external DAW, physical controller, GUI, full CPU target or universal offline-render qualification follows. The earlier intermittent offline failures remain documented separately in [state-mirror feedback](analysis/PARAMETER_STATE_MIRROR_FEEDBACK.md).
