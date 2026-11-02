# Rejected CPU experiment: exact signed-zero drift sine branch

2026-11-02 · **REJECTED AS CPU OPTIMIZATION** · never integrated into PR6,
main, published audio, acceptance pin or release · all original audio bits
preserved · all untrimmed 3-before/3-after CPU measurements retained

## Source identity and explicit preregistration

This is a **separate isolated experiment**, not the production feature. The
preregistered original-source oracle was committed **before** any source
change: `b778ceb` (test only), followed by the nine raw original CPU files
`a9d0668`. The actual local experimental source commit is **`5ffa46c`**.
The production feature's original `src` tree is the unchanged
`7dd159d491012fd47b46c0aa651e86ea959ca289`; this experiment's source
tree is `280cdd081ded9e54347c563761a94078bd7ec0cd`. Do not cherry-pick
`5ffa46c` into the feature.

The experiment replaces the original `Engine::tickRanks` one-per-voice sine
with a guarded function only for drift `+0` or `-0`, finite engine phase and
voice-offset range, and a safe margin around sine sign-crossing half-turns.
It computes the **original IEEE signed zero** from the half-turn sign and
uses an unavoidable volatile float rounding step to raise the same sticky
FE_INEXACT bit, preserving caller FP controls. All nonzero drifts, crossing
boundaries and unsupported phase domains take the original sinf expression.
This is a software-state equivalence experiment, **not** a new analog model,
sinusoidal approximation or evidence of measured hardware drift.

### Exact correctness passed

- The immovable `tests/fixtures/drift_sine/OriginalDrift.h` fixture is a
  separately compiled literal of the pre-optimization source expressions;
  original Engine.cpp SHA-256
  `a874c328d623bda38f4adffea59ac442ca721c46465f829db1c68ef20fd52a28`,
  frozen header SHA-256
  `77b5d26b69460ed91263c397902957e4aed8f5fc7d5a5b0712e77af934ef1b17`.
- Candidate **1,638,433 exact bit and FP checks**, zero mismatches, across
  four FE rounding modes × four supported x86 MXCSR FTZ/DAZ modes, eight
  independent offsets, five drift values (`+0`, `-0`, `±0.25`, subnormal),
  163,840 direct sine comparisons (94,957 exactly zero), and 491,520
  thermal/worn-selection comparisons. Same result with the production
  `-O3 -DNDEBUG` C++ flags **without** test-only `-frounding-math`.
- Real uninstrumented `Engine.cpp` audio and public-state/readout original
  golden FNV64 fingerprints match for all five policy/thermal/worn scenarios
  at callback partitions 1/17/256 and held/release/quiet/reacquisition.
  FNV is a regression fingerprint, not collision-free proof of byte equality.
- The full **strict four independent compatibility captures passed** via
  `scripts/check-compat-gates.sh --strict-v2`, using pinned original commits:
  Legacy default and tails byte-identical to `d56911a`, source V1 numeric
  bytes identical to `22d6a32`, current source V2 engine audio bytes
  identical to `83b7f76`. No tolerance or V2 pin was changed.
- All **111 unique CTests PASS across two runs**, deliberately *not* claimed
  as one green 111-test sweep. A full serial relinked sweep ran **1,531.16s**:
  109 assertion passes and two **NOT RUN/BAD_COMMAND** from independently
  verified mode `0644` on two untracked generated ELF test binaries
  (`cs80_oscillator_cache_tests`, `cs80_wsc_fast_path_oracle_tests`). With
  separate scoped execution approval, only those two binaries gained their
  missing execute bits; `ctest --rerun-failed` then passed **2/2 in 0.17s**.
  No source file was chmodded, no assertion failed.
- Six preregistered test-control mutants plus four *actual optimized source*
  mutants compiled at `-O3` and normally exited **1** on named assertions:
  wrong signed-zero sign, unsafe zero skip, lane contamination, FP sticky
  clearing, thermal term contamination, all-partitions audio corruption,
  full-source fast branch disabled, fake fast-count labeling, wrong actual
  fast-zero sign and actual fast FE flag clearing. No kill relied on a crash,
  build error or skipped test.

