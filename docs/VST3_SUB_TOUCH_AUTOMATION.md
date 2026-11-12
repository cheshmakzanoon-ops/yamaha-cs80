# Sample-offset SUB touch amounts

## Scope and compatibility

The existing `P_subAtSpeed`, `P_subAtVco` and `P_subAtVcf` controls append in that
order after the unchanged 24-lane prefix, bringing modern VST3 automation to
27 lanes. These are amount controls, not pressure events. Parameter indices,
host IDs, AU version hints, conversion, smoothing, pressure ownership and DSP
response laws are unchanged. No new parameter or modulation source is added.

All three controls follow the existing global-control path. Touch speed and
VCO amount precede the rank-parameter range; touch VCF amount is at the first
extension index, beyond that range. Consequently the rank-source exclusion in
`applyHostAutomationAt()` does not suppress their stored global values. The
existing control-frame publication delivers their physical values to the active
engines before rendering. PANEL/factory/MEMORY authority is not changed.

The original 18-, pan 20-, SUB 22- and depth 24-lane JSON contracts remain
byte-for-byte unchanged. `host-automation-sub-touch-v1.json` separately declares
the exact 27-lane capability. Historical depth and static-waveform fixtures
continue to check their 24-lane capability as a preserved prefix; their numeric
criteria, audio cases and capture generation remain unchanged. The new fixture
checks the complete production roster and appended order.

Legacy/PURE retain final-point-at-block-start timing. No-queue behavior, AU and
Standalone timing remain outside the new VST3 capability. Fixed-size lane
storage grows by three; the extension adds no queue copy or DSP path. It makes
no allocation-performance or CPU-deadline claim.

## Native verification gate

`cs80_vst3_sub_touch_automation_tests` uses the limited supported-input native
host with ordinary finite poly-pressure events. It isolates each new amount,
checks against a one-sample reference using the existing 2e-6 audio threshold,
and exercises 7/64/256/1024/4096-frame partitions, eight/sixteen voices and
PANEL/factory 0/MEMORY 11 source selections. Six registrations cover
44.1/48/96 kHz in realtime/offline mode. No malformed-input or broad suite is
part of this selection.

The gate also requires a nonzero modulation witness and empty/no-queue identity.
Its no-queue and Legacy/PURE captures are intended for byte comparison against
the frozen pre-feature module. The separate `--compat-only` invocation emits
those compatibility observations without claiming the old module supports the
new modern timing. Full candidate qualification never passes that flag.

## Verified local result (2026-11-12)

The candidate passed all 24 selected CTests in one run: 108 new touch cases,
108 static-waveform cases, 144 depth cases and 72 speed/VCO cases. The touch
cases observed zero partition error. Automation effects ranged from 0.0019374
to 0.658937, and pressure witnesses from 0.00291219 to 0.651497. Each case also
proved exact zero-pressure automated/no-queue equality: touch amounts remain
inert without pressure. No candidate failure was discarded or retried.

The same final touch host ran all six compatibility-only configurations against
the frozen 24-lane module. All 324 no-queue/Legacy/PURE touch captures matched
the candidate byte-for-byte. The prior 246 depth and 246 speed/VCO captures also
matched, for 816 exact pairs. One full 48 kHz realtime run against that baseline
exited 1 with exactly 18 expected partition failures and no other failures,
demonstrating the absent timing capability. This negative control was not
retried or counted as a pass. Earlier compatibility preflight evidence is
retained separately and is not the full candidate gate.

Exact source/module/host hashes, CTest logs, final baseline commands and all
paired capture hashes are retained in
[`experiments/sub-touch-automation/`](experiments/sub-touch-automation/).

A separate [legacy-oscillator sine follow-up](VST3_SUB_TOUCH_LEGACY_OSCILLATOR.md)
qualifies that static mode without enlarging this original-mode checkpoint.

## Limits

This is a software protocol extension, not measured Yamaha hardware parity,
pressure-event or MPE expansion, native GUI/DAW qualification, AU scheduled
ramps, or a CPU target. Other pressure patterns, source combinations, legacy SUB oscillator mode, waveform
changes, float64 and untested rates remain separate scopes. Earlier sine,
combined-depth and static-waveform results remain their own pinned checkpoints.
