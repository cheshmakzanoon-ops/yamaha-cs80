# Construct oscillator left-edge scratch only when needed
The active oscillator tick formerly default-initialized a 160-byte local
snapshot, even when no event used it. The tick now starts the snapshot's
lifetime at each original complete assignment, with no initialized union
member until that point. The control-edge, reset-start and physical/branch
edges all still generate their left and right nodes in the same places. Each
read is dominated by construction. The local object is trivially destructible;
no allocation, new oscillator state, changed event math, or omitted circuit
step is involved.

## Whole-plugin translated work
Pinned stock 0658d1b3 and the local change were built as Release Linux x86-64
VST3s with GNU 14.2. The separately rebuilt stock ELF matched the frozen
baseline exactly. The actual linked oscillator entry used 20 repeated
eight-byte stack writes per tick before, none after. [Measured counts](receipts.json)
include two identical stock runs and an exact one-callback additive control.

| Four 48 kHz callbacks in a real VST3 host | Guest instructions | Guest data writes |
| --- | ---: | ---: |
| Eight equal-velocity held notes | −0.8873% | −5.3882% |
| Eight varied-velocity held notes | −0.8810% | −5.3866% |
| Varied wake | −1.0460% | −6.3173% |
| Fresh quiet | +0.0202% | −0.0607% |

The guest count expands the x86 REP store. The stock held case executed
8,192 oscillator ticks in the four callbacks, writing 1,310,720 bytes to
the same repeatedly reused scratch storage. Counting each REP as one
instead leaves 56,167 fewer counted instructions in the whole held
comparison (about 0.23%). Neither count is a hardware timing measurement or
a claim about cache misses, memory bandwidth, or DAW CPU.

## Equivalence exercised
- [48 kHz native histories](public-parity.json): 112,640 real callbacks,
  7,208,960 stereo frames, corresponding FP observations and 113 component
  state snapshots matched byte for byte for the tested stock and candidate.
- [Other rates](cross.json): 44.1, 88.2, 96 and 192 kHz in real-time and
  offline modes; held, release, wake, active automation and a state round trip.
- [Loaded-module interruption checks](traps-native-modules.json): separate
  17,472- and 67,480-case grids matched private oscillator state, return or
  fault, FP observations and continuations. The latter included 2,680 faults
  inside event recording after partial residual updates.

The receipts are scoped to these binaries, fixtures and emitted code. No
native CPU-time comparison was performed for this change.
