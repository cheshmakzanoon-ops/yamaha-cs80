# Current core integration after retained MPE expression

## Exact input and result

Published source `863e6c5b63d2fc0a2af90cc051fa1387286e065f`, tree
`5aab7c684ba0a6c0e464f71f3a1d40f0429ee18b`, was checked out into a fresh detached
worktree and built in a fresh directory. No source, tolerance or test changes
were made for this run.

The build completed **266/266 steps**, exit 0. The complete configured CTest
registry completed **148/148 tests**, zero failures or CTest-level skipped/not-run registrations,
in **876.07 seconds**, with explicit process exit 0. Tests ran once, serially;
there was no retry or filtered second run presented as the full result.

The checkout retained no tracked changes. Tests generated an untracked Python
bytecode cache. Separate configure/build/test console logs, cache snapshot,
source/tree identity, explicit exit record and final LastTest were preserved.

## Configuration and reproduction

- GNU C++ 14.2.0, Ninja, Release (`-O3 -DNDEBUG`).
- `CS80_BUILD_PLUGIN=OFF`, `CS80_BUILD_TESTS=ON`.
- `CS80_ENABLE_AVX2_FILTER_BATCH=OFF`; core compilation defines
  `CS80_DISABLE_AVX2_FILTER_BATCH=1`.
- No extra native-ISA or fast-math compiler flags.
- Build parallelism 2; unfiltered CTest parallelism 1.

From an exact checkout with the configured dependencies available:

```sh
cmake -S . -B build-core -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF \
  -DCS80_BUILD_TESTS=ON -DCS80_ENABLE_AVX2_FILTER_BATCH=OFF
cmake --build build-core -j2
ctest --test-dir build-core --output-on-failure -j1
```

The registry contains 121 local-binary commands, 26 Python-driven commands and
one other tooling command. No CTest registration is disabled or configured to skip.
Three Python subtests are explicitly skipped on Linux: the native-Windows junction
fixture in package_binary_identity, and junction plus native Inno Setup compilation
fixtures in windows_installer_preflight. This run does not qualify those Windows
subtests. The registry includes
core/source-factory, oscillator/filter/envelope, MPE/controller, tuning/MTS,
modulation/FX, library/memory and render checks, plus registered inert tooling.

## Limits

This is a complete result for this **configured core-only registry**, not every
repository configuration. It does not qualify JUCE/native wrappers, GUI, a
physical controller or external DAW, live MTS-ESP service, sanitizers, other
compilers/ISAs, installers, hardware fidelity, full MPE conformance, or CPU and
callback-deadline targets. Historical intermittent offline-wrapper failures
are not explained or excluded by a core-only run.

An independent audit matched all 148 unique names in the console and preserved
LastTest, verified evidence hashes, source/tree and exit records.

No per-call event-free shortcut was qualified or activated in this integration
run. Retained earlier event-time-cache failures do not justify weakening
exactness. This result makes no performance improvement claim.
