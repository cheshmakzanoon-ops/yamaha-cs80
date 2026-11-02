# Remaining CPU gap: scoped feasibility review

**2026-11-01 UTC; analysis only.** CPU remains open at its original
**0.15–0.20× realtime held / ≤2% idle** targets. The existing component arrays
and exact scalar optimizations are implemented; lane-level SIMD is future work.
This review adds no candidate, test relaxation, audio change or speed claim.

## Measured timing and its scope

The serial, three-round experiment in
[Actions run 41257213038](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41257213038)
used one Linux x86-64 runner and GCC 13.3.0. Its latest checkpoint is
`rank-idle-1500`, before the separate FIR qualification still in progress.
The compact report gives:

| Checkpoint | Held median RTF range across cases | Idle median RTF |
| --- | ---: | ---: |
| `main-before` | 0.589659–0.695422 | 0.144685 |
| `inherited-before` | 0.613537–0.893367 | 0.102848 |
| `rank-idle-1500` | 0.603543–0.890282 | 0.077991 |

The latest held results require approximately **3.02–4.45×** further speedup to
reach 0.20, or **4.02–5.94×** to reach 0.15. Idle has improved but still requires
approximately **3.90×** further reduction to reach 0.02. These are ratios of
measured timing values, not predictions for an unimplemented backend.

## Existing instruction profile: where work remains

The separate earlier Callgrind profile is source
`d06648c386486cf2630de6e41195ddd5d4360ff0`, selector `pair0`, 128 callbacks at
48 kHz / 256 frames / eight voices. It reports **2,877,600,053 retired
instructions**. It is not a fresh native timing profile of the final branch.
Read-only source annotation of that existing recording gives:

| Source coordinate / operation | Share of whole-program instructions |
| --- | ---: |
| `Engine::tickRanks`, inclusive | 82.53% |
| Full filter clock path, inclusive | 29.92% |
| `ReconstructedOscillator::tick`, inclusive | 27.32% |
| `Ig00156PhaseCorrection::process`, inclusive | 24.89% |
| Phase header line 127: cubic input-derivative reconstruction | 1.97% |
| Phase header line 133: four RHS dot products | 3.50% |
| Phase header line 140: three elimination dot products | 2.70% |
| Phase header line 145: predictor | 1.60% |
| Phase header lines 157, 158, 159: derivatives, Jacobian, correction | 0.27% combined |

Coordinates refer to the unchanged
[`Ig00156PhaseCorrection.h`](../../../src/core/Ig00156PhaseCorrection.h).
Its SHA-256 remains
`26d7919d1dc8a1fe210d2bbc47798354b804476eaf6452221d51ef85c8171947`.
The existing convergence shortcut already makes additional Newton/Jacobian
work a small share of this held fixture. Higher-resonance and moving-control
cases require their own attribution; this table does not generalize their cost.

## Guarded column storage: feasible policy, modest opportunity

The unchanged [exact contract](../../../tests/cpu/exact-scalar-kernels.json)
already permits a qualified nearest-rounding path with the original scalar
fallback for unsupported modes and exceptional states. That does not qualify
the [rejected column candidate](column-layout-rejected/README.md).

Disassembly of its existing GCC qualification binaries shows **350 static
instructions** in the original live phase `process` body and **325** in the
candidate. Both contain 19 `mulpd`, 13 `addpd`, 44 `mulsd` and five `divsd`
instructions; neither contains FMA. The difference is principally moves and
lane packing, because the original already vectorizes pairs of independent
rows. Multiplying the static difference of 25 by the profile's 2,097,152 phase
calls gives approximately **1.8% of whole-program instructions**, before
eligibility checks. This is a static estimate: instruction paths, memory cost,
compiler changes and guard overhead prevent treating it as a measured reduction.

Even the unrealistic assumption of making all work in the two dot-product
lines four times cheaper removes only **4.65 percentage points** of the old
instruction total. A column-only route cannot reasonably be presented as the
solution to the measured CPU gap.

A correct guarded implementation would need canonical scalar coefficient and
state ownership, including coefficient preparation under the caller's current
FP mode. It must handle mode changes without a new control key, nonfinite
controls, nonfinite input history and restored/poisoned state. Checking only the
current input is insufficient. Packing columns once after unchanged scalar
coefficient preparation is permitted by the original work contract, but its
extra storage and dispatch must be measured. No such guarded candidate was
implemented or evaluated here.

## Larger route: complete kernels across independent chains

[`Engine.h`](../../../src/core/Engine.h), `RankArrays` at line 450, groups 16
whole scalar oscillator/filter objects by component. Their internal nodes,
coefficients and histories remain inside each object. `Engine::tickRanks`
processes one complete chain at a time. This is component-level structure of
arrays, not vector execution across independent lanes.

The material next investigation is lane-level storage and batching of the whole
phase kernel: reconstruction, predictor, OTA functions, residual and accepted
state update. Each lane must retain its original operand order and stop at its
own accepted Newton step. A hypothetical fourfold reduction of the entire
29.92% filter instruction share removes **22.44 percentage points**, before
packing, masks and dispatch. This is an optimistic instruction-cost calculation,
not a wall-time bound or a claim that the target is impossible.

WSC batching would address another large region. A smaller intermediate is a
specialized path for lanes with unchanged controls and no physical or branch
event within the sample, retaining the original `advance` and `nodes` formulas.
The old WSC annotation attributes approximately **3.27%** of whole-program
instructions to `Nodes before`, `after`, `start` and `endNodes` initialization or
copy lines; some of that storage is necessary and cannot simply be removed.
Event tolerances (`2e-15`), sided classification (`2e-11`), residual tails,
diagnostics and future gate reopening must remain exact.

Before either batching route is implemented, add the independent state/lane
oracle and candidate-specific work witness already required by the frozen
contract. Cover mixed networks and controls, partial batches, inactive lanes,
independent restore/copy, per-lane convergence, FP transitions and event
boundaries. Engine qualification must preserve HP-before-LP dependencies,
original RNG consumption and final scalar mix order. SIMD must preserve each
target's existing FMA boundaries; changing global floating-point options would
also affect immutable Legacy. Unsupported or unqualified backends retain the
scalar route. The original physical-model gates, strict compatibility captures
and three uncontended factory rounds before/after remain mandatory.

## Provenance

Only existing source, logs, ELF disassembly and an existing Callgrind recording
were read. No new executable, profiling run, benchmark or production patch was
created during this review.

| Analysis input | SHA-256 |
| --- | --- |
| Serial run `compact-summary.json` | `8a0b2d60db493e56846610b64f136563288990a162ed773da628da891522cef0` |
| Session `qualified-profile/pair0.callgrind.1` | `d1ec5ee710a6194d3f28bb6f6f31940fbf4538d2a969995eab3b16164c123ac8` |
| Archived scalar qualification ELF | `17683087d37155344492d1c214d4e9ab241cb99d3265336527264537b1184b6c` |
| Archived rejected-column qualification ELF | `80c8ec3939bc9dc6b3baf8625dd615fe4deb30c3376c7547888b9230069a7588` |

The ELF source manifests, compiler arguments and candidate patch are preserved
in [the rejection evidence](column-layout-rejected/README.md). The profile
recording is a session artifact identified above; its percentages are explicitly
separate from the serial CI timing evidence.
