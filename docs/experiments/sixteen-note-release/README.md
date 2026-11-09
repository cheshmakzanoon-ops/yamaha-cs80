# Immediate sixteen-voice Note Off: reproducible evidence

See [the bounded product contract](../../SIXTEEN_VOICE_NOTE_RELEASE.md) and the
unchanged [pre-implementation plan](PLAN.md). This record qualifies the frozen
source/test checkpoint, not a later documentation-only commit.

## Scope and source identity

- Published base: `e27c34f39320057e09a34005ad2470d1231b4f87`.
- Plan recorded before implementation: `e23931da9a21e5c4480b21a4274d9e994f0669f7`.
- Production repair: `77dfd74b0e80328a4e0f09c01b7636c65519f191`.
- First native fixture: `920cab9219f500ddc6e7f629f94df7f0a00f6dd7`.
  Its compile error was corrected at `fb53a73`; final native fixture source is
  `9e3f9a01d5a49a972a9e76e9e5bb36273f6ebdb8`.
- Final source, core fixture and native CTest registration:
  `1a23a085657a07d9f066f7cc38aaf2572a6fe714`.
- The old compatibility/header checkout is `d2c37f8d2dcad132339c6f8cd2a1a7864ad1f338`,
  not the published-base commit. Its complete `src` tree is exactly the base's
  `a1ee27dbda98a04703ab01fdf8c6fc39e9ea5536`. The candidate `src` tree is
  `079e4a5300f257e03d8b7c99dc8d88743c38558d`.

Only six production files changed: `src/core/Engine.cpp`, `Engine.h`,
`ReleaseOrder.h`, `VoiceModes.h`, and `src/plugin/PluginProcessor.cpp` and `.h`.
`src/tools/ui_snapshot.cpp` changed only for test dispatch; CMake and the two
new test files provide test wiring/coverage. Other runtime source, workflows
and download/build scripts are unchanged. Screenshot preservation is recorded
separately below. No executable, object,
static library or build tree is included in this evidence bundle.

The published screenshot-only child
`b6ab4d37eb8d6c8e8d8c6361d3c9dd4c3db0fee5` of `e27c34f` was inherited by merge
`acee137c0b7f2bb7221ff8344a102f49d015e0cd`. It changes 20 screenshot-related paths.
All **70 screenshot paths, modes and bytes** are preserved exactly against that
child, as recorded in `evidence/screenshot-preservation.json`. Its
`docs/screenshots/provenance.json` identifies published MPE source `e27c34f`;
these images do **not** depict or qualify the sixteen-voice release repair.
No screenshots were rendered or tested by the audio/processor gates. The merge
leaves production, tests and CMake exactly at `1a23a08`, so it adds no new runtime
qualification. The 20 inherited changes are excluded from unchanged-against-base
claims, while all 70 files have explicit screenshot-parent preservation checks.

The repair applies only to immediate, non-arpeggiated dispatch with both scan
queues empty. A matching waiter takes priority over a sounding group; otherwise
one newest comparable held group is released. Order means **accepted allocation**,
not MIDI-ingress chronology. A freshly promoted immediate waiter gets new order
and the existing zero initial touch. The UI source guard still blocks a selected
host-owned held group rather than falling through to another instrument.

Active/pending scanner work and ARP preserve the old broadcast path. A matching
candidate originating in deferred/ARP execution remains unqualified after the
mode is disabled; it is not assigned a made-up global age. Unknown, mismatched
or exhausted order sources also fall back. **Cross-instrument same-pitch release
remains incomplete on those paths.** This is neither complete MIDI conformance
nor a general sixteen-voice release-readiness claim.

## Recorded outcomes

### Genuine old-processor reproduction and repaired probe

`evidence/BaselineReleaseProbe.cpp` ran through the actual JUCE processor in
Legacy and Circuit at both 0 and 32 frames. All four old cases failed normally:
one Note Off released both primary and extension owners. Compile/link statuses
were 0; run status was 1, with exactly 4 checks and 4 failures.

The original probe was compiled with the old checkout's own headers and linked
against matching retained `build-mpe-onset-native` objects. This is distinct
from the separate successful dedicated baseline build in `native-build/`.
After production commit `77dfd74`, the unchanged probe was relinked against the
dedicated candidate objects: 4 checks, 0 failures, all three statuses 0. Its
historical printed label still says “baseline”; the command records and
candidate filenames establish which implementation actually ran.

### Core, compiler and sanitizer coverage

- Final core fixture: **1,418 checks, 0 failures**. The earlier combined
  focused/neighbor CTest run passed 11/11. Its release fixture precedes the
  last added cases; the final focused verbose log, profile runs and independent
  execution separately establish the final 1,418-check fixture.
- GCC 14.2 and Clang 19, generic and `-march=native`, each passed 1,418 release,
  11,508 existing core and 22,085 tuning/MIDI checks. These are Release
  `-O3 -DNDEBUG` builds with AVX2 filter batching disabled.
- A separate GCC build instruments the complete linked core with ASan, UBSan,
  float-cast-overflow, frame pointers and `_GLIBCXX_ASSERTIONS`; the same three
  suites passed. Leak detection was disabled (`ASAN_OPTIONS=detect_leaks=0`),
  and UBSan halts on error. **No native JUCE sanitizer run is claimed.**
