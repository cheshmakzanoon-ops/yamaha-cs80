# DC-acquisition intake guard: red/green and independent review

2026-11-02. **Software metadata validation only. No physical observation,
DSP change, source-model retuning or manufacturer acceptance is claimed.**
Following the user's public-source-only scope clarification, this importer
is optional provenance infrastructure, not a request or delivery prerequisite
for unavailable hardware measurements.

The [preregistered contract](ADJ_INTAKE_DC_COUPLING_PREREG_20261102.md)
was committed as `81bef644680fa18d28f049a8af5396544b4ac9e1` before the
production importer edit `91b4f72bf7083fa5798aeea7b00263227e88da0e`.
Both are based on `058224fad844b8f51e211e3b820c0ae2d8f25762`.

These first two identifiers are **local commits**, not GitHub refs. The
GitHub connector recreated commit metadata while preserving exact file
trees. Fresh Git fetch and `git rev-parse <commit>^{tree}` established:

| Local commit | Published commit | Exact shared Git tree |
| --- | --- | --- |
| `81bef644680fa18d28f049a8af5396544b4ac9e1` | [`2effb329b6887870c8e589b404081dc6ec0674fc`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/2effb329b6887870c8e589b404081dc6ec0674fc) | `2bb485a5e1e26c792dbf4cbdf32b04be3e7e6873` |
| `91b4f72bf7083fa5798aeea7b00263227e88da0e` | [`9462fe46dce0a555bbc9d74db16a6fba5984c33f`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/9462fe46dce0a555bbc9d74db16a6fba5984c33f) | `3baa2dabad497fe7cdf95e7b0556c9a8c46edeb3` |

The earlier raw logs retain their actual local source paths/identities;
they have not been rewritten to impersonate a published-commit execution.
A separate clean detached worktree at published `9462fe46` reran the 17
tests: **all passed**. Its [new raw output](dc-acquisition-20261102/dc-acquisition-remote9462.log)
and both tree identities are retained in the manifest.

| Run | Result | Retained raw output |
| --- | --- | --- |
| New tests, unchanged importer | 17 tests, 6 expected failures | [before](dc-acquisition-20261102/dc-acquisition-before.log) |
| Repaired importer | 17 tests passed | [after](dc-acquisition-20261102/dc-acquisition-after.log) |
| Isolated temporary copy, exactly the new guard removed | 17 tests, same 6 failures | [deleted guard](dc-acquisition-20261102/dc-acquisition-guard-deleted.log) |
| Restored production files | 17 tests passed | [restored](dc-acquisition-20261102/dc-acquisition-restored.log) |

The six failures are four DC-node subtests, the mixed-instrument contract
and CLI rejection. All four DC nodes reject an AC-only oscilloscope,
including a synthetic zero reading. DC scope, valid DC meter and mixed
AC-waveform/DC-meter cases remain accepted; `not_acquired` does not
invent an observation. The CLI emits no report for rejected input.

Reproduce from the repository root with:

```sh
python3 -m unittest discover -s tests/bench -p 'test_adj_intake.py' -v
python3 -m py_compile tests/bench/adj_intake.py tests/bench/test_adj_intake.py
git diff --check
```

The mutation used `tempfile.TemporaryDirectory`, copied only
`adj_intake.py` and `test_adj_intake.py`, removed the two-line DC-capability
guard and ran the same unittest command in that directory. The actual
repository repair was not deleted or restored via a working-tree reset.
Synthetic numeric sentinels stayed in tests/temporary directories and are
not published as a hardware observation file. Raw-log hashes and byte
counts are in the [manifest](dc-acquisition-20261102/manifest.json).

Independent qualification review inspected both commits, checked that
the prior observed-without-instrument guard dominates the scope lookup,
confirmed the meter independently requires `DC_V`, and separately reran
all 17 tests successfully. It found no blocking issue in this bounded
repair. A schema-valid report remains unverified external testimony;
neither numeric Yamaha target has changed.
