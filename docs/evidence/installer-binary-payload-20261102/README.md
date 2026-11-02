# Installer binary transport identity — bounded local qualification

Tested implementation: local `d55a503e15c67a5ea43dcf69db297771e36aea3d`,
tree `cb9e658127494d05247968eadc6ee2ca6a7e3447`, based on the reviewed notice/
controller series ending at `460e663`. This increment does not modify `src`,
original audio/CPU acceptance, release selection, main, or any installer signing
policy. Publication and native macOS/Windows execution remain pending.

## What is checked

Before package creation, freeze actual existing build artifacts **after** any
intentional binary signing or stripping. Independently reread those same inputs
before comparing installed/extracted component trees. Neither a self-consistent
manifest nor a source link substitutes for actual bytes. Any changed input after
capture fails; the verifier does not silently replace expected hashes.

The comparison requires the expected primary binary, exact relative inventory,
regular-file byte counts/SHA-256, POSIX permission bits, and internal relative
symlink targets. Linked artifact/expansion roots, external/dangling links, special
files, missing/empty primary binaries and wrong macOS PackageInfo destinations
fail. Debian's pre-existing Standalone `install -m0755` is recorded as one explicit
mode transformation, not a blanket permission waiver. macOS outer-installer
signing does not authorize changes to captured inner bundles.

Linux covers VST3 and Standalone. macOS covers VST3, AU and app bundles. Windows
covers **only the actually installed Standalone** and retains uninstall cleanup
for both document and binary verification failures; no uninstalled VST3 or NTFS
ACL claim is made. The final receipt hashes both the frozen manifest and actual
binary-verification receipt. It is not compiler/source equivalence, valid signing,
musical, real-DAW or release qualification.

## Observed local controls and preserved failures

- Initial preregistration: 18 ordinary failures before the helper existed.
- First 19-test implementation had one case-sensitive diagnostic assertion
  mismatch, retained verbatim; the corrected 19-test replay passed.
- Additional primary-binary/root/metadata controls produced six ordinary failures
  across 25 tests, then passed after the bounded guard repair.
- Native-controller integration produced five ordinary failures across 11 tests
  before capture/verification was wired in. Both old cleanup cases remained.
- Final helper suite: 27 tests. Final controller suite: 11 tests, including actual
  Linux Debian creation/extraction with explicitly synthetic non-loadable bytes;
  Windows command execution is mocked. Six focused CTests pass.
- Twelve actual helper-source mutants and twelve notice/controller mutants are
  rejected by their intended ordinary failures with zero runtime errors. Positive
  and restored controls pass. Deliberate runtime crashes are refused by the
  harness and are not counted as semantic kills.
- The first helper mutation batch was **refused**, not passed: AU omission also
  bypasses its install-location check, so it produced three intended ordinary
  failures rather than the preregistered two. Its incomplete receipt and raw logs
  are preserved. The amended exact assertion inventory retains all three; it
  does not loosen production checks or accept arbitrary failures.

The complete POSIX/link helper mutation replay is local Linux evidence with zero
skips. Ordinary Windows CI explicitly skips POSIX permission/symlink fixtures and
checks Standalone bytes/scope; this is not universal cross-platform proof. Actual
macOS package expansion and Windows install/uninstall await native Actions.
Neither these tests nor the packaging worktree's published predecessor a6 prove
native execution of this unpublished increment.

## Older-Python directory reparse amendment

Final amendment source `bb2d52da1c52ee63c347e54d589f30a2482ec700` retains the
preceding scope, and rejects directory reparse aliases without relying on the
newer `Path.is_junction` API. Python documents that method as added in 3.12, while
its Windows stat attributes/constants are already present in the supported 3.10
API: [pathlib](https://docs.python.org/3/library/pathlib.html#pathlib.Path.is_junction)
and [stat](https://docs.python.org/3.10/library/stat.html#stat.FILE_ATTRIBUTE_REPARSE_POINT).
The verifier reads lstat directory attributes at roots, each component ancestor
and inventory directories. Ordinary in-bundle relative symlinks retain their
separate exact-target checks.

Three explicit metadata-simulation controls failed ordinarily before the repair.
The final clean replay passes 30 helper tests, 13 actual helper-source mutants
(including three intended failures when this new guard is removed), the same
12 notice/controller mutants and all six focused CTests. Both intentional crash
controls remain refused. Raw logs and hashes are in `reparse-amendment/`.
These simulated Windows attributes are not an actual Windows junction test.

Independent review of d55 approved the bounded component-level checker and
replayed 27 helper/11 controller tests; review of this small amendment is pending.
The inventory covers declared binary components and notices, **not every possible
extra file elsewhere in an installer or a whole-package security audit**.
