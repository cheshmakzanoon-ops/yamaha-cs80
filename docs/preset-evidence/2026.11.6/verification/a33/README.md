# A33 reproducible strike probes

Run from the repository root with the core library built from the source in `receipts.json`:

```sh
c++ -std=c++20 -O2 -Isrc docs/preset-evidence/2026.11.6/verification/a33/repeated-strikes.cpp build-preset/libcs80_core.a -o /tmp/cs80-a33-strikes
/tmp/cs80-a33-strikes
c++ -std=c++20 -O2 -Isrc docs/preset-evidence/2026.11.6/verification/a33/body-seed-control.cpp build-preset/libcs80_core.a -o /tmp/cs80-a33-body
/tmp/cs80-a33-body
c++ -std=c++17 -O2 -Isrc docs/preset-evidence/2026.11.6/verification/a33/engine-seed-piano-course.cpp -o /tmp/cs80-a33-course
/tmp/cs80-a33-course
```

The course probe is the existing `tests/PianoCourseGeometryTests.cpp` with its three frequency-measurement seeds replaced by rank-I accepted-strike ages 1, 2 and 3 (`0x9e3779b9`, `0x3c6ef372`, `0xdaa66d2b`). No production file is changed.

The `initial-*` logs retain two rejected diagnostic assumptions: a post-panic gap is not exactly zero at the output/filter stage, and a normalized shape difference of 0.001 is not a supported perceptual requirement. Final attacks subtract a cloned, same-state continuation without a new note. The final direct-body check distinguishes nonidentical from exactly repeated seed response; it makes no minimum-audibility claim. The tiny electric-piano difference remains explicitly reported.
