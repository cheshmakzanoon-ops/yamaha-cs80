# Assignment 60: original Linux validator crash remains blocking

**Acceptance: BLOCKED.** This is a bounded failure investigation of source
`6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, build `45044040694`, attempt 1.
It does not supersede the earlier preparation snapshot or certify publication.

## Original result

The downloaded, digest-verified 17-file report artifact `14769486678` records
randomized GUI seed `0x4369262`, original validator PID 2883 and native
`subprocess.Popen.returncode == -11` (`SIGSEGV`). The full job log records
`SUCCESS` at `2026-11-22T19:24:05.9974371Z`, followed by the wrapper's refusal
to retry at `19:24:06.3947418Z`. Its normalized exit 139 is a failed required
gate. This is stronger termination evidence than an ordinary process calling
`exit(139)`, but it identifies the failed process, not the faulting component.

The independent headless and fixed GUI seed `0x2da72db` invocations record raw
exit 0. The selected same-seed GDB replay exits normally; the fault-thread
replay also exits 0 and its trace contains only three handler-install notices.
Neither contains an actual fault stack. A handler-install notice is not proof
that the handler remained installed. These are instrumented nonreproductions.

The original wrapper found a kernel pipe collector, so local core capture was
unavailable and it changed no system setting. Linux ignores RLIMIT_CORE for
pipe collectors; the child soft limit of zero does not prove that no collector
was invoked. No original local ELF core is present in this evidence. The helper
does not inspect or redirect the system collector. The wrapper classification
and refusal of a post-SUCCESS retry are correct as implemented.

## What source review establishes

The processor/editor headers and implementations and `ArtCommon.cpp` are byte
identical between baseline `7f55a95` and binary source `6d3cc220`. The processor
destructor removes its keyboard listener. The editor destructor stops its timer,
detaches its LookAndFeel, then destroys the implementation. The reviewed meter,
frame and activity-lamp timers also stop in their destructors; the asynchronous
zoom menu uses a safe component pointer. None of this proves all asynchronous
host/plugin paths are safe.

Function-local typeface/image caches remain relevant lifetime boundaries, not
identified defects. The pinned plugin JUCE source is
`c352e2489000ffeb9e397051377d75eef2efe641`. Its Linux `ScopedRunLoop` registers
and unregisters the shared EventHandler and owns GUI/message-thread resources;
`ModuleExit` maintains the module-entry count and calls a no-op `shutdownModule`.
Actual cleanup also depends on object/reference lifetimes and later dynamic
module/static destruction. These inspected boundaries do not provide the
missing faulting stack.

The current native `cs80_teardown_probe` passed in 0.530732 seconds. It creates
and paints an editor at both sizes, destroys editor/processor, shuts down JUCE,
then returns so static destructors run. It is a standalone executable lifecycle
probe; it does not reproduce pluginval's complete VST3 module-unload/run-loop
sequence. Its pass does not override the original validator failure.

The repository preserves earlier post-SUCCESS failures in GUI, headless and
fixed-GUI lanes, including d0c3 headless seed `0x5ba7253`. A historical replay
trace and immutable-source review make host FD dispatch, handler registration,
and unload ordering falsifiable hypotheses. They do not establish that this
new SIGSEGV has the same cause. In particular, the source-attribution correction
in `docs/VALIDATOR_DIAGNOSTICS.md` must be retained: a locally patched weak-map
guard was not part of pristine pluginval 1.0.4. No current crash is assigned to
the plugin, host, JUCE, fonts, DSP, or the separate Program-queue limitation.

## Actual new upstream comparison

Run `45060638632`, attempt 1, executed all three comparison jobs on workflow
commit `96c466f0e7c25055ac8e745d2c796af643860fc6`; it was not filtered or skipped.
The three report ZIPs were downloaded and checked against their API byte counts
and SHA-256 digests. Each contains 19 files, including provenance, JSON/XML
results, actual process/logging exits, commands and plugin hashes.

| Lane | Job | Seed | Tests passed | Process/log exits | Report artifact |
| --- | --- | --- | ---: | --- | --- |
| Headless subset | 137325542917 | 0x4f31605 | 25/25 | 0 / 0 | 14769098257 |
| GUI inclusive | 137325542836 | 0x63421ca | 31/31 | 0 / 0 | 14769073337 |
| GUI regression comparison | 137325542552 | 0x3ca4063 | 31/31 | 0 / 0 | 14769337392 |

All results have zero failures/errors and all individual test records are
passed. Both GUI reports include Editor, Open editor whilst processing,
Editor Automation and Editor stress. All three provenance records identify
source `6d3cc220`, originating run `45044040694` attempt 1, failed original
conclusion, and VST3 input artifact `14768852300`. Their shared-object hash
`270d91d746d7bc4496c0097755b997f0ba102728c3d17f7050225f1392250265`
matches the separately inspected Linux install candidate.

This comparison used the official `v2.0.0-beta3` archive, recorded validator
commit `37d6a37bc752606c524f7f3febffc73e0945a740`, host JUCE 9.0.3, and validator
binary hash `79612f9dd1a93f24920a7244a46c50bc7ff1bdf8174c013acee19032f5047f24`.
CI records a verified archive hash and no source modifications. The archive
verified locally in this review is the report archive, not a separately
downloaded validator executable. The beta CLI command/test inventory differs
from the required 1.0.4 `--validate-in-process` invocation, and none of its three
seeds is the current failing seed `0x4369262`. Success adds compatibility
evidence for these particular executions; it neither repairs the stable gate
nor isolates crash ownership or proves that the newer host fixes this fault.

## Local reproduction feasibility

No new native validator or display-server execution was attempted. At review,
gdb/lldb/valgrind/coredumpctl/Xvfb/xvfb-run were absent from PATH, DISPLAY and
WAYLAND_DISPLAY were unset, and no official Linux pluginval executable was
found in the two known scratch trees. strace/readelf/addr2line alone cannot
recover a stack from the terminated remote process. The supplied sysroot Xvfb
already failed its earlier `/bin/true` control while opening local/unix sockets.
That evidence does not distinguish the precise system cause, and no permission
or transport workaround was attempted. Remaining space was about 63 MiB at the
recorded snapshot; installing a debugger/display stack or running a broad build
would not be a useful bounded reproduction here.

## Safest next step

Preserve the failed attempt and all current candidates as such. Use an isolated
Linux diagnostic environment that already has a working Xvfb, sufficient space,
and permitted local-file core capture. Verify the frozen input artifact and the
official 1.0.4 executable first. Then make one bounded ordinary GUI invocation
with original seed `0x4369262`, strictness 10 and `--validate-in-process`, with
the original per-test limit and a 240-second outer limit plus 10-second kill
grace. Keep raw termination/PID and the original log regardless of outcome.
Enable only the child's already-permitted core limit; do not change a system
collector, ptrace/security setting or required gate. If a fault recurs, match
the core PID to that invocation and retain only an argument-free all-thread
stack and shared-library/build identity before drawing ownership conclusions.
An exit-zero diagnostic remains a nonreproduction, not qualification.

If this produces no fault, the next justified research direction is bounded,
source-specific logging of FD registration/dispatch/unregistration and module
unload ordering in a separate diagnostic build. It must be labeled altered
instrumentation and compared against the frozen ordinary invocation. Repeating
the already successful GDB replay or replacing stable1.0.4 with beta3 solely to
obtain a green gate would not answer the current causal question.

No CI run was retried/cancelled, no production/workflow/gate was changed, no
large package was installed, and no new core/security/display setting was
changed in this investigation. A release PASS is unavailable while the
required original Linux validator result remains failed.

## Reusable evidence

`summary.json` is the machine-readable assessment. `comparison/` holds the
actual API responses, all three logs, verified report ZIPs and their extracted
contents. `verify-evidence.py` rechecks the joins/digests/results using those
files without executing native code or contacting services. `SHA256SUMS`
indexes this diagnostic directory. Earlier assignment60 preparation and native
duration reports remain separate historical snapshots.
