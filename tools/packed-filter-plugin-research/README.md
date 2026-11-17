# Packed filter plugin qualification preparation

Research only; no workflow, renderer or production default change. This is not a
completed plugin qualification or benchmark. See PLAN.md for the prospective
stages and remaining implementation/review gates.

Run offline checks (GNU gcc is required for tiny inert ELF fixtures; generated
code is never executed):

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

`admit_resolved_roster` binds prebuild identities/properties to built commands but cannot independently reconstruct the 509 missing commands. Its adjoining `admit_built_tests` checks hash-bound CTest stdout, exact inventory command, raw definition bytes and the complete independently frozen executable set/bytes. Case-to-argv association still relies on trusted actual CTest against unchanged definitions, with a frozen CTest runtime and source/scripts/dependencies. A caller-supplied or forged JSON/ledger is not proof; swapping two known executable names or altering an unresolved case's arguments must never be accepted through that shortcut. These mandatory orchestration gates remain explicit in the receipt.

`validator_runner.py` is an explicit, inert-on-import execution helper, not an enabled workflow. It constructs frozen argv, requires one shared terminal ledger, binds arm-unique cases and complete sealed bundle/validator bytes before and after every invocation, hashes captured logs, and persists every admission failure as terminal. The validated bundle must be exactly the sealed root. The caller must freeze finite per-case deadlines, validator version/tool runtime identity and complete module resource inventory; no Stage A launch is authorized by this helper.
