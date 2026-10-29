# Linux validator fault diagnostics

The original strict validator gate is unchanged. Diagnostic reruns never rescue
a failed attempt, even when they exit normally.

## Seed replay
pluginval v1.0.4 supports `--random-seed [hex or int]`, verified in its tagged
[CLI implementation/help](https://github.com/Tracktion/pluginval/blob/v1.0.4/Source/CommandLine.cpp#L138-L147).
Both GDB and preload reruns parse the final original GUI log seed marker. Only a
bounded, nonzero numeric seed is accepted. Missing/malformed/zero markers are
explicitly reported as different or unmatched seeds; they never fall back to
an earlier attempt's seed. Same seed does not reproduce thread scheduling.
Earlier GDB reruns chose new seeds, so their normal exit cannot isolate debugger
timing as the reason for nonreproduction.

## Fault-thread preload
On each non-cancelled Linux validation job, CI builds a small shared library
against official Ubuntu libunwind-dev and checks fixtures, even when normal
validation passes. Only after GUI gate failure, one additional GUI run uses
the same downloaded pluginval and built plugin, bounded to 240s plus 10s forced
termination grace. LD_PRELOAD is set on the validator command, not its timeout,
Xvfb or env launcher; any descendants it spawns can inherit it. No release/plugin
code is changed. The shim is not a validation result.

A constructor warms the local unwinder and installs SIGSEGV/SIGBUS/SIGABRT
handlers. The handler starts from the supplied signal context, emits at most 32
instruction-address/raw-symbol/offset frames to inherited fd198, then uses
`_exit(128 + signal)`. This is a **numeric diagnostic failure**, not native
signal termination. It does not resume corrupt code or request a core.

Only documented signal-safe local libunwind routines and fixed buffers are used
in the handler. Output uses bounded writes, not stdio, demangling, glibc
backtrace, dladdr, allocation or file opening. No argument values, locals, data
registers, environment or memory/core files are logged. Instruction addresses
are intentional frame metadata, not a register dump. Diagnostic logs stay
outside the strict wrapper's verdict stream.

## Limits
- Preload/dependency changes can alter timing and memory layout.
- The host/plugin may replace the signal handler after installation; installation
  text is not proof it remains installed. A regression fixture demonstrates this.
  No signal API interposition or forced reinstallation is used.
- A lock-free atomic recursion/concurrent-fault guard exits immediately on nested
  entry; SA_NODEFER permits that entry. Corrupt stacks, handler replacement and
  unwinder faults can still prevent a useful trace. No blanket guarantee that
  the existing kernel collector can never run is made.
- No per-thread alternate signal stacks are installed. Stack exhaustion can
  prevent handler execution. Only the faulting thread is captured.
- Stripped/unloaded binaries or corrupt unwind metadata can truncate a trace.
  Symbol names can identify nearby labels, not necessarily the exact function.
  Address-only frames may remain unresolvable under ASLR; they do not alone
  identify a module.
- A stalled unwinder/write is bounded by the external process timeout.
- Exit0 without a fault trace is inconclusive. Frames are evidence to investigate,
  not attribution of the original crash to host, plugin, JUCE or another library.

No sysctl/core_pattern/ptrace/security settings are changed. No raw core,
environment, memory or register dumps are uploaded. Existing kernel collector
behavior remains outside this helper; child core limits do not suppress piped
collectors on Linux.

References:
- [glibc2.35 removal of catchsegv/libSegFault](https://sourceware.org/pipermail/libc-announce/2022/000033.html)
- [libunwind signal-context initialization](https://www.nongnu.org/libunwind/man/unw_init_local%283%29.html)
- [local unw_step signal safety](https://www.nongnu.org/libunwind/man/unw_step%283%29.html)
- [Ubuntu local symbol lookup and limitations](https://manpages.ubuntu.com/manpages/noble/man3/unw_get_proc_name.3.html)

Fixtures check normal0/7 exits, named raised signal numeric139, genuine faulting
stores into a private PROT_NONE mapping on the main thread and a pthread worker,
faulting-instruction frame identity, raised SIGABRT/SIGBUS numeric134/135,
frame/byte bounds, closed diagnostic fd, later handler replacement, timeout124 and seed
selection/rejection. Fixtures are CI tests, not hardware or sonic-fidelity tests.
