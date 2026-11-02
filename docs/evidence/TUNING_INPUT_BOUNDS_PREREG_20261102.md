# KBM input narrowing — preregistration, 2026-11-02

Before implementing the guard, test the current PR6 production parser with
signed 64-bit header values whose low 32 bits equal ordinary valid values.
The external integers must be checked against the already-supported header
ranges **before** narrowing to `int`. Likewise, a mapping degree larger than
`INT_MAX` cannot be silently stored as another degree or an unmapped sentinel.
Any rejection must preserve every member of the caller's existing mapping.

The fixture uses positive and negative `2^32` shifts independently in all five
integer header fields, five overflowing mapping degrees following an otherwise
valid entry, and exact controls at ordinary values and `INT_MAX`. Existing
header bounds and explicit `x` entries remain unchanged. This is an input
representation correction, not a new DSP tolerance or a change to normal tuning.

Scala's [public keyboard mapping specification](https://www.huygens-fokker.org/scala/help.htm#mappings)
defines MIDI-note header fields and scale-degree entries. Its mathematical
degree domain is broader than this implementation's `int` representation;
silently wrapping a large degree is not a defensible approximation. This
focused correction does not claim full unrestricted Scala-domain support.
Existing negative-degree/formal-octave limitations and downstream extreme-Hz
validation are separate follow-up scopes, not hidden by this test.

Required before/after evidence: the focused parser executable, then the original
tuning/MIDI suite; ASan/UBSan coverage where available; unchanged valid fixtures;
exact source and command records. Native cross-platform CI must still run on
the composed reviewed source. Original source-locked audit requests stay frozen.
