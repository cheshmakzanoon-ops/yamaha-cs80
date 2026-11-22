# Final calibrated bank identity verification — assignment 27

Status: **PASS**. This checks the final `2026.11.1` sound data against the frozen
`2026.11` binary from baseline commit `7f55a95a7534b389927defe46866456560b3a878`.
The input hashes in `summary.json` identify the exact compared snapshots.

Exactly 12 Circuit value vectors change: 38 voicing values and four calibrated
master volumes. The other 289 vectors, including Init Circuit, are unchanged.
UFO Landing has a description-only correction, so 288 complete Circuit records
(values plus all metadata) remain unchanged. Five descriptions change in total.
All 222 Legacy records remain byte-identical, including float32 bits, strings,
tags, library identity and ordering. The canonical Legacy SHA-256 is
`c485b4776e7356ce7afb4d7d5031afe2df1c9feef44324f48ae6c5a92e95c28a`.

The 190 parameter IDs retain their order. The compiled host catalogue is the
exact concatenation of 222 Legacy and 301 Circuit records in both snapshots.
All 49,152 MIDI addresses and 830 forward/reverse index lookups compare equally,
including unassigned addresses and boundary values. Eight compatibility source
files remain byte-identical to the baseline, and session schema remains 3.
The 296 host parameter total is derived from the unchanged layout; this report
does not claim native host reflection or execution coverage.

## Method

`identity.cpp` links separately against frozen baseline and final core libraries.
It serializes every Legacy, Circuit and host record into a deterministic binary
format: little-endian 64-bit counts/lengths, length-prefixed UTF-8 strings, and
exact little-endian float32 bits. The Python comparison parses every field,
checks both inventories' decimal values round-trip to the corresponding compiled
float32 bits, verifies expected differences and compares the mapping binaries.
Thus the final counts are derived from actual compiled data, not source-text
search or the earlier pre-calibration audit.

## Reproduction

From the repository root, with the frozen libraries/inventories in this folder:

```sh
g++ -std=c++20 -O2 -Isrc build-w05-final-identity/identity.cpp build-w05-final-identity/baseline-core.a -pthread -o build-w05-final-identity/baseline-check
g++ -std=c++20 -O2 -Isrc build-w05-final-identity/identity.cpp build-w05-final-identity/final-core.a -pthread -o build-w05-final-identity/final-check
build-w05-final-identity/baseline-check build-w05-final-identity/baseline > build-w05-final-identity/baseline-contract.json
build-w05-final-identity/final-check build-w05-final-identity/final > build-w05-final-identity/final-contract.json
python3 build-w05-final-identity/compare.py
```

To reconstruct those inputs, build the baseline commit and final source
separately with `CS80_BUILD_PLUGIN=OFF`, capture each complete factory inventory
using its own `cs80_preset_capture --inventory` command, and copy the corresponding
`libcs80_core.a` files into the names above. Different compiler builds may have
different static-library hashes; the complete serialized bank comparisons are
the portable data identity check.

No production source was modified by this assignment. Acoustic realism and
native host behavior are outside this specific identity check.
