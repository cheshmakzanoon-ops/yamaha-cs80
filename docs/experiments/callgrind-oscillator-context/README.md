# Can deterministic guest instruction counts stand in for native oscillator work?

**Not for this exact caller context on the tested tool.** The ordinary shipping fixture sets x86 MXCSR to `0x9fc0`: exceptions masked, with flush-to-zero and denormals-are-zero. A narrow probe using the genuine [immediate-rank candidate](../wsc-immediate-rank-scalar/README.md) admission object reads the control status, performs a real inexact double division and reads it again.

| Execution | Before division | After 1 / 10 | x87 control | Candidate guard |
|---|---:|---:|---:|---|
| Native GCC 14.2/Linux x86-64 | 0x9fc0 | 0x9fe0 | 0x037f | admitted |
| Valgrind 3.24 / Callgrind | 0x1f80 | 0x1f80 | 0x037f | admitted |

Both calls retained errno 19 and x87 status zero. The guard accepting both ordinary masked-nearest controls does **not** mean that the guest reproduced the host's requested denormal controls or the inexact sticky flag. This is an actual counterexample to using this tool under the planned *native-context fidelity* condition, not an oscillator defect or a statement about other instruction-count experiments. No comparative candidate instructions, core CPU time, native-plugin timings or speedup were reported.

The free, official [Debian 13 amd64 Valgrind package and checksum](https://packages.debian.org/trixie/amd64/valgrind/download) were verified and extracted locally, without installing a system service, changing security settings or enabling hardware counters. Package: `1:3.24.0-3`; SHA-256 `0359e30820ccee7ee5c8d4c951ca276e3978b18f157d71bd77981de05b344907`. A /bin/true smoke run showed that Callgrind could count instructions. Collection was explicitly **off** for this numerical probe; its summary had zero instruction references. A previous feasibility receipt describing a missing installed tool is therefore not the final obstacle: this independent capability check tested the official tool itself.

## Reproduce the small probe

Apply the adjacent immediate-rank patch to the recorded shipping source in an isolated working directory. With that source at `src`, and this directory's `probe.cpp` available, compile the component as in its ordinary GNU build:

```sh
c++ -O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic -Isrc \
  -c src/core/WscImmediateScalar.cpp -o WscImmediateScalar.o
c++ -O3 -DNDEBUG -std=gnu++20 -fPIC -Isrc probe.cpp WscImmediateScalar.o -o guard-check
./guard-check
valgrind --tool=callgrind --instr-atstart=no --collect-atstart=no \
  --callgrind-out-file=probe.ir ./guard-check
```

For an extracted Debian package, set `VALGRIND_LIB` to its `usr/libexec/valgrind` and use its `usr/bin/valgrind`. The original native and guest output lines are included. The matching component object SHA-256 was `4f03a4c41c1d57d548518668408f058c45f01d8fa55f74896ab6375cd512d047`. This probe does not render any oscillator tick or audio callback, and does not justify a claim about candidate audio or instruction count under emulation.
