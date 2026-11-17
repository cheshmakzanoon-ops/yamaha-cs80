# Explicit integer-OR variant sanitizer qualification

PASS: actual candidate SHA256 a612510e598be7471aa8ced68b57e56868cd7345335dc475d44bfab3758bb6fd was compiled once and run once under GNU14.2 O1 PIC contraction-off ASan+UBSan. All commands exited0 with empty stderr.

Source-only comparison proves exactly the same nine integer OR/comparison operations in the same order, replacing the initializer-list loop. No FP arithmetic, guard or solver changes relative to frozen b80.

25,329,954 checks passed (14,616 admitted,93,870 rejected). Full133,980,210-byte corpus exactly matches the previously retained stock ASan/O3 control; SHA256 af13f05f7445ebfacdc607c36ed125e4386c7c522d78af0a8d02236a2fd55c0e. Stock and b80 were not rerun. All196 recorded inputs/control files remain unchanged. Exact commands, compiler, binary hash and logs are retained. Each command was bounded at120seconds.

Leaks remain unqualified: detect_leaks=0 is explicit because of the previously documented sandbox ptrace incompatibility. This is pure component functional qualification, not CPU timing, full-engine or production certification.
