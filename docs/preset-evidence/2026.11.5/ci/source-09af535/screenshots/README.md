# Original 09af Hardware panel screenshots qualification

**Qualified:** [run 45344888473](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888473), attempt 1, job 138276946587, actually executed commit `09af5358d34a7ea431de7e8a42e35dae66e52159`. The actual artifact is [14899141895](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888473/artifacts/14899141895).

The original job passed 11,556 core checks and the complete UI self-test. The actual ZIP contains 73 files, including 70 decoded PNGs and 66 top-level gallery PNGs whose producer checksums all match. Both fixed golden candidates are byte-for-byte identical to the unchanged source goldens. The workflow observed newer main and kept this run's screenshots as an artifact; it did not publish this gallery to the repository.

Start with [the acceptance summary](acceptance-summary.json), [the complete file inventory](actual-artifact-members.json), [the source contract](source-contract.json) and [worker 48's review](../../../reviews/a48.md). Original final API receipts, complete job log, artifact smoke log, provenance and PNG checksum list are retained here. [The verifier](verify_actual_artifact.py) recomputes archive/member/pixel checks from the actual downloaded ZIP; it does not substitute a later run.

The native release source `db6b5a76b3c5e38dd9444d0cf6065a14f6302814` differs only in two bundled Markdown guides. All 821 inventoried repository UI/build/test/assets inputs are identical. These observations remain attributed to the original 09af run. This is UI evidence, not native package qualification or an acoustic realism measurement. Only the two fixed fixture candidates are golden comparisons; the rendered gallery has separate coverage.
