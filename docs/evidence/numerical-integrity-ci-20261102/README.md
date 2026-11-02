# Numerical-integrity CI qualification

This adds a read-only, nonpublishing workflow for the numerical evidence verifier.
It is not an engine render, benchmark, physical measurement or release pass.

The exact historical archive and CPU fixture are reconstructed from pinned Git
objects and verified before output creation. The archive inventory, CRC, paths,
file types and expanded size are checked; existing destinations are refused.
All original numerical limits and producer identities remain unchanged.

Final local validation passed 37 Python tests with no skips, eight Node groups
with no skips/TODO/cancellation, and seven actual-source mutation controls. The
workflow rejects missing/skipped inventories explicitly. Actionlint passed after
correcting an initially invalid job-level runner context. Initial red and lint
failures remain here; they are not passes.

The first 35-test slice and mutation replay received independent execution review.
The final two-test Node guard delta received independent source review and a full
37/8/7 coordinating-engineer replay. Exact file/log hashes and the local
implementation commit are in receipt.json. Remote publication may reauthor Git
commit metadata; the publication mapping and exact tree equality are recorded
separately, without rewriting these observed logs. Fresh Actions execution remains
pending at this checkpoint.

Reproduce by fetching the two immutable evidence commits named in the materializer,
running it into a NEW output directory, and setting CS80_ORIGINAL_AUDIT_DIR to
that directory/original-audit and CS80_ORIGINAL_CPU_BEFORE_ROUND to
that directory/original-cpu. Run Python unittest discovery over tests/release,
Node with --test --test-reporter=tap, the Node TAP guard, and the seven-mutant
runner exactly as in numerical-integrity-tests.yml. No production render is
substituted for missing evidence, and the archived numerical failures stay failures.
