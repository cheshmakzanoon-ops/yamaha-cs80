# Packed pan: terminal unusable calibration

One-shot study [44052522090](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/44052522090), attempt 1, commit `9123a9ff81356de31ea4b818b4972e964e06158d`, completed on 2026-11-17 at 09:11:42 UTC. Green means the experiment completed, not that the optimization passed.

Fresh standard Ubuntu runner: Xeon Platinum 8370C, GCC 13.3.0. Component and scheduling checks, forced scalar fallback and all 55 full-engine qualification rows passed. All 15,479,808 paired PCM bytes matched. Separate hit qualification recorded 12,386,528 successful packed pairs and matching consumptions, with identical PCM/work fields. Counters were absent from the unchanged cost binaries.

Six baseline-only calibration executions completed. The first pair's Source Performance paired-tones selector 10 release-tail CPU ratio was **0.81251919**, outside the frozen 0.90–1.10 per-row gate. Aggregate gates and the other two pairs passed, but every gate was required. No row or threshold was changed.

Decision: **TERMINAL_UNUSABLE_CALIBRATION**. Exactly **zero candidate timing executions** started or completed. No candidate native CPU improvement or slowdown was measured. No retry or promotion. Production DSP remains unchanged.

Independent audit reproduced the failure from all 216 work-matching timing rows and verified qualification, hit balance, execution order, affinity and binary/source hashes. Artifact 14314607121 ZIP SHA256: `c7beefbcd09746b07bee6343cd3cfe690806d8657660c3f70dcce84c672663d2`. Raw PCM, CSVs, command/exit records and ledger are retained separately. Correctness and reachability cannot substitute for valid calibration.
