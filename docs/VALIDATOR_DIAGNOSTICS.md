# Validator crash diagnostics

The original validation result remains authoritative: an exit-zero run with an
explicit clean SUCCESS is required. A diagnostic rerun never replaces a failed
validation attempt. The existing bounded retry eligibility is unchanged and is
not evidence of crash ownership.

Each actual Linux validator invocation is wrapped by run-pluginval-with-core.py.
It runs in a private temporary directory with absolute executable, plugin and
report paths. Only the child's core resource limit is changed, bounded to 1 GiB
when the existing kernel pattern writes a local file and its hard limit permits.
No sysctl, kernel core pattern, collector configuration or security setting is
changed. Linux pipe collectors ignore RLIMIT_CORE, even a zero soft limit;
existing collector behavior is not claimed to be disabled.

A local ELF core candidate can produce an argument-free thread backtrace and
shared-library listing. Its PID is not automatically matched, so the log explicitly
labels it an unverified candidate rather than ownership proof. No raw core,
environment, local-variable, register or memory dump is uploaded. Private core
files are removed locally when possible; cleanup/optional diagnostic failures
cannot replace a completed child process's exit status.

The separate failure-only GDB rerun checks for a live inferior before requesting
a stack. A normally exited process is not misclassified merely because registers
are no longer available. CI fixtures check exits 0, 7 and 139, an unwritable
optional log, normal-exit GDB handling, and actual signal propagation only when
the kernel is not using a pipe collector.

This is diagnostic infrastructure, not proof that a crash is in the validator,
the plugin, or a particular library. Tests printing SUCCESS before process exit
do not turn exit139 into a pass. Record exact source, validator version, run,
attempt and evidence when diagnosing intermittent failures.


## 2026-10-29 checkpoint: post-success exit remains a failure

Commit `f89de6382b40c278e02497cc439178ca442c9dc7`, [build run40925492958](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40925492958), Linux validator job123632463354:

- Headless seed `0x382bb75`: SUCCESS and exit0.
- GUI seed `0x2afb526`: SUCCESS, then exit139; this gate failed.
- Known state-regression seed `0x2da72db`: SUCCESS and exit0.
- Same-seed GDB and fault-trace diagnostic replays exited0. Nonreproduction is inconclusive; they do not replace the failed original invocation or establish ownership of this particular crash.
- All four platform build/test jobs, Windows validation and both native macOS VST3/AU/GUI validations passed. DSP, compatibility and screenshot workflows passed. The overall build workflow remains failed because of the Linux GUI process exit.

The previous executable checkpoint `1962ae5308521c6fa03ea9c3040cc9cada5472a4` passed all its workflows; that does not erase this later intermittent failure. No assertion failure was logged in this invocation, and no new crash stack was obtained. Pluginval remains the official released1.0.4; its latest release was rechecked on2026-10-29. Do not relabel a post-SUCCESS crash as a pass, retry until green, or infer a fixed teardown defect from successful diagnostic replays.