- `profiles/commands.json` records all 25 successful configure/build/test
  stages, exact flags, environment, logs and hashes. This is independent
  qualification within each profile, not cross-compiler audio identity.
- Full core CTest run: **144/144 passed in one completed invocation**. Both
  build and CTest statuses are 0; the final log reports 502.31 seconds. This is
  test elapsed time, not an audio CPU/performance measurement.

Core fixtures cover skewed local ages in both engine directions, channels,
repeated strikes, waiters/duplicates/promotions, UI aliases, unison and partial
groups, invalid input, sustain, reset/panic and eight/sixteen transitions.
Fallback witnesses compare with explicit two-engine broadcast, including
prior deferred ownership after scan disable, active ARP fallback, and ARP disable continuity. Separate-source/rebinding,
exhaustion and exact linked-versus-unlinked audio witnesses are also present.
The latter are metadata-inertness witnesses, not substitutes for old binaries.

### Native processor and neighboring tests

At `9e3f9a0`, the corrected actual-processor fixture passed **768 checks,
0 failures**, and five display-independent neighboring CTests passed 5/5:
MPE member onset (976/0), controller reset (1,408/0), undo replay (317/0),
keyboard handoff and voice activity (each reports 0 failures without a count).

At final `1a23a08`, configure/build passed, the newly registered native CTest
passed 1/1 with 768/0, and a separate direct invocation also passed 768/0.
The host fixture and native source are identical to `9e3f9a0`, and the binary
remained SHA-256 `13c7b913f7fdb55bab4967bb355e71d2d7d54f5a18906709836d354f6c55a3e6`
before and after testing. These are distinct source checkpoints with a shared
binary, not a claim that the old run occurred at the final commit.

The initial and two subsequent actual native builds reported no work. A Ninja
dry-run displayed glob/CMake bookkeeping; a subsequent real explained build
executed the glob check and again confirmed no pending compile or link.
The archived status explains that distinction.

Native tests cover real `processBlock` dispatch, release/zero-velocity Note On,
CC64, separate/same offsets, zero-frame blocks, both source engines, UI ownership,
reset/panic/mode changes and non-silent continuation/prefix witnesses. No editor
was opened. There is no GUI, external DAW, plugin-format-host, physical-controller,
CPU/deadline, hardware-fidelity or full native-suite qualification here.

### Behavioral controls

The private-copy unmutated control passed 1,418 checks with 0 failures. All
11 scored behavioral faults completed the full 1,418 checks, emitted matching
failure diagnostics and exited normally with status 1:

- Restore two-engine broadcast: 154 failures.
- Invert global release order: 256.
- Lose waiter priority: 3.
- Remove UI host protection at both router and Engine execution guards: 18.
- Filter host-owned held candidates during UI preview, causing fall-through: 18.
- Qualify prior deferred scanner allocations/promotions: 27.
- Accept unknown-order candidates: 7.
- Accept mismatched shared-order sources: 1.
- Remove exhaustion guard: 4.
- Ignore pending scanner work: 12.
- Reuse waiter order on immediate promotion: 21.

The literal router-only UI guard deletion is **undetected**: 1,418 checks,
0 failures, exit 0, because the existing Engine execution guard is redundant.
It is retained separately and is not counted among the 11 detections. The
UI-protection detection above intentionally changes two sites.

The executed driver's description of the deferred-order fault says “deferred/ARP,”
but that edit changes deferred scanner allocations/promotions only. ARP directly
uses unknown order and is not changed by that mutant. The original script and
summary are retained with this scope correction; no ARP-specific detection is
inferred. Top-level driver wording was subsequently clarified without rerunning;
`run-1/reproducer.py` is the exact executed version. The external experiment's
private source/build directories are not bundled; pinned repository source,
exact mutation diffs, source hash inventories, link maps and command logs are.
The worker report's original scratch paths describe that external experiment.
Its audit script requires those retained scratch artifacts or a fresh rerun.

Driver and audit statuses are 0; the post-run audit passed 347 checks. Every
build/link succeeded. Link maps exclude
the unchanged archive Engine object, and original source/tests/library/object
hashes remained unchanged. The parser rejects 12 malformed/truncated/crashing
result forms. Build errors, crashes, incomplete summaries and unexpected exits
are not detections. The bundle verifier separately enforces the redundant case,
rather than trusting the driver's aggregate gate flag alone.

### Unchanged-path compatibility

For each GCC generic/native profile, the old and candidate builds used their
**own headers and matching libraries**. Each serialized 27,552 finite scalar
values across six ordinary eight-voice scenarios. Each old/candidate pair is
byte-identical within its profile; the generic/native hashes differ.

The driver exited 0. Strict Legacy default/tails, V1 circuit numerics and V2
circuit-engine audio gates each report byte identity against their established
historical references. This is a bounded ordinary-path witness, not exhaustive
state/preset compatibility or a claim that the deliberately repaired path is
bit-identical to the old bug. The four small 27,552-value captures are included as data, not executables.
The larger strict Legacy/V1/V2 output files and all probe executables/libraries
remain external; `compatibility/strict-compatibility.json` records their exact
source pins, sizes, hashes and matching pairs. `--external` checks their retained
workspace bytes without rendering them again.

