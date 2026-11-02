# Combined host/tuning/Scala checkpoint

Tested local commit `74f7c3e9db7f821cb982a7794e3a6488d812691a`, src tree `3de8680bda3d7107463708df1151965fbf72a09e`.

On the reviewed host/tuning integration, only three reviewed Scala commits were applied:

- `a24cb794244b2777e573832658f9ccc96151157d` → `36ce592` (tests/preregistration)
- `55726dcc8300d5795295687d67573263cd0d826f` → `114e425` (nominal float-frequency guard and CTest registration)
- `1152cceb682778b8e13687a7eac67df25ca5b896` → `74f7c3e` (original source-specific evidence)

Both root and the independent qualification reviewer checked the bounded source change. It reproduces the core's nominal 440 Hz `midiToHz` operation order before assigning the staged output table; it does not impose a new transposition-headroom policy or claim all downstream modulation/FP-mode safety.

The exact combined source built and passed **12/12 focused CTests** under the shared lock, 2026-11-02 12:53:06–12:53:37 UTC, GCC14.2 Release. CTest runtime:3.33 seconds. Raw log SHA256: `9bdff0533d40d99fd4068bd978240bc4ccd56fa34abc54cc3551a6acca691e0d`.

An independently executed actual guard-deletion mutant compiled successfully and exited1 with exactly six ordinary failures out of26 checks; accepted invalid cases reconstructed8,39 and128 zero/nonfinite core frequencies. No runtime error was counted as a semantic failure. Both mutant-positive controls and restored-original positive arrays retained SHA256 `a505b7fd85f10408eb3b7a947566dfcbaa6e49aa0086894548131dfea5f45ca2`. The original test passed26/26 afterward. `check-scala-mutant.py` reproduces this check from the repository root after building `build-integration/cs80_scala_float_pitch_bounds_tests`.

This is focused local integration evidence, not full-suite, native-host, audio, CPU or final release qualification. Previous evidence and registrations are not reassigned to this new source. Publication is held; main remains unchanged.
