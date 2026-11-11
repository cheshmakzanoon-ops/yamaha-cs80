# MCM role-transfer cancellation

## Chosen safety policy

The [membership reset](MPE_MEMBERSHIP_RESET.md) stopped channels crossing between no MPE role and an assigned role. A full lower-to-upper transfer can keep all sixteen channels assigned while changing every role, leaving old notes alive under different controller ownership. On `6f90e1b5`, a two-policy core witness confirmed that a channel-2 note survives a lower-15 to upper-15 transfer.

This follow-up intentionally cancels old note obligations whenever a channel's role changes among none, lower manager, lower member, upper manager and upper member. Same-role channels retain their notes and existing range-retarget behavior. It is a broader bounded product safety policy, not an invented exact role-transition mandate: MPE v1.1 §2.2.3 describes entering/leaving MPE control, while Appendix F summarizes stopping ongoing notes more broadly. Full specification/reset conformance is still not claimed.

## Preserved boundaries

The same batched cancellation path handles voices, scanner input, waiting keys and ARP sources, followed by unaffected-key reconsideration. Existing channel-owned pedal/expression/RPN cleanup and native held-mask/CC88 coherence use the exact role-difference mask. Global pressure-gesture attribution remains conservatively invalidated as previously documented.

Shared controller buses, scaled manager bend and their smoothers are deliberately preserved. A conventional-origin voice can retain historical lower-bus routing across an MPE-mode edge while its current upper-member role remains unchanged. Resetting a departed lower manager's bus would retune that unaffected voice. Deciding independently per engine could also diverge the primary and extension instruments. This patch does neither. Shared effects/noise histories are not cleared, and it is not a zero-output panic guarantee.

## Qualification

The initial actual-processor regression on `6f90e1b5` failed 560 of 2,712 checks: 336 note-cancellation and 224 host-mask assertions. All same-role, extension, finite-render and fixture controls passed. After the mask correction the same fixture passed 2,712/2,712. The final expanded fixture passed 6,044/6,044. It adds CC88 ordering, channel state/selectors, deferred scanner/waiter/ARP obligations, real UI ownership and shared-controller witnesses. Conventional-origin unchanged-role voices are compared with actual/reference/neutral processors, checking exact audio/frequency/matrix continuation in flight and settled, plus a positive one-semitone contrast to the neutral wheel.

The affected native build completed 50 steps, followed by two expanded-fixture and four corrected-fixture build steps, with two existing snapshot enum-conditional warnings. The final selected core/native integration run passed 28/28 in 116.70 seconds, including all six VST3 rate/mode configurations. Independent source/test review found no blockers. The same 28-test selection independently passed 28/28 in 116.20 seconds, with the original acceptance criteria and no production changes between runs. No external DAW/controller, full MPE/reset, CPU/deadline or universal offline-render guarantee is implied.

Earlier membership, member-range, released-target, master-routing and tuning-expression tests intentionally preserved still-MPE reassigned voices. Those expectations are changed to termination plus fresh-note routing under the new role. Same-role and MPE-mode-edge historical routing remains independently protected. No pitch tolerances are relaxed.

The first expanded integration run passed 27/28 in 113.23 seconds. The expanded role fixture reported 6,044 checks and two failures, both from incorrectly demanding an in-flight pitch-wheel matrix value in Legacy. Legacy intentionally bypasses that controller smoother; Circuit uses it. The fixture now checks the existing policy explicitly: Circuit in-flight values are intermediate, while Legacy/settled values are half-scale. Runtime and audio-comparison criteria are unchanged.
