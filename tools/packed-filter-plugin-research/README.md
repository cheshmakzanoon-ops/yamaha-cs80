# Packed filter plugin qualification preparation

Research only; no production default change or runnable qualification workflow. This is not a
completed plugin qualification or benchmark. See PLAN.md for the prospective
stages and remaining implementation/review gates.

Run preparation checks (GNU compilers required; ELF proof fixtures are not
executed, while explicit SDK/CTest controls execute inert fake programs only):

    PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools/packed-filter-plugin-research -v

Current checks:
- elf_proof.py verifies process function instruction bytes plus all supported
  resolved relocation targets, constants and the helper function body. It is
  intentionally x86-64/ELF/PC32+PLT32-specific and rejects unproved indirection.
  It does not replace archive/map identity or actual plugin execution.
- compile_inventory.py parses actual literal compiler commands and captures every
  listed output object, rejecting missing outputs, duplicate entries, metadata
  conflicts and unsupported response files. It does not prove final linkage.
- identity.py checks complete independently supplied inventories and requires only
  the candidate filter source/object to change. capture_files hashes actual bytes
  with strict owned-relative paths and rejects symlinks/missing inputs.
- roster.py compares the complete current CTest commands/properties and rejects
  disabled tests, omitted/duplicate JUnit cases, failures/errors/skips and invalid
  execution durations.

Neither helper authorizes execution or returns a production-qualified result.
The future orchestrator must independently establish inventory completeness,
freeze quiescent input ownership, prove actual changed-object linkage, retain
actual logs/artifacts, and run original plugin checks. Tests use synthetic fixtures, including tiny compiled/linked but never executed C
fixtures. Separate read-only proof checks used the retained GNU13 standalone
benchmark object/binary pairs (32 stock and 36 candidate relocations). Neither
establishes actual plugin speed, sound or release qualification.

## Configuration-only preflight

The manually dispatched packed-filter-plugin-preflight workflow captures normal
current Linux configuration without applying the candidate or building/running
the synth. It checks the exact reviewed main SHA before setup, uses the same
standard Linux dependency list and CMake flags, records compiler/dependency
identities and compile/link/test plans, and preserves failures. CMake compiler
checks and JUCE's own setup helper are not synth qualification. Unresolved
prebuild test commands are explicitly recorded, never accepted as passed tests.

A configuration capture is only input to the next implementation/review step.
The workflow cannot ship production changes or launch plugin timing.

`filter_outputs` derives the exact changed-source output set from the frozen full compile database. The normal core PIC object is named separately for plugin binding; experimental test variants must never substitute for it. `link_binding.py` joins a unique exact archive member, live GNU map provider and relocation-aware ELF proof. `clean_relink.py` removes only frozen owned outputs after verifying equivalent sealed copies, then requires one successful bounded Commands-ledger build and fresh output hashes. Both require a reviewed orchestrator, quiescent roots and independent full input/lifecycle gates; neither is production admission. See CHECKPOINT.md for the authentic configuration-only result and its unresolved prebuild tests.

`validator_contract.py` preserves Linux release validation breadth (headless, GUI, fixed GUI regression seed), strictness 10, 120000 ms internal timeout, Xvfb geometry and the core-capturing wrapper. It deliberately omits the ordinary release shell's retry behavior: every first failure is terminal in research. Command construction and receipt checking are pure helpers. Their expected argv must come directly from the frozen constructor; actual ledger case uniqueness, complete hash-bound stdout/stderr, validator version/bytes and bounded outer headless/GUI deadlines must be enforced by the orchestrator. A returned receipt is not proof of attempt count, log completeness or qualification.

`admit_resolved_roster` binds prebuild identities/properties to built commands but cannot independently reconstruct the 509 missing commands. Its adjoining `admit_built_tests` checks hash-bound CTest stdout, exact inventory command, raw definition bytes and the complete independently frozen executable set/bytes. Case-to-argv association is now independently bound to the generated literal `add_test` commands by `definition_commands.py`: substitutions of existing executables and argument changes fail. This parser supports only the observed literal subset and rejects dynamic/escaped/multiline forms instead of guessing. Relative executable names need an independently frozen resolution map (the authentic capture uses `bash` → `/usr/bin/bash`). The trusted CTest runtime, source/scripts/dependencies and actual command/stdout provenance remain mandatory; the helper is not a general CMake evaluator.

`validator_runner.py` is an explicit, inert-on-import execution helper, not an enabled workflow. It constructs frozen argv, requires one shared terminal ledger, binds arm-unique cases and complete sealed bundle/validator bytes before and after every invocation, hashes captured logs, and persists every admission failure as terminal. The validated bundle must be exactly the sealed root. The caller must freeze finite per-case deadlines, validator version/tool runtime identity and complete module resource inventory; no Stage A launch is authorized by this helper.


