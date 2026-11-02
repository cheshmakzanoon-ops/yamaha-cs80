# CPU evidence workload witnesses — independent gate repair

Preregistration: `fb8f66e`; implementation: `f5fc991`. Both descend from the
reviewed numerical workflow candidate `7145e36`. Production `src` remains the
original tree `7dd159d491012fd47b46c0aa651e86ea959ca289`.

## Reproduced defect and bounded correction

The preceding release validator returned PASS for six otherwise consistent
synthetic CPU rounds reporting **zero active held voices**, **zero warmup**, or
**zero callbacks in the moving-controls fixture**. Fixture labels, receipt
identity and 51 rows did not prove that the registered work occurred. These
counterfeits are test inputs, not performance measurements.

Before the implementation, 29 preregistered negative subtests failed because the
invalid evidence was accepted. The independently sealed real 51-row historical
round remained an accepted *integrity* control. The repair requires the frozen
ordered CSV schema, fixed producer metadata and original callback, active-voice,
selector-event, compilation, release-observation and baseline-controller
witnesses. The registered benchmark source and all audio/CPU limits are unchanged.

This does not invent a p99 or deadline ceiling. The original minimum-of-three
median selection still reports p99 and deadline counts from that same round,
retaining every other raw round. It does not establish real-time host or physical
hardware equivalence.

## Verification

- All 25 Python release tests passed with **zero skips**, supplying genuine
  original audit and registered-before CPU captures through the optional fixture
  environment variables. The original 885-row audit was correctly rejected for
  its registered click failures.
- All 8 Node test groups passed (their nested assertion counts are retained in
  `after-node-contracts.log`).
- Seven independently imported, compilable source mutations were detected by
  ordinary assertion failures, with no runtime errors; the sealed real positive
  control passed for every mutant. Detailed counts and logs are preserved here.
- A read-only sweep of checked-in baseline evidence accepted all 99 complete
  matching historical 51-row / 1024-callback rounds. Nine VCA diagnostic captures
  used a different callback count and were rejected; those original bytes were
  neither changed nor relabelled as release evidence.
- The full original audit ZIP was losslessly reconstructed from the published
  eight Git-tracked base64 shards: 227,720 bytes, SHA-256
  `a76dd6ee33dcfa4f14be4223ecb06331330d19d90a0baf84147745cef2c88f19`,
  all ten ZIP members valid. No new full audio render or timing was performed.

Reproduce the mandatory synthetic and sealed-real controls:

```sh
python -m unittest discover -s tests/release -p 'test_*.py'
node --test tests/release/*.test.cjs
```

To enable the two optional original-artifact controls, set
`CS80_ORIGINAL_AUDIT_DIR` to the exact extracted original audit root and
`CS80_ORIGINAL_CPU_BEFORE_ROUND` to a genuine `7c864f2...` round (for example
`phase-engine-simd-rejected/raw/before-1`, results SHA-256
`b5a21b58e9af9a085de26775853d6cb85793221d554356a92d7399f51dc657ed`).
The sealed workstation-control round used by the new integrity test has its
three original hashes pinned directly in that test; its measured CPU values
remain above the required production limits.

**Not claimed:** numerical project acceptance, current-main workflow activation,
new performance, audio equivalence of any optimization, native package results,
or original-hardware measurements. All corresponding gates remain independent.
