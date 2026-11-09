# Branch audit: 2026-11-09

## Read this as a dated audit, not a live release status

The inventory baseline is main `1b146c45226e14f46af9d62e6e3bd0ced4476213`.
All 112 real branches were inspected for ancestry, head identity, common base,
ahead/behind counts, changed paths and shared historical containment. `origin/HEAD`
is excluded. This is exhaustive structural triage, not a claim that every line
of every branch has received a fresh code or runtime review.

The initial snapshot contains 25 non-main merged ancestors, 55 broad divergent
histories, 30 narrow unmerged topics, one image-only orphan, and main. The broad
histories are 75–315 commits ahead and 157 behind the baseline, commonly with
about 1,700 or more merge-base changed files. Their names may say `docs` or
`evidence`, but their histories are not documentation-only. Extract bounded
changes rather than replacing or blanket-merging current main. No branch
closures, deletions, or blanket merge are recommended by this report.

Main changed during this work. The follow-up observations below use immutable
heads and explicitly identify their comparison base. Initial-table labels such
as “missing,” “review,” or “recommend” describe the original snapshot only.
By main `a83acd5e7e9be08d4bb33dd685d4c13a46d16563`, #91, #103, #89, #90 and
#104 were integrated. This document does not certify their complete combined
release state or keep a moving tally of subsequent merges.

### Follow-up against `b542c17`

A fresh GitHub inventory on November 9 still has 112 branches and 72 open draft
PRs. Open status is not proof that a change is absent: narrow replacements for
Scala #41, Drive/Tape #73/#75, adaptive Ensemble #77 and renderer #100 are already
in this main ancestry. See their scoped reports rather than reapplying old heads.

**Preset preview boundaries, #82**, head
`58e6070c5a750689aabf68108c0c2b5129bb1f3b`, remains genuinely absent at that base.
A narrow port is independently reviewed and tested at `9b7059d`; see
[Preset preview boundaries](PRESET_PREVIEW_BOUNDARIES.md) for the fresh negative
controls, sanitizer tests, native build and historical validator failure. It
does not alter processor restore, factory values or audio DSP. This dated audit
does not imply the port has been published or passed current remote CI.

**Live ring timing, #81**, head
`b585ae5e26492933132d2323e7acbb2a8a3f5d0f`, is still a separate candidate.
Its update of active PRA attack/decay is an engineering-inferred control policy,
not merely a no-audio-change optimization or a measured PRA law. Its historical
tests do not establish current-main combined compatibility. The preview safety
fix was prioritized; no #81 integration or new current-main runtime result is
claimed by this follow-up. No broad branch is recommended for blanket merging.

## Fresh focused findings

### Safety changes

* **MSEG exponent guard, #89**, head
  `3c968e2ad043cb64515da72ba4dda04634ecdfe9`: source review plus a fresh
  instrumented JUCE 8.0.11 core build passed 8,456 checks. Against the initial
  main parser, the new regression aborts at JUCE `CharacterFunctions.h:377`
  with signed overflow `999999999 * 10`. The candidate rejects oversized
  explicit exponents before JUCE's signed-int accumulator and leaves room for
  mantissa adjustment. This result is scoped to that regression, not all
  JUCE parsing or native plugin behavior.
* **Trash-only deletion, #90**, head
  `cb33f53e792da141244b62f2071e84b75a46cb50`: the handler no longer falls
  back to irreversible `deleteFile()` after `moveToTrash()` fails. An inert
  handler/double regression passed 23/23 checks on the candidate and failed
  5 checks on initial main, including a requested permanent-delete call.
  No actual file was deleted or moved to trash. Native trash service behavior
  was not tested by that double.
* **Scala/KBM and MTS admission, #40/#42**: narrowly ported missing checks
  onto main `a83acd5`, retaining later whole-message MTS validation rather
  than replacing the old header wholesale. See
  [Tuning admission safety](TUNING_ADMISSION_SAFETY.md) for exact provenance,
  4/4 focused CTest results, independent sanitizer review, and four failing
  protection-removal mutants. This does not include #41.
* **Scala/KBM session, #41**, head
  `1d860d1f332fcff5e427cd3612a53b30d080f356`: still a separate integration
  candidate. Source review confirms editor-local `sclText`/`kbmText` can be
  empty after reopening even when the restored description enables the KBM
  button; replacing a session can also leave a stale editor-local scale.
  The branch moves mapping-only import to the processor and checks the
  session version before publication. Its old processor/UI base conflicts
  with newer work; no fresh native-runtime test or safe current-main port
  of #41 is claimed here.

