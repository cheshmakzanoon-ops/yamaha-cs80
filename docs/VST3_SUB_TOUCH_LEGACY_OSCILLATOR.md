# SUB touch automation with the legacy oscillator selected

## Scope

A separate compile-time variant of `Vst3SubTouchAutomationTests.cpp` selects
legacy SUB oscillator mode statically (`sub_original_mode=0`), with legacy
waveform 0 (sine) and a fixed 12 Hz base speed. This oscillator choice is not
the Legacy render policy: the full automation assertions still run in modern
Source Performance policy, while separate compatibility checks exercise
Legacy and PURE final-point behavior.

The variant uses the same existing touch amount lanes, ordinary finite
poly-pressure, one-sample references, partition checks, positive witnesses,
zero-pressure inertness and compatibility captures as the original-mode gate.
Full legacy-variant cases additionally require a positive PCM difference when
only the mode selector is changed to original mode; this witness is not run in
the original variant or compatibility-only path.
It retains 18 cases per rate/mode: three touch amounts, two voice capacities
and PANEL/factory 0/MEMORY 11 source selections. Six configurations cover
44.1/48/96 kHz and realtime/offline processing.

No production code, roster, parameter contract, shared renderer or original-mode
fixture behavior is changed. The extra variant supplies its own initial oscillator parameters,
log label and mode-selection witness. Original-mode capture names remain unchanged;
new registrations use separate `sub-touch-legacy-automation-capture-*` folders.

## Reproduction

Build `cs80_vst3_sub_touch_legacy_automation_tests` with plugin tests enabled,
then select only its six registered full tests:

```sh
ctest --test-dir build -R '^cs80_vst3_sub_touch_legacy_automation_[0-9]+_(realtime|offline)_tests$' --output-on-failure
```

The existing CLI and explicit `--compat-only` mode are retained. The registered
tests never pass that flag. Compatibility-only observations must not be called
a full automation pass. Runtime claims require exact source, executable and
module hashes; compilation alone does not establish a passing result.

## Verified local result (2026-11-12)

All six direct full runs passed: 108 cases, zero failures and zero observed
partition error. Automation effects ranged from 0.00210884 to 0.679666;
pressure effects from 0.00317124 to 0.660846; and mode-selection witnesses from
0.00115828 to 0.789577. Exact zero-pressure inertness, no-queue identity and
Legacy/PURE flattening also passed. All 324 compatibility captures matched the
frozen 24-lane module byte-for-byte using the same legacy-variant host.

The original-mode standalone executable is byte-identical to its prior build,
so its previous behavior and evidence remain unchanged. Production code and
all 18 existing tolerance contracts are unchanged. CMake configuration passed
and exposes six new registrations; the observed executions used the compiled
host directly, not CTest. No new production module was built for this step.
Exact commands, hashes and logs are retained in
[`experiments/sub-touch-legacy-oscillator/`](experiments/sub-touch-legacy-oscillator/).

## Limits

This gate does not switch oscillator modes or waveforms during processing. It
does not expand legacy speed automation, test other legacy waveforms, qualify
arbitrary pressure patterns or source pairs, or establish hardware fidelity,
DAW/GUI behavior, float64, AU scheduling or CPU budgets. The original-oscillator
gate and its historical captures remain separate checkpoints. No broad or
malformed-input test bodies are compiled into this scoped executable.
