# Combined MSEG rates and queue ordering

Test-only qualification on the retained92-lane module. Production sources,
metadata, DSP and routing remain byte-for-byte unchanged. Two ordinary sessions
(all three routes on / all off) are created through public ModernLayerState and
processor serialization APIs, roundtripped and loaded natively as original bytes.
No raw XML editing, malformed input, editor/window or display workaround is used.

Three fixed unsynced poly-triggered always-loop bipolar shapes route to both-rank
pitch/cutoff/level at amount0.15. Capacity8 with two notes, manual/manual sources,
16,384frames and held notes through14336. Rate histories have independently
noncoincident interior breakpoints (37-frame offsets per lane); all lanes hold
zero octaves through1023. Physical targets stay within the existing octave range.

Six configurations (44.1/48/96kHz x realtime/offline),19 renders each: scalar1,
forward blocks7/64/256/1024, reverse parameter queue order at those4 blocks,
constant1, one lane's rate motion held constant in each of3 witnesses, and
paired moving/constant controls for no routes, zero amount and PURE.

Planned totals:114 finite PCM capture identities,24 scalar/block comparisons
using existing host tolerance,24 exact queue-order comparisons,18 per-lane held
positive witnesses using predeclared1e-5 minimum, initial1024-frame exact hold,
and12 verified native canonical-config readbacks. These are not baseline pairs.
No old/new module compatibility comparison is performed by this test.

Generator-only compilation reuses the frozen92 build's definitions and link
libraries. The native runner is standalone; the retained module/archive are not
rebuilt. Source and binary identities are verified before/after. New CMake targets
are source integration only, not qualified through CMake by this run.

Do not infer native DAW, cross-platform, full-range, shape/sync switching, source
mask, capacity16, CPU or hardware-fidelity coverage. Historical evidence,
contracts and suites remain untouched; historical suites are not rerun.

Independent review and explicit source freeze are required before runtime.
