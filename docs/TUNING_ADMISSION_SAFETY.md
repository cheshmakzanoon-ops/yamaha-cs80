# Tuning admission safety

## Scope and provenance

This is a narrow port of the missing safety checks from PR #40
(`f0ae466191079c40f1290edc8eb5037e964fc3e8`) and PR #42
(`351ff242c38a11d1654198cfb35239a4d2a81b47`), prepared on main
`a83acd5e7e9be08d4bb33dd685d4c13a46d16563` on 2026-11-09. It does not
merge their older branch histories. The existing whole-message MTS validation
and Engine-level tuning/MIDI regression suite are retained.

The two added test files are unchanged from those source heads:

| File | Git blob |
| --- | --- |
| `tests/TuningFrequencyDomainTests.cpp` | `08ee442f055ae5a23a84d2bdf0b362c9646ccdcf` |
| `tests/TuningRobustnessTests.cpp` | `9b949e5e52910bfeab897d1f998ed1060673e768` |

## Admission contracts

* KBM header integers are checked in their parsed wide type before conversion
  to `int`. Values that formerly wrapped into an apparently valid map size or
  MIDI note are rejected. A mapped degree must also fit a nonnegative `int`.
* Every constructed table entry must reconstruct a positive, finite nominal
  frequency using the core's actual float `midiToHz` operation order. A finite
  double frequency or finite stored pitch alone is not sufficient: the float
  power calculation can underflow before multiplication by 440, or overflow.
  A rejected table is never published, including failures at key 127 or in an
  unmapped key's fallback pitch.
* Scale/octave MTS messages (`0x08` and `0x09`) reject reserved bits in the
  first channel/options byte before changing the destination. The existing
  exact-length and seven-bit-data validation remains in place.

There is no audible-frequency clamp. Positive subnormal frequencies and large
finite frequencies are still accepted if the nominal conversion produces them.
This does not qualify arbitrary pitch modulation, an alternate audio-thread
floating-point environment such as flush-to-zero, complete MIDI conformance,
or physical-instrument tuning accuracy. Scala/KBM session and editor lifetime
changes from PR #41 are intentionally outside this patch.

## Local verification

Linux, GCC 14.2.0. The integrated patch passed these Release CTest targets:

| Target | Checks | Failures |
| --- | ---: | ---: |
| `cs80_tuning_frequency_domain_tests` | 826 | 0 |
| `cs80_tuning_robustness_tests` | 2,785 | 0 |
| `cs80_tuning_midi_tests` | 19,811 | 0 |
| `cs80_core_tests` | 11,508 | 0 |

The two new header-only tests also passed independently with each of:

* `-O2 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all
  -D_GLIBCXX_ASSERTIONS`;
* `-O3 -march=native -ffp-contract=fast`.

A second reviewer independently inspected the integrated production diff and
reran both new tests with UBSan, explicit float-cast-overflow instrumentation,
and libstdc++ assertions: 826/826 and 2,785/2,785 checks passed.

The same new tests against the previous main tuning header detect 395 frequency
failures and 46 robustness failures. Four separate mutation runs against the
integrated patch prove that each new check remains test-sensitive:

| Removed protection | Failing checks |
| --- | ---: |
| Validate KBM header before narrowing | 20 |
| Bound KBM degree before narrowing | 6 |
| Check nominal float frequency | 395 |
| Reject reserved MTS bits | 20 |

Each mutation returned a failing process status. These are negative software
tests, not evidence of an exploitable security issue.

## Reproduce

```sh
cmake -S . -B build-tuning -DCS80_BUILD_PLUGIN=OFF \
  -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-tuning --parallel 2 --target \
  cs80_tuning_frequency_domain_tests cs80_tuning_robustness_tests \
  cs80_tuning_midi_tests cs80_tests
ctest --test-dir build-tuning --output-on-failure \
  -R '^(cs80_tuning_frequency_domain_tests|cs80_tuning_robustness_tests|cs80_tuning_midi_tests|cs80_core_tests)$'
```

For a focused instrumented run without configuring the plugin:

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror -I src \
  -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all \
  -D_GLIBCXX_ASSERTIONS tests/TuningFrequencyDomainTests.cpp -o tuning-frequency
./tuning-frequency
c++ -std=c++20 -O2 -Wall -Wextra -Werror -I src \
  -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all \
  -D_GLIBCXX_ASSERTIONS tests/TuningRobustnessTests.cpp -o tuning-robustness
./tuning-robustness
```

This record covers focused local core tests and source review. It does not
claim a full-suite, native plugin, GUI, DAW-host, or cross-platform release pass.
