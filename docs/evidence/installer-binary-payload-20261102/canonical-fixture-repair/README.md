# Canonical temporary-root packaging fixture repair

This archive records local source `3fda883d2d786895731ade5eb74b45ab4a80e5a2`. It does not establish native candidate success or release acceptance.

## What failed remotely

The prior published packaging head `dcd5f7332f8ca67c2a66eef73f3adf9ab2bd08f4` completed build run `41517359655` with failure. Both macOS jobs failed the original simulated directory-reparse artifact and inside-bundle assertions; Windows failed the artifact assertion. The expansion-root assertion passed. Linux build, package transport, notice/binary checks and strictness-10 pluginval passed. DSP `41517359363` and screenshots `41517359556` passed. Intel macOS also failed two older asynchronous UI assertions and failed to upload its test artifact after service timeouts. All these failures remain failures.

`prior-native-ci/` retains terminal job/step state and the three failed decoded job logs, exactly re-encoded as UTF-8 from the connector's decoded text (including line endings/BOM). This is not a hash claim about GitHub's original compressed log archive.

## Reproduced defect and bounded repair

Production canonicalizes the build root; the old simulation selected only an unresolved lexical target. A portable existing-directory `noncanonical-ancestor/..` spelling reproduces the injection miss. Exact native temporary-root aliases were not printed by the old CI logs, so native causation remains an inference pending the repaired native run. The portable failure itself is directly observed.

The repaired simulation precomputes `{target, target.resolve()}` before patching lstat, then injects attributes only for these selected identities. Production verifier blob `db3431f9da62ca65ae89e89688895efc7332f81e`, original mutation-runner blob `44370d84f7ac1d8c2e5a37744ed49fb42360c3da`, all original30 assertion predicates/skips and source subtree `7dd159d491012fd47b46c0aa651e86ea959ca289` remain unchanged.

## Honest first-red history

Initial test-only commit `ed6c64f` inadvertently exposed the imported30-test class and used a missing POSIX stat attribute directly. Its first run was35 tests/two ordinary failures/one error. `first-red.log` preserves this rejected diagnostic; it is not the expected preregistered red. Before any fixture implementation, correction `78e1247` imported the module and made absent attributes an ordinary assertion. `registered-red.log` then recorded exactly5 tests/three intended ordinary failures/zero errors or skips.

## Complete local results

- Python3.12: original30 and new5 tests pass.
- Python3.13: original30 and new5 tests pass; both complete inventories and zero failures/errors/skips checked, not only the final shell exit.
- All13 unchanged actual production-source controls produce exactly their original ordinary failure sets in complete30-test runs. Positive/restored pass; the runtime-error substitute is refused. `replay-original-percase.py` serializes each original runner subprocess under the shared lock, without editing that runner or its classifiers.
- The new actual fixture-source rollback to lexical-only targeting reproduces exactly the three registered ordinary failures in complete5-test runs. Positive/restored pass and the five-error crash substitute is refused. Mutated fixture/test bytes and all raw logs are retained.
- All6 selected CTest packaging targets pass, including the newly registered fixture target. The first configure command lacked cmake in PATH and failed before configuring; its log is preserved. The official process-local workspace toolchain then configured and ran the6 targets successfully. No native binary was compiled or executed in this local CTest check.

Every control log SHA-256 was rechecked against its receipt. Exact source inputs, SHA-256 hashes and Git blobs/modes are recorded in `receipt.json`. The generated CTest file and CMake cache document registration, not a built plugin.

## Still required

Independent review, separate composition of the already-reviewed asynchronous UI test fix, and actual immutable remote native CI. The unreviewed notice-directory-reparse branch remains excluded. No originalV1/HOST pin, original885 inventory, numerical threshold, CPU gate, production UI/DSP, main, tag, release or security change follows from this fixture proof.
