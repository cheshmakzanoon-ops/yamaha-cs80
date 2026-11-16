# Reviewed finite-ring residual experiment: hold, not a speed claim

This publication contains documentation only. It does **not** enable or ship the experimental residual backend, change DSP/tests/build settings, or combine it with the pitch-memo experiment. The reviewed prototype remains local and experimental. No native timing or calibration retry was run for this revision.

## Decision
An exact finite-ring certificate avoids repeated finiteness preloads by checking already-computed packed results. It passed focused and full-core correctness qualification. Its modest instruction-count improvement does not establish CPU-time benefit, and memory traffic remains higher than the original raw implementation. **Hold experimental; do not promote or extend this mechanism without a distinct, evidence-backed opportunity.**

### Fixed whole-core instruction result
Core Engine::render only, not the JUCE processor callback: eight held source-0 voices, 48 kHz, 128-frame blocks, 34 warmup blocks, then 16 steady/moving blocks. Separate matching headers and core archives for original raw, old guarded residual SIMD, and new certificate. Setup, control setters and validation are outside collection.

| Case / arm | Instructions (Ir) | Reads (Dr) | Writes (Dw) |
|---|---:|---:|---:|
| Steady raw | 198,370,590 | 52,571,736 | 25,275,800 |
| Steady old guarded | 197,998,835 | 52,713,857 | 25,291,304 |
| Steady new certificate | 197,934,609 | 52,660,209 | 25,285,301 |
| Moving raw | 241,447,520 | 60,717,617 | 28,562,655 |
| Moving old guarded | 237,760,225 | 63,017,424 | 28,846,133 |
| Moving new certificate | 236,590,014 | 62,200,239 | 28,782,707 |

Relative to old guarded SIMD, new instructions fell **0.0324% steady / 0.4922% moving**. Relative to raw, moving instructions fell 2.0118%, but reads rose **2.4418%** and writes **0.7704%**. These are deterministic instruction/read/write counts, **not cycles, elapsed time, CPU percentage, physical memory transactions, or a measured speedup**.

The earlier fixed 2,048-event synthetic mechanism comparison showed new-versus-old Ir −5.396%, Dr −11.804%, Dw −6.884%, but new-versus-raw writes +22.236%. Restore scans were excluded there, so that result is not a lifecycle-amortized or whole-engine estimate. Whole-core measurements above supersede any extrapolation from that microkernel result.

## Correctness and scope
- Generic and ASan/UBSan focused tests: 16,436,813 checks each, zero failures; 143,808 nonzero packed admissions. Native and OFF: 8,219,269 checks each, zero admissions/failures. Lifecycle: 55,845 checks per profile, zero failures. Leak detection explicitly disabled for sanitizer runs.
- Nine fresh raw/old/new × generic/native/OFF builds: all 27 Engine/history/dispatch corpora match within profile; two existing registered batch/scheduling tests pass per build. This is not the full repository test suite.
- Separate count-only Engine corpus: 2,643,214 true-proof/nonzero packed entries; zero false-proof rejects/raw fallbacks in that corpus. Exceptional fallback is covered by focused tests, not inferred from these counts.
- All 12 native/Callgrind core runs passed exact PCM and named-public-state checks. Full status matched across arms within each environment. Across native/Callgrind, controls and values matched; precision sticky differed on 16 steady / 48 moving segments. Callgrind does not qualify native sticky semantics.
- Actual packed arithmetic and integer classification instruction hits were verified. Function code grew 195 bytes; stack frame stayed 184 bytes. Measured oscillator/Engine sizes stayed 6,000 / 290,376 bytes versus the old residual prototype, not necessarily versus original raw. Matching consumers were built; no stale plugin archive was used.

## Provenance and compact evidence
Original raw source: `5836041138bc1708c3fd2d4240cf63a3e1def4b3`. Old guarded local lineage: `fbb9df9a5c88c84f7e5df8b85849553f7039b8a6`. New tested local source: `2abb1cfac291391fa477d0b38b52adedf8e553dd`; qualification/instruction run pinned docs-only descendant `9400dd1dee87272cb03723f4546fa66c450d4901`. Full local preservation: `1c5aa114e73034145ff4e69ce5d14bbc96f8c37f`. These local experimental commits are provenance identifiers, not promises of reachable public branches.

`evidence.json.gz.b64` is a deterministic, text-encoded compact evidence archive; `BUNDLE-MANIFEST.json` gives encoded/decoded hashes and every included record's original byte hash. It includes protocols, numeric results, independent audits, scripts, provenance receipts and original evidence inventories. **Bulky binaries, waveform/state corpora, raw profiler dumps, full disassembly and build trees are intentionally omitted.** Their original hashes/sizes remain in the embedded inventories and full artifacts remain preserved locally. Consequently this public bundle is not a substitute for independently re-running or inspecting the omitted raw evidence.

Decode and verify without executing experimental code:
```python
import base64, gzip, hashlib, json
from pathlib import Path
p = Path('docs/experiments/residual-ring-reviewed')
m = json.loads((p / 'BUNDLE-MANIFEST.json').read_text())
b = (p / 'evidence.json.gz.b64').read_bytes()
assert hashlib.sha256(b).hexdigest() == m['encoded_sha256']
z = base64.b64decode(b)
assert hashlib.sha256(z).hexdigest() == m['gzip_sha256']
data = gzip.decompress(z)
assert hashlib.sha256(data).hexdigest() == m['decoded_json_sha256']
for record in json.loads(data)['files']:
    assert hashlib.sha256(record['text'].encode()).hexdigest() == record['sha256']
```
