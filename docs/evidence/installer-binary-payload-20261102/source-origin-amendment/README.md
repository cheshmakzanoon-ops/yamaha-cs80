# Raw notice origin — observed amendment

Tested clean local source `e55540b098af022137c1f90582386673b683bca2`, tree
`3f751625edd795177d80704c6411a562c4c7a0d2`. The production repair is
`a19a5f0da5a3fe32d04ea1aa4b57cfe452e4ab88`; later commits freeze source
mutations and clarify canonical-root/line-ending scope without changing it.
These local identities are not advertised as remotely available commits.

## Reproduction and repair

The initial real synthetic-Git probe changed required notice bytes behind both
`assume-unchanged` and `skip-worktree`. A clean-looking status was insufficient:
the old stager and independent verifier accepted changed content under the old
commit. The registered tests additionally exposed staged index content differing
from HEAD while physical files matched HEAD. Required source notice bytes now
bind independently in producer and verifier to raw Git blobs, regular-file types,
POSIX executable mode and non-linked source ancestors/leaves. Explicit staged
index checks supplement, not replace, physical-byte checks.

The first preregistered run had 40 failures: 38 intended witnesses, one newly
exposed staged-index defect and one regex-case mismatch against a correct dirty
rejection. That last result is **not** a semantic defect. Before production repair,
`bff2dda` split the staged-index witnesses and corrected only the expected-case
regex. The repeated baseline had 12 groups and 40 ordinary intended failures,
zero errors. Both original raw runs remain unchanged here.

## Final observations

- Eight focused CTests passed from clean e555 in 15.49 seconds, plus 34 synthetic
  archive selfchecks. The earlier d4be run is retained separately.
- All nine actual source mutants reached exactly their registered ordinary failure
  families/counts: 24, 12, 1, 1, 1, 1, 1, 1 and 2. Unmodified and restored 12-group
  positives passed; the deliberate runtime crash was refused, not counted as a kill.
- All eight inherited staging and sixteen controller/workflow source mutants also
  reached their registered ordinary assertions, with positive/restored and runtime
  refusal controls. Their platform fixtures remain synthetic, including Windows.
- The initial d4be and final e555 mutant snapshots/logs are both preserved. The
  receipt hashes every retained raw artifact, input snapshot and result.

## Explicit limits

A top-level checkout alias is canonicalized and intentionally accepted; the added
positive checks this. Linked notice components inside that canonical checkout are
rejected. Native Windows `core.autocrlf=true` permits only exact LF-to-CRLF byte
conversion. This is an explicit equivalence policy, **not** attestation that Git
performed the transformation or that no attributes override exists. The Windows
policy tests run with simulated platform metadata, not native Windows execution.
Required tracked notices and the CI distribution-status text are covered. The
supplied JUCE licence is a separate required input, not a new dependency-source
attestation. Previous actual a6 archive notice comparisons remain valid observed
byte comparisons; they are not retrospectively recast as this repair's tests.

No binary execution, native installer exercise, signing, compiler/source identity,
audio/CPU/DAW acceptance or release qualification follows from this amendment.
Independent review is recorded separately when terminal; publication remains held.

## Archive inventory correction

The first archive commit `56464fe75dc267110cdd6cc25bbb3b50662c8a5b` counted
120 generated Python bytecode-cache files in its filesystem receipt; Git correctly
ignored those caches. The corrected receipt hashes only the retained tracked
source/log/result files, plus this explanation. No raw test result or log was
rewritten. The first receipt remains visible at its original commit; bytecode
caches are not required evidence and remain in the original scratch run trees.