### Exact arithmetic and FX caches

All local runs in this section used GCC 14.2.0, `-O2`, UBSan with no recovery,
and libstdc++ assertions. They do not measure whole-plugin CPU savings or
replace integrated plugin/DAW validation.

* **VCA exact-zero identity, #91**, head
  `b14f0a7afb5a18c16db747efd7142c96be90c7c2`: 1,200,054 checks passed. The
  frozen reference matched initial main after namespace normalization. The
  expression preserves multiplication order and all state; +/-0 exponent
  yields exactly 1, while every other value uses the original `exp2` path.
  Independent PR review inspected green exact-head platform and DSP checks.
  A historical timing comparison also improved its nonzero control, so it
  is not evidence of a guaranteed isolated 3.6% speedup.
* **Drive, #73**, head `542674580a1dc9283359e39a1c01434c9ed1712a`:
  396 checks passed. Raw control values key an exact control-law cache;
  prepare/reset invalidates it. No approximate key or changed gain/tone law.
* **Tape echo, #75**, head `cac8c5440a92da8512d880af34ad1a6f81780769`:
  231 checks passed, with 21,845,986 stereo frames compared by bit pattern.
  Only the rate-independent laws are memoized; feedback state still moves
  every sample. Exact keys mean NaNs miss, not falsely reuse a value.
* **Ensemble, #77**, head `d7dd23adc94eaddc72da6128aaee95800b33945d`:
  865 checks passed, with 11,962,739 stereo frames compared. The clamped rate
  keys phase increments; prepare/reset invalidates the key and phase state
  still advances each sample.

The three FX changes affect disjoint portions of `FxModels.h`. Each individually
produced a conflict-free `git merge-tree --write-tree` against main `a83acd5`.
That establishes neither simultaneous runtime compatibility nor a passing CI
result. Exact-head failures remain relevant:

| PR | Passing checks sampled | Failing exact-head run and step |
| --- | --- | --- |
| #73 | PR build `42218499118`; DSP `42218446858`; screenshots | Push build `42218446793`: Linux `Validate VST3 headless` |
| #75 | PR build `42236387977`; DSP `42236308703`; screenshots | Push build `42236308737`: Linux `Validate VST3 headless` |
| #77 | DSP `42288096596`; screenshots | PR build `42288153169`: Linux `Validate VST3 headless`; push build `42288096594`: Intel macOS `Configure` |

These step outcomes were read from GitHub Actions job metadata. This audit does
not establish their root causes. #77 is not CI-ready simply because its isolated
arithmetic test passes. Revalidate current combined source rather than waiving
or relabeling historical failures.

### Useful pieces in broad histories, and rejected shortcuts

* **DC blocker coefficient hoist**, commit
  `01f75f4b26774077b34ef64c1f8e016b82cc69db`, with preregistered tests
  `1fa16fa0b3241f6839c386f374e498bb80fe9bc3`: a promising small extraction.
  Main still evaluated the same 10 Hz coefficient in the internal sample
  loop during source review. The candidate moves the original float
  expression, including operation order, into internal-rate configuration.
  Its old tests cover 44.1/48/96 kHz, quality/policy, factor and offline
  changes. A fresh current-main integrated test and independent review are
  still required; the broad parent history should not be merged for this.
* **Exact SUB converter memoization**, commit
  `2ba811d2764d8ef0838547ed8b2a3b74cd750658`: worth separate qualification.
  It retains the recurrence and memoizes exact repeated target/log and
  settled exponential values, invalidating on reset. Main remains uncached.
  Later historical `ExactControlTransformTests.cpp` also covers broader
  caches, so its existence is not a fresh isolated acceptance result.
* **Stationary smoothing/optics shortcut**, commit
  `8dac795558eab6bae99a0812cfa1fc6632cfc024`: **not ready**. The pre-change
  headers exactly match the initial main headers. A fresh run of the
  candidate's frozen-reference suite reported 373 checks, **2 failures**,
  over 1,346,220 controller and 14,044,472 expression samples. Both failures
  are `nonfinite controller arithmetic follows the original fallback`
  (rate 0, variants 23 and 29). Finite/moving cases passed, but the original
  bitwise contract includes these nonfinite cases. Do not silently weaken
  it or call the candidate exact. This is not a demonstrated audible bug.
