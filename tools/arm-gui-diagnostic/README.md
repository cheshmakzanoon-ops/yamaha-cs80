# Frozen ARM GUI abort investigation

Production run43862818521 failed GUI AU Editor Automation twice (jobs133283691068 and133478562383), with AppleParavirtCommandBuffer endCurrentChunk currentOffset assertion, SIGABRT and exit9. Seeds differed:0x6adb8d5 and0x41dd1ce. Other platform gates passed. Root cause remains unproved.

Source audit found no demonstrated direct Metal misuse in plugin code. pluginvalv1.0.4 CommandLine.cpp installs a SIGABRT handler that calls _Exit(9), explaining why usual crash reporting may not capture a useful stack. Its BasicTests.cpp EditorAutomationTest sets every parameter from a background thread1000times. This is stress relevant to repainting, not proof of a particular defect.

This separate workflow rebuilds the exact unchanged production source/settings at baseline5fa6ace, with pinned JUCE and official pluginval1.0.4. It is a fresh build, NOT a claim of binary identity with the unavailable failed-run AU. Only AU target is needed for this diagnostic; production build.yml and all its gates remain unchanged. Recording hashes before/after proves only that these diagnostic inputs did not mutate.

The initial protocol ran one fixed-seed GUI validation under LLDB, breaking at abort before pluginval's signal handler can terminate. It attempted to capture all-thread backtraces and image addresses after execution. No disabled tests, changed renderer, relaxed timeouts or retries. A 300s outer process-group timeout bounded that replay; the 45 minute job limit bounded the job. Debugger launch/permission failure is a blocker, never something to bypass. The initial script failed for a captured abort, missing SUCCESS, input mutation or timeout. A clean run is non-reproduction under changed debugger timing, not a fix and cannot replace the original failed validation.

The original GUI test was preceded by headless tests in other processes. This standalone replay does not reproduce every prior filesystem or runtime condition. Exact source, platform provenance, command, seed and input hashes are recorded. The assertion text plus symbolicated stack is needed before assigning blame to application, JUCE, validator or virtualized graphics. Do not disable Metal or GUI tests merely to turn CI green.

Local mocked process tests exercise reporting/timeout policy; native LLDB launch and macOS reproduction remain actual-runner checks.

## Bounded paired follow-up

The first LLDB run (43926100090) completed Editor Automation and printed SUCCESS;
the inferior exited zero. Its diagnostic step failed only because an unconditional
post-exit backtrace command had no stopped process. It did not reproduce or fix the
original assertion. Those archived logs are preserved, not overwritten.

The follow-up performs exactly one native fixed-seed run, then exactly one LLDB
fixed-seed run, using the same fresh AU and validator on the same runner. Both retain
strictness 10, the full GUI test sequence, seed 0x41dd1ce and the 120000ms validator
timeout. Each execution has its own 300-second process-group limit, separate logs
and pluginval reports. The workflow permits 12 minutes for the paired step and
45 minutes overall. A native failure does not suppress the debugger execution;
changed or missing binary hashes do suppress it because equivalence is lost.

LLDB collects stacks/images only when its inferior is stopped/crashed/suspended,
including through the batch crash hook. A clean exited inferior records its actual
exit code instead. The receipt distinguishes native failure, captured stop,
inferior failure, debugger/launch failure, missing evidence, timeout and clean
non-reproduction. Only two clean non-reproductions with unchanged inputs yield
zero; this diagnostic result never replaces production validation. Missing native
LLDB evidence fails closed. Native execution and debugger execution receipts are
both preserved even when either fails.

Order is fixed, not randomized: the debugger run follows the native run. Cache,
preset or system state, transient runner contention, debugger timing and fresh
build differences remain confounds. Binary identity on this runner does not prove
identity with either earlier failing AU. Opposite outcomes strengthen a timing or
state hypothesis but do not isolate its cause; two clean outcomes remain
non-reproduction. Do not repeat until a favorable result or disable rendering/tests.

Mocked tests cover classifications, both invocations despite native failure,
mutation blocking, launch/timeout handling and the conditional inspector source.
The actual macOS run still must verify LLDB's state APIs and crash-hook behavior.
