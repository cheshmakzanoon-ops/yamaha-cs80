# Assignment 56 — verified public candidate bytes; release unqualified

**CANDIDATE_BYTES_VERIFIED_RELEASE_UNQUALIFIED.** Five actual credential-free public artifact GETs returned HTTP 200 and matched fresh GitHub artifact metadata and the existing shared-cache archives. Verification completed at `2026-11-22T19:32:22.382858+00:00`.

The combined build remains **FAILED**, source `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, run [`45044040694`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45044040694), attempt **1**. No combined prerelease or seven-asset release is certified.

| Candidate download | Artifact ID | Actual bytes | SHA-256 |
| --- | ---: | ---: | --- |
| [Windows x64](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763872941.zip) | 14763872941 | 6,261,285 | `aa472ff6516371482b7e73f76dc2fbc58d15b52aa8eb86c91f62e091dac06ec8` |
| [macOS Apple Silicon](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763761497.zip) | 14763761497 | 14,900,540 | `662f66e20a8658374b3d4d30102b698234e01bb99e3753a49206a04f99b201a7` |
| [macOS Intel](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763644216.zip) | 14763644216 | 16,057,766 | `325b42beb55ca861b65e9f8416ec286a4b6061946181210fdb41d041e7d3cf1f` |
| [Linux x86-64](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14768807632.zip) | 14768807632 | 13,225,022 | `4295d202bacb20ab41e7a776290dac51cf4a7458abc66d185a86bc351efd4d30` |
| [24 preset audition WAVs](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14769036590.zip) | 14769036590 | 41,014,591 | `9b164379e214d8a15e6907d94dd48842628c09d5970579386818596a0488e16b` |

All five API records identify the same source and run, are unexpired, and record **2027-02-20 16:28:38 UTC** as `expires_at`. They are expiring artifact links. The actual redirected bodies were streamed into hashes and discarded; no duplicate ZIPs or extracted products were retained. Only the final CDN hostname is recorded, not signed redirect query strings.

## Preserved release failure

The randomized Linux GUI validator in job [`137324977642`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45044040694/job/137324977642) printed one SUCCESS marker, then its actual child terminated with **SIGSEGV, raw return code −11, normalized wrapper exit 139**, seed `0x4369262`. The preserved termination log was read and its SHA-256 checked against assignment 54's gate proof. Fresh job metadata independently shows the required GUI validation step failed. Successful diagnostic executions do not change that required gate outcome.

The [automatic publisher run 45060638607](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45060638607) and its `publish` job `137325593929` both completed **SKIPPED**. A fresh credential-free public API read of tag `presets-2026.11.1` returned **HTTP 404**. This establishes that no public prerelease is visible; it does not claim visibility into private drafts.

The original strict release verifier remains byte-identical, SHA-256 `d59337b09ab4739036c22b85458f65cdf9343f4c9298c836b07286823140179a`. Its `validate_build` function was separately called with the actual failed-run metadata and rejected it with `Build did not complete successfully`. That result is recorded in `strict-refusal.json`.

## Evidence and reproduction

- `candidate-verified/summary.json`: authoritative five-download proof, source/run/attempt, hashes, expiry, preserved failure and skipped-publisher results.
- `candidate-verified/*-api.json` and `release-http.json`: fresh public API observations.
- `candidate-verified/artifact-*-proof.json`: each actual GET, byte count, digest and matching cache identity.
- `verify-candidate-artifact-bytes.py`: the separate bounded fallback verifier.
- `verify-public-release.py`: unchanged strict seven-release-asset verifier; its earlier preparation remains documented in `README.md`.
- `strict-refusal.json`: rejection of the actual failed run by the original strict gate.

```sh
python3 build-w12-public-release/verify-candidate-artifact-bytes.py
```

The candidate verifier requires this exact failed first-attempt build, the recorded skipped publisher and absent public prerelease, and all five fixed artifact identities. Exit zero means only that this explicitly unqualified candidate-delivery check passed. A changed build, publisher or public-release outcome must be reviewed separately. It cannot produce a qualified-release result.

This assignment performed no CI rerun, publication, production or public-document edit, archive extraction or product execution. Native architecture/content validation, complete CTest analysis and audition sample checks remain the corresponding assignments' evidence. Public-byte integrity does not establish a fix for the crash, signing/notarization, every-DAW compatibility, acoustic equivalence or a listening verdict.
