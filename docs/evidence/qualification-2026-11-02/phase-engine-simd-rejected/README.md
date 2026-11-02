# Experimental IG00156 full-Engine phase SIMD — CPU gate rejection

**2026-11-02 UTC. Local isolated experiment only. Decision: REJECT for PR6/main.**
The source held-voice RTF target 0.15–0.20 and quiet-idle RTF at most 0.02
remain failed. No hardware, DAW or release qualification is claimed. This
report archives a completed, serial three-before/three-after whole-engine
benchmark; this evidence-only follow-up does not modify DSP source.

## Fixed source, benchmark and build identities

- Original baseline: worktree HEAD
  `7c864f266a939aa2a42422440dd4c1c0cabe3814`, frozen production `src`
  Git tree `7dd159d491012fd47b46c0aa651e86ea959ca289`.
- Candidate: `experiment/cpu-phase-simd-engine-20261102` commit
  `b1b722bdbd09f534cd0ad9a7d30d6636e19e8906`, `src` tree
  `ceb7e16c026f4a0d645b9d00002f838c47c5a9ac`. Relative to original
  `src`, only `Engine.cpp`, `Ig00156Filter.h`,
  `Ig00156PhaseCorrection.h`, `ContinuousVoiceCircuit.h` changed; no
  benchmark, physical constant, tolerance or frozen oracle was edited.
- Both commits use the **identical unchanged benchmark source** blob
  `92ea7e0b1801bf98fccaed21c13e676890039c94`; each run reports source
  fingerprint `a95212770767733c`. Both builds: GCC 13.3, CMake Release,
  `-O3 -DNDEBUG`, plugin OFF, core tests ON. The candidate's separately
  compiled mutation-test targets are never benchmarked.
- Original benchmark executable SHA-256
  `3fad41e7ff9659e7dcc4c1a73ce771892de74dc7386eea24d8185635236808a5`;
  candidate SHA-256
  `288eea25ea80a274b26b3ae6997c3df9ebdf9acbcb22d1b04402cb55949c7148`.
- Measured machine: virtualized Intel Xeon Platinum 8573C with AVX2,
  nine exposed logical CPUs and eight-CPU cgroup quota
  (`cpu.max=800000 100000`). This differs from historical AMD EPYC
  CI machines; absolute cross-machine comparisons are invalid.

## Six complete uninterrupted, serial benchmark rounds

All original-source runs preceded every candidate run. Each invoked the
unchanged stock `cs80_source_factory_benchmark` as
`--suite baseline --rate 48000 --block 256 --callbacks 1024` (no quick
subset). Each `completed.json` confirms **27 cases, 51 rows**; all 51 keys
match across six runs and every result reports zero nonfinite samples.
Original `results.csv`, `metadata.json` and `completed.json` artifacts are
preserved byte-for-byte in `raw/<round>/`. Their starts and raw SHA-256s:

| Round | UTC start | Raw CSV SHA-256 |
| --- | --- | --- |
| before-1 | 04:46:31 | `b5a21b58e9af9a085de26775853d6cb85793221d554356a92d7399f51dc657ed` |
| before-2 | 04:48:31 | `b554892842c7153206a8c433b971d875ddea2a88efa506bfa52fa5fa68b1ff2e` |
| before-3 | 04:50:17 | `81c3c277c8a20de3352b0051fb355d1dba40d4c1f6476d9e7c242fb657cafa82` |
| after-1 | 04:52:07 | `b654873af20d61b970919234e838db006a386b26bb681855cedf831c1dc79e8e` |
| after-2 | 04:53:55 | `9272db985bd0b5758f6df409400e79acad8a955f8dc899d8e6e82c3e20663215` |
| after-3 | 04:55:26 | `7c1dad9337cdf74ac1ecd224d0912574f1605bce8c7b41fe3df83a8bf7116ac4` |

The existing `scripts/summarize-cpu-benchmarks.py` convention selects the
**minimum callback median from three whole rounds for each fixture**;
its p99 and missed deadlines come from that *same* selected round, not
independent favorable selections. RTF is callback time / audio time,
not a measured DAW/driver utilization. All raw rounds, outliers and Legacy
controls remain inspectable; no hidden quick-mode or narrowed fixture list.

