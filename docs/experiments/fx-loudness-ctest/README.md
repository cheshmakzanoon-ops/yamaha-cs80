# Scoped CMake/CTest integration smoke

On 2026-11-13, local source `c0b17dea57b14436418337a93f55df8176993475`
was configured once in a fresh directory with plugin builds OFF and core tests
ON, Release/Ninja. Independent source review first checked the configure path,
selected target dependencies, and exact CTest selection. No downloads, plugin
builds, Engine target builds or unrelated suites were requested.

Only `cs80_fx_loudness_core_tests` and `cs80_fx_loudness_gate_tests` were built.
The machine-readable dry list was checked to contain exactly these two before
execution. Both passed once under CTest: **2/2 tests, 60/60 internal assertions**
(42 lifecycle and 18 closed-gate). CTest added zero prerequisite tests.

This closes the CMake registration gap left explicitly unexecuted in the
historical standalone reports, without rewriting those reports. It does not
qualify the full registered suite, whole core engine, plugin, effect models,
platform portability, loudness standards, CPU target or hardware fidelity.
Timing printed by CTest is incidental runner output, not a performance result.

See `result.json` for frozen source/binary identities, exact commands and exits;
`list.log` for selection; `build.log` and `compile-commands.json` for the scoped
build; and `test.log` for full assertion output. `run.py` is the retained one-shot
runner: its recorded original workspace is provenance, not a portable default.
Use a fresh directory and adjust the workspace/toolchain paths for reproduction.
Do not overwrite an earlier attempt. All four steps exited zero and source hashes
remained unchanged. Binaries/build tree remain local, not committed.
