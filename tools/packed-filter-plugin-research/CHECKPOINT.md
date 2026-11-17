# Preparation checkpoint: November 17, 2026

Base: 0cb62b51 (audited standalone packed-coefficient result). No production
source, build workflow or candidate default is changed by this package.

Validation:
- 58 offline tests passed on local GNU gcc 14.2.0. Tiny C fixtures are compiled
  and linked only, never loaded or executed. These are harness tests, not synth
  execution or plugin qualification.
- Read-only ELF proof against authentic retained study 44106223173 GNU13.3
  standalone artifacts passed both arms: stock process size 2879, 32 proved
  relocations; candidate size 3878, 36 proved relocations. Candidate object
  against stock final binary is rejected. These observations do not qualify an
  actual VST3 module.
- Independent review found JUnit container/counter inconsistencies, arbitrary
  addend shifts, unproved helper-internal relocations, and an imm8 -5-to--4
  addend asymmetry. Each now has a fail-closed check and regression test.
- Constant accesses must match explicitly supported instruction forms and fit
  entirely in the verified constant entry. Unknown forms, helper relocations,
  unsupported indirection and dynamic rewrites are rejected, not masked away.

Independent final review reproduced all 58 tests and both authentic proofs,
and rejected all ten real-artifact -5-to--4 mutations (five per arm). Approved
for preparation-only checkpoint publication, not execution or qualification.

Outstanding: complete current source/dependency/link
inventory preflight, archive/live-map/fresh-link binding, owned single-attempt
orchestration, real plugin host equivalence, full exact-candidate suite and
pluginval, and separately frozen actual-plugin timing. No new study launched.

## Configuration preflight checkpoint

Added a manual-only exact-SHA public Ubuntu 24.04 configuration capture and ten
offline controls, for 68 reviewed tests. Independent review reproduced those
controls and approved one explicitly selected configuration-only dispatch. The
normal workflow's dependency list and CMake flags are preserved. No candidate
patch, synth target build, training, rendering or timing is performed.

## Configuration-only preflight, 2026-11-17

Run [44123289466](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/44123289466), attempt 1, completed successfully at exact revision `6ef4d109bdfa6d976818c2ac033be5ffd4825d67`. This was the separately reviewed one-shot configuration capture, not a production-plugin experiment. No duplicate dispatch or retry was made.

Artifact `14347365131`, 795114 bytes, SHA256 `9fe19b246c6ab9471171d93406653cd50567df6ae2e8501d6b634bfc8d3a7516`, was downloaded and verified against the service digest. All 41 manifest entries were independently rehashed; the complete 587-entry compile database reparses identically. There are 569 unique enabled configured tests, of which 509 executable commands are unresolved before build. These are **not passed tests**. Six raw CTest definitions enumerate exactly the same 569 identities. An independent reviewer checked all 7059 tracked source names against the exact commit and SHA256-verified 839 relevant Git blobs. All 14 command ledger entries exited zero; clean-before/after receipts are empty. GNU 13.3.0 and pinned JUCE 8.0.11 identities were captured. Compiler and dependency byte hashes are runner receipts, not locally reverified binaries/dependency copies.

The fresh inventory exposes two filter outputs: the normal PIC core object and an experimental-recovery cap-test object. Identity admission now requires exactly the independently derived set of changed filter outputs; production archive/map/ELF binding must select only the normal core output. This corrects the earlier single-object assumption without allowing unrelated changes.

The new pure archive/live-map binding and recoverable clean-relink receipt helpers remain preparation, not qualification. They cannot establish source/object inventory completeness or actual host lifecycle/audio equivalence by themselves. The relink helper has no CLI entry point and is not called by either configuration-only workflow. No candidate was applied to production source and no synth/plugin target was built, loaded, or timed by this preflight.