| Eight-held source preset pair | Before median RTF | SIMD median RTF | Change | SIMD p99 RTF | SIMD missed callbacks |
| --- | ---: | ---: | ---: | ---: | ---: |
| String 1 / String 2 | 0.632643 | 0.517676 | -18.2% | 0.716220 | 0 |
| String 3 / String 4 | 0.680713 | 0.572941 | -15.8% | 1.083757 | 18 |
| Brass 1 / Brass 2 | 0.734360 | 0.594931 | -19.0% | 1.066422 | 14 |
| Flute Brass 3 | 0.675020 | 0.573527 | -15.0% | 1.612126 | 73 |
| E. Piano Bass | 0.751711 | 0.616178 | -18.0% | 1.293577 | 22 |
| Clavichord 1 / 2 | 0.764375 | 0.661500 | -13.5% | 1.250853 | 56 |
| Harpsichord 1 / 2 | 0.596753 | 0.523965 | -12.2% | 2.172499 | 34 |
| Organ 1 / 2 | 0.726036 | 0.538621 | -25.8% | 1.621131 | 38 |
| Guitar 1 / 2 | 0.718056 | 0.571637 | -20.4% | 0.840414 | 2 |
| Funky 1 / 2 | 0.742949 | 0.625600 | -15.8% | 1.039491 | 14 |
| Funky 3 / 4 | 0.976408 | 0.811451 | -16.9% | 1.819812 | 103 |

The median of the eleven selected held medians improved
**0.726036→0.573527 RTF** (about 21.0%) and every pair's selected
median improved (12.2–25.8%). But the candidate held range
**0.517676–0.811451** remains far over the hard ceiling, and candidate
selected held p99 reaches **2.172499**. Selected held callback misses:
**871 before, 374 after**. This is real Engine work, not a vector
microbenchmark, yet it is **NOT a CPU qualification pass**.

| Other source fixture | Before median RTF | SIMD median RTF | Before p99 | SIMD p99 | Missed before / SIMD |
| --- | ---: | ---: | ---: | ---: | ---: |
| Quiet idle | 0.060735938 | 0.061194375 | 0.426032625 | 0.111610875 | 1 / 0 |
| Moving tabs/pressure/wah | 0.895808063 | 0.771731813 | 1.313547188 | 1.964090813 | 264 / 97 |

Quiet idle worsened slightly and is **3.06×** the allowed 0.02 RTF.
The moving-control selected median improves, but **p99 becomes worse**;
high tail latency cannot be buried under a favorable median. Legacy
selected held medians range 0.255067–0.319534 (before) and
0.248912–0.292428 (after); the Legacy engine takes the scalar path, so
those round-to-round differences are not attributable to SIMD.

All six rounds are text-identical at each of the 51 fixture keys in the
thirteen non-timing fields `checksum`, `peak`, `rms`, `nonfinite_count`,
`abs_at_or_above_one`, `tail_voices_complete`, `final_active_voices`,
`source_compilations`, `last_block_peak`, `selector_events`,
`controller_events`, `mpe_member_state_checks` and
`pedal_held_callbacks` (zero discrepancies). These summaries supplement,
not replace, independent sample/state bit-exact checks.

## Original numerical oracle and true work

Retested on the same local candidate after benchmarking:

- `cs80_phase_batch_backend_tests`: PASS; widths 8, rates 8,
  **3,387,616 active** and 282,400 masked lanes,
  **37,170 real four-lane AVX2 calls**, **76,827,127** exact bit
  comparisons, zero mismatches.
- `cs80_phase_lane_oracle_tests`: PASS, **79,337,496** exact original
  scalar bit comparisons, four FP rounding directions, supported MXCSR
  FTZ/DAZ modes, zero mismatches.
- `cs80_phase_engine_dispatch_tests`: PASS; 27 FP/policy/partition cases,
  **331,776** output bits and 648 readout checkpoints compared,
  61,248 qualified samples, **244,992 HP** plus **244,992 LP** actual
  vector groups, zero failures.
- `cs80_phase_idle_wake_state_golden_tests`: PASS;
  **576** original-host idle/wake checkpoints, zero failures.
- Previously verified eleven actual-source negative mutation binaries
  (SIMD_OTA, SIMD_LANE, SIMD_NEWTON, MASK, NO_BATCH, RNG_DRAW, HP_LP,
  MIX_ORDER, FAKE_WORK, WAKE_CLOCK, SPLIT_FP) compiled and failed
  named real assertions with ordinary exit 1. SPLIT_FP specifically
  catches removal of the x87/MXCSR adversarial rounding guard.

The kernels preserve the original HP→LP dependency, per-rank mix/RNG order,
caller FP state, masked lane state and unsupported ISA/FP scalar fallback
over their registered fixture coverage. These are software-continuation
checks, not a universal physical-hardware equivalence claim.

## Rejection and remaining direction

Phase SIMD is held-only and cannot close the idle gate. Earlier Callgrind
instruction shares identify independent full-source WSC tick and IG00156
work (approximately 27% and 25% of held instructions); these figures
are **not native time**. The separately preregistered Touch idle fixed-state
workstream is not modified here. No narrow source change identified in this
review would credibly close both absolute gates without risking exact
continuation. Keep this rejected candidate isolated; do not merge it into
main/PR6, re-pin tolerances, tag or release.

Remote publication of this isolated experiment is **not complete**:
HTTPS git lacked credentials and the authenticated connector stalled;
the remote branch was independently checked absent. Under lead direction,
this evidence follow-up made **no further remote push/API attempts**.
