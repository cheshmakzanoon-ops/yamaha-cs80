# Linux UI smoke environment

## Observed failure

The screenshot job for commit `932c6d4781cad174d90fcc6297b11e685238393d`
([run 40994561144, job 123864558184](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40994561144/job/123864558184))
built successfully and wrote all 13 editor PNGs, then exited 1 with:

```text
BadAtom (invalid Atom parameter)
Major opcode: 18 (X_ChangeProperty)
Atom id: 0x0
```

There was also an earlier, separate stale tooltip assertion, corrected in
`fd00da6`. Successful PNG output was not a passing smoke run. Linux CTest without
any `DISPLAY` likewise could not exercise the new native confirmation dialog.

For comparison, Apple Silicon job `123864558336` at the same commit completed
the full UI smoke (26.32 seconds) with exactly one reported failure: that stale
tooltip assertion. The preset/dialog, metadata and readiness assertions produced
no additional failures there. This is targeted cross-platform evidence, **not**
a clean UI-suite pass or validation of the Linux harness.

## Source diagnosis and limits of the evidence

The new preset tests open the real overwrite confirmation after the screenshots.
Previously the screenshot harness painted editor components without attaching a
native desktop peer. In official JUCE 8.0.11, `AlertWindow` constructs
`TopLevelWindow(title, true)`, which immediately creates such a peer. An
`associatedComponent` only supplies context/placement; it does not avoid peer
creation. Merely adding a host peer would therefore encounter the same path.

In the pinned official JUCE source
[`juce_XWindowSystem_linux.cpp`](https://github.com/juce-framework/JUCE/blob/c352e2489000ffeb9e397051377d75eef2efe641/modules/juce_gui_basics/native/juce_XWindowSystem_linux.cpp),
the atom constructor uses `getIfExists("WM_PROTOCOLS")` (`XInternAtom` with
`only_if_exists=True`), and native window creation unconditionally uses that atom
in `XChangeProperty`. A bare Xvfb server has no window manager establishing these
protocol atoms. That source path matches the observed opcode, zero atom and first
native-dialog timing. This is a source-backed diagnosis, **not a locally executed
Xvfb reproduction**: this workspace lacks the required GUI build/runtime tools.
The wrapper logs atom absence/presence and manager readiness. The successful
native CI run below now supplies that runtime evidence directly.

## Supported test desktop

`scripts/run-ui-smoke-linux.sh <executable> [arguments...]` creates its own
`1920x1200x24` Xvfb display, starts Openbox inside it, and waits at most 15 seconds
for `_NET_SUPPORTING_WM_CHECK`. It verifies `WM_PROTOCOLS` before launching the
unchanged test executable and logs the before/after atom observation. Dependencies
are the official distribution packages `xvfb`, `xauth`, `openbox` and `x11-utils`.

Both Linux `cs80_ui_smoke` CTest and direct CI screenshot commands use this
wrapper. macOS/Windows commands and locale-preservation fixtures are unchanged.
No JUCE source, X-error handler, preset assertion, validator, retry policy or test
exit code is changed. The CTest timeout remains unchanged. Startup/readiness
failures fail the invocation; test exit codes propagate. Cleanup targets only
the Openbox PID and temporary log created by that invocation; `xvfb-run` owns its
private X server. No user's existing desktop/session settings are altered.

## Verification status

Shell syntax passed, and all seven controlled fixtures in
`python3 tests/test_ui_smoke_environment.py -v` passed: successful launch and
argument forwarding, preserved child failure, missing dependency, window-manager
startup failure, missing protocol atom, unsuppressed X-query failure, and the
15-second readiness deadline. Every launched fixture manager PID was reaped.
These fake-tool tests do not stand in for native GUI execution.

Native Linux verification **passed** at commit
`54a96f2af4bc5ce3fe8fd68695afa8ddaec8961e` in
[screenshot run 40995783622, job 123868460296](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40995783622/job/123868460296).
The 2026-10-30 UTC log records:

- **03:13:52:** `WM_PROTOCOLS` absent before window-manager startup.
- **03:13:53:** Openbox ready on `:99`, with `WM_PROTOCOLS` present.
- **03:14:10:** `UI SMOKE TEST PASSED`, with process exit 0 and a successful job.

This establishes the atom transition and successful full screenshot/UI smoke
execution for the harness fix, without suppressing X errors or removing checks.
It does not establish completion of the separate cross-platform main-build
queue, nor validate the subsequent source-performance UI or initial-sizing
changes, which are not part of this verified snapshot.
