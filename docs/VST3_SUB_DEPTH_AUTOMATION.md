# Sample-offset SUB VCF and VCA depth

## Scope and compatibility

`P_subVcf` and `P_subVca` append after the unchanged 22-lane prefix, bringing
modern VST3 automation to 24 lanes. Together with the previous original-panel
speed and VCO-depth lanes, this covers the three continuous SUB depth controls.
It does not add the pressure-dependent amounts, legacy SUB speed or discrete
waveform/mode controls. Existing normalized conversion, smoothing and DSP laws
are unchanged. The added depths are global controls and remain live with PANEL,
factory-tone and MEMORY selections; no source-authority exception was added.

The original 18-lane, pan 20-lane and SUB 22-lane JSON contracts remain byte-for-
byte unchanged. A new `host-automation-sub-depth-v1.json` registers the exact
24-lane roster. The historical SUB executable still tests its original two
lanes and old-20 trajectories on the preserved 22-lane prefix. A separate build
of the same supported-input fixture tests the new depths and old-22 trajectories.
The complete production roster is checked exactly by the new capability gate.

Legacy/PURE final-point-at-block-start and no-queue behavior remain unchanged.
AU is not expanded. Fixed-size lane storage grows by two; there is no new queue
copy or DSP path, and no new CPU/deadline or allocation-performance claim.

## Actual-module verification

The exact pre-feature 22-lane Linux VST3 was preserved before the production
edit. Native tests exercise real IParameterChanges/process calls with ordinary
finite queues only. Each new depth is isolated by zeroing the other two depths.
The fixture enables original SUB mode, selects function 0, and uses a fixed
12 Hz original speed.
It spans note onset/release, multiple ramps, adjacent knots, the implicit sample
-1 and final-sample values. Each one-sample reference is compared against
7/64/256/1024/4096-frame partitions using the unchanged 2e-6 audio threshold.

Across 44.1/48/96 kHz and realtime/offline, the twelve per-configuration cases
cover each depth, eight/sixteen voices and common rank source selections
PANEL/factory 0/MEMORY 11. All 72 candidate scenarios passed with zero observed
partition error and nonzero modulation witnesses. An independent run repeated
all 72 successfully. The preserved baseline completed each configuration with
exactly twelve expected partition failures, demonstrating missing capability.

All 246 compatibility captures match the pre-feature module byte-for-byte:
no-queue, Legacy/PURE and old-22 modern trajectories within each rate/mode.
The preserved speed/VCO suite was also rerun across all six configurations,
retaining its original contract. No failure was discarded or rerun into a pass.

## Combined queues and mixed rank sources (2026-11-12)

A test-only extension adds simultaneous original-speed, VCO, VCF and VCA
queues for each ordered pair of different PANEL/factory 0/MEMORY 11 sources,
with eight and sixteen voices. The same six rate/mode configurations now run
24 depth scenarios each: twelve historical single-lane scenarios and twelve
combined/mixed-source scenarios. All 144 passed, including the 72 new cases,
with zero observed partition error against the one-sample reference. Every new
case had a nonzero modulation witness (observed maximum-absolute differences
from its no-queue render ranged from 0.0617174 to 0.82271). Empty/no-queue
identity and Legacy/PURE flattening assertions also passed for every new case.

The speed/VCO matrix retained its original 72 passing scenarios. These are
supported finite-input tests only. No production code, parameter roster,
normalized conversion or tolerance was changed. Historical capture generation
is unchanged; the additional cases assert and log without adding capture files.

Original-HEAD hosts were compiled separately and run against the same unchanged
production module. All 246 historical depth captures and all 246 historical
speed/VCO captures match the extended hosts byte-for-byte. This checks test-host
compatibility, not a new production pre/post comparison. Logs, source/module
hashes and per-file capture hashes are retained in
[`experiments/sub-combined-mixed-sources/`](experiments/sub-combined-mixed-sources/).

## Limits

These are Linux float32 protocol/render observations, not a real DAW or native
GUI qualification. Other rates, float64, other rank-source combinations,
pressure-driven modulation, waveform changes, program-boundary expansion and
CPU deadlines remain outside this gate. Comparisons are within a rate/mode,
not cross-rate waveform identity. Earlier unrelated offline limitations remain.
No broad suite or malformed-input fixture was executed for this change.
