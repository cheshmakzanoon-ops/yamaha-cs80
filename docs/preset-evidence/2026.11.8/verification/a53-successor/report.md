# A53 follow-up — final-source audition verification preparation

**Prepared for source 1b5b3c70471e369481b1f813880a6bcc762a2f57**, tree
`b235b67a801d33e9572076b8dfcfbb89e71fac29`, native run **45457535944 / attempt 1**,
Linux producer **138670916222**. Actual audition archive acceptance is pending.

The preserved ef1 packet's 16 manifest entries were rehashed unchanged. Reviewed
successor copies change only five content-checker pins: source, tree, run,
producer job and edition. The other four helper/test files are byte-identical.
The complete `src` tree, CMake/native workflow and audition exporter remain
identical to ef1; the source review records exact hashes and the four
non-documentation paths that changed.

## New build and inventory evidence

A genuine clean Release core/capture rebuild used **parallel 1**. Successful
fresh configure, clean and capture build commands each returned original
**exit 0**. The post-clean check found **zero** core/capture objects/products
before compilation. Exact commands, timestamps, raw logs, hashes and output
identities are retained in `clean-build-receipt.json`.

Two prior local configure failures remain under `history/`, each with original
exit **1**: Ninja was unavailable, then three committed reference documents were
absent in the sparse checkout. The installed Makefiles generator and exact
committed document materialization resolved those setup causes. No source,
checksum or native test gate changed.

The independently executed inventory command returned **0** and generated
**710,475 bytes**, SHA-256
`399d4c1e8fc1d638f11a22dccb9643a77026acb40ea7fe2a96c66b940938b621`.
It is byte-identical to the earlier recovered inventory. The newly observed
capture executable pre/post hash is `3b6f0cc6…3e0a1`. The source is clean and
exact before/after. Receipt SHA-256:
`6e2929072752477ef8a3321065dc568a323e13b85bd30d2e93ca5d8e8057dcd0`.
It records a local recovery execution separately from the original native run.

## Gates and verification

The existing **20 content and 7 CI-binding methods freshly pass**, with original
command exits and log hashes retained. The exact **256/95/60/321/320/1,487.2 s**
contract, all **523 × 204** inventory values, complete PCM/frame/settings/
provenance/metrics checks and original CI/upload identity gates remain intact.
Missing actual archive input returns **1** and creates no receipt.

A46 independently checked the new source/inventory/build receipt and log
bindings. All prior source evidence remains separately attributed. No actual
audition ZIP, public download or native platform is qualified by this packet;
no listening or acoustic-equivalence claim is made. No refs were changed.
The packet and compressed actual inventory are published as immutable Git blobs
for root's integration. Operational detail and reproduction commands are in
`verification/a53-successor/README.md`.
