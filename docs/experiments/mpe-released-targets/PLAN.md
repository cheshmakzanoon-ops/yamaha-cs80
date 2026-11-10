# MPE released per-voice targets: bounded plan

Base: reviewed `2ade213bc4dec34b04352063a72e97dac4cc5667`.

The deferred sixteen-voice release investigation found that a safe complete
fix requires a common dispatch boundary across independently rendered engines.
Ingress-time previews cannot account for later lower-pitch events in a scan,
and render traversal cannot establish global musical chronology. That scheduler
work remains separate; no shortcut is introduced here.

The next bounded product repair is MPE member expression after an accepted
voice release. Current bend, channel-pressure and CC74 handlers update every
matching active voice, including release and pedal-held tails. The retained
MPE v1.1 specification explicitly separates Released Notes from Active Notes
and ends per-note member control after Note Off, including sustain/release.
Source URL/hash are preserved in `docs/MPE_MEMBER_ONSET.md`.

## Contract fixed before implementation

- Ordinary Pitch Bend, Channel Pressure and CC74 on a current effective MPE
  member channel write per-voice targets only while the existing recipient is
  active with its accepted engine gate held (`held=true`). Pedal hold alone
  does not qualify; generated ARP gates use this same existing field.
- Preserve existing recipient selection, range/curve arithmetic and smoothing.
  This is target retention, not a snap/freeze of an instantaneous smoothed value.
- Continue raw member-cache updates even with no eligible voice, so a later
  accepted onset uses the latest controls. Both zones and full-zone endpoint
  members use the same rule.
- Preserve manager, gap, conventional and PURE paths; do not retag voices,
  implement manager/member combination, or change polyphonic-pressure routing.
- Preserve CC121's existing explicit reset behavior through a private reset
  exception for bend and pressure, including active released/pedal-held voices.
  Raw bend/pressure defaults, retained CC74, shared controls and pedal handling
  remain unchanged. CC121 is not one of the three ordinary MPE control messages.
- Eligibility follows current effective mode/role. Disabling MPE or changing a
  member into a gap/manager restores its existing route for old voices.
- The boundary is accepted release (`held=false`), not queued MIDI ingress.
  KAS may continue to admit controls until its queued release dispatches. ARP
  generated gates own this boundary; future steps consume the retained cache.
- Shared/global effects remain: the explicit global Channel Pressure matrix
  source and CC74 ribbon adapter can still affect tail audio. No complete
  whole-audio isolation, MIDI/MPE conformance or hardware claim is made.

## Qualification gates

- Reproduce the current defect with untouched, matching old headers/library
  or processor objects. Keep real behavioral failure distinct from setup errors.
- Test each dimension on held, released and pedal-held notes, both zones and
  endpoint members; overlapping same-channel notes, unison and Sustain I/II.
- Assert later onsets retain new raw values while old targets stay unchanged;
  verify in-flight smoothing and non-silent continuation against an independent
  no-later-event engine with unrelated global routes disabled.
- Protect manager/global/gap/PURE behavior, current mode/role transitions,
  malformed inputs and nonzero CC121 reset of held/released/pedal-held targets.
- Exercise deferred pre/post-dispatch and ARP gate/future-step boundaries.
  Test global matrix/ribbon exceptions rather than hiding them.
- Actual processor MIDI ordering, zero-frame callbacks, zero-velocity Note On
  release and both already-active sixteen-voice instruments.
- Field/eligibility/cache/reset omission faults must fail with complete normal
  test output, not crashes or compilation errors. Retain baseline compatibility
  for genuinely unchanged paths, four compiler profiles, full-core sanitizer
  and complete core suite; qualify native scope separately.
- Independent source, fixture and evidence review. No push before coordination.
