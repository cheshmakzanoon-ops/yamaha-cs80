# Ring and precise-charge continuity: independent mutation qualification

2026-11-01. This receipt archives the unchanged preregistered gate and actual
source mutation runner at integration `b7673fefb50a8662c70f6701a4aa760a09e3732f`.
Its production audio is `481c7e9fdb20372288864eecf746bf7efb28190a`, before the
subsequent connected-VCA and modern tail-pan corrections. This is a source-
continuity qualification, not a current full-bank click-score or CPU pass.

The zero-bit contract was published first at
`97147ed03ee325d6d6cd1277cf47efcdf121e412`; its SHA-256 is
`19a5cc8091bba8c6f058696aa844620b3092517af7ece7d4d555ca81aab73a8e`.
Audio `481c7e9` and separate full-SHA V2 pin `8a85faa` followed that contract.
The inspected PRA pp53–54 topology and alternatives remain in
[RING_EVENT_CONTINUITY_PROPOSAL.md](../../../RING_EVENT_CONTINUITY_PROPOSAL.md).
The existing envelope law is a declared inference; no hardware measurements,
new fade, component fit, or scoring-threshold change were introduced.

## Results

The unmodified and restored controls each pass **2,998 assertions**. All **22
mutations** are rejected for their intended condition, with compilation
successful before every executable test. These include Source and PURE phase
and charge resets, telemetry, lost double precision and attack origin, each
rank separately, legato and first-key trigger semantics, Legacy reset behavior,
observer timing, neutral ring PCM, explicit engine reset, four omitted-policy
coverage assertions, and weakening the registered zero-bit bound.

`manifest.json` records all assertion counts, exit codes and observed intended
failures. `results.json`, every exact patch and its build/run log, plus the
complete `runner.log`, are preserved. A contract-weakening mutation intentionally
exits 2 before numerical evaluation; it is identified separately from the 21
executable regression failures. No compile error is counted as a killed defect.
The registered numerical limits and event fixtures remain unchanged.

Reproduction (from a checkout containing the committed sources):

```sh
python3 tests/click/run-ring-continuation-mutations.py \
  --source . --evidence /tmp/cs80-ring-mutations
```

`sha256.json` hashes each archived evidence file other than itself. The later
final integration runs the permanent ring CTest again against its own source;
that result belongs to the final integration receipt, not this older snapshot.
