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
