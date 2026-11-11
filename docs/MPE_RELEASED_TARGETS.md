# MPE member targets after accepted release

Ordinary member-channel Pitch Bend, Channel Pressure and CC74 no longer
retarget an active voice after its accepted engine gate closes. This prevents
reuse of a member channel from changing the previous note's retained per-voice
bend, pressure and timbre while its release or damper-held sound continues.
Legacy and Circuit use the same eligibility rule.

This is a bounded MIDI/software correction, not complete MPE conformance or a
physical CS-80 model claim. In particular, shared modulation routes can still
change the sound of released voices.

## Target and cache contract

- The channel must currently be an actual lower or upper member, with MPE
  effectively enabled. This includes channel 1 in a full upper zone and channel
  16 in a full lower zone. The existing matching recipient must be active and
  have `held=true`. `pedalHeld` alone does not qualify.
- `held` is the engine's accepted gate, including generated arpeggiator gates,
  rather than a separate physical-key history. KAS controls received while a
  release is still queued remain eligible until that release dispatches.
- The event-set targets are retained. A smoother already moving toward a target
  continues normally after release; no sample value, envelope or filter is
  frozen or reset. A new voice age still starts its own controller smoothers.
- The [raw member cache](MPE_MEMBER_ONSET.md) continues to receive finite,
  clamped values even when no current voice is eligible. A later accepted
  onset uses the latest bend with its current effective range, pressure with
  its current curve/threshold, and the existing CC74 timbre path.
- Eligibility uses the current mode and channel role, not a new captured MPE
  flag. Disabling MPE or turning a member into a gap/manager uses the existing
  routing for that new mode/role, including retained voices. Existing captured-upper
  routing, zone negotiation, range behavior and cache invalidation remain.
- Conventional, PURE, gap and manager routes are unchanged. Manager controls
  can still affect their existing active-tail recipients; direct polyphonic
  pressure retains its existing note/channel routing. This increment concerns
  only the three ordinary member expression messages named above.

## Explicit reset and global exceptions

[CC121](CONTROLLER_RESET.md) remains an explicit reset. It resets the addressed
member's bend and pressure targets, including active released and pedal-held
voices, and their raw cache defaults. CC74 targets/cache are retained. Manager
CC121 keeps its previous scope without clearing independent member caches.
Pedal release, selector nulling, shared controls and PURE exceptions are not
changed. The reset calls private target-routing helpers with a reset exception;
it does not synthesize ordinary member events or reset the engine.

The latest Channel Pressure value remains a global modulation-matrix source.
An enabled CC74-to-ribbon adapter still updates the independent global ribbon.
These routes may affect a released voice even though its three member targets
are retained. The [shared pressure-to-speed bus](SHARED_PRESSURE_SPEED.md) still
admits force only from active held gates; the Circuit TWS nodes can retain
residual decay. No global-pressure combination model or whole-audio isolation
is introduced.

## Event ordering and sixteen voices

With scanning and ARP off, messages are handled at their existing event position.
At the same MIDI offset, a member event inserted before Note Off remains
eligible; the same event inserted after the accepted release is not. A
zero-velocity Note On follows the existing release route. Zero-frame callbacks
clamp event offsets to their boundary, so they do not demonstrate elapsed
separation between nominal offsets.

The two already-active instruments apply the rule independently in sixteen-voice
mode. It composes with [immediate repeated-note release ownership](SIXTEEN_VOICE_NOTE_RELEASE.md):
a later member message can update an older still-held primary onset without
retargeting a newer released extension onset on the same channel. No coherent
zone/expression handoff was added by this historical released-target change. The later [controller activation handoff](CONTROLLER_ACTIVATION_HANDOFF.md) now transfers the primary's retained controller/protocol state at activation.

Deferred/scanner and arpeggiated cross-instrument Note Off ownership remain
separate work. The local accepted-gate rule does not repair their broadcast
fallback or change their scheduling.

## Source and qualification

MPE v1.1 (M1-100-UM, 14 April 2022), §1.5.1, §§2.2.6–2.2.8, §2.4 and Appendix
A.4 distinguish released notes and require ordinary per-note member control to
stop after Note Off receipt, including damper/release tails. The accepted-gate
boundary and queued KAS exception above explicitly limit this implementation;
it does not claim receipt-time conformance for deferred events. The [official MPE page](https://midi.org/mpe-midi-polyphonic-expression)
links the [specification](https://drive.google.com/file/d/1QGjiK2QPPbii8YmES3vDEZlP63OGLiKr/view).
The locally verified PDF SHA-256 is
`c5f6727af065e454fe9bc4ff80f66414e8b25ffb6c1019ce7795ab4a89da1f1e`.
The PDF is not redistributed. CC121 is separately listed in Appendix E;
preserving the instrument's documented reset route is an explicit bounded
compatibility decision, not a claim of complete reset conformance.

The [experiment and evidence record](experiments/mpe-released-targets/README.md)
separates the old-code reproduction, new core and processor tests, unchanged-path
raw witnesses, compiler/sanitizer qualification, fault controls and independent
reviews. Its sixteen-voice audio checks establish finite, non-silent **combined**
processor output plus target-engine identity and state, not isolated extension
audibility. Eight-voice single-note and core paired tests provide isolated
non-silent tail continuations.

Source inspection finds only event-time eligibility checks and private reset
routing helpers: no new voice fields, heap allocation, locks, audio-rate loops,
parameter IDs, saved-state format, UI or circuit equations. No performance,
callback-deadline, external DAW/controller, GUI, hardware-fidelity or full-native
sanitizer qualification is claimed. Full MCM note termination, complete zone
reset, manager/member pressure and timbre combination and live active-note range retargeting remain open.
