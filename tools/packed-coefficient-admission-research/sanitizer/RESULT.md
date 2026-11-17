# Packed coefficient admission: bounded ASan/UBSan component qualification

PASS for stock and frozen candidate b80b7f5f183139a007156df952ce11f74d3b336101822d6eb2a830bdd20d4307. Actual unchanged component.cpp was built separately for both arms with GNU14.2 O1, PIC, contraction disabled, address+undefined sanitizers, frame pointers and no sanitizer recovery. Each command had a 120-second bound. All commands exited0 and all stderr logs are empty.

Each arm passed 25,329,954 checks, with 14,616 admitted and 93,870 rejected cases. The complete 133,980,210-byte state/output/FP corpus is byte-identical across sanitizer arms, SHA256 af13f05f7445ebfacdc607c36ed125e4386c7c522d78af0a8d02236a2fd55c0e. All195 recorded input files were unchanged afterward. Exact commands, compiler identification, input/binary hashes, logs and exits are retained.

Leak detection was explicitly disabled because the sandbox's ptrace environment does not support LeakSanitizer, as documented previously. Leaks remain unqualified; no new leak-enabled attempt or suppression was used. This is a bounded pure-filter component check, not plugin lifecycle, full-engine, production real-time or CPU-performance qualification. No original source/evidence was modified.
