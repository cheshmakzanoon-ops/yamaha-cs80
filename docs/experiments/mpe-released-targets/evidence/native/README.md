# Native processor MPE released-target evidence

## Untouched baseline reproduction

- Source: `/workspace/scratch/ee447c520988/sixteen-note-release`, clean `2ade213bc4dec34b04352063a72e97dac4cc5667`.
- Relevant tracked source/tests/CMake files have no diff versus `1a23a08`, which produced the existing native build.
- Matching build: `/workspace/scratch/ee447c520988/build-sixteen-note-release-native`.
- Existing CS80Snapshots SHA-256: `13c7b913f7fdb55bab4967bb355e71d2d7d54f5a18906709836d354f6c55a3e6`.
- Probe: `BaselineReleasedTargetsProbe.cpp`, compiled with that build's exact ui_snapshot command and baseline include paths, linked against that build's existing actual processor/core/JUCE objects. Only the ui_snapshot main object is replaced. No source worktree or existing build objects are edited.
- One attempt: `attempt-001/`. Compile exit 0, link exit 0, run exit 1 with complete normal assertion output. This is a reproduced behavioral defect, not a build or setup failure.
- Result: **16 scenarios, 256 checks, 64 retention failures, zero fixture failures**.

Scenarios cover Legacy and Circuit, 0- and 32-frame event callbacks, negotiated lower member 2 and upper member 15, and ordinary versus member-pedal-held releases. Scan and ARP are off, MPE is on, dispersion/noise/drift are zero, and keyRelease is 2 seconds. Each initial note is rendered before accepted Note Off. Different later member bend/pressure/CC74 are sent only after release. The original voice is verified active, physically unheld, correctly attributed, with intended pedal state, and finite/non-silent before and after the later values.

Bend changes from 12 to -12 semitones and pressure from 89/127 to 23/127 in all scenarios. Timbre is observed through the same voice's timbre-to-pulseWidth modern slot, for both ranks, and changes after the later CC74. Its observed instantaneous offset may still be smoothing toward the later target; the assertion is retention of the prior settled offset, not a claim that smoothing must snap. No global Channel Pressure matrix source is installed. The probe does not claim whole-audio isolation from the existing global CC74 ribbon adapter.

`attempt-001/commands-statuses.json` contains exact argv, working directory, exit status and log hashes. `inputs-sha256.json` hashes baseline source/tests/CMake, linked objects/libraries, original binary and probe source. `provenance.json` records source head, clean status, no relevant source diff, build path and original binary hash. `binary.sha256` records the separate probe binary. Complete raw compile/link/run logs, Ninja command extraction, and driver output/status are retained. No attempts have been removed or replaced.

## Candidate readiness

`configure-candidate.sh` is prepared but **not run**. It selects release-followup as source and a new dedicated build-mpe-released-target-native build directory, preserving the baseline build and using existing official JUCE 8.0.11, native sysroot, CMake and Ninja. It requires no new dependencies. Candidate native compilation is gated on the parent's frozen-ready source head and final fixture/dispatcher assignment. No GUI/display tests, timing measurements, CI waits, pushes or production edits have been performed.

## Final candidate qualification

Completed on clean frozen source `300356958b3cde471f8dc14a7b505070a4cffc8c`, tree `faee3ef1e7f6184fee74f0e1c7916fecc133d4c3`. Production core/plugin source remains identical to `c0219bd0de8559d72fce4194a849414de5ac7e61`.

- New dedicated build: `/workspace/scratch/ee447c520988/build-mpe-released-target-native`.
- Final native binary SHA-256: `11fbae61a11d603c50e0f8d915e50d3731c42eacdd19e0378e79d36884130cf4`.
- Final native fixture `tests/MpeReleasedTargetHostTests.h` SHA-256: `966d7ef15b12ed087facca684d898cbb176bc56fa8ab8dc5b2f6cd30d1113f66`.
- **Unchanged baseline probe: 16 scenarios, 256 checks, zero failures** against candidate's own headers/objects. Source hash remains `202d208cf377252d716b6d1e6743ce836112b1258f8bb74106bd57bdd7ea435f`, identical to the untouched-baseline probe. Exact commands and logs: `candidate-probe-c0219bd-attempt-001/`. Despite the preparatory directory name, its provenance records clean frozen `3003569`. All manifest files were reverified unchanged after final qualification.
- **Real processor entrypoint `--mpe-released-target-tests`: 6,048 checks, zero failures**.
- **Registered CTest `cs80_mpe_released_target_host_tests`: 1/1 passed** with explicit `--no-tests=error`.
- Authoritative final command/status/source/binary records: `final-native-3003569-attempt-002/`. Configure, build, actual no-work check, direct test and CTest all exit zero. `ctest-LastTest.log` retains the registered test's complete 6,048-check output.

No full native suite, GUI/display tests, timing qualification, external DAW, wrapper/host product qualification or CI claim is made here. These are actual native processor-only tests in the CS80Snapshots executable. No production or fixture source was edited by the native worker, no dependencies installed, and nothing pushed.

### Complete attempt ledger

1. `attempt-001/`: untouched baseline, compile/link 0 and behavioral run 1; 256 checks/64 intended retention failures/zero fixture failures.
2. `candidate-native-c0219bd-attempt-001/`: initial dedicated configure/build both 0. While compiling, parent corrected native fixture settling time and labels. The first header build's binary is preserved as `CS80Snapshots-initial-header` (SHA-256 `469c5e470625e58322f5dcde3e1b86c09b2eee1409a231fdfead9bea58690698`); it was not behaviorally tested.
3. `candidate-native-c0219bd-attempt-002/`: stable native header rebuild 0, recompiling ui_snapshot and relinking. No behavior run on the older header is hidden.
4. `candidate-host-c0219bd-attempt-001/`: stopped setup gate before running a test. `ninja -n` predicts the phony VerifyGlobs/CMake recheck, so dry-run text does not establish no-work. This gate was replaced with a real `ninja -d explain CS80Snapshots` invocation.
5. `candidate-probe-c0219bd-attempt-001/`: unchanged probe compile/link/run all 0; 256 checks/0 failures.
6. `final-native-3003569-attempt-001/`: actual build/no-work/direct test all 0; direct reports 6,048/0. Focused CTest exits 8, `No tests were found`, because the initial configure had read CMake before the native test registration was added during that configure. This is explicitly a stale generated-registration setup failure, not a test or product failure. No source changes were made to address it.
7. `final-native-3003569-attempt-002/`: explicit reconfigure from frozen source resolves registration. Configure/build/no-work/direct/CTest all 0; direct and registered test each report 6,048/0.

All logs, commands, exit statuses, source/object/binary identities and driver outputs remain in their original attempt paths. The baseline `attempt-001/` remains untouched.

### Focused native neighbors

`final-native-neighbors-3003569-attempt-001/` runs a single exact-regex verbose CTest command against the same final binary, retaining its output, command/status, copied LastTest log and pre/post source/binary provenance outside CTest's mutable log directory. **6/6 passed**: new released-target host, existing member-onset host, sixteen-voice-release host, controller-reset host, keyboard-handoff host, and undo-replay-gesture. The source is clean frozen `3003569` before and after; the binary SHA-256 is unchanged at `11fbae61a11d603c50e0f8d915e50d3731c42eacdd19e0378e79d36884130cf4`. This extends the final attempt ledger with one passing focused-neighbor run, not a full native suite or native sanitizer qualification.
