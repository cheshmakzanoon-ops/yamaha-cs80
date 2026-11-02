# Component-only lever/S2 baseline refinement (2026-11-02)

Before production UI edits, root prospectively approved executing the unchanged
e4f791c95ce64b8d0df9dbfa331512af40a033c9 owner-direction and S2 assertions
through an unpeered component driver. Their frozen tests/manual inventory,
float bound and 351+102 assertion inventory stay unchanged. This does not
relabel the earlier compile-only result or satisfy native visual acceptance.

The driver creates no window/peer, calls no addToDesktop, performs no snapshot
or render capture, starts no display server and does not retry socket binding
or use an alternate display transport. It uses the normal JUCE message manager
and unchanged synthetic MouseEvent/key/geometry checks. Desktop-component count
must be zero before and after. The existing drag readout is only a child of its
unpeered top-level component. Process-local settings/cache paths stay in the
evidence workspace; no user desktop or persistent host setting is changed.

Compile with the matching e4f/b91 headers and project objects in the frozen
cs80-panel-s2/build-native-red build. The old source tree is
dbbefcc5ca0eeac5b741a90b916c0e79336246fd; do not mix new core-API headers with
those objects. Verify both worktrees' production/header identity and the exact
frozen PanelEditorTests.h bytes, record all linked object/archive hashes,
compile/link commands, JUCE commit and executable digest.

Source inspection predicts92 owner-direction failures and2 S2 adjacency
failures, with all other453 assertions passing. This prediction is not an
observed RED. Any different failures must be diagnosed rather than attributed
automatically to direction. No production fix may precede the actual baseline.

After a valid component RED, a separately registered bounded fix may address
physical gesture geometry and the proven TOUCH-before-KEYBOARD front-strip
order only. Parameter IDs/defaults/value meaning/state/host automation remain
unchanged. Signed detune and the portamento prose/diagram conflict retain their
original explicit evidence boundaries. Native screenshots, real input, visual
goldens, host integration and final-source qualification remain pending Actions.

## Prospective SUB-binding fixture correction after the first execution

The original unchanged run is preserved in `PANEL_COMPONENT_RESULTS_20261102.md`: owner directions351/92, S2 order102/12. The ten extra S2 failures come from looking for legacy SUB FUNCTION/SPEED IDs while the default editor binds those physical slots to the Original SUB IDs. This is an oracle binding error, not ten new placement defects. The original prediction remains failed and is not rewritten.

Before production edits, the corrected S2 oracle explicitly selects PURE off and each legacy/original SUB mode. It retains26 physical slots, enumerates the two applicable IDs independently, and checks positive component size plus every adjacent pair at both1600x1000 and1120x700. This gives204 checks with four prospectively expected genuine TOUCH-before-KEYBOARD adjacency failures: one per mode/size cell. No missing or hidden control is skipped or counted as a success.

The original27-control owner-direction function and351 assertions remain byte-for-byte unchanged. A separate54-check supplement applies its same thirteen assertions to original FUNCTION and SPEED, both directly constructed and rebound from MIX after hardware setup; the latter adds two successful-rebind assertions. Its prospectively expected16 failures are the four physical-direction assertions for each case. Rebinding from the opposite-direction MIX control independently challenges stale orientation. Both the unpeered driver and the native panel suite invoke this supplement.

Corrected component baseline prediction:609 checks,112 ordinary failures = original owner92 + original-SUB16 + S2 order4; all497 other assertions must pass. This is a prospective prediction, not an observed result. Production source stays exactly dbbefcc5; only tests/docs change. No native window, physical input, screenshot, rendering or DAW acceptance is implied.

The first fixture-edit transaction stopped before its second commit because it incorrectly looked for the owner-function invocation directly in `ui_snapshot.cpp`; the actual registration is through `runPanelEditorTests` in the test header. No corrected fixture executable ran before the complete committed registration.
