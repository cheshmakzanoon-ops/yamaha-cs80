# Hosted libm inspection, not memo qualification or timing

This separately requested workflow collects evidence needed to decide whether the optional pitch memo can be studied on a new runtime. It never builds Engine, enables the memo, measures performance, changes host settings, or promotes source. Current glibc2.41 proof cannot be assumed to cover Ubuntu's runtime-selected implementation.

The fixed request refers to candidate46812652364337aec1c8d51443427d1b63df5351 only as provenance. The actual compiled program contains the original `std::pow(2.0f,x)` wrapper and inspection/status tests, not any memo code. A new narrow `request.json` main-branch push trigger and manual dispatch are supported. No DSP change or prior research request triggers this job. Attempts after1 are refused. Contents permission is read-only, actions are pinned, credentials not persisted, no installs/network/privilege changes. Timeout is15minutes, each command180seconds. Artifacts uploaded on failure contain diagnostic evidence, not approval.

## Binding and supplementary observations

The non-PIE strict-GCC fixture reads the actual powf GOT slot and compares it with `dlsym(RTLD_DEFAULT,"powf")`; `dladdr` identifies mapped library/base/resolved offset. The runner verifies the wrapper's tail jump to `powf@plt`, that PLT entry's jump to the same GOT slot, and that slot's dynamic `powf` relocation. Unexpected LD_PRELOAD, LD_AUDIT or LD_LIBRARY_PATH is rejected rather than cleared. Resolved implementation/function bytes and library/build IDs, not a version string or ASLR address alone, establish a reviewable identity.

Record complete libm text disassembly, a4096-byte-address-range resolved-function window, symbol tables, libc/libm/loader SHA256 and build IDs, CPU/IFUNC capabilities, compiler and executable/source hashes. A window alone is not a function boundary or complete control-flow proof: inspect the complete disassembly and any branches beyond it. Unknown/interposed/mismatched binding stops collection; a recognized binding still requires independent semantic review.

Supplementary dense grid:2,097,168 `pow(2,x)` calls over finite[-120,120], all four DAZ/FTZ combinations and four x87 rounding modes. Normal results, errno sentinel preservation, x87 status/control and MXCSR controls are checked. This sample grid is empirical evidence, not proof for all admitted values. Full normal-domain control-flow and error-tail exclusion require independent review; SSE/AVX-only observation is not inferred from a successful test alone.

`RESULT.json` says evidence collected, review required. It cannot authorize memoON. Any later cost host must independently match the specifically reviewed library hashes/build IDs/resolved IFUNC offset/function bytes and wrapper contract before enabling the option. Drift or unknown implementation stops; no fallback result may be presented as memo speed evidence.

Local validation requires parser unit tests and an actual inspection run. No CPU timings are collected. The locally generated executable is not uploaded; its digest is retained. Runtime library binaries are not copied, only their hashes and textual diagnostics. Artifact retention requested30days, not permanent preservation.

`lscpu` is optional metadata: a nonzero exit is preserved (some container /sys views are unavailable), while a missing executable or command timeout still stops the inspection. /proc/cpuinfo, process affinity, runtime binding and all proof assertions remain required. This is not a relaxation of a binding or numerical gate.
