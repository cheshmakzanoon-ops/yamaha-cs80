# Compact text-evidence archive

Read DECISION.md first: no material CPU benefit was established, no DSP promotion or retry is authorized.

`raw-text-evidence.zip` is a deterministic container of 315 original **text** receipts, not executable software or binary audio corpora. It retains all 36 timing CSVs, analyses, commands, completion records, qualification logs and environment details. Members are sorted with fixed 1980 timestamps and nonexecutable regular-file permissions; no links or traversal paths.

Archive SHA256: `3242d95f47019cc5c9bb612f45d8cafb6448eade5b916a1bb23b88ed4d70903f`
Archive bytes: 268122

Extract into a new directory with Python's standard library:

```sh
python3 -m zipfile -e raw-text-evidence.zip NEW-evidence-directory
```

Verify each extracted file against `RAW-MANIFEST.json`'s `copied_unmodified` entries. The six omitted large qualification corpora have hashes/sizes there and remain in the original Actions artifact, not this text archive. All315 archived bytes were independently checked against the original downloaded ZIP before this compact packaging; final packaging is also checked against that preserved layout.

Original run: https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/43804904995 . Original ZIP SHA256 e56d1b971fa2c3182e7c8bfeca570032e49686f5e4ccd04154691f27d22d8379. Workflow requests 30-day retention; artifacts can expire or be deleted, so the run link is not a permanent download guarantee. The original ZIP/local full archival commit ec8e8b2c2a6a206e875071f6a1601573f90fa26f are separately retained; no original evidence was deleted. Git now preserves these raw timing text receipts independently of artifact expiry.
