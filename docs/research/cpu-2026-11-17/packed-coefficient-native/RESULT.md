# Packed coefficient admission: native study result

Research evidence only; no production DSP or release change.

Run [44106223173](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/44106223173), first attempt at commit `0c351b9c509e2d3e8ae1ff978fbc0a3a739974e3`, completed. Candidate source SHA256 `a612510e598be7471aa8ced68b57e56868cd7345335dc475d44bfab3758bb6fd` was selected before timing. Authentic artifact 14343696283: 131,207,101 bytes; SHA256 `9880e473506c9f5f18ec48e1a7964ee0f3c4c833c73d706edc46a47f6922d78d`. GitHub retention ends 2026-12-01; full ZIP preserved locally, not embedded in Git.

## Result

Independent recomputation from 945,000 callback clock rows confirms **1.103223466% lower primary callback CPU time**, above the frozen 0.630182212% noise-derived minimum. All three four-held-workload primary round ratios improve: 0.9882012644, 0.9856948299, 0.9930212109. Every AA row and primary calibration gate passed. Every Legacy negative-control ratio stayed within the frozen ±3% gate; the worst was +2.759331%, so this does not establish a Legacy speedup. No tested row exceeded the 3% regression guard. No retry or threshold adjustment was performed.

Scope: AMD EPYC 7763, GNU 13.3, Linux x86_64 PIC standalone core, 48 kHz/256-frame callbacks/eight voices. This is **not** actual VST3/DAW timing, other hardware/compiler/platform qualification, or a universal CPU-saving claim.

## Correctness and evidence audit

- 84 timed runs and 14 dry runs; seven workloads, exact retained PCM hashes across all arms/repetitions. Seven canonical PCM files independently checked finite.
- 1,757 evidence-manifest file hashes verified against retained bytes and artifact SHA256 against GitHub digest.
- Raw component output/state/FP corpus: 133,980,210 bytes identical. Component logs: 25,329,954 checks each, 14,616 admitted and 93,870 rejected cases.
- Engine corpus: 22,400,288 bytes identical. Both unchanged Engine tests report 4,135,998 checks and zero failures. Both unchanged batch tests report 1,183,425 checks and zero failures.
- Independent event verification: 1,102,500 event rows, including release events and pressure float32 values; all 98 parameter tables match frozen oracle bytes. FP control, policy, factor and latency receipts checked.
- Retained linked binary and object hashes verified. Compiler executable identity remains receipt-based, not independent compiler provenance verification. Timing PCM does not establish arbitrary internal-state equivalence beyond the separate tested corpora.

## Next

Preserve this closed study. Review source and integration separately; qualify the actual plugin and relevant platforms before promoting the candidate to production. Existing production builds are not candidate qualification because this change remains isolated research.
