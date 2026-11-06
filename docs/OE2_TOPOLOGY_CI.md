# Experimental OE2 test registration

The separate `Experimental OE2 topology checks` workflow runs on the exact
experimental branch after a matching push. No manual-dispatch availability is
claimed for this branch-only workflow. It does not select the
experimental audio path in Engine, establish gain/audio fidelity, change any
existing workflow or weaken an existing test.

The new CMake option `CS80_ENABLE_EXPERIMENTAL_OE2_TESTS` defaults OFF. With it ON,
`CS80_BUILD_TESTS` must also be ON and the compiler must use supported GCC/Clang
command-line flags. MSVC and clang-cl/MSVC-front-end requests fail configuration explicitly. The
new workflow tests Ubuntu/GCC only; it does not claim Windows/MSVC or all
platform coverage. Existing default builds/tests retain their prior registration.

The workflow fetches only the exact pinned main commit needed by the original
runner's frozen comparison. The runner still verifies that header's SHA-256 and
the unchanged Dsp.h dependency. It never substitutes a newer baseline or silently
skips compatibility when a checkout is shallow. Local use requires that baseline
commit to be available in the repository too.

```
cmake -S . -B /tmp/cs80-oe2-ci -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON \
  -DCS80_ENABLE_EXPERIMENTAL_OE2_TESTS=ON
ctest --test-dir /tmp/cs80-oe2-ci --output-on-failure -R '^cs80_experimental_oe2_topology_tests$'
```

The runner compiles its own small fixtures, so this specific CTest does not need
the plugin/full-project build first. It executes the 61+8 source-route checks,
three wrong-route/phase mutants, observed independent paths, two BOTH-path
mutants, and the frozen-main byte comparison. Audio generation is not requested;
NumPy is therefore not a dependency of this CI job. This is a deterministic
software test, not a hardware or listening verdict.

Each invocation gets a fresh randomly named receipt directory below the build
directory, which must be outside the source repository. Opt-in configuration
rejects an in-repository build before CTest runs. Actions uses RUNNER_TEMP for
the build and receipts. Randomness affects only the directory name, not fixture inputs or
results. No old receipt is overwritten/deleted and repeat CTest invocations do
not conflict with the runner's fresh-directory requirement. Failed runners
propagate a failing CTest status. The workflow uploads only newly generated
JSON/log receipts, identified by exact commit and attempt.
