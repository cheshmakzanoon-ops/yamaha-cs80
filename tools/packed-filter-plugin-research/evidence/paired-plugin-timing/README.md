# Matched whole-plugin timing: no supported CPU gain

Campaign v2 completed all 96 frozen fresh-process slots. **No policy demonstrated
a credible CPU reduction under the predeclared noise/interval rule. The candidate
was not promoted into production.** This is a narrow exploratory result, not proof
that the candidate has exactly zero effect on every machine/workload.

Positive values below mean reduced callback CPU time. Intervals resample whole
ABBA blocks, not individual callbacks.

| Policy | Estimated CPU reduction | 95% interval | A/A noise floor | Gain supported |
|---|---:|---:|---:|---|
| Legacy scalar guard | +1.2033% | −1.4637% to +3.8637% | 3.7413% | No |
| Circuit | −0.4365% | −3.9931% to +2.3435% | 1.9781% | No |
| Pure | −0.6547% | −5.2310% to +2.9078% | 4.2051% | No |

## Frozen scope and protocol

- Matched local GNU 14.2 / Debian 13 VST3 builds, 48 kHz realtime, block 64.
- Six comparison ABBA blocks and two stock/stock A/A blocks per policy: 24 complete
  blocks, 96 processes. Every slot was retained; no replacement or dropped run.
- A 16-note input cohort. **Pure has a source-enforced eight-voice limit**, unlike
  the 16-voice request/configuration for Legacy/Circuit. Active voices were not
  measured as telemetry. This is not a 16-active-voice Pure result.
- Four correctness-history segments, 65,536 warmup frames and 524,288 measured
  frames per process. Worker-thread CPU clocks surround actual process callbacks.
- All 909,312 callback CPU samples and 73,728 empty-clock samples are retained.
  Clock overhead was measured but not subtracted. Bootstrap seed 20261117,
  10,000 whole-block samples; confidence lower bound must exceed the A/A/clock floor.
- Every process exited zero within its 25-second wall bound, with no watchdog
  trigger and an empty owned process group. Recorded wall times were
  6.286–19.036 seconds; measured-segment CPU totals were 4.861–15.947 seconds.
- Full recorded PCM hashes match across all six segments of all runs of each
  policy, including stock/candidate. Recorded PCM is finite/nonzero, and every
  measured callback is nonzero. This does not establish general audio equivalence.

The execution context was reported as Intel Xeon Platinum 8573C. The retained
hardware-context file explicitly distinguishes a post-campaign `/proc/cpuinfo`
read, agreeing with pre-trial tool output, from pinned hardware attestation.
Pre-trial dispatch source/output hashes show GNU14/AVX2 eligibility and supported
floating-point state; they do not measure packed-call frequency inside the plugin.
Do not generalize this result to the earlier AMD/GNU13 standalone experiment.

## Matched-build evidence

Stock used genuine commit `0c351b9c509e2d3e8ae1ff978fbc0a3a739974e3`.
Candidate changed only `src/core/Ig00156Batch.cpp`, frozen source SHA256
`a612510e598be7471aa8ced68b57e56868cd7345335dc475d44bfab3758bb6fd`.

The export retains both complete source/object hash inventories, exact command/
configuration records as explicitly path-sanitized exports, tool/dependency/module
hashes, and the matched-build report. Only the batch object/archive member changed;
compile/build contracts matched, both link maps referenced that member and module
metadata matched. Raw archive/ELF/link-map artifacts remain local with hashes
retained. This is not an independent rebuild or proof of complete runtime closure.
Local dependency/FetchContent paths differ from ordinary Ubuntu/GNU13 CI.
No full CTest/validators, Standalone, AU, Windows or macOS qualification is claimed.

## The rejected first campaign stays rejected

Campaign v1 stopped at its first stock/Legacy steady-measured slot, exit 24:
`owned host pump failed`. Its original plan/host/receipt hashes, recorded failure,
metadata and attempt history are retained; no v1 timing enters the result.
A separate idle reproduction exhausted the 4096-row observer ledger on pump 4097.
That is **consistent with**, not direct proof of, the original generic failure's
internal cause; its actual runloop failure enum was not captured.

V2 was a separately reviewed/frozen timing-only correction to a 32,768-row ledger.
The ordinary qualification limit remains 4096, and full-capacity behavior still
fails terminally. V1 was not relabeled successful or silently retried.

## Compact public evidence and replay

`campaign.json.gz` is 4,274,831 bytes of deterministic gzip, expanding to
14,159,804 bytes of compact JSON. It retains the frozen
plans/schedule, every per-callback timing array, PCM/protocol/render hashes and
quality measurements, process totals/outcomes, whole-block inputs/statistics,
source/build proofs, and independent raw-audit report. Each original included file
has its pre-redaction SHA256 and byte count. The manifest protects the compressed
and expanded export and pins the statistical helper.

Private workspace paths are replaced by portable labels. Provider object-path
lists are explicitly omitted. The 540,417,984 bytes of raw success receipts and
all raw maps stay local; their hashes are retained. Raw PCM samples are not in this
public export, only recorded hashes/quality. **Public replay cannot independently
reproduce native execution, omitted PCM/maps, or full runtime closure.** Sanitized
argv/environment/build records are not claimed to be exact original text.

Read-only integrity, array-total, schedule, PCM-hash and statistical replay:

```sh
python tools/packed-filter-plugin-research/inspect_paired_plugin_timing.py
python tools/packed-filter-plugin-research/test_inspect_paired_plugin_timing.py
```

These commands do not execute a plugin. The retained independent audit originally
reparsed all 96 raw receipts and recomputed the result; replaying its exported
report alone is not a new native audit. No universal speedup, release qualification
or production candidate adoption follows from this study.

## Verification checkpoint

The frozen source snapshot passed 1,392 preparation tests in 338.771 seconds,
with no skips. The public export adds 12 focused integrity/privacy/statistical
checks, passed separately. An independent raw-receipt audit redecoded all 96 runs;
a separate export comparison matched all 909,312 callback values and complete
six-segment PCM/protocol/quality records to their original decoded files.
