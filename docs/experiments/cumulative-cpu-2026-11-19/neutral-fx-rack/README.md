# Skip a proven-neutral modern FX rack

All three slot calls remain in their original order when a live tail needs
clearing, gain compensation changes, or any base or modulated mix operand is
not raw positive zero. When all six operands are positive zero and the rack
already has no live slots and the matching state is stable, the skipped scalar
operations change no sample, effect state, or exception flag.

The [matched whole-VST3 guest receipt](work.json) on the preceding filter and
meter/automation build covered fresh silence, equal and varied held notes, and
varied wake at 48 kHz / 64 frames. It saved 2.317% guest instructions in fresh
silence, 0.382–0.385% on eight held notes, and 0.279% in wake. Reads and writes
also decreased in all four dry scenes. Both repeated windows and the separate
fifth callback identities passed for both modules. The
[combined count since d62f3ae](combined-work.json) includes all three changes.

Ten independent real-plugin native histories matched byte for byte in the
captured PCM, public state, events, latency/tail and floating-point observations.
A separate directed Linux real-module postmix test matched all 336 cases per
arm, including 120 actual traps per arm and masked continuation afterward.
The in-tree test compares real wet/dry/re-enable lifecycles to an independent
forced-ordinary dispatch. These are work counts and exactness checks, not
native CPU timing or a physical CPU budget verification.

The [three active-rack controls](wet-work.json) separately recorded a small
cost for the additional guard: +0.131–0.143% guest instructions and
+0.55–0.58% guest writes with classic or modern effects live. This is a
dry-rack improvement, not an improvement on every patch. Six wet/dry and
reactivation histories (576 real 64-frame callbacks each) matched captured
public output, state and 63-/67-sample latency; audible wet controls verified
the effects had not been bypassed by the test itself.