### Independent review

A separate reviewer approved the production, core fixture and corrected native
fixture source, then independently configured and built a fresh GCC generic
core target: 1,418/0 direct and focused CTest 1/1. Before/after source hashes are
identical. `evidence/independent-review/` records commands, statuses, source
hashes, binary/library identity and source/document-review conclusions.

The reviewer subsequently reran the exact hash-matched native binary: direct
768/0 and registered CTest 1/1, both exit 0, with clean canonical source and
unchanged binary verified. No build was run in that review; existing no-work
build evidence was inspected. The reviewer did not rerun sanitizers or the
full core suite. Earlier source-only review statements remain preserved in
`review-before-native.json`; the later record identifies the added execution.

Final evidence-bundle review is separate and is not inferred from source
approval. Independent fault review verified all 13 cases, private source/header/link
provenance and parser behavior under normal and optimized Python. It independently
reran two retained hash-matched mutants: broadcast (1,418/154, exit 1) and unknown
candidate (1,418/7, exit 1). Other mutation outcomes were source/log reviewed,
not independently rerun. No builds or source edits were made in this review.
The separate integration review verifies unchanged qualified source/tests/CMake
and six reviewed documents, plus all 70 inherited screenshot paths/modes and
68 PNG hashes. It repeats no runtime tests and makes no remote-CI claim.

## Failed and incomplete attempts retained

- The original 4/4 real-processor failure is the product regression, not a
  fixture problem. The repaired 4/0 probe does not replace its old output.
- Native fixture `920cab9` failed to compile because a braced initializer list
  mixed enum types. No new fixture ran and no stale binary was counted. The
  correction is test-only; it is not a production defect or a passing test.
- Core configure attempts first selected a Python CMake entry point without
  its module, then mistook a Ninja package directory for the binary. The
  correct already-installed executable paths resolved those tool errors.
- An initial core fixture named private `Engine::kNumVoices`; correcting the
  test to the existing eight-voice convention changed no production API.
- The first core runtime fixture had 1,326 checks and one incorrect late-binding
  expectation. Reset deliberately preserves prior-use allocation age, so late
  binding remains denied. Corrected test expectations and additional cases
  produced the final 1,418/0 result. `core/ATTEMPTS.md` retains the progression.
- The independent reviewer initially lacked `cmake` on PATH, then used the
  existing installed path. No build had started on that lookup failure.
- Independent fault review initially stopped on a whole-driver hash mismatch
  before any reruns. The exact scanner-versus-ARP description-only change was
  then verified against the retained executed script before review continued.

## Integrity and reproduction

From the repository root:

    python3 docs/experiments/sixteen-note-release/verify.py
    python3 -O docs/experiments/sixteen-note-release/verify.py

The verifier uses explicit errors, including under optimized Python. It checks
all archived bytes, frozen repository inputs and the structure/completeness of
recorded outcomes. It runs no production tests or builds and makes no new
runtime claims. `--external` additionally hashes retained workspace binaries,
libraries and raw outputs; those paths are historical provenance, not portable
download URLs or a claim that executables are bundled. A manifest is an
integrity index, not a signature or independent authentication.

To reproduce runtime results, use `1a23a08` in a fresh directory when that
commit is available, or use the published integration whose source, test and
build-input hashes match this manifest. The local qualification commit ID is
not a promise of remote reachability; equivalent published source can run the
core/native CMake gates normally. After verifying equivalent input hashes,
adapt absolute workspace paths and expected frozen-commit guards in **fresh
copies** of the drivers. Never rewrite the archived drivers or historical
results. For the standard core run, configure CMake with Ninja, Release,
`CS80_BUILD_PLUGIN=OFF`, `CS80_BUILD_TESTS=ON` and
`CS80_ENABLE_AVX2_FILTER_BATCH=OFF`, build with two jobs, then run CTest.
`evidence/qualify_profiles.py` records the full compiler/sanitizer matrix;
`evidence/core/run-complete-core.sh` records the full-suite invocation.

Compatibility uses the pinned old source and its own built libraries, the
unchanged `docs/experiments/mpe-member-onset/CompatibilityCapture.cpp`, and
`evidence/run_compatibility.sh`. Rebuild both old and candidate libraries from
their respective source trees before using a different checkout or machine.
The old library provenance is inherited from the earlier MPE qualification;
its source tree is checked here, not relabeled as a new old-library build.

Native bootstrap reuses official JUCE 8.0.11 commit
`c352e2489000ffeb9e397051377d75eef2efe641` and hash-verified official Debian
packages extracted locally. Package pins and exact configure/build commands
are retained; [the earlier bootstrap](../mpe-member-onset/evidence/native-bootstrap/README.md)
contains dependency acquisition provenance. No download, dependency edit or
system setting change was needed for this increment. Xvfb was present for
unrelated CMake registrations, not used to run these focused checks. The local
source override is not eligible for CI-only dependency provenance validation.
