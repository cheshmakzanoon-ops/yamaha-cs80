# MTS realtime semantics — focused results, 2026-11-02

See [the prior registration](MTS_REALTIME_SEMANTICS_PREREG_20261102.md) for the
public protocol evidence and unchanged acceptance bounds. The production repair
is confined to `Engine::handleSysEx`: an accepted 7E setup message updates the
table for future notes without immediately retuning sounding voices. Accepted
7F messages retain live retuning. The separate live `setMtsTuning` path is not
modified. This is not complete channel/bank/checksum or all-DAW qualification.

## Exact chronology and outcomes

| Stage | Identity | Result |
| --- | --- | --- |
| Original registered driver | `c6350d119d5c967e6b5e54380762e0010e5a89ba`, src `7157d3a2ee6e0ec57e7413a22f203ab981ed377d` | 288 checks, exactly 24 failures: all four NRT formats × both engines × three rates retuned held notes incorrectly |
| Narrow production repair | `ed5f6cd9ba73509ec88c1ffe05c780c15dbd48f4`, src `a1424ee1b4dc26d125805c9c482502ab2c790efc` | Fresh Release Engine object: 288/288; original tuning/MIDI suite: 81/81 |
| Fully instrumented core | same repaired source | Fresh Engine and FactoryPresets objects, ASan/UBSan: 288/288 |
| Supplementary positive controls | `c4bed666224260e994ec29a7d57def50d4d32c72`, same src | Original 288 plus 144 live-setter/RT-SysEx/PURE-preservation checks: 432/432 Release and 432/432 ASan/UBSan |
| Fresh complete baseline replay | original `c6350d1` source materialized separately after the repair | Fresh Engine and FactoryPresets: exactly the same 24/288 failures; expected exit 1 |
| Composed parser regressions | repaired source | KBM boundaries 61/61; counted MTS atomicity 46/46; original tuning/MIDI 81/81 |

The first RED used the existing PR6 core objects (Engine.cpp/Engine.h themselves
were unchanged before this repair). Those objects predate the separate inline
parser-bound fixes, which do not change these complete, valid packets. This
reuse is **not** represented as a fresh exact-composed-source baseline. The
explicit later replay recompiles both core objects from the exact registered
source and reproduces all 24 failures, removing that provenance ambiguity.
The fresh FactoryPresets object matches the reused original object byte-for-byte.

All executions were serialized with `cs80-heavy-check.lock`; none is a CPU
benchmark. Raw logs and SHA-256 digests are in
[`mts-realtime-semantics-20261102`](mts-realtime-semantics-20261102/).
The earliest RED ran 11:30:17–11:30:22 UTC; original positive 11:31:43–11:32:03;
full-core instrumentation 11:34:02–11:37:00; supplementary positives
11:44:23–11:44:42; fresh baseline and parser replay 11:46:47–11:47:14.

## Reproduction and limitations

GCC 14.2.0, C++20. Release objects and drivers use `-O3 -DNDEBUG`; instrumented
objects and driver use `-O1 -g -fsanitize=address,undefined
-fno-sanitize-recover=all -fno-omit-frame-pointer`. Include `src`, compile both
`src/core/Engine.cpp` and `src/core/FactoryPresets.cpp`, and link
`tests/MtsRealtimeSemanticsTests.cpp` against those matching objects. CMake target
`cs80_mts_realtime_semantics_tests` is registered alongside the tuning tests.
For the original tuning suite define `CS80_TUNING_DATA_DIR` to the checkout's
`tests/data/tuning` directory, as its existing CMake target does.

ASan runs explicitly use `ASAN_OPTIONS=detect_leaks=0` because this container's
ptrace restriction prevents LeakSanitizer operation. There is no leak-clean
claim. The compiler's variable-tracking fallback note is retained in the raw
instrumentation log; it is not a sanitizer failure.

No parameter range, state format, render-loop timing, original tolerance or
preset was changed. Focused frequency/ownership checks do not establish audible
retuning-transition quality, realtime deadline safety or full host acceptance.
Whole-source native/numerical qualification must be registered for the later
composed source; the original frozen audit registration remains unchanged.