### Real source-site work was eliminated—but not CPU time

A **separate test-only build of Engine.cpp** counted real entries adjacent to
the source site's optimized return and original sinf fallback. Production
`cs80_core` contained **no work counters**. At 48 kHz / 1,536 host frames
with eight held voices, the exact original PURE-isolation drift `+0` path
executed **24,267** fast branches and **309** original sine calls, out of
**24,576** actual original-site opportunities (98.74% site calls removed).
sourcePerformance thermal drift0 produced the same 24,267/309; nonzero
thermal and nonzero Legacy each produced **0 fast / 24,576 real sine**.
The negative actual-source test that lies about fast entries but still calls
real sinf failed `work.realEngineIsolationSavedSinfCalls`. These are genuine
source-entry work observations, **not benchmark speedups**.

## Same-host CPU: three complete serial rounds before and after

Parent-coordinated original quiet **02:18:32–02:24:35 UTC**; after quiet
**03:37:39–03:44:04 UTC**, with all other workstream agents completed or
explicitly paused, no sustained host-heavy process at boundary checks.
Host: Xeon Platinum 8573C/KVM, 9 vCPU, GCC13.3.0, CMake Release
`-O3 -DNDEBUG`. Unchanged `source-core-timing-v2` `--suite baseline`
48,000 Hz / 256 frames / 1,024 callbacks, one-second warmup, thread CPU
clock, MXCSR FTZ/DAZ. Three serial rounds in each arm, **27 scenarios,
51 rows each**, all six `completed=true`. Original benchmark ELF SHA-256
`3fad41e7ff9659e7dcc4c1a73ce771892de74dc7386eea24d8185635236808a5`;
candidate ELF SHA-256
`1ccb71b9bbb5860d3488b076cb30f9568e02c0b80357512e4fdffb73536988c2`.
The original source fingerprint `a95212770767733c` and all 51 fixture
identities stayed the same. A standalone exact comparator checks **all
306 rows** across both arms: every non-timing field—original checksum,
peak, RMS, voice and event counters, rack identities, tails, nonfinite
counts—was **identical**, without omitting any preset or round.

| Metric | Before 1 | Before 2 | Before 3 | After 1 | After 2 | After 3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Held: median of 11 source preset medians (RTF) | 0.858290 | 0.752764 | 0.729353 | 0.830539 | 0.836778 | 0.819617 |
| Quiet-idle source median RTF | 0.069322 | 0.059082 | 0.062213 | 0.065114 | 0.066208 | 0.065255 |
| Moving-controls source median RTF | 0.875949 | 0.878074 | 0.882760 | 0.968114 | 1.098622 | 1.137174 |
| Worst held source p99 RTF (untrimmed) | 36.877030 | 5.071613 | 3.324330 | 4.583700 | 2.576190 | 9.536232 |
| Held source deadline misses / 11,264 callbacks | 3,142 | 2,368 | 1,368 | 2,474 | 2,776 | 2,738 |
| Legacy selector0 held median RTF (host anchor) | 0.355426 | 0.288158 | 0.404925 | 0.322678 | 0.331355 | 0.327063 |

For each of the 11 matching held presets, first take its *median across
three rounds*, then compare before vs after. The median across all 11
three-round preset medians rose **0.730701 → 0.8133474375 RTF**
(**11.31% higher CPU consumption**). The median of the 11 *paired preset*
improvements `(before-after)/before` is **-12.90794%**; **10 of 11
presets slowed**. Only String 3/4 improved; moving controls worsened as
well. Three-round quiet-idle median rose **0.0622126875 → 0.0652554375
RTF**, approximately **4.89% slower**. Host/VM timing variation is real;
this six-round result does not isolate every cause of the regression. It
unequivocally **does not establish the demanded 3–4× speedup**.

