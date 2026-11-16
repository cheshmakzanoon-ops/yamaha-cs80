# Default-only synthetic residual instruction gate

Independent audit completed. This is NOT a callback/Engine or native CPU-time result.

2048 fixed synthetic events, unchanged coefficient/cursor/phase/fraction schedule. Native raw/helper full residual+MXCSR snapshots match; Valgrind raw/helper full snapshots match. Across environments all residual/control bits match.1791 records differ only by precision sticky0x20, explicitly unsupported by the simulator. The earlier four-environment run FAILED because the simulator also omitted DAZ/FTZ control bits; it remains preserved, not reclassified as passing.

Client collection totals, independently reconstructed from all non-call-edge PC rows and checked against Callgrind annotate:
- Ir:1,844,648 raw →1,417,177 helper (−23.17%).
- Dr:518,813 →424,119 (−18.25%).
- Dw:65,488 →77,776 (+18.76%).
- Simulated D1read misses:1593→2613; simulated last-level read misses:389→1412. These are model counts, not physical hardware observations.

Actual packed SUBPD/MULPD/ADDPD execution is present inside the helper symbol; e.g75b5 executes2048times and7d95/7e06 each8184times. Raw mode has no hits at helper packed addresses. Accepted2048 includes zero-event earlyreturn, not2048 nonzero arithmetic events.

Initial analyzer incorrectly expected summary to contain every event; these dumps have summary:0 and populated footer totals. Initial parser preserved. No rerun: revised parser reads footer, and separate direct-PC reconstruction verifies it exactly. The failed initial four-mode run and its data are independently sealed in residual-instruction-check.

Conclusion: the bounded synthetic mechanism reduces emulated instructions/reads, with higher write traffic and modeled misses. This justifies examining minimal integration/lifecycle compatibility, not promotion or a CPU-speed claim. Native timing calibration remains unusable and was not retried.
