# ARP RATE116 scoped qualification

Local qualification completed 2026-11-13. Publication is tracked separately.

The only executable production change appends `P_arpRate` at host automation index 115; the first 115 lanes are unchanged. No DSP, UI, metadata, MEMORY, state-parser, or scanner changes are included. The six historical FX fixture roster guards now accept a literal unchanged 115 prefix. Those guard changes were source-reviewed, not exercised by rerunning historical suites.

## Source and baseline provenance

The candidate starts from local synthetic commit `1e7113b4e040548471c5d42d80b24a57ffff48d9`, whose tree `3951ad3b59964f73720d814e48e89ad1fa554489` exactly matches published main `6db7de3eb090fa659ec2b794891637f53d5008af`. The synthetic commit itself is not a published revision.

`tests/arp-rate-source-freeze.json` pins 15 reviewed files, including the unchanged pre-run plan and staged runner. `baseline-tracked-identities.json` retains 5577 frozen baseline source identities (SHA256 `30a7419dea97826545061092d32f3575459ec7b9687f44b4d290858e59514742`). The comparison module is the previously qualified FX115 module built from `2305421d50aab146cb348256234eaf71a2d0538e`, SHA256 `8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`; it was not rebuilt from current main. Production sources between that module's source and the current baseline differ only in `FxLoudness.h` comments. The baseline helper was built separately against the frozen115 bridge, using identical new fixture/header files.

## Completed gates

Both explicit builds, all six run stages (44.1/48/96kHz, realtime/offline), and collection completed once. Every stage preserves logs and source/binary/evidence hashes. No stage was retried.

- 300 finite stereo float32 PCM captures: 240 candidate, 60 baseline; 32768 frames each.
- 60 actual frozen115 cross-build PCM pairs are byte-identical, including prior-lane curves with FX1 mix.
- 48 scalar/block comparisons (blocks 7/64/256/1024): maximum absolute error 0, against existing tolerance 2e-6.
- 24 combined-lane queue-order comparisons and 12 initial holds: byte-identical.
- 12 overall moving-rate effect witnesses: minimum 0.03233593609184027, against 1e-5.
- 12 combined individual-lane witnesses: minimum 0.0031028445810079575; the six ARP-specific witnesses have minimum 0.03601296991109848.
- No-queue/null, Legacy flattening, PURE rate-inertness, and ARP-off rate-inertness controls are byte-identical for each scenario/configuration. In combined controls the volume curve remains moving while only rate is held.

`collect/complete.json` stores recomputed metrics. Run directories retain per-capture SHA256 inventories and compatibility pair records. Raw PCM and binaries remain in the local build directories referenced by those records; they are not included in Git. Evidence paths are original local capture locations, not portable download links.

## Scope limits

This is native Linux VST3 fixed-control evidence for ascending ARP and 8–16Hz curves, alone and with volume. It does not qualify the entire rate range, every ARP mode, external clock, state readbacks, arbitrary host queues, DAW/AU/cross-platform behavior, hardware fidelity, CPU performance, or a release. Previous qualification records retain their historical meanings.
