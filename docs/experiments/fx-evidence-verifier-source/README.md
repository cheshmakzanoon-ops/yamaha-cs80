# Fixed source-layout gate added to the read-only verifier

The independently source-reviewed extension passed all six existing retained
FX datasets once. Aggregate rechecks: 2,232 finite PCM captures, 536 scalar
comparisons (maximum error 0), 136 exact queue-order checks, 134 exact initial
holds, 168 actual frozen112 pairs, 24 outer-matching ON/OFF witnesses and 48
selected-source-layout/manual-manual witnesses. The new source-layout witnesses
range from 0.09908725880086422 to 0.3040674068033695, above the unchanged
predeclared 0.00001 bound.

The first five suite outputs remained byte-identical to the previous verifier's
recorded outputs. A seventh valid-data replay used the existing relocated
classic-tail captures, with source/evidence read from the historical commit
through this different worktree; its output also remained byte-identical.
All stderr files were empty. These are read-only retained-data rechecks, not
new audio renders, native tests or native module execution.

The earlier verifier qualification records remain unchanged. `summary.json`
pins this script/document revision, exact reviewed local input commits and all
result hashes. Local review hashes may differ from guarded publication hashes;
use a locally available reviewed commit containing the gate's frozen evidence.
Only the verifier and coverage guide changed; no production file or historical
qualification threshold changed. No broad/parser/state/scanner, GUI, benchmark
or privileged workflow was used.

The source-layout gate uses fixed reconstructed String1/manual and manual/String2
hardware-source addresses, with separate fresh manual/manual controls and the
same active curves. Its witnesses establish an output contribution in these
histories, not all-factory correctness, panel-queue masking, MEMORY behavior,
live source switching or hardware fidelity. Shared layout iteration adds no
separate copy of the audio validator. All previous suite result formats remain
unchanged. The verifier checks frozen source and retained PCM against collector
records, not all original stage provenance or runtime binary identities.
See [usage and scope](../../FX_AUTOMATION_EVIDENCE.md).
