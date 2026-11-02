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
