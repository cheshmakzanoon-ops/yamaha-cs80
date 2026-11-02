# Scala nominal float-frequency bounds — results, 2026-11-02

The preregistered input cases reproduced a real acceptance/atomicity defect at
`a24cb794244b2777e573832658f9ccc96151157d`, src
`a1424ee1b4dc26d125805c9c482502ab2c790efc`: **6 of 26 checks failed**.
The accepted 2400-cent, 3600-cent and tiny-reference tables respectively yielded
8, 39 and 128 nonpositive/nonfinite frequencies through the actual core
`midiToHz`, despite finite stored pitches. Each also replaced the caller's table.

The repair at `55726dcc8300d5795295687d67573263cd0d826f`, src
`6c66a20d33ffb5b1aabdd6fb9d9a49d28fba4065`, checks the same nominal float
operation order before assigning the staged table. It does not change parsing
syntax, valid-table values, shipped programs, tuning reference semantics, or
any existing acceptance bound. It does not add a transposition-headroom policy.

Under the shared heavy-check lock:

- New fixture: **26/26 Release and 26/26 ASan+UBSan checks passed**. Leak detection
  was disabled because this workspace's ptrace restrictions do not permit the
  inherited leak-sanitizer execution path; no leak qualification is claimed.
- Five positive tables (12TET, just-five, tritave, negative interior degree,
  nonmonotonic degrees) have byte-identical 640-float arrays before/after, and in
  the sanitizer replay. SHA-256:
  `a505b7fd85f10408eb3b7a947566dfcbaa6e49aa0086894548131dfea5f45ca2`.
- A **fresh exact-source CMake/Ninja core build**, not cached original engine
  objects, passed all five selected CTests: Scala26, KBM61, counted-MTS46,
  realtime-MTS432, and original tuning/MIDI81 checks. The CMake positive array
  has the same hash. This verifies the new target's actual CTest registration.

Raw logs and their SHA-256 inventory are in
[`scala-float-frequency-20261102/`](scala-float-frequency-20261102/).
The shell UTC records identify runs at 12:25:14–12:25:17 (RED),
12:31:06–12:31:19 (Release/sanitizer), and 12:33:27–12:33:59 (fresh CMake).
CTest's unedited `LastTest.log` displays the container's AST timezone; do not
reinterpret those local clock labels as UTC.

This is a narrow file-input safety result, not complete Scala/KBM conformance,
all downstream-modulation safety, whole-engine numerical qualification, or a
native plugin/DAW result. Original whole-source audit locks are unchanged.
The composed source needs its own later qualification. Remote publication is
held by root pending the user's project test/evidence approval.
