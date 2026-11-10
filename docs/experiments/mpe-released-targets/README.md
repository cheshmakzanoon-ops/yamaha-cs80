# MPE released-target retention: evidence and reproduction

This record accompanies the [bounded contract](../../MPE_RELEASED_TARGETS.md)
and [pre-implementation plan](PLAN.md). The recorded bounded qualification is
complete. Source/fixture and fault reviews are recorded separately from the final evidence-bundle review; no
remote CI or publication success is inferred from these local results.

## Source identity and preserved product paths

- Untouched baseline: `2ade213bc4dec34b04352063a72e97dac4cc5667`.
- Plan: `271a902`. At `3003569`, “physically held” was clarified to the
  existing accepted engine gate, including generated ARP gates. The original
  KAS/ARP accepted-dispatch clause already stated this scope. The plan is not
  byte-identical to its first version. Production repair: `c0219bd0de8559d72fce4194a849414de5ac7e61`.
- Focused core/native fixture freeze: `300356958b3cde471f8dc14a7b505070a4cffc8c`.
  Native execution, independent focused execution and fault controls identify
  this checkpoint, not the later neighboring-test correction.
- Final neighboring-test freeze: `04597f5ee87da1f6390a8682298a1a0e656f834c`.
  Only `tests/TuningMidiTests.cpp` and `tests/PolyPressureRoutingTests.cpp`
  differ from the focused freeze. The production and focused fixtures are
  byte-identical. Final broader qualification must identify this later pin.

Production changes are confined to `src/core/Engine.cpp` and `Engine.h`.
`src/tools/ui_snapshot.cpp` changes only test dispatch; CMake and two new
fixtures wire the coverage. This is not a claim that the entire core is
unchanged. `evidence/source-pins.json` records exact Git paths, modes, blobs,
SHA-256 hashes and sizes for **320 preserved entries**, including plugin source,
other core source, experimental/reference/vendor source, assets, artifacts,
installer/scripts/workflows and screenshots. The verifier also inventories all
source, test and CMake inputs against the final test freeze.

At the original qualification, all 70 screenshot paths are unchanged from
the reviewed baseline, including
its 68 PNGs and existing provenance. Those inherited screenshots depict the
previous MPE work, not this repair. No screenshots or GUI checks were produced
by the audio qualification. Presets and parameter/state definitions are among
the preserved source/assets; sampled compatibility is not exhaustive preset
or saved-state qualification. A later explicitly recorded screenshot-only
publication integration is documented separately in `INTEGRATION.md` when
present. It may substitute only the verified screenshot subtree for current
checkout checking; original run/source pins and non-screenshot preservation
remain unchanged.

The new eligibility rule applies to ordinary bend, channel pressure and CC74
on a **currently effective MPE member** only while a selected voice's accepted
engine gate is held. Raw caches still update; CC121 explicitly resets bend and
pressure targets on active released/pedal-held recipients. Shared Channel
Pressure matrix and CC74 ribbon routes remain live. KAS accepted dispatch and
ARP-generated gates define eligibility. This does not repair deferred/ARP
cross-instrument release ownership or inactive-extension expression handoff.

## Completed focused results

### Old-code behavioral reproductions

The untouched core probe compiles against the old checkout's own headers and
matching old core archive. Compile status is 0; normal run status is 1 with
**48 checks, 24 expected retention failures**. The actual old JUCE processor
probe independently gives **16 scenarios, 256 checks, 64 retention failures,
zero fixture failures**, with successful compile/link and normal exit 1.
They are behavioral failures, not setup errors or mixed-header comparisons.

The unchanged processor probe against candidate objects passes 256/0. The
complete new core fixture gives **7,488/0** on the repair and **7,488/1,076**
normal intended failures on the old implementation. Its explicit unchanged-path
subset passes **842/0** against the old archive.

### Focused core and actual processor

- Core: **7,488 checks, 0 failures**. The five focused tests pass;
  `core-focused-ctest.log` is the retained complete **LastTest.log**, not the
  usual CTest console summary. The command/status is in the focused manifest.
- Native processor: **6,048 checks, 0 failures** through actual `processBlock`.
  Direct and registered CTest both pass on `3003569`. Six focused native tests
  pass together, using the same binary SHA-256
  `11fbae61a11d603c50e0f8d915e50d3731c42eacdd19e0378e79d36884130cf4`.
- A separate reviewer executes the existing core binary directly and through
  CTest, recording 7,488/0 plus 842/0 unchanged-path checks and unchanged binary
  hashes. This is independent execution, not an independently rebuilt core.

