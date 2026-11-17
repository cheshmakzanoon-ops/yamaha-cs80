# Rejected historical-stock parameter inventory

This is **failure evidence, not a completed A-D run or qualification**. The
historical stock plugin exited24 at `initial-flush` with
`full flush complete ID-set binding`. No initial parameter flush was admitted.
The Connected metadata predates that failure. Identity labels are preload
identities, not post-failure verification.

The lossless198985-byte framed receipt is stored as deterministic gzip16206bytes.
It preserves all2378 parameter rows, original UTF16 strings, bus information,
flags, default/current values and exact floating-point bits. No2.4MB expanded
JSON duplicate is committed. File/decompressed hashes are in `manifest.json`.

Reconstruct without running a plugin:

```sh
python tools/packed-filter-plugin-research/inspect_failed_parameter_roster.py
python tools/packed-filter-plugin-research/inspect_failed_parameter_roster.py --full > /tmp/failed-parameter-roster.json
```

Full output separates runtime metadata from source-pattern classification. It
uses an integrity-checked **path-redacted portable execution export** for
nonce/PID/exit, not independent attestation or a new plugin run. `DIAGNOSTIC_ROOT`
is a portable path label. This export is not exact raw argv/environment evidence;
all numerical/outcome/input/output-hash/PID/nonce facts are retained. The SHA256
of the unmodified original execution JSON is retained in the export and manifest;
the original log stays local. `source-roster.json` was independently generated
from source, not inferred from the captured rows; source hashes are retained.

Observed counts:192 workload controls (already including two hardware-source
selectors),104 memory controls, one bypass, one program proxy and2080 MIDI proxies.
No generated/memory ID is missing and no row is unclassified. Memory values equal
their defaults, but55 workload current values differ from defaults. Bypass,
program and MIDI values were zero at this Connected point. Program step522 agrees
with the source523-program bank. `report.json` retains detailed role statistics.

These observations do not establish DSP initialization or authorize mass-writing
proxy defaults. A new source-aware binding policy needs separate routing review
and explicit frozen selection. This rejected attempt must remain rejected.
