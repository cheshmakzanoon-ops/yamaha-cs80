# Canonical temporary-root reparse fixture identity — prospective repair

## Observed native failures, retained unchanged

Published packaging head `dcd5f7332f8ca67c2a66eef73f3adf9ab2bd08f4`
has exact local tree `9bce169b919216da3cd1e50c8c7395ca2a2b1f3f` /
`9a5683883cd402559d8fe2c415723beb09e9297d`.
Build run `41517359655` finished FAILURE. Both Mac jobs failed the original
artifact and nested-bundle simulated directory-reparse tests; Windows failed
the artifact test. Every one reports the original `ValueError not raised`
assertion. The expansion-root test passed. Linux tests, package transport,
notice/binary checks and strictness-10 pluginval passed. DSP `41517359363`
and screenshots `41517359556` passed. These are not release acceptance.
The Intel Mac also has two old asynchronous UI assertions and artifact-upload
timeout; those remain separate observations, not masked by this fixture repair.

## Hypothesis and exact prospective tests

`independent_root()` resolves the build path before walking its descendants.
The simulation currently compares only the original lexical `Path` with the
subsequently visited path. Native temporary directories may resolve to a
different spelling. The direct expansion-root check precedes resolution,
explaining the asymmetry, but the exact native alias is not recorded in the log.
This is a hypothesis until the registered portable control reproduces it;
it is not evidence that actual reparse production checks are bypassed.

Five new tests exercise the existing fixture through an existing-directory
`noncanonical-ancestor/..` spelling. They require unchanged production rejection
at artifact, expansion and nested-bundle boundaries; verify the exact selected
lexical/canonical paths receive the simulated reparse bit; and verify an
unrelated directory does not. This creates no symlink or native junction.
Expected first RED: artifact rejection, nested-bundle rejection and canonical
attribute witness fail ordinarily; expansion and unrelated-directory pass.
There must be five complete tests, no runtime errors or skips.

## Allowed repair and proof boundary

After retaining RED, precompute only the selected target's lexical and resolved
identities before the lstat patch and inject attributes for those identities.
Do not change production verifier bytes, any original assertion or skip, the
30-test inventory, 13 original exact source controls or their ordinary failure
sets. Do not relax predicates, replace guards with mocks, or claim native
junction/installer execution. Retain green new five tests and original30 tests,
the13 exact source controls with positive/restored/crash refusal, and a control
restoring lexical-only matching that reproduces the three new ordinary failures.
Register the new test in CTest only after its RED has been observed.

Use the shared heavy-check lock per case. Record exact commits, source identities,
raw logs and first failures. Independent review and an actual remote native run
are required before describing native CI as repaired. No main, tag, release,
security, source/DSP or original885/CPU acceptance change is authorized here.
