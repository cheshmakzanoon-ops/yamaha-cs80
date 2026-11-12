# Sample-offset VST3 original SUB controls

The later [VCF/VCA depth extension](VST3_SUB_DEPTH_AUTOMATION.md) appends two
lanes without changing this capability contract or its historical evidence.

## Bounded capability

Two lanes append to the unchanged twenty-lane roster: `P_subOriginalSpeed`
(original-panel SUB speed) and `P_subVco` (SUB VCO depth). Only modern VST3
queued automation changes; AU remains block-rate, and Legacy/PURE retain their
existing final-point-at-block-start policy. Legacy SUB speed, other SUB depths
and unrelated parameters are not newly supported. Original speed affects the
original SUB mode; switching waveform/mode is still ordinary block-rate state.

Both added controls are global, so PANEL, factory-tone and MEMORY selection
do not mask them. No selected-source bypass exception, parameter ID, DSP law,
smoothing, queue storage or wrapper-format selector was changed. Fixed-size
lane arrays grow by two, using the existing borrowed native queue mechanism.
This does not claim a new CPU or allocation-performance result.

`host-automation.json` and `host-automation-pan-v1.json` remain byte-identical.
The separate `host-automation-sub-v1.json` registers the 22-lane capability.
The historical scalar fixture retains its exact twenty-lane prefix admission;
the new fixture independently asserts all twenty identities/order and the two
new identities. Existing numerical tolerances are not relaxed.

## Actual-module supported-input verification

A pre-change Linux VST3 module from `d46ec8d9` was built and preserved before
editing the lane roster. The same dedicated native host executable ran against
both modules. It reuses the ordinary module/process renderer but excludes the
historical suite's other feature/boundary fixtures and main. Only finite,
in-range ordinary queues and self-generated MIDI were supplied.

At 48 kHz realtime float32, twelve scenarios cover each new lane separately,
eight/sixteen-voice settings, and both ranks selected to PANEL, factory source 0
or MEMORY source 11. Each 4096-frame timeline contains no initial point at zero,
multiple ramps, consecutive-sample knots and final-sample publication, spanning
note onset/release. An independently interpolated one-sample native-process
reference is compared to partitions of 7, 64, 256, 1024 and 4096 frames using the
unchanged 2e-6 audio limit. Three valid scalar-curve fixtures retain the original
normalized-curve limit.

The pre-change module completed with twelve expected partition failures. The
candidate completed with zero failures and zero observed partition error in
all twelve scenarios. Every scenario had a nonzero modulation-versus-neutral
PCM witness. No-queue and empty queues matched; Legacy/PURE native queues
matched their flattened forms. Five original-twenty-lane combined trajectories
also retained the old numerical limit.

All 41 retained compatibility captures were byte-identical between modules:
12 no-queue, 24 Legacy/PURE and five original-twenty-lane modern trajectories.
This establishes those recorded windows, not all sounds or a universal offline
render guarantee. Baseline/candidate modules, executable hashes and raw logs
are retained with the local verification evidence.

## Expanded rate/mode qualification

The dedicated helper now accepts optional 44.1/48/96 kHz and realtime/offline
arguments. Defaults remain 48 kHz realtime. Five additional CTest registrations
use separate capture directories, leaving six supported-input configurations.
No production code or tolerance changes are part of this expansion.

One retained run per configuration completed all 72 native SUB scenarios with
zero failures and zero observed partition error. The smallest modulation versus
neutral PCM difference was 0.051143. The same helper run against the preserved
pre-feature module completed each configuration with exactly twelve expected
partition failures; none was discarded or rerun into a passing baseline.

All 246 compatibility captures (41 per configuration) matched the corresponding
pre-feature module bytes exactly. Comparisons are within the same rate/mode,
not across sample rates or realtime/offline: a 4096-frame timeline has different
physical duration at each rate, and offline processing uses its existing
quality policy. This does not erase earlier unrelated offline limitations.

## Limits

No real DAW, native GUI, sample rates outside this matrix, float64, mixed
rank-source selection, program-boundary or new CPU/deadline qualification is
claimed by this bounded gate. Historical larger-suite results remain historical;
the broad native suite and malformed-input tests were not run for this change.
