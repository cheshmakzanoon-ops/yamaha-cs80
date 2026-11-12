# Combined RING queues with asymmetric rank sources

## Test-only integration scope

This candidate qualification combines the existing RING speed, amount, depth,
attack and decay queues at lanes 28–32. It introduces **no new lane** or
production behavior: the complete 33-lane roster, shared host renderer, parameter
metadata, DSP and UI remain unchanged. All 23 earlier contracts are preserved;
`host-automation-ring-combined-v1.json` is a qualification contract.

The existing global controls are independent of the two rank-source selectors.
This gate exercises asymmetric pairs `(13,0)`, `(0,13)`, `(13,11)`, `(11,13)`:
PANEL/factory, factory/PANEL, PANEL/MEMORY and MEMORY/PANEL. It does not claim
coverage of every factory slot or arbitrary MEMORY-bank contents. Eight-voice
capacity uses two active keys; sixteen-voice capacity uses sixteen. Those eight
cases per configuration make 48 cases over 44.1/48/96 kHz and realtime/offline.

The unchanged [two-cycle fixture](VST3_RING_RETRIGGER_AUTOMATION.md) supplies
onsets 17+key and 2048+key, with releases 1024+key and 3584+key. Speed/amount/depth
use their existing continuous queue paths. Attack/decay are held through all
first onsets, changed before the next trigger, and held through its full onset
cohort. They retain [trigger-latched semantics](VST3_RING_TIMES_AUTOMATION.md),
not live AD reshaping. No pedal/scan/arpeggiator expansion is implied.

## Independent controls and interpretation

The planned 15 renders per case remain distinct:

- One scalar reference and five alternative block partitions compare the same
  five queued trajectories using the unchanged numerical contract.
- One **matched five-active-constant** control compares against that reference
  to demonstrate an aggregate audible effect. It starts from the same initial
  normalized values on the same active lanes. This is not proof that each
  parameter independently caused the measured difference.
- Two amount-zero renders omit the amount queue on **both** sides. One varies
  the other four controls; the other supplies matching active constant queues
  for those four lanes. Their PCM output must agree. AD and carrier phase still
  run internally, so equality is not internal-state equivalence.
- Six separate no-queue/policy renders retain the fixture's compatibility
  observations. They are not substitutes for matched-active controls and do
  not add a new frozen-host comparison claim.

The matched-active comparisons avoid confounding no-queue initialization with
active normalized-to-physical conversion. Distinct queue knots and preserved
physical conversion do not justify comparing with an assumed linear-seconds
mapping. The aggregate and bypass witnesses are software-protocol observations,
not measured Yamaha fidelity or whole-voice circuit qualification.

## Build and evidence requirements

`cs80_vst3_ring_combined_automation_tests` registers six configurations with the
standard module/original-contract/new-contract/output/rate/mode CLI. The frozen
already qualified 33-lane module is reused; only the supported host needs to be
built against the official SDK. The unchanged shared helper needs no new
historical-helper rerun for this gate. No plugin rebuild or production change
is required or claimed.

The bounded plan totals six native processes and 720 renders. Exact source,
host/module hashes, commands, source/capacity cases and all outcomes must be
retained before a result is described as passing. Compilation or registration
alone is not execution. The retained local result is recorded below; publication or broader release
qualification is not implied.

The prior [depth](VST3_RING_DEPTH_AUTOMATION.md),
[times](VST3_RING_TIMES_AUTOMATION.md) and
[next-trigger](VST3_RING_RETRIGGER_AUTOMATION.md) qualifications retain their own
pinned evidence. No broad, malformed-input, fuzz, GUI/DAW, AU, hardware,
CPU-deadline or allocation-performance qualification follows from this gate.

## Verified local result

[Retained verification, commands and logs](experiments/ring-combined-automation/verification.json)
record one successful direct official-SDK host compilation and six direct
native processes, all exit 0. The frozen 33-lane module was reused; no
production build, CMake configure/build or CTest execution occurred.

All 48 cases passed. Scalar versus 7/64/256/1024/4096 partition errors were 0.
Five-lane aggregate PCM effects versus matched constant-active controls ranged
.0540867–.508186. All amount-zero four-queue versus four-active-hold comparisons
were exactly PCM-output inert. These aggregate observations do not isolate each
lane's contribution, prove internal-state identity or establish a universal
numerical bound. The earlier isolated-lane checkpoints remain separate evidence.

The finite matrix uses ordered mixed rank pairs (13,0), (0,13), (13,11),
(11,13), voice capacities 8/16, rates 44.1/48/96 kHz and realtime/offline modes.
It uses the unchanged two-cycle event helper; attack and decay hold initial
values through sample 64 and reach their second targets at 1408 and 1664,
respectively, before all second onsets 2048–2063.

SDK/compiler/source provenance was captured before compilation and checked
after runtime; all recorded identities remained unchanged, as did host/module
runtime hashes. All production sources, prior scripts, the shared helper and
23 earlier contracts match the base. No runtime failures or retries occurred.
The 144 local PCM captures are inventoried, not compared against a baseline;
**no baseline PCM-pair claim is made**. Raw PCM and executable files remain in
ignored local build directories, so committed evidence is not a complete replay
bundle. No negative, broad, malformed, fuzz, GUI, DAW, AU, CPU or hardware tests
were run.
