# Matching-enabled gate added to the read-only verifier

The independently source-reviewed extension passed all five existing retained
FX datasets once. Aggregate rechecks: 1,476 finite PCM captures, 344 scalar
comparisons (maximum error 0), 88 exact queue-order checks, 86 exact initial holds,
168 actual frozen112 pairs and 24 matching ON/OFF witnesses. The smallest matching
witness is 0.00003229454159736633, above its fixed 0.00001 threshold.

The previous four suite outputs remained byte-identical to the prior verifier's
recorded outputs. A sixth valid-data regression rechecked the existing relocated
classic-tail dataset through a different worktree and its historical source
commit; its output also remained byte-identical. All stderr files were empty.
These are retained-data rechecks, not new native executions or audio renders.

The original verifier qualification artifacts remain unchanged. `summary.json`
pins this script/document revision, exact reviewed local input commits and all
result hashes. Local review hashes can differ from guarded publication hashes;
use a locally available reviewed commit containing the frozen evidence files.
Only the verifier and coverage guide changed; no production or historical
qualification thresholds were changed. No broad/parser/state/scanner, GUI,
benchmark or privileged workflow was used.

The matching-enabled gate fixes optional outer matching ON in main modern
scenarios, OFF in paired controls, and retains PURE's disabling policy. Its
witnesses establish an output contribution, not loudness calibration/convergence,
standards compliance or live toggle behavior. The verifier checks source/PCM
against collector evidence, not all runtime binaries or stage provenance.
See [usage and coverage](../../FX_AUTOMATION_EVIDENCE.md).
