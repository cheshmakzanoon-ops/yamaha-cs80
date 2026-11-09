# Immediate sixteen-voice release: bounded behavioral fault controls

## Result

The unmutated private build completed **1,418 checks, 0 failures, exit 0**.
All **11 meaningful behavioral mutants** compiled and linked successfully,
completed the same **1,418 checks**, and returned **exit 1** with numerical
failures. A separately reported literal router-only UI guard deletion survived
with **1,418 checks, 0 failures, exit 0** because the existing Engine execution
guard protects the same operation. It is not counted as a detection.

The independent post-run evidence audit passed 347 checks. Production/test
source hashes, the matching core archive, and its original Engine object were
unchanged. No repository production/test files were edited and nothing was
pushed by this work.

| Control | Failures | Exit | Outcome |
| --- | ---: | ---: | --- |
| Unmutated private build | 0 | 0 | Pass |
| Broadcast to both engines | 154 | 1 | Detected |
| Invert global allocation order | 256 | 1 | Detected |
| Lose waiter priority | 3 | 1 | Detected |
| Omit both router and Engine UI host guards | 18 | 1 | Detected |
| Filter host-owned held candidates during UI preview | 18 | 1 | Detected |
| Qualify prior deferred scanner allocations/promotions | 27 | 1 | Detected |
| Accept an unqualified matching candidate | 7 | 1 | Detected |
| Accept mismatched order sources | 1 | 1 | Detected |
| Omit order-exhaustion guard | 4 | 1 | Detected |
| Ignore pending scanner events | 12 | 1 | Detected |
| Reuse waiter order on promotion | 21 | 1 | Detected |
| Omit only the redundant router UI guard | 0 | 0 | Survived; disclosed, unscored |

All rows completed exactly 1,418 checks. The loss-of-waiter-priority mutant
failed the ownership assertion plus extension audio and voice-telemetry
comparisons. The mismatched-source mutant failed the explicit fallback
assertion for distinct sources even with no candidates. It is a guard-contract
detection, not proof of an audible mismatch in that fixture.

## Scope and construction

- Frozen input commit: `1a23a085657a07d9f066f7cc38aaf2572a6fe714`.
- Repository: `/workspace/scratch/ee447c520988/sixteen-note-release-impl`.
- Matching GCC generic Release build:
  `/workspace/scratch/ee447c520988/build-sixteen-release-profile-gcc-generic`.
- GCC 14.2.0, `-O3 -DNDEBUG -std=gnu++20`, AVX2 filter batch disabled.
  Compile flags were obtained from that build's `compile_commands.json` and
  preserved. No compiler flags were changed to force mutant failures.
- Every case uses its own copied source tree and freshly compiled matching
  `Engine.o`, including header changes. The unchanged original test source is
  compiled against that same tree. The link puts private test and Engine
  objects before the unchanged core archive. Each saved link map confirms the
  archive's Engine member was not selected.
- The archive's Engine member hash was checked against its matching build
  object. The private control proves this test/link construction passes before
  interpreting any mutation results.
- Compile operations were sequential. No timing measurements or performance
  claims were made. The promotion mutant produces one ordinary unused-parameter
  compiler warning after the intentional removal; compilation succeeds, and
  detection comes only from the subsequent completed test run.

## Honest interpretation of UI guard deletion

Deleting just the router's `keyboardOnly && !chosen.keyboardOrigin` guard does
not change the tested behavior: `Engine::applyNoteOff` independently checks
`keyboardOnly && !voice->keyboardOrigin`. The surviving literal deletion is
retained in the evidence, not silently replaced or described as detected.

The `ui-host-protection-omitted` control explicitly removes **both** guards. It
is a two-site behavioral omission, not a single-line kill. The separate
`ui-held-source-filter` control adds inappropriate source filtering to the
held-candidate preview and demonstrates that the fixture detects cross-engine
fall-through while the execution guard is intact.

## Deferred qualification wording correction

The executed reproducer and run metadata originally described the
`qualify-prior-deferred-candidates` edit too broadly as deferred/ARP allocation.
Its actual replacement removes `immediate` from `nextReleaseOrder` and changes
deferred scanner allocations/promotions. **It does not change ARP `startVoice`
calls that use default-zero order directly.** No ARP stamp-assignment mutation
coverage is claimed here.

The executed reproducer is preserved verbatim at `run-1/reproducer.py` with
SHA-256 `5c509f1c3585086d68f636409f9a651b72a8f412565ad46c0033c9e8e5c75f47`.
The top-level `check_fault_controls.py` differs only in that description string,
corrected after execution, with SHA-256
`25591c5896e8fab179dc517304e8bba1da568911edd41a4a9c9efaf0e48fbcf3`.
The mutations, compiler commands, assertions and outcomes are unchanged.

## Validation and reproducing

The parser requires exactly one correctly formatted terminal numerical summary,
exactly 1,418 checks, a normal expected exit, and one diagnostic per numerical
failure. Runtime stderr, crashes, timeouts, incomplete/duplicate/nonterminal
summaries, wrong counts and a successful scored mutant are rejected. Two
positive parser controls and twelve invalid-input controls passed. Build
failures never count as detections.

Run `check_fault_controls.py REPO MATCHING_BUILD FRESH_OUTPUT`, using the
paths above and a fresh output directory. `invocation.txt` contains the executed
command. To reproduce the original metadata wording exactly, run
`run-1/reproducer.py` instead. `audit_evidence.py RUN_OUTPUT` independently
checks outcomes, all logged output hashes, source/artifact hashes, fixture
identity and link order. The outer invocation exited 0 and is recorded in
`driver.status`; the audit invocation exited 0 in `audit.status`.

Important files:

- `run-1/summary.json`: machine-readable outcomes.
- `run-1/input-hashes.json` and `after-invariants.json`: input identity and
  unchanged-source checks.
- `run-1/commands.json` and `logs/`: complete compile/link/run commands,
  stdout/stderr, exit codes and log hashes.
- `run-1/<case>/mutation.diff`, `source-hashes.json` and `link.map`: exact
  mutation, private source identity and selected objects.
- `run-1/parser-self-test.json` and `audit.json`: validation evidence.
- `run-1/frozen/`: unmodified source/test snapshot used by every case.

This is a bounded mutation gate for the new immediate release contract, not an
old-version regression build. It does not claim the unmodified old baseline
compiles against the new API. The genuine old real-processor baseline is a
separate experiment. It does not establish complete deferred/ARP coordination,
MIDI conformance, external host/controller behavior, performance or circuit
fidelity.
