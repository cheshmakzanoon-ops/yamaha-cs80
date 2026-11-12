# Stored render-policy request context actions

SOURCE PERFORMANCE and PARITY ISOLATION now reuse `ParameterContextButton`
for right-click and Shift+F10 reset/MIDI learn/cancel/forget. Each binds its
actual stored APVTS boolean, matching the existing ButtonAttachment. Drawing,
layout and effective-policy precedence are unchanged.

These are two stored requests, not a single three-way enum. SOURCE context
reset changes only SOURCE; isolation is retained. Isolation context reset
changes only isolation; SOURCE is retained. Effective policy follows both
requests, with isolation taking priority. A reset can therefore change the
effective policy. LEGACY remains the existing composite action clearing both
requests, with no invented parameter or ambiguous MIDI target.

Learned MIDI remains inactive in PURE/isolation through existing processor
behavior. In particular, an isolation mapping cannot turn isolation back off
while that policy suppresses learned MIDI. Turn isolation off manually before
learning or using a controller. Tooltips explain this limitation. Reset remains
available in isolation; it uses the real parameter default and one balanced
host gesture, with no redundant gesture at the default.

The unchanged shared helper retains disabled-state and delayed-action lifetime
checks. No DSP, parameter IDs, serialization, shared helper or protected fader
implementation changes are included.

## Focused qualification

`cs80_render_policy_context_tests` constructs the actual Studio controls and
checks both attachment identities, ordinary direct pointer activation, context
pointer/Shift+F10 dispatch without selecting a policy, reset with the other
request in both states, balanced gestures, repeated-reset no-ops, learn/cancel
target identity, PURE learn suppression, disabled ancestors, LEGACY clearing
both requests, and retained action inactivity after editor destruction.

These headless checks do not establish native popup presentation, physical
keyboard routing, DAW automation recording or new MIDI conformance. Context
presentation is intercepted; menu actions are called directly. Neighboring
safe focused suites are run separately, not the broad screenshot smoke suite.
