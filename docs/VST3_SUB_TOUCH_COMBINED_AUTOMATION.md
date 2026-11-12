# Combined SUB touch queues with mixed rank sources

## Scope

This separate supported-input fixture exercises simultaneous touch speed, VCO
and VCF amount queues (lanes 24–26) while the two ranks select different sources.
It checks all six ordered differing pairs of PANEL (13), factory 0 and MEMORY
11, in both eight- and sixteen-voice capacities: twelve cases per rate/mode.
Original SUB sine is selected statically, with 12 Hz original base speed.
The inherited note workload uses two keys in eight-voice capacity and sixteen
keys in sixteen-voice capacity; this is not an eight-note saturation test.

The three queues have distinct ordinary finite trajectories. The unchanged
shared native renderer supplies supported note and poly-pressure events. Each
full case checks one-sample agreement with 7/64/256/1024/4096-frame partitions,
a positive automation witness, a positive pressure witness, exact zero-pressure
inertness, empty/no-queue identity and Legacy/PURE final-point flattening. The
unchanged 2e-6 audio threshold and exact 27-lane roster are retained.

No production code, contracts, shared renderer, existing fixtures or historical
captures are changed. This supplements the individual-touch and legacy-mode
gates; it does not replace them.

## Reproduction and evidence

Build `cs80_vst3_sub_touch_combined_automation_tests` with plugin tests enabled.
Its six full registrations cover 44.1/48/96 kHz and realtime/offline processing:

```sh
ctest --test-dir build -R '^cs80_vst3_sub_touch_combined_automation_[0-9]+_(realtime|offline)_tests$' --output-on-failure
```

The explicit `--compat-only` path emits no-queue and Legacy/PURE captures but
never claims the baseline supports the new modern timing. Registrations run
the full path without that flag. Exact commands, hashes and run outcomes belong
in `experiments/sub-touch-combined-mixed-sources/`; compilation alone is not a
runtime pass.

## Verified local result (2026-11-12)

All six direct full candidate runs passed: 72 cases, zero failures and zero
observed partition error. Aggregate automation witnesses ranged from
0.0356504 to 0.61175, and pressure witnesses from
0.0434618 to 0.645346. Every zero-pressure inertness,
empty/no-queue and Legacy/PURE assertion passed. Six compatibility-only runs
against the frozen 24-lane module passed; all 216 complete capture pairs are
byte-identical. No failure was discarded or retried.

The exact previously verified 27-lane production module was reused, with its
source tree checked unchanged. These were direct scoped-host executions, not
CTest runs or a new plugin build. CMake configuration passed and the generated
six full-mode registrations were checked separately. Commands, compiler/source/
host/module hashes, logs and per-file capture hashes are retained in
[`experiments/sub-touch-combined-mixed-sources/`](experiments/sub-touch-combined-mixed-sources/).

## Limits

Linux float32 only; fixed original sine and fixed ordinary pressure pattern.
No waveform or source switching, other source indices, legacy oscillator mode,
pressure-event/MPE expansion, hardware-fidelity, DAW/GUI, AU, float64 or CPU
qualification. A combined positive witness does not independently isolate each
lane; the existing individual-touch gate provides that separate coverage.
No broad, malformed-input or fuzz body is compiled into this fixture.
