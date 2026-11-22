# Final read-only development-link acceptance

**Accept the four development links with the explicit unresolved Linux GUI failure. A qualified versioned release remains blocked.**

The reviewed native source is `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, tree `88ee322bf66b380c4227e224821783f2db04bbee`, build **45044040694**, attempt **1**. All four native jobs passed. Actual CTest totals are **1,286 registered, 1,279 passed, 0 failed, 7 formal skips**, with **24 nested Python skips** recorded separately. Windows and both Macs passed required stable validators. Linux stable 1.0.4 strictness-10 GUI validation failed: seed **0x4369262**, PID **2883**, stdout SUCCESS followed by raw **SIGSEGV −11 / wrapper 139**. Headless and fixed-seed GUI gates passed. Successful instrumented replays do not supersede the failed gate; crash ownership remains unresolved.

Retained API metadata, four installation archives, full public GET receipts, both complete validator-input payload joins, and the actual **24-WAV / 117-second** audition reconcile to that source and run. Install artifact IDs are ARM **14763761497**, Windows **14763872941**, Intel **14763644216**, and Linux **14768807632**. Validator inputs are Windows **14763658388** and Linux **14768852300**; audition is **14769036590**. Archive coverage is **114 inventory entries / 10 native headers**.

The initial detailed review verified **293** manifest entries. Two completed beta comparison reports were then added verbatim; the subsequent full pass verified **295** file sizes and SHA-256 values with zero mismatches. The qualification JSON and all four prose hashes were unchanged.

Publisher **45060638607** is **completed/skipped** following the failed originating build. The qualification record blocks the versioned release and records the coordinator's API observation that no `presets-2026.11.1` tag or release existed. The original d0c3c23 failure, seed **0x5ba7253**, remains separate from the current first-attempt GUI failure.

## Preserved review correction

The initial draft's sole finding came from requiring literal **0x4369262** in every prose file. README already contains all current links, exact source/run/attempt, experimental/unqualified Linux status, first-attempt post-SUCCESS SIGSEGV and both exit values, non-curative diagnostic replays, blocked release, skipped publisher, and links to detailed seed-bearing records. This was an overly strict review predicate, not a substantive omission. No README change was required. The original draft remains byte-for-byte in `acceptance.draft-20261122T194007.json`; `acceptance.json` records its hash and the correction.

## Separate comparison and bounded next step

Beta run **45060638632** used the same 6d Linux input with official **v2.0.0-beta3**. Its three lanes passed **31/31, 31/31, 25/25**, with all process/capture exits zero. None replayed current failed seed **0x4369262**; different tool/JUCE/seeds mean this does not replace stable qualification. The separate capture proposal recommends original-process core capture only for a future authorized diagnostic invocation, retaining any failure. No CI change or rerun was performed.

This final review reconciles retained evidence; no product tests, archive downloads, audio listening, or remote release-list queries were repeated. No repository files were changed.
