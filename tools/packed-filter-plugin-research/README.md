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
