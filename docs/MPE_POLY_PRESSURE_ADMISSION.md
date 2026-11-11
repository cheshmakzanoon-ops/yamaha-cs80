# Native MPE Polyphonic Key Pressure admission

MPE v1.1 Table 7 (page 30) explicitly tells receivers to ignore Polyphonic Key Pressure on member channels. Table 5 also prohibits member-channel reception. Section 2.2.7 describes Channel Pressure as the member pressure message. The retained official specification and source are identified in [MCM defaults](MPE_CONFIGURATION_BEND_DEFAULTS.md).

Previously, the native processor dispatched every Polyphonic Key Pressure message to the internal per-key API, including MPE members. This could overwrite member Channel Pressure, change released-note pressure, or replace pressure supplied by the on-screen keyboard.

The processor now rejects only native MIDI Polyphonic Key Pressure when effective MPE is enabled and the addressed channel currently has a lower-member or upper-member role. The role is inspected at event dispatch, so preceding MCM messages in the same block take effect immediately. Channel 1 and channel 16 are not assumed to be managers: either can be a member of the opposite zone.

Manager, unassigned-channel, conventional and effective-MPE-off behavior is unchanged, including PURE's existing effective-mode masking. Channel Pressure remains available. The public core `Engine::polyPressure` API and the token-authorized on-screen keyboard path remain unchanged. This is an intentional host-MIDI behavior correction, not a redesign of pressure combination or complete MPE conformance.

## Regression evidence

On source `a61cb206`, the actual-processor fixture ran 2,472 checks with 160 expected failures: 96 held-member rejection checks, 32 released-tail rejection checks, 16 UI-pressure protection checks and 16 same-offset entering-member checks. All positive controls passed. With the native ingress guard, the same fixture passed 2,472 checks with zero failures (registered CTest: 3.03 seconds).

The matrix covers Legacy/Circuit, eight/sixteen voices, zero/rendered blocks, lower/upper members and managers, channel 1/16 reversed member roles, gaps/disabled zones, MPE-off and PURE, valid Channel Pressure, unaffected pitches/channels, future-onset state, released tails, extension-engine ownership, actual UI pressure tokens and same-offset role changes. It is a direct processor test, not external DAW/controller or physical UI qualification. Existing core per-key-pressure and keyboard handoff regressions are retained.

The affected native build completed 15 steps. The selected core/native integration passed 31/31 in 119.84 seconds, including all six VST3 rate/mode configurations, the core poly-pressure routing test and native keyboard handoff. Independent source/test review found no blockers. A completed independent repeat passed the same 31/31 tests in 123.79 seconds, and an additional focused pressure/core/allocation repeat passed 3/3 in 5.59 seconds. The first independent attempt was interrupted by execution-environment loss after 15 completed passing tests, without a suite result; its log was preserved separately before restarting the unchanged source. Historical intermittent offline-render failures remain documented elsewhere; this change does not explain them or establish universal absence. No CPU/deadline or full MPE qualification is implied.
