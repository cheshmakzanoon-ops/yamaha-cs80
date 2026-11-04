# Waiting-key lifecycle regression

Scope: core MIDI allocation, based on `102b079`. This is a software event/state
correction, not a new hardware claim or a change to circuit equations.

## Defects

1. When all eight circuits were occupied, the source policies remembered excess
   held keys. Ordinary note-off and pedal release reconsidered those keys, but
   channel-specific All Sound Off (CC120) and All Notes Off (CC123) did not.
   A still-held key on another channel could remain silent despite free circuits.
2. Enabling the arpeggiator discarded directly played voices but kept their
   excess-key queue. After a key-up in arpeggiator mode, a later pedal release
   could revive the stale key. The channel-mode correction would expose this
   same stale state through CC120/123 as well.

## Correction

Channel-mode cleanup now finishes by running the existing waiting-key allocator.
It runs only after the target channel's voices and queued keys are handled, so
cleared keys cannot be reintroduced. Lowest-note priority, zero initial touch,
channel identity and pedal protection remain those of ordinary note-off.

Arpeggiator activation also clears waiting direct keys, matching its existing
choice to discard the direct-play gesture. This holds after disabling it again.
Legacy still does not remember excess keys. Global panic still clears all keys.

## Regression coverage

`cs80_kas_scanner_tests` covers CC120/123 under Legacy, source performance and
PURE CS-80; scan timing off/on; waiting-key priority and target-channel removal;
ordinary note-off after reassignment; pedal protection; global panic; and stale
key revival through CC64/120/123 both during and after arpeggiator operation.

The channel-mode regression failed 17 checks before its fix. The separate
arpeggiator lifecycle regression failed all six trigger/mode combinations before
clearing the queue. No host validator or DAW qualification is implied.

Final Release verification: seven focused CTest suites pass (core, KAS/scanner,
tuning/MIDI, parity, source performance, source-performance audit and free-running
voices). The expanded KAS suite passes 187 checks. An independent direct-source
rebuild and separate channel-boundary/arpeggiator probes also pass.
The final KAS suite also passes all 187 checks with AddressSanitizer and
UndefinedBehaviorSanitizer. Leak detection was disabled for this focused run;
this is not a leak-sanitizer qualification or a full project/host validation.
