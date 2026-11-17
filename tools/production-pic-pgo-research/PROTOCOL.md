# One-shot production-PIC / real-plugin qualification

Prospective research only. No production CMake or DSP default changes. The earlier strict-PGO experiment and its result remain unchanged. Actual configuration-only run44067129187 passed on main9467d903:587 compile entries (one Engine,586 ordinary including21 C units),569 tests, normal/generate/use command scope, patched JUCE identity and GNU13.3 integer-only profile plumbing. This study is the separately reviewed production-PIC assessment, not a rerun of that earlier non-PIC experiment.

## Boundary and one-shot controls

One first run/first attempt on a free standard public Linux x86-64 runner.300-minute hard experiment deadline;330-minute job ceiling includes setup/artifact time.90minutes is a progress milestone only, not a compatibility abort, automatic pass or excuse to reduce coverage. Every subprocess has the remaining global deadline and any declared shorter timeout. No retries. Nonzero exit or timeout latches terminal failure and kills the newly created process group, including descendants. Existing production CI is untouched.

Production sources/tests/CMake trees, current scripts tree and the normal build.yml verification workflow are frozen by Git object ID, with clean checks before/after. Record CPU/platform/allowed-affinity/selected-core/runner-image metadata and hashes of all research/imported strict-study helpers, header and workflows; recheck helper bytes before cost. GNU13.3.0 driver and cc1/cc1plus identities must exactly equal the independently observed preflight hashes in frozen-compiler.json. Record source revision, commands, compiler/cache/configuration and raw test definitions. Ordinary GNU Release production options remain unchanged: Engine uses -O3 -DNDEBUG -std=gnu++20 -fPIC; benchmark driver uses its actual CMake -fPIE options. No native-ISA, unsafe/finite/associative math, contraction change, LTO, coverage fallback, profile correction or partial-training exemption. FP option queries must agree across modes.

The opt-in deferred overlay changes only Engine.cpp profile options and research link-map output. All587 compile entries, paths, definitions and ordinary command vectors must remain identical. Exact source/ordinary object byte hashes are checked before and after profile builds and qualification. The real production dependency is static libxml2 at96498992efa48d52b0e8b83058bd88dbdaf153c1, alongside JUCE8.0.11 atc352e2489000ffeb9e397051377d75eef2efe641. Both dependency HEADs and working-tree content manifests are retained/compared across configurations, including the normal JUCE host/state patches. A pinned dependency HEAD alone is insufficient.

## Build, training and sealing order

1. Build normal baseline VST3/Standalone, benchmark, five core regressions and original Sub/Ring host fixtures with CMake/Ninja. Build the qualification-only external FP/PCM driver once from the original stock driver with its actual compiler options. Link it against the baseline PIC core. Preserve baseline artifacts **without executing held-out audio/tests**.
2. Reconfigure generation at the same canonical source/build/Engine.cpp.o path. Remove the owned old Engine object/core archive and rebuild only cs80_core, never an instrumented plugin. Link a separate trainer using the unchanged ordinary stock benchmark driver and instrumented PIC core archive, with the generation runtime only in that trainer.
3. Train once: unchanged stock quick performance suite,48kHz/128,16held callbacks, selectors0/1/10,15cases/36rows.300-second invocation limit. A fresh empty profile directory is mandatory. Freeze/copy the single recursively inventoried Engine.cpp.gcda before any held-out observation; no second training, merge, relocation or profile mutation.
4. Reconfigure use at the same canonical object path with fatal missing-profile/coverage-mismatch diagnostics. Preserve baseline copies, remove candidate Engine/archive/VST3/Standalone outputs and maps, then run actual verbose CMake target builds with normal JUCE moduleinfo post-link steps. This forces a fresh candidate link rather than accepting a coincidentally plausible stale map. Ordinary object hashes stay equal. Link the candidate qualification driver from the same driver object.
5. Seal both arms' actual Engine object, core archive, benchmark, qualification/core test executables, maps, Standalone and entire VST3 bundle including moduleinfo. Retain bytes, not merely hashes. Only now begin held-out qualification. No rebuild between qualification arms.

Check archive-member byte identity and the actual three-argument Engine::render symbol through live GNU map sections into final VST3. Preserve verbose link argv/exit, fresh output evidence, maps, ELF symbols/exports/dependencies and binary hashes. Require VST3 factory/module exports and no profile-generation runtime in assessment artifacts. Map/symbol checks alone are not execution proof: every actual host invocation is separately bound to the sealed module/bundle hash.

