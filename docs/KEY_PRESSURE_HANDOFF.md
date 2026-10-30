# On-screen pressure: onset ownership, not pitch ownership

The pointer gesture is intentionally limited: no direct-pointer pressure while
the effective arp runs, and no pressure when any active same-pitch voice has an
unknown/host origin. The latter includes other channels, pedal holds and release
tails, even with MPE on. This is conservative isolation, not unrestricted
on-screen pressure support. Host MIDI aftertouch remains unchanged.

## Publication and timing

The outer JUCE mouse-down/drag calls run inside a stack RAII capture scope. Only
the top scope on that thread, for that processor and expected channel/note, can
capture the first actual listener note-on attempt. Nested foreign scopes cannot
populate an outer scope; rejected/zero-velocity attempts remain invalid. Scope
unwinding restores its predecessor. No assumed current token or pitch lookup is
used. Same-key drags retain the original token; cross-key slides capture their
new JUCE onset, or clear the token if no onset was accepted.

The existing bounded MPSC keyboard FIFO now carries tagged pressure too. Its
non-audio producers serialize publication; its audio consumer uses only bounded
array scans and lock-free index/epoch atomics. Token identity is queue + epoch +
monotonic onset + channel + note. Onset exhaustion fails closed, not wraps. The
consumer filters old epochs even if a delayed producer publishes after reset.

This changes UI timing deliberately: pressure is FIFO/timestamp-interleaved with
UI notes and host MIDI rather than collapsed to one float per pitch at block end.
FIFO order preserves same-block initial pressure and distinguishes
pressure → off → same-pitch on. Host events retain their existing equal-sample
tie priority. Zero-frame callbacks consume the same ordered events.

## Audio ownership

FIFO acceptance is not proof a voice was allocated. Before/after snapshots of
the existing eight voice diagnostics bind an onset only to a newly held slot;
eight occupied held/pedal-held slots can refuse a ninth onset without assigning
ownership. Host allocations clear any old UI origin in reused slots. Current
gesture ownership is revoked by UI release/restrike, host onset/release,
CC120/123, resets and both arp edges. Attribution is cleared throughout effective
arp operation because its autonomous slot reuse cannot be attributed to a
particular pointer onset by these diagnostics.

Pressure requires the exact current token and its still-active, held slot. Every
active same-pitch slot must have a matching UI origin, including release tails.
This retains fresh UI restrike pressure beside prior UI tails without allowing
an old gesture to pressure the new strike. Engine pitch grouping can still apply
the fresh pressure to those old UI tails; no host MIDI or DSP law is changed.

After a host same-pitch event or an arp edge, old attribution stays revoked even
when the collision later ends; lift and make a fresh UI strike to capture a new
gesture. Any still-active host pitch alias continues to suppress that pressure.

Prepare/release/PANIC and effective render-policy changes reset the handoff epoch.
Policy changes discard the already-taken old UI batch before fresh host MIDI is
processed. Post-ack fresh gestures work; old same-key drags do not acquire new
ownership. A pressure flood uses the existing overflow/ack handshake. Recovery
only releases tracked UI keys: successful later host allocations revoke their
UI release bits, whereas refused host onsets must not strand accepted UI notes.
Genuinely overlapping UI/host voices on the same channel and pitch retain the
Engine's newest-note-off ambiguity: protecting a newer host strike can leave
an older UI voice for PANIC recovery. Arp keys also retain their existing shared
pitch/channel identity. This is not per-voice source-targeted note-off support;
unrelated host notes are not released. PANIC remains the explicit global
stuck-note recovery. No parameters, presets or state formats
are added or changed.

## Evidence boundaries

`KeyboardPressureAuditTests.cpp` exercises production queue/capture/ownership
helpers without JUCE. `test_keyboard_epoch_interleaving.py` pauses real header
publication after epoch validation for both notes and pressure; removing the
consumer filter must fail. `KeyboardPressureHostTests.h` exercises processor
dispatch, resets/modes, host collisions, arp restrictions, overflow and actual
editor mouse-down/cross-key drag. Native host/UI execution is required in CI;
local syntax checks alone are not a runtime pass.
