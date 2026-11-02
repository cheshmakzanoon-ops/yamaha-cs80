# Distribution notices and checkout identity — 2026-11-02

**Qualified scope:** synthetic distribution-document staging and Linux `.deb`
payload extraction. **Not qualified:** production binaries, native macOS/Windows
installer execution, signing, numerical acceptance, real DAWs or a release.

- Base inspected: `9c62d7b8813a76ce178712c82da9604a1355b112`.
- Local preregistration: `484187a` (`tests/release/distribution-docs-contract.json`);
  independently published equivalent tree at `45d10da794c4e85324695be715bc75d496e77d83`.
- Repair: `2952693` (`scripts/distribution_docs.py`, shared CI payload helper,
  three installer definitions, bounded Windows workflow staging).
- Baseline: nine tests recorded nine failures and five errors. The existing CI
  bundles omitted MTS and embedded Barlow notices; native installer scripts also
  omitted their claimed exact-source documents. Missing not-yet-written staging
  helper errors are distinguished from payload assertion failures in the log.
- Repaired: **11 tests pass**. A `.deb` containing explicitly synthetic, non-loadable
  binaries is built, independently extracted with `dpkg-deb`, and each required
  document compared byte-for-byte. Installer tests are not audio or host tests.
- Existing native archive-path/mode/hash fixtures: **34 checks pass**.
- Eight deliberate defects are rejected in isolated copies; a positive and a
  restored copy each pass. Logs and SHA-256 identities are retained in `mutations/`.
- `bash -n` and Python compilation pass. The focused `cs80_distribution_docs_tests`
  CTest is registered but **not run locally yet**: `cmake` and `ctest` were not
  found on PATH. The same 11-test Python command passes directly. No DSP code or
  numerical limit changed.

The manifest identifies the **clean packaging checkout** and the exact notice
bytes, not the compiler's input/output relationship. It does not certify a stale
binary's source. The source-locked release workflow and native built-package
inspection remain separate gates. No source archive or release has been published
by these tests.

## Reproduce

```sh
python3 tests/release/test_distribution_docs.py
python3 tests/release/test_distribution_mutations.py --out-dir artifacts/distribution-mutations
python3 scripts/package-ci-artifacts.py --self-test
cmake -S . -B build-metadata-only -G Ninja -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
ctest --test-dir build-metadata-only -R '^cs80_distribution_docs_tests$' --output-on-failure
```

Native macOS and Windows release-package execution is still required after all
actual hard acceptance gates pass. The Windows structural fixture only checks
that generated documents are required and wired into Inno Setup; it is not a
native installer execution result.

## Addressable post-publication check

Connector publication reauthors commit metadata; local commit IDs are not assumed
remotely addressable. The preregistration maps local `484187a56ce67d8b2e334be3d09212834c8cb60b`
to published `45d10da794c4e85324695be715bc75d496e77d83`, with identical tree
`f662c498bf67e5b5ec0a2b1cfcb3bcb649c0bdc6`. The repair maps local
`2952693f1d7014f2b88cdc517399c5d2190627ce` to published
`a6e568543f3d194f10f75af2216bd1dc686ff3e5`, with identical tree
`0c84ed5d1d164696507e6507b76922b031520a2c`.

A separate clean detached checkout of **published `a6e5685`** passes the same 11
payload tests and 34 archive checks, with no tracked changes before or after.
See [the exact-source receipt](published-a6e5685/receipt.json) and its hashed logs.
The original local red/green evidence remains unchanged; these newer results do
not retroactively change its source identities. `cmake-unavailable.log` preserves
the original failed configure attempt. Focused CTest execution is still pending
shared timing-window availability; the direct Python checks above were observed.
