# A41 reproduction and evidence scope

Run from the repository root:

```sh
python3 docs/preset-evidence/2026.11.7/verification/a41/verify.py
```

The script reads the A12 `.7` inventory and authenticates the `.6` inventory
directly from Git commit `18711f93a39aa49f3d6f9a41784be456224facbe`. It compares
the complete records and revision-adjusted bytes, compiles one small
header-only metadata/articulation emitter under `/tmp/cs80-a41-1cee5a`, and
accounts for all 1,046 saved rank selectors. It checks the nine material and
four variant compatibility mappings, both ranks of every specialized recipe,
the three separate Standard Steel dulcimer identities, source-file equality
to both frozen sound and final source, and exact `.6` retention of the
relevant material/variant/excitation/loss function bodies.

The probe renders no PCM and links no product core archive. No large build,
audio matrix, preset edit, test edit, or remote action is required.

## Outputs

- `receipt.json`: source/inventory digests, checks, artifact digests, evidence
  reused and limitations. Sound source is `c5549f7d`; final source is
  `ee0dabca`, with full hashes retained in the receipt.
- `checks.json`: individual accounting results, with failures exposed.
- `metadata.json`: actual compiled names and compatibility for 300 model /
  selector pairs, plus 480 outer-articulation cases. These are metadata and
  coefficient observations, not acoustic measurements.
- `recipes.json`: complete unchanged vectors for all 18 specialized programs
  and the three Standard Steel dulcimer comparators, both ranks, source recipe
  and original finish, plus conditional `.7` exposure and nominal choir counts.
- `rank-inventory.csv`: concise exact per-rank settings for those 21 programs.
- `all-rank-selectors.json`: all 1,046 ranks, including neutral and inactive
  ones, so the specialized population is inspectable without a name heuristic.
- `metadata_probe.cpp`, `verify.py`, `compile.log`: reproduction source and
  compiler output. The temporary executable is not retained in Git.

The review reuses `.7` A12/A15/A17/A35/A36, A07/A21, A27's authenticated current
test logs, and `.6` A07/A09/A10/A11 material-development evidence. Their exact
file digests are recorded. A27 log digests are checked against its source-bound
receipt. Reused pass counts remain attributed to those executions; A41 does
not claim to rerun any audio suite. Source functions proven unchanged support
carrying forward the material-specific behavior, while separate `.7` evidence
covers noise compensation, live string response and low vocal coverage.

The report makes no acoustic-recording, perceptual, coupled-physics or
packaged-plugin qualification claim. The bounds on those interpretations are
part of the deliverable, not a request for a speculative product change.
