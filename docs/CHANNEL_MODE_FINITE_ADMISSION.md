# Non-finite channel-mode event admission

Base: published `dc086d2`.

`Engine::controlChange` documented non-finite values as non-events but dispatched
CC120 (All Sound Off) and CC123 (All Notes Off) before that validation. A NaN or
infinite value could silence valid notes. Validate finiteness at entry before any
channel-mode state change. Finite payload semantics, channel-zero application
panic, MIDI channel scope, sustain handling and direct `allNotesOff` are unchanged.
This is defensive admission of the core's float event API; MIDI wire bytes do not
encode NaN or infinity.

Regression: both legacy and circuit policies, CC120/123, channel zero and channel
2, and NaN/positive infinity/negative infinity. Each compares held state and
bit-exact continuing audio with an untouched engine, then verifies that a valid
command still releases the target and preserves channel scope. Existing reset,
MPE, sustain, selector and ribbon tests run unchanged.

The expanded controller test linked against the pre-fix core reports 3,705 checks
and 48 failures. Only scoped core tests are used here; no native plugin/host,
real-time performance or full-suite qualification is implied.

A fresh Release build of the corrected core passes all 3,705 controller checks
with zero failures. Both targeted CTests pass: controller reset and tuning/MIDI
(the latter retains 22,321 checks). `git diff --check` passes.