| Selector | Held preset pair | Before 3-round median RTF | After 3-round median RTF | Paired improvement |
| ---: | --- | ---: | ---: | ---: |
| 0 | String 1 / String 2 | 0.602564 | 0.735630 | -22.08% |
| 1 | String 3 / String 4 | 0.911505 | 0.802239 | +11.99% |
| 2 | Brass 1 / Brass 2 | 0.807766 | 0.832969 | -3.12% |
| 3 | Flute / Brass 3 | 0.683794 | 0.807558 | -18.10% |
| 4 | E. Piano / Bass | 0.765984 | 0.873256 | -14.00% |
| 5 | Clavichord 1 / 2 | 0.858290 | 0.866108 | -0.91% |
| 6 | Harpsichord 1 / 2 | 0.618247 | 0.695107 | -12.43% |
| 7 | Organ 1 / 2 | 0.720363 | 0.813347 | -12.91% |
| 8 | Guitar 1 / 2 | 0.730701 | 0.803376 | -9.95% |
| 9 | Funky 1 / 2 | 0.729353 | 0.836778 | -14.73% |
| 10 | Funky 3 / 4 | 0.899385 | 1.037133 | -15.32% |

Additional untrimmed noise includes candidate after-round3 Legacy selector1
p99 RTF **6.06524** and source selector5 p99 RTF **9.53623**, while the
original before-round1 worst source p99 was **36.87703**. P99 is per
fixture, not a fabricated pooled percentile. Full raw callback percentile
statistics, deadline misses, host anchoring rows and event/tail counters are
retained in their original 51-row CSVs and SHA-256 metadata.

| Evidence arm | Raw result CSV SHA-256, rounds 1 / 2 / 3 |
| --- | --- |
| Original before | `94bb6b342bf5c7781b112410fde26528039e78985ff5b41057e8273e4f4110af` / `9e7714f0d7b0995e77a7a7bfc80973a5dccaa8629c6c74f3433eedaa36f929f0` / `6d25b782a7b658a537e1559df4762226b2d0b3694d54b3e748989bbb9cab5f8e` |
| Candidate after | `6da3d3d4e62bfb4162cca1ea5d3adb2b2d844898f89ac40caf0bf14349c25923` / `5ae348dd4151e5b981654d578bbc7e979576f4aebc55dd42f3d39731d0d40f77` / `4a87cb59b827730cd4a90661c5f7c5bb775a264155043e48c86d61f467b92ae6` |

## Hard gates and decision

- Eight-voice held median RTF **0.81335**, required **0.15–0.20**:
  **FAIL**, roughly 4.07× above the allowed 0.20 ceiling.
- Quiet-idle median RTF **0.06526**, required **≤0.02**:
  **FAIL**, roughly 3.26× the limit.
- Zero byte regressions and 111 unique CTest passes **do not override**
  failed CPU objectives, existing click/audit failures or lack of actual
  recovered-hardware accuracy for unrelated unresolved circuit parameters.

**Decision:** preserve the preregistration, independently frozen fixture,
real-work negative controls, rejected source and *all 18 raw benchmark files*
in this isolated experimental history for reproducibility. Do **not**
cherry-pick optimized source into PR6/main, re-pin V2, ship binaries, tag,
merge or release it. The original production `src` tree remains untouched.

**Next high-leverage target:** 34.39% oscillator WSC physical tick,
29.22% `Ig00156PhaseCorrection::process` nonlinear solve and 14.69%
rank orchestration from the original warmed-held instrumented `gprof`.
The preregistered independent **4/8/16-lane full nonlinear phase oracle**
(test-only `71a0226`, 79.34 million exact/mutation checks) is prepared for
a substantive true-vector-work experiment; its 4×4/Newton lane state,
FP modes and unaltered observed fields must remain exact. Idle needs a
*separate* global control/touch/output scheduling optimization, because
phase process is nearly absent when cold and idle. Neither anticipated
speedup has been implemented or claimed here.
