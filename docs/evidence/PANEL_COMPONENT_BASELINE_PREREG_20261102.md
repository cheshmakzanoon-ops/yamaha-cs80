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
