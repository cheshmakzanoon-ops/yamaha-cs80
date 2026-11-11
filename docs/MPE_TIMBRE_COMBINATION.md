# Separate MPE manager and note timbre

## Product policy

MPE v1.1 section 2.2.8 requires meaningful, separate combination of Manager and Member Channel CC74 for each sounding note. Appendix D permits multiple strategies. This implementation chooses **Max**, a zero-neutral manager timbre floor, matching the existing zero-default unipolar timbre control. This is a product policy, not a mandated centred or bipolar CC74 law.

Each voice retains its note-local input independently of two retained manager inputs. The target is the maximum of the local input and eligible actual managers. Existing captured-zone routing and own-channel manager routing both apply, including the rare conventional-origin note with distinct captured and own managers. The existing timbre smoother, brilliance contribution and modern modulation source consume the resulting target without a new sample-loop pass.

Member messages update held notes and future-onset state. Accepted releases freeze their local input against channel reuse, while eligible manager messages continue to affect those tails. New notes inherit both components. Manager events never overwrite local input.

Reset, effective MPE-mode edges and endpoint role changes invalidate manager records; extension activation copies them. Mode edges retain existing written voice targets until subsequent eligible input, matching the existing pressure lifecycle. CC121 retains CC74 state, including both new components, exactly as before. Same-role MCM and CC120/123 retain these CC74 components. No full controller-reset conformance is claimed.

MIDI-learn consumption and the independent optional CC74-to-ribbon path retain their ordering. PURE and conventional channel routing remain unchanged. Pressure and pitch are not changed by this patch.

## Reproduction and qualification

On published source `e349294d`, with only a read-only timbre field appended to VoiceInfo, the new core fixture reported **678 checks, 168 failures**. An initial build exposed an accidentally duplicated diagnostic field declaration; it was corrected before that executable baseline was built. The fixed core fixture reports **678 checks, 0 failures**.

The test covers both engine policies and zones, event ordering, onset, manager decrease/zero revealing local input, released/pedal-held channel reuse, independent zones and gaps, manager-channel notes, finite admission/clamping, reset/mode/role/inheritance, CC121 retention, conventional/PURE behavior and dual-manager history.

The strengthened core fixture passes **1,142 checks**, including both ranks' actual timbre-to-pulse-width modulation readouts and finite, non-silent settled PCM. In the bounded deterministic cases, manager .7 plus local .4 is byte-identical to local .7 alone and has different PCM from local .4 alone. This is not a universal audio-equivalence claim.

The initial four existing-suite run passed controller reset and failed three old-policy fixtures: member onset (4 failures), released targets (8), and tuning/MIDI (202). Their explicit manager-onset/retained-floor expectations and independent neutral setup were updated, while preserving routing controls and tolerances. New independent manager-CC74 decrease/zero assertions retain held local input while released neutral-local tails follow the manager, with pressure unchanged.

Expanded native testing initially reported 1,960 checks with four failures from demanding settled Circuit ribbon telemetry after only 32 frames. The existing 5 ms smoother makes that an in-flight state; the fixture now renders a declared 128 ms settling interval before requiring exact centre. No production ribbon change or tolerance relaxation was made.

The final affected 66-step rebuild retained two pre-existing snapshot enum-conditional warnings; it is not described as warning-free. Selected integration passed **38/38 tests in 137.77 seconds**. The native fixture passed **2,336 checks**, including both policies/zones, actual extension ownership, zero/rendered callbacks, released-tail reuse, dormant activation, learned manager CC74 consumption and the independent mapped ribbon. An independent repeat passed **38/38 in 135.78 seconds**, with explicit process exit 0 and separately preserved logs. A second independent reviewer reran the two new timbre suites plus allocation: **3/3 passed in 5.61 seconds**, with 1,142/0 core, 2,336/0 native and 204/0 allocation checks. Independent source, regression and policy reviews found no blocker. No external controller/DAW, GUI, complete MPE conformance, live-service, full CPU target or callback-deadline qualification is claimed. Historical intermittent offline-render failures are not declared universally explained or absent.