Coverage includes both engines/zones and full-zone endpoints; held, release
and Sustain I/II tails; overlapping channels/unison; cache seeding of later
onsets; in-flight smoothing and paired non-silent tail continuation; malformed
inputs; current role/mode changes; nonzero CC121 resets; deferred pre/post
release and ARP gate/future-step boundaries; and explicit global exceptions.
Actual processor cases cover ordering, equal/separate event offsets,
zero-frame callbacks, zero-velocity Note On, pedals and already-active
sixteen-voice instruments. Zero-frame events share the clamped boundary, so
nominal distinct offsets are not elapsed separation. Sixteen-voice audibility
is finite/non-silent **combined output plus target-engine state and identity**,
not isolated extension audio. Single-note/core paired cases supply isolated
non-silent tail checks.

### Unchanged-path raw witnesses

GCC 14.2 and Clang 19, generic and native profiles, use each revision's own
matching headers/archive. Eight builds and sixteen final runs each produce
**270 scenarios, 6,529,556 checks, zero failures, 7,167,448 scalar fields and
57,339,584 capture bytes**. Most checks are finite-scalar checks. The count
must not be confused with 6.5 million independent semantic scenarios.

Each baseline/candidate pair and each revision's two block partitions are
byte-identical **within that profile**. There is no cross-profile identity
claim. `evidence/compatibility/COVERAGE.md` states the exact cases and excludes
ordinary post-release effective-member events deliberately changed here.
Native/16-voice host execution, KAS/ARP timing and enabled AVX2 batching are not
covered by this unchanged-path witness. Large raw captures remain external;
bundled comparison/provenance/logs record the original equality results, and
`--external` checks the retained raw files by size and hash without rendering.

### Completed behavioral fault controls

An unmutated private-copy control passes 7,488/0. Each of eleven scored faults
finishes all 7,488 checks, emits the matching number of failure diagnostics
and exits normally with status 1:

- Omit pressure/bend/timbre held guards: 272 / 364 / 536 failures.
- Treat pedal hold as a held gate: 336.
- Omit the CC121 reset exception: 64.
- Incorrectly filter manager/gap pressure and timbre routes: 52.
- Suppress shared pressure or the ribbon adapter: 12 / 12.
- Skip released-member pressure/bend/timbre cache writes: 132 / 132 / 232.

The exact old Engine.cpp/h control gives 7,488/1,076. That control uses the
current unchanged focused fixture and the matching remaining core archive
objects; it is not a complete old historical binary build. The earlier
untouched probes separately establish the old implementation's defect.

All compile/link stages succeed; link maps reject accidental inclusion of
the archive's original Engine object. Original source/fixture/archive/object
invariants remain unchanged across the run. Twelve malformed, crashing,
incomplete or wrong-exit result forms are rejected by the fault parser.
Private trees/binaries/objects remain external; exact mutation diffs, source
hash inventories, link maps, commands and full logs are bundled.

Independent fault review validates all thirteen cases and source/link
provenance, then reruns only the hash-matched omitted-pressure-guard (7,488/272)
and omitted-CC121-exception (7,488/64) binaries. Other outcomes are reviewed
from source/logs, not independently rerun. No build is repeated by that review.

### Compiler, sanitizer and complete core suite

At final neighboring-test freeze `04597f5`, all four GCC 14.2 / Clang 19
generic/native profiles pass seven targets each. The separate GCC sanitizer
build instruments the complete linked core with ASan, UBSan,
float-cast-overflow, frame pointers and `_GLIBCXX_ASSERTIONS`; the same seven
targets pass. Leak detection is disabled and UBSan halts on error. This is
**seven sanitizer targets**, not an all-145 sanitizer suite or native/JUCE
sanitizer qualification.

Each profile passes 7,488 released-target, 2,501 member-onset, 3,513 controller
reset, 1,418 sixteen-voice release, 11,508 core, 22,085 tuning/MIDI and 4,504
poly-pressure checks. Final `profiles-final/commands.json` retains 41 successful
stages: 35 test runs, five builds and one sanitizer configure. The four
ordinary profile configure/core-build records are separately retained in
`profiles-preparation/`. Generic Release uses `-O3 -DNDEBUG`; native adds
`-march=native`. AVX2 filter batching is disabled. No cross-compiler audio
identity is claimed.

The final complete default-core CTest invocation passes **145/145 in 462.29
seconds**, with build and CTest status 0. That is test elapsed time, not audio
performance. The initial 143/145 invocation remains archived separately.
`full-suite-final/source-bracket-and-builds.json` shows all 411 tracked
`src/`, `tests/` and `CMakeLists.txt` inputs match the earlier `profiles-final/inputs.json` and
the final tracked source diff is empty. It also hashes 131 build artifacts and
the complete final LastTest output. These are **before/after snapshots
bracketing the run**, not continuous filesystem enforcement or a freeze
implemented by the full-suite script itself.

Strict compatibility also passes all four byte comparisons: Legacy default
and tails against `d56911a`, V1 circuit numerics against `22d6a32`, and V2
circuit-engine audio against `00ceccf`. Exact command/status, historical pins,
raw artifact sizes/hashes and original local paths are retained under
`evidence/strict-compatibility/`. The larger raw outputs remain external.

