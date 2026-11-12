# Opt-in native CC1 fixture (bounded Linux qualification)

This test-only executable connects the VST3 edit controller for the entire audio
render, discovers channel-one/event-bus-zero CC1 through `IMidiMapping`, and
supplies its normalized mapped parameter points through `IParameterChanges`.
`Instance` owns the connection and releases it after processing stops.

The historical renderer, production DSP, 81-lane automation roster, and UI are
unchanged. This fixture does not claim support for sample-accurate mod-wheel
parameter automation. It uses only finite normalized CC1 values 0 and 1, normal
notes, and supported 44.1/48/96 kHz realtime/offline processing.

## Bounded verification completed

Built only `cs80_vst3_cc1_fixture_tests`, then ran exactly its six registered CTest
cases. Each case renders blocks 1, 64, and 256, five fresh instances each (90
renders total). Compare the unchanged renderer with opt-in disabled byte-for-byte;
compare connection-only and connected CC1 zero against that baseline; compare a
held-note CC1 sequence (0 at frame 0, 1 at 1024, 0 at 2048) against zero, requiring
a byte-exact pre-change prefix and positive finite output difference specifically
during frames [1024, 2048). All complete renders are checked for finite samples;
byte-exact comparisons use `memcmp`, including the sign bit of zero. The point positions
are deliberately aligned across the tested partitions, not a sample-offset or
partition-invariance qualification. No broad host, malformed-input, scanner, GUI,
or performance benchmark suite is part of this plan.

If controller setup or a gate fails, report that failure and inspect the fixture
before claiming it works. Build passed. All six cases passed (90 renders, zero failures). The 54 baseline
comparisons and 18 onset-prefix comparisons were byte-exact. All 18 held-interval
positive-effect gates passed. See `experiments/cc1-fixture/native-ctest.log` and
`sha256.json` for captured output and tested identities. This is Linux-only
fixture qualification, not GUI, DAW, hardware-fidelity, or mod-wheel automation
qualification. Historical broad suites were not rerun.
