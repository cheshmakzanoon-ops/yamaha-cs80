# Intermediate host-only V2 audit registration

This is a new request, not a completed capture or numerical PASS. It freezes the
reviewed host integration at source commit
`b91bb20978fff2c2bc0f5a3a745b939b1b23a6d3`, `src` tree
`dbbefcc5ca0eeac5b741a90b916c0e79336246fd`. The actual production-audio anchor
remains `83b7f7670f000a95c6b44f96ffb0df3894755d14`; a host commit is not an audio
repair. CPU, click and remaining engineering work still require later separately
preregistered source identities. Do not silently repin this request.

The separate `.github/full-engine-audit-request-v2.json` retains schema version 1
for the unchanged V1 loader and explicitly declares request version 2. Every V1
command, rate/order, suite count, policy, reporter, numerical contract and
acceptance-policy value is unchanged: 566 phrase, 168 alias, 102 FX, 40 latency and
9 causal rows, 885 total. V1 files and its historical failed scores remain intact.

Original request/reporter/tolerance bytes are pinned to V1 registration
`00da4498792523cf8d97fd40c17e12f215e190b8`. The gate did not exist at that commit;
its separately pinned implementation is the completed original-audit gate at
`39ae6c2684456950302cb4ed10ecfae980c37ddd` (SHA-256
`4cd9d4a453d5edd7069350728e7edb99bf22f7db757c2d1b9d55a2248ade0adf`). The request
also freezes the unchanged V1 workflow and selected source commit's explicit
CMake build inputs. These are integrity pins, not new acceptance thresholds.

Publish this registration first as immutable commit **R**. Only then may a separate
thin wrapper bind the actual published R SHA, validate R/F/audio ancestry,
F/HEAD source-tree equality, source working-tree cleanliness (including untracked
and ignored files), imported gate bytes against R and original V1 identities,
and invoke the unchanged gate. No placeholder SHA or self-selected current HEAD
is a valid registration identity. Preregistered controls are listed in
`tests/release/full-engine-audit-v2-contract.json`.

The wrapper must preserve V1 result bytes and add a separate hash-bound V2
provenance sidecar. Its distinct `audit-v2.complete` marker is written last after
the V1 completion/provenance/raw hashes agree; absence means V2 is incomplete even
if the imported gate already emitted its own marker. The distinct workflow/check
uses read-only permissions and its own concurrency group. No existing V1,
release, numerical-release producer, main or ruleset change is part of this work.

A successful all-row capture still reports every failing numerical score. It is
not final numerical acceptance, real-host qualification, signing, physical
measurement or release authorization. The main-controlled release producer still
selects V1; any later V2 selection needs its own review and source-locked proof.