### Independent review

Source and focused-fixture reviews are in `evidence/independent-review/`.
They distinguish source inspection from execution and disclose review limits.
The final two neighboring test corrections were reviewed separately. The later profile audit verifies all 35 recorded test outcomes and independently
reruns only the hash-matched sanitizer released-target (7,488/0) and corrected
poly-pressure (4,504/0) binaries. It independently byte-compares all four
strict reference/current output pairs, without rebuilding or rendering them.

The final full-suite audit checks all 145 complete console/LastTest entries
against registration, the 411-file source bracket and 131 post-run artifact
identities. It repeats no full suite or build. Fifteen referenced repository
scripts outside that temporal bracket currently match the freeze; this is a
current check, not a retrospective before/after claim. Generic/native names
are compiler code-generation profiles, not native processor qualification.

Final evidence-bundle review is a separate step and is not inferred from
source approval. No independent rerun of the entire compiler matrix or all
145 tests is claimed.

## Failed, superseded and incomplete attempts

Historical output is retained without replacement:

- Initial core fixture: 7,474 checks, 40 failures. It requested ARP 20 Hz while
  the parameter clamps at 16 Hz, so the assumed gate/step times were premature.
  Correcting timing and strengthening a timbre oracle yields 7,488/0 without
  production changes. The first raw log and explanation remain archived.
- The first native build used an earlier fixture header while its settling
  time and labels were corrected. That binary is hash-indexed externally and
  was not behaviorally tested. A separate rebuild used the stable header.
- A candidate native setup gate stopped on a Ninja dry-run's glob bookkeeping.
  It did not run tests; a real explained build established no pending work.
- The first final native direct test passes 6,048/0, but CTest exits 8 with
  “No tests were found” because configuration predated the registration.
  Explicit reconfiguration from frozen source resolves it. Both attempts remain.
- Initial broad qualification exposes superseded old oracle assumptions:
  released own-channel expression after current-role change, and using member
  Channel Pressure after release as a reference for direct Poly Pressure.
  The exact old test files, correction diff and initial binary identities are
  retained under `evidence/superseded-oracles/`. The original full run finishes 143/145 with only those two failed tests
  (6 tuning and 72 poly-pressure assertions). Final corrected runs remain
  distinct. No production fix was needed. The corrected poly-pressure oracle
  also passes 4,504/0 against old headers/library. An ad hoc old-code tuning
  build first fails because its command omitted CS80_TUNING_DATA_DIR; after
  adding the recorded definition it normally finishes 22,085 checks with the
  6 intended retention failures. Neither is a passing old tuning result.
- `evidence/native/README.md` retains an early “candidate not run” readiness
  paragraph followed by later completed sections. Its final attempt ledger is
  the later outcome; the historical paragraph is not the final status.
- `evidence/staging-observations/` contains an in-progress driver-log snapshot
  captured while preparing this bundle. It is not terminal qualification.

## Integrity, external artifacts and reproduction

From the repository root:

    python3 docs/experiments/mpe-released-targets/verify.py
    python3 -O docs/experiments/mpe-released-targets/verify.py
    python3 docs/experiments/mpe-released-targets/reproducibility/self_test.py

The verifier checks all bundled bytes, source/test identities, exact preserved
paths/modes/blobs, and completed-result structure. Checks use explicit errors,
not Python assertions. It performs no build, audio render or product-test run.
The mutation self-test must reject corrupted, missing, duplicate, truncated
and misattributed evidence in both normal and optimized Python. A manifest is
an integrity index, not a signature or proof of independent execution.

`--external` additionally checks historical local binaries, objects, archives,
raw captures and omitted private mutation source trees. These are **not bundled**
and are not required for normal fresh-clone integrity verification. Their
absolute paths are original provenance, not download links or portable build
inputs. Recorded hashes cannot by themselves reproduce an external execution.
No MPE PDF, third-party binary, object/archive, capture or private mutant tree
is redistributed. The official specification link/hash remains in the product
contract and the earlier MPE source record.

For runtime reproduction, use `04597f5` or a published integration with the
same checked source/test/build hashes. A local qualification commit is not a
promise of remote reachability. Use a fresh output directory, CMake/Ninja,
Release, `CS80_BUILD_PLUGIN=OFF`, `CS80_ENABLE_AVX2_FILTER_BATCH=OFF`, build with
two jobs and run CTest with `--no-tests=error`. `reproducibility/README.md`
provides portable commands and explains the archived drivers' local-path
requirements. Fresh runtime results are new evidence, not replacements for
these historical runs.

No performance/deadline, external DAW/controller, GUI, hardware fidelity,
complete MPE conformance, complete native suite or native sanitizer claim is
made. Deferred/ARP scheduler coordination remains separate work.
