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
