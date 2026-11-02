# Supported MTS envelope results (2026-11-02)

Registered production-unchanged head75eb14848bb952c5b8342a289712ae5fad7a613c
(source335930af02743a272b0da6ddd0544024794ea296) ran at15:39:29 UTC:
**2420 checks,1140 ordinary failures,exit1**. Failures are malformed high-bit
bytes/counts, excess payload and reserved octave-mask bits: each is wrongly
accepted and/or changes the128-entry sentinel. All valid independent table
oracles, directed/broadcast device checks, truncated-message atomic controls,
no-change/zero-offset controls and permitted original01 checksum variants pass.
No source, old test, threshold or data has been changed.

Before implementation, add complete127-entry positives for all three counted
variants, proving the maximum legal count still works. This supplements rather
than replaces the original2420-check RED.

The first supplementary2426 replay overtook its queued test commit between
separate lock acquisitions. Its receipt names75eb148 while hashing the new
test bytes. That raw red-maximum run is retained as UNQUALIFIED provenance,
not a clean commit baseline. No production code had changed.

After test commit7fca00e594998849f52f9d2abbd4e3c657f98164 was terminal and
the tree was clean, the exact test/header blobs were checked against HEAD and
the supplemented fixture was freshly compiled under one uninterrupted lock.
It produced **2426 checks,1140 ordinary failures** with all six complete127-
count controls passing. See red-maximum-clean; this is the qualified extended
baseline. Future write/commit/test transactions retain one lock or await the
preceding commit explicitly.

## Narrow repair and intended guard removals

Productionc552559d0e02f0171a8eca7ee10e72636e21a078 / source
0cdd66ff03ad2e359dddde97472af22928bdf019 adds20 validation lines before the
unchanged table-writing consumers. Exact supported layouts are checked first,
so the following seven-bit scan is bounded by the one-byte count. Reserved
first-mask bits are rejected; original01 checksum tolerance is retained.

The unchanged2426-check fixture passes. All five isolated actual-header mutants
compile/link and fail their exact intended ordinary assertions: seven-bit guard
1036 failures; bulk length8; counted length24; octave length32; reserved mask40.
All2426 checks execute in every run. Original/restored positive executable bytes
match and both pass. See qualification-ordinary contract/summary and per-case
receipts. No source is edited during mutation.

## Sanitizer capability and existing regressions

The first ASan/UBSan-instrumented invocation aborts with LeakSanitizer reporting
that it cannot operate under ptrace. There is no assertion summary; this remains
UNQUALIFIED, not a sanitizer pass. Agent5 reviewed the narrower diagnostic:
the IDENTICAL hashed executable passes2426 checks with detect_leaks=0, only
address/undefined local coverage. Leak checking and native sanitizer gates are
still pending. No security/ptrace setting or CI gate is changed.

A fresh CMake Release core build passes all six registered focused CTests at
15:56:31 UTC: envelope, counted messages, realtime semantics/preservation,
original tuning/MIDI, Scala float bounds, and KBM/tuning input boundaries.
The first command guessed two wrong target names and stopped at Ninja setup;
that failure is preserved separately. Corrected exact targets compile the real
Engine/FactoryPresets and tests. Source/header/test identities, commands and
binary hashes are retained. CMake O3 emits a GCC14 STL vector-insert warning in
the test-only excess-payload builder; its source vector is always nonempty by
construction (minimum16 bytes), and address/undefined checks cover every case.
No warning suppression or production-source workaround was added.

This is bounded parser acceptance/rejection evidence. Bank/program/channel
semantics, new message types, all-DAW behavior and full composed-source numerical
qualification are not claimed. Independent final review and root integration
remain pending; original numerical source locks are untouched.
