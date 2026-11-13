# Read-only FX evidence verifier qualification

The independently source-reviewed `scripts/verify-fx-evidence.py` passed against
all four existing retained datasets, using their exact reviewed local source
commits. It checked 1,098 PCM captures, 248 scalar comparisons (maximum error 0),
64 exact queue-order checks, 62 exact initial holds and 168 actual frozen112
compatibility pairs. Tail and contribution witnesses also matched each collector.
These are rechecks of existing captures, not new audio renders or native runs.

A fifth valid-data check copied the 96 classic-tail PCM files beneath a different
capture root, retained their build-relative layout and read source/evidence at
the historical commit through a different worktree. Its result was byte-identical
to the initial classic-tail result. This verifies capture relocation and historical
source lookup without opening recorded absolute workspace locations.

All five invocations passed once with empty stderr. `summary.json` pins the
verifier, coverage document, reviewed local source commit inputs and result hashes.
Local review commit IDs may differ from guarded publication commit IDs; use a
locally available reviewed revision containing the corresponding frozen files.
No production file changed. No native module was loaded, rebuilt or executed;
no broad/parser/state/scanner, GUI, benchmark or privilege workaround was used.
The verifier does not certify all original stage logs or runtime binary provenance.
See [usage and coverage limits](../../FX_AUTOMATION_EVIDENCE.md).