* **Host automation offset/readback guards**, commits `d934868135cb2fae8d62b7d5492ee79e366ecbbf`
  and `6637537afa645fa8c22266b190a20145bdfcc27a`, are already represented
  in main. Do not reapply their broad histories.
* **PRA held-coefficient optimization, #78**, head `54a08a6f`: its own
  historical evidence reports CPU savings of -3.39% in legacy-idle and
  -1.63% in live-FX-sustain, negative in 4/5 rounds each. Those are measured
  regressions in that record, not regression-free gains; targets remain unmet.
* **Rejected pitch memo** `d1b67da8`: preserve the evidence of its 96 kHz
  active-LFO slowdown (median -3.35%, 6/7 slower). Current #92 `425e7c2f`
  adds active-modulation bypass and is a different proposal, not a reason
  to erase the rejection. Combined timing with other optimizations remains
  necessary and savings must not be added arithmetically.
* **OE2 topology, #31**, is isolated experimental source/API rather than a
  production Engine connection. **Ring carrier continuity, #35**, changes
  source-mode carrier phase behavior, while **live ring timing, #81**, changes
  active envelope parameter routing. These require explicit model/sonic
  review, not generic “cleanup” merges. Neither is certified by this audit.

### Superseded topics

CC121 `9d78a260` and main `ea0882d` have the identical stable patch ID
`a04b7389ae3faa124fd266ce3eba4d0167d56c19`. Hermite #72 has the same
ordered-bounds clamp as merged #99 (`c191d22`). Mono MSEG #71 and poly MSEG
#79 are both included by the combined #80 (`b07eced` / `e6a6292`). Existing
main regression tests should be retained rather than importing those stale
branches again.

## Evidence and reproducibility

The [compact inventory](evidence/branch-audit-2026-11-09/inventory.json)
records full head SHAs, base, counts, tip paths and branch containment without
committing megabytes of duplicated changed-path lists. The source identities
and raw result lines for the FX, stationary, and original tuning tests are
stored beside it. Files listed in each manifest are recoverable with
`git show <head>:<path>` and verifiable with `git rev-parse <head>:<path>`.

For the header-only snapshots, materialize the manifest's files into a separate
directory, then use (with the corresponding test filename):

```sh
c++ -std=c++20 -O2 -I src -fsanitize=undefined \
  -fno-sanitize-recover=all -D_GLIBCXX_ASSERTIONS \
  tests/TapeEchoControlCacheTests.cpp -o test
./test
```

Reproduce ancestry and path counts from a fetched repository with:

```sh
git rev-list --left-right --count <baseline>...<head>
git merge-base <baseline> <head>
git diff --name-only <merge-base> <head>
git merge-tree --write-tree <comparison-main> <head>
```

`rev-list` prints baseline-only (behind) first, then head-only (ahead).
No binary artifacts or local workspace paths are required by this report.
Historical CI status is not a guarantee about a later run or source tree.

## Narrow topics at the initial snapshot (historical)

