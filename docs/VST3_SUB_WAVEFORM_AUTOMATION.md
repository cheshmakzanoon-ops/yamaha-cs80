# Continuous SUB automation with static waveform selection

## Scope

The separate `Vst3SubWaveformAutomationTests.cpp` fixture exercises the existing
original SUB falling saw (function 1), rising saw (function 2), and square
(function 3). Function selection is static initialization, not an automated
parameter. Only the existing continuous original-speed and VCO/VCF/VCA-depth
lanes carry queues. The 24-lane roster, production DSP and historical fixtures
and capture generation are unchanged.

Each configuration contains 18 cases: three functions, eight/sixteen voices,
and rank-source pairs PANEL/PANEL, PANEL/factory 0 and MEMORY 11/PANEL. Six
registered configurations cover 44.1/48/96 kHz in realtime/offline mode.

All curve offsets and normalized values are ordinary supported finite inputs.
A one-sample reference is compared to 7/64/256/1024/4096-frame partitions using
the existing `host-automation.json` audio threshold (2e-6). Each case also checks
a nonzero PCM effect against its no-queue render, a positive difference from
sine under otherwise identical queues/settings, empty/no-queue identity, and
Legacy/PURE final-point flattening. These checks observe the actual built VST3
module through the existing limited native protocol renderer.

## Reproduction and evidence

Build the `cs80_vst3_sub_waveform_automation_tests` target with plugin tests
enabled. Run only the six registered tests:

```sh
ctest --test-dir build -R '^cs80_vst3_sub_waveform_automation_[0-9]+_(realtime|offline)_tests$' --output-on-failure
```

Runtime results must identify the exact source, host executable and plugin
hashes. Fixture implementation or compilation alone is not a passing result.

## Verified local result (2026-11-12)

All six direct scoped-host runs passed: 108 scenarios, zero failures and zero
observed partition error. Modulation witnesses ranged from 0.0525242 to
0.745734; static-function differences from sine ranged from
0.0935069 to 0.904. All no-queue and Legacy/PURE checks passed.

CMake configuration passed and exposes six registrations. These measurements
used the separately compiled scoped host directly, not CTest, against the exact
previously verified production module. Production sources are unchanged; this
is not a new plugin build or a historical capture comparison. The inherited
combined/sine qualification remains its own checkpoint. Exact commands, hashes
and per-configuration logs are retained in
[`experiments/sub-static-waveforms/`](experiments/sub-static-waveforms/).

## Limits

This is Linux float32 continuous-queue qualification, not measured Yamaha
hardware fidelity, a DAW or GUI test, waveform-shape calibration, or discrete
function switching. Noise (function 4), external input (function 5), pressure
routing, other rates/source pairs and CPU deadlines remain outside this gate.
No broad malformed-input/fuzz fixture is compiled into this executable or run
by this six-test selection. The existing sine and compatibility gates remain
separate; this fixture does not replace them or create new historical captures.
