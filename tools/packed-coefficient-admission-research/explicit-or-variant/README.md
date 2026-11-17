# Explicit-OR research refinement and qualification

This separately preserved variant expands only nine initializer-list integer OR entries into explicit calls in the same order. Source SHA256 a612510e598be7471aa8ced68b57e56868cd7345335dc475d44bfab3758bb6fd. The earlier b80 checkpoint remains unchanged. candidate.patch applies to the original pinned stock source; from-b80.patch records the refinement alone.

GNU14 component codegen shrinks the kernel from 4114 to 3846 bytes and stack allocation from 1184 to 576 bytes; vector stack-store sites fall from 42 to 24. This is compiler output evidence, not measured CPU speed. No other refinement variant was tried.

Qualification: 25,329,954 adversarial component checks, exact 133,980,210-byte state/output/FP transcript versus retained stock. Unchanged batch tests pass 1,183,425 checks; full-engine tests pass 4,135,998 checks and 285,328 stereo frames, with the 22,400,288-byte PCM/state corpus exactly matching stock. Disabled backend object matches the already tested disabled object. ASan/UBSan component checks pass and reproduce the stock transcript; leaks remain unqualified. Independent review verifies these bindings and results. Stock controls were reused without rerunning them.

This variant is selected before any timing for a prospective separately reviewed GNU13 cost study. No native CPU measurement, production change, plugin/DAW or other-platform conclusion is made here. Full-engine correctness is not full product qualification.