| Branch | SHA | Ahead / behind | Assessment |
|---|---|---:|---|
| `evidence/rejected-pitch-memo-nov7` | `d1b67da8` | 1 / 38 | **ARCHIVE**. Evidence only: earlier exact memo rejected after 96 kHz modulation regression; preserve separately, not a current optimization. |
| `experiment/oe2-source-topology-20261103` | `4812b82d` | 2 / 157 | **EXPERIMENT**. Isolated OE2 topology, needs dedicated model/sonic review; do not enable by generic cleanup. |
| `experiment/ring-carrier-continuity-20261103` | `a5d71adc` | 2 / 157 | **EXPERIMENT**. Changes source ring carrier phase plus audit tool; requires dedicated continuity/fidelity evidence. |
| `fix/cache-drive-controls-20261106` | `54267458` | 1 / 50 | **REVIEW**. Exact control-law cache candidate in FxModels; require fresh integrated output/performance qualification. |
| `fix/cache-ensemble-rate-20261106` | `d7dd23ad` | 1 / 43 | **REVIEW**. Rate increment cache in FxModels; coordinate with other FxModels cache PRs and validate together. |
| `fix/cache-tape-controls-20261106` | `cac8c544` | 1 / 50 | **REVIEW**. Tape control-law cache in FxModels; coordinate with other FxModels cache PRs and validate together. |
| `fix/cc121-pedal-rpn-reset-20261106` | `9d78a260` | 1 / 66 | **SUPERSEDED**. Exact stable patch-id match to main ea0882d. No merge needed. |
| `fix/core-reliability-next-20261107` | `b585ae5e` | 1 / 38 | **REVIEW**. Live ring envelope timing without retrigger; source-policy compatibility needs dedicated check. |
| `fix/hermite-minimum-buffer-20261106` | `c110aacb` | 1 / 50 | **SUPERSEDED**. Equivalent ordered-bounds clamp already merged by c191d22 / PR99. Main has its own regression tests. |
| `fix/mono-mseg-dormant-onsets-20261106` | `62e11869` | 1 / 50 | **SUPERSEDED**. Production changes included by b07eced / e6a6292 / PR80, alongside poly fix. |
| `fix/mseg-coordinate-exponent-overflow` | `3c968e2a` | 1 / 19 | **HIGH-PRIORITY REVIEW**. Missing input-safety fix. Reject overflowing MSEG coordinate exponents before parsing; Separate product review. |
| `fix/poly-mseg-retrigger-rate-20261107` | `06fc9df0` | 1 / 43 | **SUPERSEDED**. Production changes included by b07eced / e6a6292 / PR80, alongside mono fix. |
| `fix/preset-state-safety-20261107` | `58e6070c` | 2 / 38 | **HIGH-PRIORITY REVIEW**. Preset input/name/finite-preview protections; not present in main. Separate product review. |
| `fix/preset-trash-no-destructive-fallback` | `cb33f53e` | 1 / 19 | **HIGH-PRIORITY REVIEW**. Removes permanent-delete fallback if trash fails. Missing in main; Separate product review. |
| `fix/scala-frequency-domain-20261103` | `f0ae4661` | 1 / 157 | **REVIEW**. Reject unrepresentable tuning frequencies; 157 main commits behind, port narrowly rather than replace Tuning.h. |
| `fix/scala-kbm-session-20261103` | `1d860d1f` | 1 / 157 | **REVIEW**. Retain Scala mapping across editor sessions; old processor/UI base needs integration review. |
| `fix/tuning-input-validation-20261103` | `351ff242` | 1 / 157 | **REVIEW**. Tuning input limits; old base, review overlap with subsequent main tuning validation. |
| `integration/clean-tuning-audits-20261103` | `5c56e12a` | 1 / 157 | **BUNDLED / STALE**. Bundles three tuning fixes, old OE1 docs/probe and phrase audit; prefer narrow fixes/current test branch, not bulk merge. |
| `perf/pra-held-coefficients-20261106` | `54a08a6f` | 1 / 43 | **REVIEW**. PRA coefficient reuse. Existing evidence itself reports legacy-idle -3.39% and live-FX sustain -1.63% CPU savings (regressions), each negative in 4/5 rounds. Do not present as regression-free; targets remain unmet. |
| `test/expanded-library-phrase-20261103` | `bdde45e8` | 2 / 15 | **REVIEW**. Opt-in library phrase diagnostics reconciled with newer main; useful test-only/tooling increment. |
| `work/bounded-offline-render` | `34ee3086` | 2 / 11 | **REVIEW**. Additional bounded-render allocation/resource limits after dimension checks. Distinct from merged PR97. |
| `work/ci-bundle-reparse-guard` | `640e24f5` | 1 / 14 | **REVIEW**. Reject Windows reparse points before archive traversal; packaging safety candidate. |
| `work/debian-staging-umask` | `9ce36bb4` | 1 / 14 | **REVIEW**. Normalize generated Debian staging permissions under restrictive caller umask. |
| `work/macos-staging-umask` | `47b4c954` | 1 / 13 | **REVIEW**. Normalize generated macOS staging permissions under restrictive caller umask. |
| `work/matrix-route-lifecycle-tests` | `c36e6d33` | 2 / 14 | **REVIEW**. Tests only: matrix route lifecycle + explicit rounding portability. Low runtime risk. |
| `work/memory-port-reference` | `60975903` | 3 / 12 | **SEPARATE PR REVIEW**. Offline-only reference Jacobian/loaded ports and identity metadata; 10k source evidence does not change audio defaults. |
| `work/offline-bundle-verifier` | `053b5983` | 1 / 14 | **REVIEW**. Offline inventory/integrity verification tool plus tests; useful distribution verification. |
| `work/pitch-pow-memo` | `425e7c2f` | 1 / 14 | **REVIEW / PERFORMANCE WATCH**. Different from rejected Nov7 ungated memo: now bypasses active modulation. Fresh combined performance qualification required; gains not additive with VCA. |
| `work/vca-zero-ei` | `b14f0a7a` | 2 / 4 | **RECOMMEND**. Exact zero exp2 identity only, full exact-head CI green per separate PR audit; fresh local 1,200,054 UBSan bit checks pass. |
| `work/windows-installer-preflight` | `68a5024c` | 1 / 14 | **REVIEW**. Preflight Windows payload existence/shape before wildcard packaging. |

