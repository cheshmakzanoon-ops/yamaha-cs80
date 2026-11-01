# Current-sample modern modulation of VCA coupling tails

2026-11-01. Baseline audio: `b75b4d653b6ed6f0aa5ef8ee99b670d83e975250`.
This is a modern software integration question, not a Yamaha circuit law.

Independent review found that the newly connected inactive VCA tail consumes
`modern.rankOffsets()` before the old decimated `processVoice` call. A live
matrix-to-pan route can therefore use an earlier sample's offset; a poly MSEG
still advances only at the former silent-voice cadence. Pure and the neutral
default have no active matrix and are unaffected by that code path.

The preregistered contract is
`tests/parity/tolerances-v2/vca-tail-modulation.json`. No candidate behavior or
new witness has been evaluated at this registration. The existing original
phrase score criteria, capacitor recurrence, idle-clock bounds and Legacy/V1
compatibility requirements remain unchanged.

The proposed narrow repair computes current tail pan before mixing, only when
a source-policy inactive voice has nonzero stored coupling and a live pan
destination. The original idle deadline still owns note identity, per-note RNG,
mono triggers and global newest-voice publication. Additional tail samples may
advance used poly MSEGs and evaluate offsets, without repeating those ownership
operations. The later scheduled call must not tick a second time.

One MSEG continues to serve all its routes. If a poly MSEG drives pan and another
destination, its corrected elapsed-time phase also affects that other route at
its scheduled evaluation. A separate cloned pan runner would create two
inconsistent versions of one modulation source. No-pan configurations retain
their existing path; Pure and neutral defaults must remain exact.

The registered witnesses cover current raw rank mix, zero observer effect,
full-rate poly elapsed time, both ranks/policies/rates/qualities, global selection
timing, late matrix activation before note identity was latched, and subsequent
note-RNG ordering. Production changes remain pending qualification of the prior
VCA audio checkpoint and its separate published V2 pin.
