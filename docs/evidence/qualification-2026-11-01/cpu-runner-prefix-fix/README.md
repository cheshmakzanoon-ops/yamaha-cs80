# CPU collector mutation-witness correction

Date: 2026-11-01. Published correction: `ef166ea41c309c3d12ae1c7dc27ea3009762b0a0` (local `ec6d25d579e5beb3918f1c0edbfba92f1b7f77fa`, based on published `1e5893854c3d0d9b0aa1ab1c725fd2e8084717f6`).

## Observed failure

[CPU workflow run 41261598525](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41261598525), at `4de251d9fe3b83966cc264367ce768f01670d89a`, failed in [job 124715818941](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41261598525/job/124715818941). Its preflight Python mutation gate completed the first four mutants, then mutant 05 raised `ValueError: historical checkpoint identity or order changed`. The actual decoded [job log](job-124715818941.log.gz) and [job metadata](jobs.json) are retained. The log is compressed losslessly; the hash manifest records both its stored bytes and the exact decoded UTF-8 bytes.

The compilation/timing step never started. The subsequent artifact uploader reported that the evidence directory did not exist, and the [artifact API response](artifacts.json) contains an empty list. This run produced no CPU observations to compare. The prior successful seven-checkpoint run's raw results remain separate evidence.

## Mechanism

The `frozen_prefix_shortened` source mutant removes the runner's minimum-prefix-length guard. Its old malformed-input fixture removed index 6 from the live request.

With the original seven-entry request, that operation leaves the first six historical entries. The unchanged runner rejects the insufficient length; the mutant accepts it because all six entries still match the corresponding immutable prefix. This isolates the intended length invariant.

With the new nine-entry request, removing index 6 leaves eight entries. The appended pre-FIR checkpoint now occupies historical position 7, where the frozen prefix requires `rank-idle-1500`. The independent identity/order guard correctly rejects that shifted entry, even in the source mutant whose length check was removed. The harness correctly refuses to count this unrelated exception as a mutation kill, so it fails before any benchmark build.

## Minimal correction

The derived minimum-length fixture now keeps exactly the first six historical entries, regardless of later appended checkpoints. This changes only the input that isolates the existing mutation. The unchanged runner still rejects missing, reordered or rewritten historical checkpoints through its two separate guards.

The correction also adds `tests/cpu/test-cpu-qualification-runner.py` to the CPU workflow's narrow push-path trigger so changes to its preflight witness requalify the same request. No workflow step, assertion or artifact-error policy is disabled.

The acceptance JSON, collector runner, reporter and request JSON are byte-identical to the failed head. All 27 source mutants remain. All nine checkpoint identities remain: the exact seven-entry historical prefix followed by `475063ca4b5640c1b17cb45eb89a23706ad18873` and `23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162`. [The qualification manifest](qualification.json) records their source hashes and the reproduction outcomes.

## Local reproduction and qualification

The seven-entry input is copied verbatim from the successful run 41257213038's artifact. The nine-entry input is copied verbatim from the failed head and matches the current request. Both inputs are retained here.

| Gate source | Request | Result |
| --- | --- | --- |
| Original | Exact successful-run seven entries | All 27 compiled source mutants killed; sealed fixture unchanged |
| Original | Exact failed-run nine entries | Reproduces mutant-05 unrelated identity/order exception after four completed mutants |
| Corrected | Same seven entries | All 27 compiled source mutants killed; sealed fixture unchanged |
| Corrected | Same nine entries | All 27 compiled source mutants killed; sealed fixture unchanged |

Every mutation must compile/import, accept the original valid control, and expose its intended invariant using normal execution. Syntax errors, crashes, unrelated exceptions and timeouts remain invalid evidence. The four complete local logs are retained alongside the original remote log. These are Python-only runs; this diagnosis performs no DSP build, audio render or CPU benchmark.

## Fresh workflow confirmation

[Fresh run 41262684915](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41262684915), job `124718989712`, passed the same mutation preflight and reached compilation/timing. [The retrieved job snapshot](fixed-run-jobs.json) records preflight success and the subsequent step in progress. It does not assert that the complete workflow has finished.

A complete fresh result remains necessary for the nine-checkpoint CPU comparison. This correction proves the preflight failure mechanism and its removal; it makes no CPU-target or hardware-performance claim.