For the complete existing native-fixture capture scope see `capture_roster-notes.md`. For the additive observer's exact implemented and missing pieces see `native_observer_STATUS.md`, and its fake-interface runtime evidence in `native_observer_fake_STATUS.md`. Original native tests and validator breadth remain mandatory. `split_ctest.py` constructs a prospective sequential full-roster schedule only; its toy equivalence check is not real suite evidence. Native initialization/rendering, actual postbuild admission and the reviewed launch workflow are still missing.

The prospective split schedule uses CTest `-T Test` to retain **same-attempt** dashboard `Test.xml`, alongside JUnit and the tag-specific `LastTest_<tag>.log`, before any next invocation overwrites them. `dashboard_exit.py` adds a strict raw-exit gate for the frozen CTest 3.31.6 producer: that version emits exit fields for every nonzero or abnormal exit, even when a pass regex makes JUnit pass. Passed/Completed without those fields implies zero only under that producer's reviewed format. The inert toy covers regex-output plus exit 42, skip 77 and timeout; these cannot pass the exit gate. A separate successful rerun would never prove the first process exited cleanly. Runtime identity, fresh-output/same-attempt binding, hook/config absence and complete log archival remain mandatory orchestration work, not claims made by this parser.


## Native observer preparation checkpoint

With explicit pinned `CS80_OBSERVER_SDK` and `CS80_CTEST` paths, 452 preparation controls pass. These include injected lifecycle fakes, inert dynamically loaded library/export checks, 50 render adapters, 24 canonical render encoding controls and 25 explicit state-contract controls. They do not load the actual CS80 plugin. The render adapters allocate and are correctness-only, never performance evidence. Native loader support has no executable launch entry point.

The pinned official SDK host attribute helper has independently reproduced overwrite and zero-size allocation leaks. Its wrapper is explicitly defect-control-only and inadmissible as the real observer host. See `native_observer_host_LIMITATIONS.md`; a separately reviewed bounded replacement is still required. No dependency or synth code was patched.

The pinned JUCE controller intentionally has no separate serialized controller state. `native_observer_state.hpp` records its exact unsupported/empty result under a preselected policy, while requiring full successful component state and supported controller synchronization. It never treats arbitrary failures as optional. Restore does not restore RNG/DSP history: eventual audio equivalence must compare identical full histories across arms, not assert that one arm returns to initial PCM.

See each `native_observer_*_STATUS.md` for precise limits. Still outstanding: complete conforming host integration, frozen workload and state/reset/transport semantics, UI-thread/message handling, immutable actual module/runtime identity, same-attempt subprocess evidence, production clean-link proof, full original release tests/validators and separately reviewed timing. No production integration or real qualification launch is justified by this checkpoint.

## Bounded host and control workload checkpoint

A separate bounded host implementation now has32 independently reviewed inert controls, including transactional attribute replacement, zero-size data, allocation failure rollback, references, concurrency and exact null-message-ID clear semantics. The original SDK defect-control wrapper remains inadmissible and unchanged. The new host still requires real integration, externally guaranteed quiescence and explicit health/accounting checks; it is not full native interoperability proof.

The data-only workload manifest covers108 configurations and validates complete generated control IDs against a supplied native metadata inventory. Its private offline enum must be explicitly mapped to SDK kOffline, never cast.29 inert flush controls verify exact zero-audio parameter processing and versioned evidence. The flush requires an owned Processing-phase wrapper; real lifecycle/thread integration remains unfinished.

The isolated checkpoint suite totals515 controls with explicit pinned SDK/CTest. No real plugin was loaded, and lifecycle/runloop drafts are deliberately excluded from this checkpoint.

## Phase-owned lifecycle preparation

The initializer now supports an owned Connected metadata checkpoint, one setup and three checked activation cycles with a same-instance saved-state restore only in the first suspension. Complete parameter IDs are retained from the same metadata enumeration. Processing operations use a private phase guard and terminal cleanup; rejected nested operations cannot release objects until the outer callback unwinds. 58 new inert phase controls plus two SDK-enabled workload-bridge controls bring the isolated suite to 575. The concrete bridge requires the owner’s successful Connected metadata, explicitly maps offline mode and reuses the existing flush/render formats; each segment declares project time starting at zero. Fresh-module reload/state transfer, actual threads/message-loop integration, full driver orchestration and real-plugin qualification remain outstanding.

## Standalone Linux runloop preparation

The separate UI-owner-only dispatcher has 40 reviewed controls, including a hung-callback watchdog fixture. It retains callback snapshots, revalidates generations, bounds dispatch/timers, and requires checked shutdown. It does not advertise an optional host interface or load the synth. Actual JUCE registration-thread compatibility is not established, and sampled idle is never treated as global callback quiescence. The isolated preparation suite now totals 615 controls.

## Actual file-identity prerequisite

A separate verifier uses EVP SHA-256 over actual file bytes, comparing the complete declared bundle inventory and explicitly frozen runtime/input files with trusted expected manifests. Its immutable result and reverify checks have 36 reviewed inert controls, bringing this isolated checkpoint to 651. The owned/quiescent-root model is mandatory: this is not race-proof against adversarial writers, does not authenticate a caller's manifest or discover complete dependencies, and does not prove which image dlopen loaded. Loaded-provider binding and fresh-transfer authority remain blocked prerequisites.