## Complete initial inventory

| Branch | SHA | Ahead / behind | Merge-base files | Classification |
|---|---|---:|---:|---|
| `audit/cs80-original-repeat-20261102` | `844c4fc4` | 207 / 157 | 1746 | Broad historical branch; scoped extraction/integration only |
| `claude/adoring-fermi-alhg4x` | `4a47a20a` | 0 / 158 | — | Fully merged ancestor; nothing to integrate |
| `claude/clever-babbage-sm9ifa` | `ad4b00e5` | 0 / 292 | — | Fully merged ancestor; nothing to integrate |
| `claude/peaceful-dirac-y8is3k` | `4dc05d08` | 0 / 463 | — | Fully merged ancestor; nothing to integrate |
| `codex/cs80-completion` | `2ee33dcc` | 87 / 157 | 454 | Broad historical branch; scoped extraction/integration only |
| `diag-win` | `21b055e5` | 0 / 532 | — | Fully merged ancestor; nothing to integrate |
| `docs/cs80-public-source-scope-20261102` | `2d8778ea` | 207 / 157 | 1747 | Broad historical branch; scoped extraction/integration only |
| `docs/full-audit-v2-source-lock-plan-20261102` | `b801ddde` | 205 / 157 | 1743 | Broad historical branch; scoped extraction/integration only |
| `docs/pr6-audit-status-accuracy-20261102` | `9c62d7b8` | 206 / 157 | 1746 | Broad historical branch; scoped extraction/integration only |
| `docs/qualification-status-20261102` | `3236c0cf` | 204 / 157 | 1742 | Broad historical branch; scoped extraction/integration only |
| `evidence/adj-parts-bench-intake-20261102` | `b778694a` | 214 / 157 | 1747 | Broad historical branch; scoped extraction/integration only |
| `evidence/bass-funky-event-zoom-20261102` | `56c2616a` | 208 / 157 | 1751 | Broad historical branch; scoped extraction/integration only |
| `evidence/clavichord-steal-zero-time-20261102` | `73aedaa4` | 207 / 157 | 1748 | Broad historical branch; scoped extraction/integration only |
| `evidence/click-audition-ab-20261102` | `f007f137` | 205 / 157 | 1748 | Broad historical branch; scoped extraction/integration only |
| `evidence/click-classifier-validity-20261102` | `f0996982` | 207 / 157 | 1752 | Broad historical branch; scoped extraction/integration only |
| `evidence/full-engine-audit-20261102` | `0151b869` | 201 / 157 | 1760 | Broad historical branch; scoped extraction/integration only |
| `evidence/m41-vca-click-path-20261102` | `f64f964e` | 210 / 157 | 1757 | Broad historical branch; scoped extraction/integration only |
| `evidence/rejected-pitch-memo-nov7` | `d1b67da8` | 1 / 38 | 11 | ARCHIVE |
| `evidence/yamaha-adj-technician-measurements-20261102` | `8586492a` | 212 / 157 | 1743 | Broad historical branch; scoped extraction/integration only |
| `experiment/click-cue-guards-20261102` | `6792617f` | 218 / 157 | 1772 | Broad historical branch; scoped extraction/integration only |
| `experiment/cpu-phase-simd-engine-20261102` | `3a85d66a` | 210 / 157 | 1780 | Broad historical branch; scoped extraction/integration only |
| `experiment/cpu-wsc-four-lane-backend-20261102` | `9785f9e5` | 211 / 157 | 1746 | Broad historical branch; scoped extraction/integration only |
| `experiment/cpu-wsc-lane-prereg-20261102` | `58108ba7` | 205 / 157 | 1745 | Broad historical branch; scoped extraction/integration only |
| `experiment/cs80-wsc-denormal-sara-20261102` | `b1c95691` | 213 / 157 | 1844 | Broad historical branch; scoped extraction/integration only |
| `experiment/engine-phase-simd-prereg-20261102` | `b92e9846` | 208 / 157 | 1757 | Broad historical branch; scoped extraction/integration only |
| `experiment/host-curve-extreme-offset-20261102` | `d9348681` | 212 / 157 | 1738 | Broad historical branch; scoped extraction/integration only |
| `experiment/oe2-source-topology-20261103` | `4812b82d` | 2 / 157 | 16 | EXPERIMENT |
| `experiment/phase-lane-oracle-20261102` | `51cc6b23` | 205 / 157 | 1745 | Broad historical branch; scoped extraction/integration only |
| `experiment/rejected-drift-sine-20261102` | `a5cf41b1` | 210 / 157 | 1778 | Broad historical branch; scoped extraction/integration only |
| `experiment/rejected-wsc-fastpath-20261102` | `fe50198c` | 202 / 157 | 1761 | Broad historical branch; scoped extraction/integration only |
| `experiment/ring-carrier-continuity-20261103` | `a5d71adc` | 2 / 157 | 9 | EXPERIMENT |
| `experiment/touch-idle-exact-oracle-20261102` | `da91b1a9` | 206 / 157 | 1750 | Broad historical branch; scoped extraction/integration only |
| `experiment/ui-readiness-async-click-20261102` | `dcb3ebb7` | 213 / 157 | 1739 | Broad historical branch; scoped extraction/integration only |
| `experiment/wsc-steady-path-20261101` | `5809a528` | 200 / 157 | 1742 | Broad historical branch; scoped extraction/integration only |
| `feat/close-cs80-handover-gaps-20261101` | `9d771675` | 75 / 157 | 108 | Broad historical branch; scoped extraction/integration only |
| `finish/cs80-handover-20261031` | `db24a471` | 0 / 157 | — | Fully merged ancestor; nothing to integrate |
| `finish/cs80-qualification-20261031` | `9c62d7b8` | 206 / 157 | 1746 | Broad historical branch; scoped extraction/integration only |
| `fix/cache-drive-controls-20261106` | `54267458` | 1 / 50 | 4 | REVIEW |
| `fix/cache-ensemble-rate-20261106` | `d7dd23ad` | 1 / 43 | 5 | REVIEW |
| `fix/cache-filter-resonance-20261106` | `92ee3ec9` | 0 / 48 | — | Fully merged ancestor; nothing to integrate |
| `fix/cache-tape-controls-20261106` | `cac8c544` | 1 / 50 | 5 | REVIEW |
| `fix/cc121-pedal-rpn-reset-20261106` | `9d78a260` | 1 / 66 | 6 | SUPERSEDED |
| `fix/cc88-release-prefix` | `e6be6af8` | 0 / 27 | — | Fully merged ancestor; nothing to integrate |
| `fix/ci-archive-binary-identity` | `a084c391` | 0 / 20 | — | Fully merged ancestor; nothing to integrate |
| `fix/conventional-pitch-bend-range` | `ac27687f` | 0 / 60 | — | Fully merged ancestor; nothing to integrate |
| `fix/core-reliability-next-20261107` | `b585ae5e` | 1 / 38 | 6 | REVIEW |
| `fix/deb-architecture-guard` | `11d5b82a` | 0 / 22 | — | Fully merged ancestor; nothing to integrate |
| `fix/hermite-minimum-buffer-20261106` | `c110aacb` | 1 / 50 | 3 | SUPERSEDED |
| `fix/macos-installer-architectures` | `0bc1aa43` | 0 / 29 | — | Fully merged ancestor; nothing to integrate |
| `fix/mono-mseg-dormant-onsets-20261106` | `62e11869` | 1 / 50 | 5 | SUPERSEDED |
| `fix/mseg-coordinate-exponent-overflow` | `3c968e2a` | 1 / 19 | 2 | HIGH-PRIORITY REVIEW |
| `fix/poly-mseg-retrigger-rate-20261107` | `06fc9df0` | 1 / 43 | 4 | SUPERSEDED |
| `fix/preset-state-safety-20261107` | `58e6070c` | 2 / 38 | 9 | HIGH-PRIORITY REVIEW |
| `fix/preset-trash-no-destructive-fallback` | `cb33f53e` | 1 / 19 | 2 | HIGH-PRIORITY REVIEW |
| `fix/scala-frequency-domain-20261103` | `f0ae4661` | 1 / 157 | 3 | REVIEW |
| `fix/scala-kbm-session-20261103` | `1d860d1f` | 1 / 157 | 6 | REVIEW |
| `fix/sparse-external-interpolation-20261106` | `49a4c049` | 0 / 49 | — | Fully merged ancestor; nothing to integrate |
| `fix/tuning-input-validation-20261103` | `351ff242` | 1 / 157 | 3 | REVIEW |
| `freebuff/98848ef40b5d961a964f7ad8` | `97c29f93` | 0 / 525 | — | Fully merged ancestor; nothing to integrate |
| `integration/adj-hp-mixer-evidence-20261102` | `b4c175f2` | 201 / 157 | 1737 | Broad historical branch; scoped extraction/integration only |
| `integration/adj-hp-noise-loading-20261102` | `56535703` | 203 / 157 | 1738 | Broad historical branch; scoped extraction/integration only |
| `integration/adj-hp-vr7-vr8-sensitivity-20261102` | `564d4f4a` | 211 / 157 | 1742 | Broad historical branch; scoped extraction/integration only |
| `integration/au-initial-immediate-20261102` | `bebd100d` | 208 / 157 | 1737 | Broad historical branch; scoped extraction/integration only |
| `integration/clean-tuning-audits-20261103` | `5c56e12a` | 1 / 157 | 18 | BUNDLED / STALE |
| `integration/host-automation-finite-20261102` | `6637537a` | 206 / 157 | 1737 | Broad historical branch; scoped extraction/integration only |
| `integration/mseg-correctness-20261107` | `e6a62926` | 0 / 41 | — | Fully merged ancestor; nothing to integrate |
| `integration/mts-esp-safe-20261102` | `745f94a5` | 202 / 157 | 1737 | Broad historical branch; scoped extraction/integration only |
| `integration/mts-esp-sysex-pair-20261102` | `37d4fbc5` | 204 / 157 | 1737 | Broad historical branch; scoped extraction/integration only |
| `integration/mts-esp-windows-max-20261102` | `e3302c69` | 210 / 157 | 1738 | Broad historical branch; scoped extraction/integration only |
| `integration/numerical-release-gate-20261102` | `0e332211` | 201 / 157 | 1741 | Broad historical branch; scoped extraction/integration only |
| `integration/release-entrypoint-preflight-20261102` | `9c6c01cd` | 207 / 157 | 1747 | Broad historical branch; scoped extraction/integration only |
| `integration/release-trusted-main-dispatch-20261102` | `431a4d84` | 213 / 157 | 1749 | Broad historical branch; scoped extraction/integration only |
| `integration/reviewed-parallel-fixes-20261105` | `f7d1b755` | 0 / 66 | — | Fully merged ancestor; nothing to integrate |
| `integration/trusted-release-cpu-producer-20261102` | `e0d71051` | 203 / 157 | 1744 | Broad historical branch; scoped extraction/integration only |
| `investigate/adj-hp-loading-guard-20261102` | `058224fa` | 219 / 157 | 1750 | Broad historical branch; scoped extraction/integration only |
| `main` | `1b146c45` | 0 / 0 | — | Baseline |
| `perf/pra-held-coefficients-20261106` | `54a08a6f` | 1 / 43 | 6 | REVIEW |
| `test/expanded-library-phrase-20261103` | `bdde45e8` | 2 / 15 | 5 | REVIEW |
| `test/numerical-audit-fixture-identity-20261102` | `c55a8757` | 204 / 157 | 1742 | Broad historical branch; scoped extraction/integration only |
| `test/numerical-gate-cpu-fixture-identity-20261102` | `5116fd80` | 206 / 157 | 1742 | Broad historical branch; scoped extraction/integration only |
| `test/oe1-speed-endpoint-20261103` | `1544d717` | 0 / 155 | — | Fully merged ancestor; nothing to integrate |
| `ui-preview` | `e5bb9e50` | 1 / 535 | 69 | Orphan image artifact branch; never source-merge |
| `ui-wip` | `b69d6aa4` | 0 / 317 | — | Fully merged ancestor; nothing to integrate |
| `work/benchmark-preflight` | `f351306a` | 0 / 13 | — | Fully merged ancestor; nothing to integrate |
| `work/bounded-offline-render` | `34ee3086` | 2 / 11 | 2 | REVIEW |
| `work/ci-bundle-reparse-guard` | `640e24f5` | 1 / 14 | 4 | REVIEW |
| `work/cs80-circuit-evidence-sara-20261102` | `abb5bc4d` | 224 / 157 | 1759 | Broad historical branch; scoped extraction/integration only |
| `work/cs80-composed-default-audit-v1-sara-20261102` | `598bbcce` | 283 / 157 | 2264 | Broad historical branch; scoped extraction/integration only |
| `work/cs80-distribution-notices-sara-20261102` | `3c1ea4e0` | 261 / 157 | 2362 | Broad historical branch; scoped extraction/integration only |
| `work/cs80-handover-completion-20261101` | `db24a471` | 0 / 157 | — | Fully merged ancestor; nothing to integrate |
| `work/cs80-host-audit-v2-sara-20261102` | `78da91e7` | 226 / 157 | 1788 | Broad historical branch; scoped extraction/integration only |
| `work/cs80-host-compatibility-sara-20261102` | `b91bb209` | 225 / 157 | 1785 | Broad historical branch; scoped extraction/integration only |
| `work/cs80-integrity-ci-sara-20261102` | `d3e71774` | 231 / 157 | 1789 | Broad historical branch; scoped extraction/integration only |
| `work/cs80-numerical-integrity-sara-20261102` | `81217d5e` | 227 / 157 | 1770 | Broad historical branch; scoped extraction/integration only |
| `work/cs80-reviewed-integration-sara-20261102` | `2ff1b5bb` | 315 / 157 | 2357 | Broad historical branch; scoped extraction/integration only |
| `work/deb-runtime-dependencies` | `635ad6e8` | 0 / 36 | — | Fully merged ancestor; nothing to integrate |
| `work/debian-artifact-qualification` | `27f7166c` | 0 / 31 | — | Fully merged ancestor; nothing to integrate |
| `work/debian-staging-umask` | `9ce36bb4` | 1 / 14 | 2 | REVIEW |
| `work/delay-minimum-hermite` | `c191d225` | 0 / 13 | — | Fully merged ancestor; nothing to integrate |
| `work/macos-installer-fixture-notices` | `5de33483` | 0 / 13 | — | Fully merged ancestor; nothing to integrate |
| `work/macos-staging-umask` | `47b4c954` | 1 / 13 | 2 | REVIEW |
| `work/manual-only-release-entrypoint-prereg-20261102` | `c09eb09a` | 218 / 157 | 1749 | Broad historical branch; scoped extraction/integration only |
| `work/matrix-route-lifecycle-tests` | `c36e6d33` | 2 / 14 | 1 | REVIEW |
| `work/memory-port-reference` | `60975903` | 3 / 12 | 9 | SEPARATE PR REVIEW |
| `work/offline-bundle-verifier` | `053b5983` | 1 / 14 | 4 | REVIEW |
| `work/offline-event-timing` | `ab07eeb5` | 0 / 11 | — | Fully merged ancestor; nothing to integrate |
| `work/packaging-notices-current` | `f45f6426` | 0 / 16 | — | Fully merged ancestor; nothing to integrate |
| `work/pitch-pow-memo` | `425e7c2f` | 1 / 14 | 11 | REVIEW / PERFORMANCE WATCH |
| `work/reaper-host-smoke-sara-20261102` | `dcbb7e11` | 219 / 157 | 1789 | Broad historical branch; scoped extraction/integration only |
| `work/release-numeric-source-lock-prereg-20261102` | `7145e36c` | 222 / 157 | 1754 | Broad historical branch; scoped extraction/integration only |
| `work/vca-zero-ei` | `b14f0a7a` | 2 / 4 | 9 | RECOMMEND |
| `work/windows-installer-preflight` | `68a5024c` | 1 / 14 | 5 | REVIEW |
