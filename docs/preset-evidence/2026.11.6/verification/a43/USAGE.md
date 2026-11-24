# Frozen-source publication check

Run from the repository root. The manifest is derived from immutable source
`fcc3c8e055223fe4f786ca162ff46076c33471c8`, with one row per protected file.
Its SHA-256 is `ddefdc9e0006839a7e07c5e58b7ed68fd010a581d0133e6db9fe8aebcfc5f56a`.
The verifier SHA-256 is `9e698e09f328aad75a67ea631fb4c826932326d758ad3c9f117caae46bed7a9a`.

After the documentation-only publication commit is created or fetched, check
its exact commit and the current protected index/worktree. Replace the shown
screenshot commit with the publication commit:

```sh
python3 docs/preset-evidence/2026.11.6/verification/a43/freeze-source.py \
  --repo . \
  --manifest docs/preset-evidence/2026.11.6/verification/a43/source-manifest.json \
  --candidate 2a8d6ed580c6527226c81437393206ac8b0cd5a4 \
  --check-worktree \
  --out /tmp/cs80-publication-source-freeze.json
```

The candidate must descend from the qualified source and preserve the exact
protected path set, modes and Git blobs. The optional worktree check also
compares SHA-256/byte counts and rejects additional untracked non-documentation
paths. Omit `--check-worktree` when only checking an immutable remote commit.

Protection covers every non-README/non-`docs/` tracked blob and four explicit
document build/test inputs listed in the manifest. It intentionally permits
publication documentation and screenshots to change. It does not prove their
contents correct, validate a download, discover all documentation dependencies,
or qualify a release; `release_qualified` remains false.

Regenerating a manifest requires the explicit `--create-manifest` flag and a
nonexistent output path. Ordinary checks regenerate the expected metadata in
memory from Git and reject a modified supplied manifest. The known preceding
source `c76ad288` is rejected because its protected source differs.

The current authenticated remote advance is screenshot-only commit `2a8d6ed`.
Preserve it and re-read `main` immediately before the final leased publication
update. Its expected tree is `bde8d156e39b59215bd0c42b6a81e18f53214e3c`.