## Retained module-file capability

The identity verifier now retains a typed module stamp from its original admitted scan. A separate RAII capability pins a no-follow read-only descriptor, verifies its actual hash/stamp, and reverifies the complete identity before and after pinning. Its 17 new controls bring the isolated suite to 668. The held descriptor is not evidence that dlopen selected it; loaded-provider binding, trusted dependency closure and disciplined borrowed-descriptor use remain required. No actual plugin is loaded by these controls.

## Dedicated processing-worker preparation

The separate real-thread worker has 32 reviewed controls, bringing the isolated suite to 700. It guards queue/await sequencing and UI pumping, retains callback failures and completion timestamps, and distinguishes actual joined cleanup from healthy consumed-task admission. It never detaches hung callbacks. This is not native processing evidence: the current synchronous instance wrapper must not run on the worker because its failure cleanup would invoke UI-only teardown there. A private UI-owned in-flight lease and the full case coordinator remain required.

## Joined inert-library provider checks

The retained-file capability now joins a concrete normal-path loader and bounded ELF/maps checks.63 new controls, including 16 actual inert-library load/close cases, bring this isolated checkpoint to 763. The checks reject preloaded/replaced-path providers and bind actual handle/export mapping offsets to the retained file. They do not prove in-memory code-byte integrity or detect all private/COW modifications, and do not establish complete runtime closure. A sole native owner must keep loader operations inaccessible while instances or callbacks are live. No CS80 module or qualification driver is launched.

## Private worker-to-instance integration

26 additional controls bring the isolated suite to 789. A private UI-owned processing lease now joins the real worker to the existing flush/render helpers over inert SDK instances. It preserves native lifetime through synchronized completion and joins before UI-only failure cleanup, including nested pump misuse. The existing synchronous wrapper remains unchanged by default. Module/handler ownership, the complete history coordinator and real plugin interoperability remain external prerequisites. Clean closure is not full-history admission; no synth or qualification run is launched.

## Original-suite transaction integration

27 real inert-process controls bring the isolated suite to 816. The prospective transaction now joins exact inventory/definition binding, ordered single-attempt CTest, first-attempt exit evidence, retained logs and native capture sealing/comparison. Its public entry requires the complete original 569-case cohort and source-bound 294 native cases. Pre-execution audits reject hooks, stale evidence, special bundle nodes and identity drift; deadlines and bounded failure retention remain terminal. Shared command defaults are preserved, with opt-in exact environments and owned-process-group checks. Complete trusted manifests, build/link admission, canonical arm restoration, runtime closure, non-escaping process assumptions and final artifact budgets remain required. These controls run inert fixtures only, not the real suite or synth. See SUITE-TRANSACTION-STATUS.md.

## Fixed A-D history and map-only build preparation

74 additional controls bring the isolated suite to 890: 25 full/subset-control owner cases, 21 fixed A-D history cases and 28 real inert configure/link cases. The history joins initial controls, A render, saved state, altered B render, restore/C and reactivation/D on the actual worker/instance owner, admitting immutable evidence only after successful cleanup and final encoding. Native runtime coverage remains one inert SDK workload, not the full real grid. The overlay proves unchanged inert compile commands and exactly two added final-link map options. Complete real module ownership/runtime closure, E reload, final build/suite integration, external watchdogs and authentic production configure equivalence remain required. No CS80 build, plugin execution or qualification launch occurs. Candidate release packaging remains deferred until genuine clean source-commit/CI provenance; original packaging checks are unchanged.

## Configured all-target build transaction

22 additional inert build controls bring the isolated suite to 912. The transaction performs actual stock/candidate compile, archive and both-final linking, with verified recoverable stock copies, exact source/two-object change gates and final archive/map/ELF proofs. A frozen cross-arm contract binds configuration/actions/tools/input identities and an environment digest without publishing raw environment values. Complete bounded build-tree checks reject symlinks, hardlinks and special nodes before any command, including compiler depfile/Ninja side-output escapes. This is preparation under trusted, quiescent roots, not a hostile-writer sandbox. Complete authentic configure/resource/runtime closure, real module/history execution and final Stage A integration remain outstanding; no CS80 build or qualification run is launched.

## Configuration and strict validator integration

62 additional controls bring the isolated suite to 974: 38 deliberate bootstrap/normal/map-overlay configuration controls, 21 added validator controls and three configured-tool alias regressions. Configuration retains raw evidence from all three planned stages, enforces exact warm graph/compile/test parity and cumulative retained-byte caps, and labels newly generated baseline artifacts as observed evidence. The validator runner preserves all three original workloads while binding exact environments, resolved tools, scripts and module resources; later cases cannot silently overwrite earlier evidence. Only inert fixtures execute in these controls. The configure-to-build handoff binds compiler/archive aliases and the exact ar/ranlib chain; complete compiler/linker subprocess closure and the private module bridge remain separate review gates, and no real plugin trial or production qualification is launched.
