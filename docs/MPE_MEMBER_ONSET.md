# MPE member-channel initial expression

This bounded repair retains member-channel pitch bend, channel pressure and CC74
received before an accepted note allocation. Legacy and Circuit now initialize
that note from the latest finite, clamped raw values. MPE must be effectively
enabled and the channel must currently belong to a lower or upper member role.

## Contract

- Bend is retained before range multiplication; pressure before its curve and
  threshold. Accepted onset uses the current negotiated/fallback member range
  and current pressure mapping exactly once. CC74 uses the existing timbre path.
- Retention is per MIDI-channel role, not per voice slot. Waiting keys, KAS
  allocations and arpeggiator steps consult the latest state at allocation.
- Engine reset, an effective MPE mode edge and a channel-role change invalidate
  retention. Same-role zone reconfiguration retains raw values and uses the new
  range at future onset. Existing sounding-note range behavior is unchanged.
- Existing CC121 routes reset the addressed member bend/pressure, retaining
  CC74. Manager resets do not rewrite independent member state.
- No retention is added for conventional, PURE, gap or manager channels, or for
  polyphonic pressure. No-message defaults remain zero, including CC74.
- This onset increment preserves event arithmetic, smoothing, manager routing,
  parameter/preset state and UI. No replay of global controller side effects
  is used to initialize a note. The later [released-target repair](MPE_RELEASED_TARGETS.md)
  narrows ordinary member-event recipients to accepted held gates while keeping
  this raw cache available for subsequent onsets.
- Sixteen-voice coverage applies when both instruments are already active.
  Inactive-extension activation is deliberately not a partial controller-state
  handoff. MIDI-learn consumption still takes precedence over learned CC74.

The change is a fixed-size, audio-owned cache used at event/allocation time.
Source inspection found no added heap allocation, locks or per-sample loop;
this is not a measured CPU or realtime-deadline claim.

## Evidence and limits

The [experiment record](experiments/mpe-member-onset/README.md) preserves the
pre-implementation [plan](experiments/mpe-member-onset/PLAN.md), source pins,
compiler/sanitizer results, behavioral fault controls, baseline compatibility,
native processor qualification and independent reviews.

MPE v1.1 (M1-100-UM, 14 April 2022), sections 2.2.6–2.2.8 and 2.4, motivates
initial-expression retention. The [official MPE page](https://midi.org/mpe-midi-polyphonic-expression)
links the [specification](https://drive.google.com/file/d/1QGjiK2QPPbii8YmES3vDEZlP63OGLiKr/view).
The downloaded PDF SHA-256 is
`c5f6727af065e454fe9bc4ff80f66414e8b25ffb6c1019ce7795ab4a89da1f1e`.
The PDF is not redistributed in this bundle.

This is **not full MPE conformance**. The later [released-target repair](MPE_RELEASED_TARGETS.md)
retains per-voice targets after accepted release, with explicit reset/global
exceptions; it does not provide whole-audio isolation. Full MCM note
termination/zone reset, manager/member pressure and timbre combination,
inactive-extension state handoff and live active-note range retargeting remain
separate. No GUI, external DAW, physical controller, plugin-format host,
performance, filter/memory activation or circuit-fidelity qualification is
claimed. Same-pitch sixteen-voice Note Off ownership is addressed separately by
the [bounded immediate release coordinator](SIXTEEN_VOICE_NOTE_RELEASE.md);
deferred-scanner and arpeggiator ownership remain open.