## Mandatory compatibility before timing

Both arms run original full baseline51rows and performance100rows at44.1kHz/256,16held callbacks. Compare every non-timing field, every PCM byte including warmup and the external-render FP/errno transcript. The shared external wrapper is qualification-only, never in training/cost; it adds no internal lifecycle or callback hooks. Each invocation has300seconds.

Run unchanged CMake-built Smoothing, ActiveTailContinuity and MpeMasterSmoothing tests with120-second limits; SourcePerformanceAudit and FilterBatchEngine with180seconds. Original stdout/stderr must match and exits must pass. FilterBatchEngine uses only --write-corpus, never --benchmark; complete nonempty state/PCM corpus must match.

Preserve the original12 Sub/Ring CTest command vectors **and build working directory**, all six rate/mode cases per fixture, original180-second timeouts and assertions. Restore the sealed baseline complete bundle into the exact canonical VST3 path, verify it before/after every invocation, then restore the candidate bundle likewise. No path substitution or rebuild. Capture directories must start absent. Require exact native12-scenario/zero-failure footers and no FAIL/ERROR; never --compat-only. Preserve and compare all462 declared captures (41Sub/36Ring per case),32768bytes each, no extras/symlinks. These cover declared noqueue/flattened-policy/historical-lane output, not every active automation path; original active automation assertions remain required separately.

Candidate stays canonical after its host tests. Baseline/candidate sealed artifacts, profile, original ordinary objects and raw CTest definitions are rechecked throughout.

## Native cost gate

Unchanged production-linked benchmark, full performance100rows,44.1kHz/256,128held callbacks, stock callback-CPU timer. Pin each measured process to one verified allowed CPU. No builds/tests run concurrently with timing. Require clock validity and exact non-timing work/output fields.

Primary24 source-performance warmed-held rows use unseen selectors2..9, three scenarios each. Three fresh A/A pairs must each pass original2% primary/source-performance/all aggregate,3% isolation/legacy aggregate and10% individual-row limits. Failed calibration is terminal, with zero candidate cost executions. Then only three A/C pairs, orders A/C,C/A,A/C. Every primary pair improves and geometric primary gain is strictly greater than max(0.5%, twice largest absolute primary A/A deviation). Secondary aggregates may regress at most2%; individual rows at most5%. All raw results retained. No result-based row exclusion, retraining or retry.

The result is production-PIC **core** CPU, not measured DAW/plugin-host CPU or a cross-platform claim. A compatibility/cost receipt is preliminary. Only a scoped cost pass proceeds to full release qualification; a failed/noisy cost result never becomes an approved production optimization.

## Complete candidate release gate

Build remaining ordinary targets against the unchanged sealed candidate archive. Recheck canonical Engine/archive/benchmark/Standalone/maps and whole bundle before/after this build and all later tests. Any candidate byte change invalidates the experiment. No retraining, retiming or replacement build is permitted.

Run the complete original569-test suite serially, preserving commands, properties and timeouts. Raw CTestTestfile.cmake texts/hashes must match the validated configuration after all builds; this binds the509 command vectors omitted by prebuild JSON. After full build every test must have a command. Require exit0 plus retained JUnit containing exactly569 unique expected names and zero failure/error/skipped entries. No filtered, empty, partial or skipped run counts as success. Retain LastTest.log and final inventory.

Then run normal demo render, UI smoke and direct pluginval1.0.4 strictness10 headless, GUI and required GUI state seed0x2da72db. Use normal GUI/Xvfb options and120000ms validator timeout; outer limits600seconds for randomized headless/GUI,240seconds for seeded GUI. Each direct invocation gets one attempt and must exit0 with an exact SUCCESS line and no FAILED occurrence. No crash diagnostic, retry wrapper or blocked lifecycle instrumentation. Run normal packaging self-test and Linux packaging only after these gates; packages remain experimental artifacts, not an automatic release/default change.

Overall PRODUCTION_PIC_QUALIFIED requires every gate within the300-minute hard deadline. Any failure/timeout leaves release qualification false, retaining partial receipts and actual artifacts. No automatic promotion, other-architecture support, portable profile distribution or consumer CPU claim is authorized by this study.
