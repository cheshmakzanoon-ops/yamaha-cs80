# A39 capture-storage evidence

This directory records the original Windows failure and a narrowly scoped heap-ownership repair in `src/tools/parity_capture.cpp`. It makes no repaired native-platform or release qualification claim.

## Inputs and build context

- `original-failure-observation.json` binds the original Windows job, API/logs, native artifact and archive-member hashes. `original-windows-parity-failure.txt` is the direct JUnit failure output.
- `baseline-context.json` identifies frozen source `c5549f7d095a5c5f49a5ad22ec0ccc819245d201`, the initially observed documentation HEAD, unchanged source hashes, GCC/Linux host, ordinary stack limits, and the exact existing core archive snapshot.
- `before-commands.json` and `after-commands.json` contain every actual compilation, link and fixture-render command, working directory, normal return code, elapsed time, stdout and stderr. Both binaries reuse the same core archive. Only the capture translation unit is compiled privately. No shared build files or runtime stack limits are changed.
- `parity-capture-heap-repair.patch` is the exact source patch against the frozen source. Reversing its two declaration replacements and include addition reproduces the original file exactly.

The private working directory is `/tmp/cs80-a39-1cee5a`. Its `before/parity_capture.cpp` is an unchanged copy of the frozen source; `after/parity_capture.cpp` is the repaired source. `libcs80_core.frozen.a` is the unchanged snapshot of `/tmp/cs80-build-1cee5a/libcs80_core.a`. The command arrays retain absolute source, output and archive paths, and the validation receipt hashes both resulting executables.

## Static diagnosis

`before-capture.stack-usage.txt` and `after-capture.stack-usage.txt` are unmodified GCC `-fstack-usage` output. `before-main-disassembly.txt` and `after-main-disassembly.txt` are complete `objdump -d --demangle --disassemble=main` outputs for the corresponding local binaries. `static-stack-analysis.json` retains the selected frame sizes, exact report/binary bindings, source linker-rule analysis, official Microsoft references and native-measurement limits.

The frozen `sizeof(Engine)` value is reused from A45's already completed constant-size-only measurement, whose receipt hash and relevant record are included. No additional size probe, fault probe or stack-limit experiment is run for this package. Local GCC measurements are not described as MSVC frame measurements. The original Windows executable's PE header and fault backtrace are unavailable in the retained native artifact.

## Ordinary regression and exact comparison

`regression-commands.json` records the existing focused Python harness passing 13 cases with each local executable. The existing CMake registration invokes this same harness with the target executable path. A39 ran it directly against its private binaries so A45's shared build could continue independently.

`exact-capture-comparison.json` records seven complete-file byte comparisons and 384,000 sample-value comparisons across both fixtures, including every file hash and both channels' metrics. It was generated with normal captured files using:

```sh
python3 docs/preset-evidence/2026.11.7/verification/a39/compare-captures.py \
  --before /tmp/cs80-a39-1cee5a/before \
  --after /tmp/cs80-a39-1cee5a/after \
  --output docs/preset-evidence/2026.11.7/verification/a39/exact-capture-comparison.json
```

The comparison exits zero and reports seven byte-identical files and 384,000 zero-difference audio samples. It only reads those captures and writes its comparison result; it does not run the executable or alter stack settings. Raw reproducible captures remain in the private working directory, while the repository retains hashes and metrics.

`repair-validation.json` binds the final allowed source diff, identical frozen core, unchanged DSP/test/CMake/workflow source, original/repaired executable hashes, normal regression exits, exact captures and explicit qualification limits. The native Windows repair must still pass the fresh source-specific job.
