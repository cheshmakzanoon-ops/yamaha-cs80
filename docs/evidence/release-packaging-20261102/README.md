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
- `bash -n`, Python compilation and focused `cs80_distribution_docs_tests` CTest
  also pass. No DSP code or numerical limit changed.

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
