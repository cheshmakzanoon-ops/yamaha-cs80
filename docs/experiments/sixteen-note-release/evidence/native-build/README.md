# Dedicated sixteen-note-release native build evidence

The clean baseline product source at `e27c34f39320057e09a34005ad2470d1231b4f87` configured and built `CS80Snapshots` successfully with two parallel jobs. The checkout advanced only to docs-only `e23931da9a21e5c4480b21a4274d9e994f0669f7` during the build; source, tests, CMake and assets remained identical. No tests were run at this baseline preparation stage.

`configure.sh` is a copy of the existing successful `mpe-native-tools/configure.sh`, changing only the source and build directory names. Dependencies were reused from the retained official JUCE 8.0.11 and Debian sysroot, with existing CMake/Ninja. No dependencies were downloaded or edited. No old builds or product sources were edited, and nothing was pushed.

- `baseline-provenance.txt`: initial commit/status, tool versions, retained input hashes and configure-script diff.
- `build-baseline.sh`: exact baseline configure/build wrapper. Do not rerun it against a candidate; this would overwrite baseline logs.
- `baseline-run.log`, `configure-baseline.log`, `build-baseline.log`: commands and complete output.
- `configure-baseline.exit`, `build-baseline.exit`: both are 0.
- `baseline-binary.sha256`: final executable hash.
- `baseline-final-verification.txt`: ending source/dependency state, unchanged product-source diff and native build command inventory.
- `baseline-status.json`: concise machine-readable result.

The dedicated build is `/workspace/scratch/ee447c520988/build-sixteen-note-release-native`. Its executable is `CS80Snapshots_artefacts/Release/CS80Snapshots`. Await the frozen candidate before incremental rebuild/testing, preserving candidate commands and results separately.

## Production candidate 77dfd74

The frozen clean candidate `77dfd74b0e80328a4e0f09c01b7636c65519f191` rebuilt successfully with two parallel jobs. `build-candidate-77dfd74.sh` and matching run/build logs, exit status, binary hash and JSON status are retained separately from baseline evidence.

`build-candidate-probe-77dfd74.py` relinks the unchanged `../BaselineReleaseProbe.cpp` against matching objects from this dedicated candidate build. Compile, link and run all exited 0. All four reproduction cases passed: Legacy and Circuit engines at zero and 32 frames retained the older primary owner and released only the newer extension owner. The probe's unchanged printed summary uses the historical word “baseline”; the candidate command manifest and output filenames establish this run's actual identity. This is a targeted regression result, not a full suite or shipping-plugin qualification.

The source advanced after baseline build completion. `post-baseline-source-advance.txt` retains the first post-build observation at the new source; `baseline-final-verification.txt` anchors the unchanged baseline product diff to its actual recorded build-end commit.

## Native fixture 920cab9: build failure

The frozen `920cab9219f500ddc6e7f629f94df7f0a00f6dd7` test fixture did not compile. GCC 14.2 rejected line 21 of `tests/SixteenVoiceReleaseHostTests.h`: the braced range-for list mixes an unnamed enum and `cs80::GlobalParam`, preventing `std::initializer_list<auto>` deduction. Build exit was 1; no fixture tests were run and no check count exists. The old binary was not presented as the new fixture. The worker did not change production or test sources. `build-test-fixture-920cab9.sh`, build/run logs, exit file and `fixture-920cab9-status.json` retain the exact attempt.

## Corrected native fixture 9e3f9a0: passed

The frozen clean fixture `9e3f9a01d5a49a972a9e76e9e5bb36273f6ebdb8` rebuilt successfully with two parallel jobs. Its dedicated `--sixteen-voice-release-tests` invocation exited 0 with exactly **768 checks, 0 failures**. Source remained clean at that commit before and after the test. Production `src/core` and `src/plugin` are unchanged from `77dfd74b0e80328a4e0f09c01b7636c65519f191`. The earlier fixture compile failure remains retained; it was not erased or relabeled.

`build-test-fixture-9e3f9a0.sh`, `fixture-9e3f9a0-run.log`, separate build/test logs and exit files, executable hash and `fixture-9e3f9a0-status.json` retain the exact successful attempt. No full CTest suite or external-DAW validation is implied.

## Neighboring display-independent CTests at 9e3f9a0

All five requested native CTests passed (CTest exit 0, 5/5): MPE member onset 976 checks/0 failures, controller reset 1408/0, undo replay gestures 317/0, plus keyboard handoff and voice activity, which each report 0 failures without a check count. The source stayed clean at `9e3f9a01d5a49a972a9e76e9e5bb36273f6ebdb8`, and the matching binary hash was checked before the run. No display-dependent suites ran. The exact script, verbose log, exit status and JSON manifest use the `neighbors-9e3f9a0` name.

## Final registered gate at 1a23a08

Frozen clean source `1a23a085657a07d9f066f7cc38aaf2572a6fe714` (tree `91c8f1d6e609db57479b74d9204402a750bfcb04`) reconfigured successfully. The registered `cs80_sixteen_voice_release_host_tests` CTest passed 1/1, with **768 checks, 0 failures**. The direct mode independently returned **768 checks, 0 failures**. Every configure/build/test/verification command exited 0. Native source and host fixture are unchanged from 9e3f9a0; only CMake registration and a separate core fixture were added.

The executable SHA256 stayed `13c7b913f7fdb55bab4967bb355e71d2d7d54f5a18906709836d354f6c55a3e6`, identical before/after final tests and identical to 9e3f9a0. Source stayed clean at the exact final commit. The initial build and two later real build checks reported “no work to do.” The dry-run alone shows glob/CMake bookkeeping because it cannot execute the always-dirty VerifyGlobs edge and observe its restat result; the subsequent real `ninja -d explain CS80Snapshots` ran the glob check and confirmed no work to do. This is not pending compilation/linking.

The exact wrapper is `qualify-final-1a23a08.sh`. Separate `final-1a23a08-*` logs, statuses, source-tree inventory, executable hashes and `final-1a23a08-status.json` retain the full attempt. No display-dependent suite, full suite, production edit, dependency edit or push was performed by this worker.
