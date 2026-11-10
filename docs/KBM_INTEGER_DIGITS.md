# KBM integer fields require digits

A KBM header integer consisting of a sign and a whitespace-separated annotation
(e.g. `+ comment` or `- comment`) previously parsed successfully as zero. For
fields where zero is legal, the complete mapping could then replace the caller's
existing mapping with unintended values. The fix requires at least one digit
after an optional sign before accepting the integer.

This is a two-line parser validation change. Valid signed integers and
whitespace-separated annotations are unchanged. In particular, Scala's documented
rule that text after a valid pitch value is ignored remains unchanged: the
initial proposal to tighten decimal suffixes was withdrawn during independent
review. There is no new decimal grammar, MTS behavior or realtime processing.

## Verification

Base: `830f1d4aeae97eedf67363e98a95352198598458`.
Production/test freeze: `88a53fe`.

- Regression against the base header: **2,818 checks, 20 failures**, covering
  erroneous admission and unwanted destination replacement for five KBM integer
  fields with both signs.
- Fixed GCC optimized build: **2,818 checks, zero failures**.
- Address/UB/float-cast sanitizer build: **2,818 checks, zero failures**.
  LeakSanitizer was disabled under ptrace.
- A fresh core-only CMake build passed all four targeted CTests: core,
  tuning MIDI, tuning frequency-domain and tuning robustness.

Build the targeted tests with:

```
cmake -S . -B build -G Ninja -DCS80_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target cs80_tests cs80_tuning_midi_tests cs80_tuning_frequency_domain_tests cs80_tuning_robustness_tests
ctest --test-dir build --output-on-failure -R '^(cs80_core_tests|cs80_tuning_midi_tests|cs80_tuning_frequency_domain_tests|cs80_tuning_robustness_tests)$'
```

Portable raw-result records, input/binary hashes and targeted CTest output are in
`evidence/kbm-integer-digits/`. This does not claim full Scala/KBM conformance,
native GUI import qualification, full-suite execution or release readiness.
