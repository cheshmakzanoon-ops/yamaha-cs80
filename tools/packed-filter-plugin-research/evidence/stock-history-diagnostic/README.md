# Historical stock A–D diagnostic checkpoint

One historical stock-only case completed A–D with recorded exit 0: Legacy policy,
48 kHz, realtime, eight voices, block 64. This is **not** a candidate comparison,
full-grid pass, production qualification, CPU benchmark or release approval.

Independent decoding verified the source-aware 2,378-parameter inventory, exactly
192 initial and two altered control writes, no residual writes, and four complete
2,188-row residual/Panel readbacks. The historical provider remained mapped after
successful native close. Process exit is not physical dlclose unload evidence.
No equality of A/C/D PCM or fresh-state reset is inferred from this history.

The complete 504,090-byte canonical history is retained losslessly in
`payload.bin.gz` (95,156 bytes). It includes metadata, state, all four PCM/render
observations, lifecycle evidence and readbacks. SHA256:

`42b824e15aa9bf8e4540e87b522ff8a945aca91378feffe04c671f30aa4566e5`

The raw success receipt contains private process maps/object paths and stays
local. Only its digest is retained:

`85d104be2a7adb2ba56abc41fd71461acb7ef04a54efc5d284e5af79637ec8ec`

`execution.json` is explicitly a path-redacted portable export using
`DIAGNOSTIC_ROOT` labels, not exact raw argv/environment evidence. `report.json`
omits provider object paths. Original execution/report digests are preserved.
All numeric outcomes, nonce/PID, input/output hashes and source expectations remain
available. Provider retention is a recorded verified result; it cannot be freshly
revalidated from this path-free payload export alone.

Read-only replay of retained history checks, with frozen helper hashes:

```sh
python tools/packed-filter-plugin-research/inspect_stock_history_diagnostic.py
python tools/packed-filter-plugin-research/test_inspect_stock_history_diagnostic.py
```

This runs no plugin. It verifies storage integrity, decodes the actual history,
compares immutable source expectations and validates all four readbacks. It does
not reproduce process execution, authentic production build provenance, complete
runtime closure or stock/candidate audio equivalence. Source-only probe outputs
and the portable payload contract are retained separately from runtime outcomes.

The pre-existing isolated binding aggregate passed **1,350 tests in 336.045 seconds, with no skips**,
against the frozen staged source snapshot before these evidence/docs additions. This diagnostic
adds genuine one-case stock interoperability evidence, without changing any
qualification gate or treating older rejected attempts as successful.
