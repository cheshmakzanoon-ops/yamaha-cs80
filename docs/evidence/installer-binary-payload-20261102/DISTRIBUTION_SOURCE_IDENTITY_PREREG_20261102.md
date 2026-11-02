# Required-notice source binding — prospective controls

Baseline: local `0f2ed3ea1be0b116bb28af60f3fca2f28f39641c`, with independently
reviewed document transport and binary/component guards. A new real synthetic
Git-repository diagnostic found that changing LICENSE behind either
`assume-unchanged` or `skip-worktree` leaves `git status` empty. Both stager and
document verifier then accept the altered notice under the old source commit.
The independent a6 downloaded-archive byte comparisons remain valid; this is a
new source-binding gap, not grounds to rewrite old observed receipts.

## Frozen intended contract before repair

Required tracked notice inputs, plus CI's `CI_DISTRIBUTION_STATUS.md`, must match
the **captured commit's Git blobs**, not just the index, working-tree bytes or a
self-consistent payload manifest. Preserve actual checkout bytes in the package.
Allow unchanged physical files bearing index hints; a flag alone is not proof
of changed source. Reject hidden content changes, registered type/executable-mode
drift (where POSIX modes are observable), staged/index divergence, and linked
notice leaves/ancestors. Existing missing/untracked notice controls remain.

The observed Windows checkout conversion is explicit and narrow: canonical Git
LF text may match its exact LF-to-CRLF form **only on native Windows with
`core.autocrlf=true`**. Do not normalize arbitrary text, execute Git clean/smudge
filters, or silently accept conversion under another policy. The current tracked
`.gitattributes` does not prescribe other notice conversions. A future checkout
policy would need separate review. Synthetic Windows-policy witnesses are not
native Windows execution.

Stager and independent verifier must implement separate raw-blob checks; the
verifier must not import or trust the staging helper's binding result. Bind only
the required tracked notice inputs and CI status text here. The external
configured JUCE licence remains a separate supplied input; this does not assert
a raw-byte audit of every unrelated repository file or dependency.

`tests/release/test_distribution_source_identity.py` registers 11 test groups.
On native Linux the old code is expected to have 38 intended ordinary assertion
failures: 10 staging, 10 CI notice, 10 independent forged-payload, 2 CI status,
2 source-mode, 2 changed-CRLF and 2 unregistered-CRLF subtests. Existing positive,
staged-index and link behavior must retain their real result. A runtime error,
skip or unrelated assertion is not a qualified semantic RED or mutant kill.

After the ordinary RED and minimal independent repairs, run all old focused
CTest/notice/archive controls and new tests. Pre-register actual source mutants
with exact named ordinary failures, preserve positive/restored runs, and prove
the runner refuses runtime crashes. If a new redundant raw-blob guard protects
an old single-guard deletion, report that honestly and preregister its joint
guard control; do not count a surviving single deletion as a killed mutant.

No remote publication, signing, installer release or numerical acceptance is
authorized by these tests. Actual native package execution remains a separate
bounded check after its source identities and guard review are frozen.

## First-run disclosure and pre-implementation amendment

The initial c113030 run observed 40 failures/zero errors in 11 groups, not the
predicted38. One additional failure was a real staged-index divergence: restoring
physical bytes to HEAD makes `git diff HEAD` empty even while the index differs.
The other was a case-sensitive test regex rejecting the correctly raised
`Source checkout is dirty`; it is **not** an additional provenance defect.
Preserve that raw run. Before any production repair, correct only that regex and
split staging/verifier index divergence into separate groups so an early staging
failure cannot hide the independent verifier result. The corrected inventory is
12 groups, expecting40 intended ordinary failures if both index paths admit it.

The corrected bff2dda run observed exactly40 intended failures in12 groups, zero
errors/skips. The first working-tree repair then passed12/12. Before evaluating
new or inherited source mutants, register these inherited-runner migrations:

- The old dirty-checkout test now changes tracked non-notice `CMakeLists.txt`
  instead of LICENSE, retaining the same criterion while remaining sensitive to
  the Git-wide dirty guard independently of the new raw-notice guard. The mutation
  anchor is narrowed to the original worktree-vs-HEAD command, not the newly added
  staged-index check.
- Removing only `ls-files` membership no longer permits an untracked notice,
  because the immutable blob check remains. The existing untracked-defect witness
  is explicitly a **joint** control: remove the membership check and restore the
  old working-file payload lookup. Expected ordinary failure set remains the one
  original untracked-notice test. Do not report the harmless single removal as a
  killed defect.
- The deliberate runtime-error probe moves only its unique insertion anchor to
  the current `notice_payload` entry. Error-only output remains refused.

New raw-source mutants are registered in a separate runner before execution:
stager bytes24, verifier forged origin12, staging mode1, verifier mode1, staged
index staging1/verifier1, unregistered Windows conversion staging1/verifier1,
and CI status binding2. Each number is an exact ordinary subtest failure count;
the runner also requires exact named test-family counts and zero runtime errors.
