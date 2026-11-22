# Independent V3 archive-verifier review

Reviewed 2026-11-22 16:39 UTC. Candidate source: `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`; intended build: `45044040694`.

**No concrete bug found in the bounded V2-to-V3 review. All eight focused fixtures passed independently in an isolated scratch copy. No actual candidate artifact was downloaded or qualified.**

## Source binding and revision

V3 obtains the helper, inner verifier, three guides and canonical preset header through `git show <expected-full-commit>:<path>`. It imports the reviewed helper/verifier from that source snapshot, with the expected directory layout; it does not use modified worktree content. Both executable Python source blobs must match their fixed reviewed hashes before import. All seven files in the prepared 6d3 source snapshot, including the fixture helper, matched the corresponding immutable Git blobs during this review.

The CLI and source helper accept canonical `YYYY.MM.PATCH`, reject `2026.11.01`, and require the expected revision to match the sole header declaration and package manifest. The expected run ID is now mandatory. Exact manifest target/source and generated `SOURCE.txt` content checks remain.

## Packaged Markdown on every target

Every guide must equal the exact bytes returned by the source-commit `packaged_markdown()` helper, using the fixed source SHA and bundled-guide destination mapping. This comparison is unconditional across all four target mappings. The helper reads source text with universal-newline handling and encodes UTF-8, so expected guide bytes use LF on Windows as well as Linux/macOS. V3 neither normalizes downloaded guide bytes nor retains V2's Windows CRLF exception.

The independently replayed fixtures accept transformed LF Linux/Windows bundles and reject Windows CRLF bytes, URLs pinned to the superseded source, untransformed raw guides, and the original revision in a patch manifest. The standalone transform fixture spells its expected result independently and checks local destinations, commit-pinned links, query/fragment/title preservation, external/anchor links, fenced examples and CRLF-input-to-LF-output behavior.

## Retained checks and immutability

The outer-ZIP selection/safety function and binary-path mapping are AST-identical to V2. Inner inventory, checksums, paths, modes, links and embedded-manifest checks still execute through the repository verifier, whose bytes are identical to the d0c3 verifier. Native architecture and standalone/plugin role inspection remains present and uses temporary non-executable copies. Source/run/revision checks were retained or tightened; none was removed.

- V3 driver SHA-256: `e310f7ef18eb2c1c47f2063b7bf5f6e265e2a2f278c6e19ee0e0f898d0792de6` — matches the supplied identity.
- Original V2 SHA-256: `87f97e8ef3b5824f25497d68d748306d88792596ab33f02078b10305d9ec30a6` — unchanged.
- Original four-package report SHA-256: `4724f69ef5fe9d78e3b40dc7bf742d07353a307f526f554b8efda3420637dfaf` — matches the preserved record.
- Original V3 driver, fixture report, preparation record and preparation document were hashed before/after the isolated fixture execution and remain unchanged.

The eight checks use synthetic header-only inputs. They establish verifier-contract behavior, not current binary integrity, CI success, host behavior or publication qualification. Real artifact verification must still supply the candidate source/run, actual paths and trusted outer digests.

Independent results are under `v3-independent-fixtures/`: `fixture-run.log`, `contract/fixture-checks.json`, `source-contract-checks.json`, and `original-file-hashes.json`. No repository files, original review files, builds, workflows or remote releases were changed.
