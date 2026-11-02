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
