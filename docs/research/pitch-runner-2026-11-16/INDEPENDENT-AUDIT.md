# Independent audit: frozen pitch Actions run43804904995

Read-only recomputation completed. No benchmark, retry, source change, or threshold adjustment.

- Artifact ZIP SHA256 independently matches e56d1b971fa2c3182e7c8bfeca570032e49686f5e4ccd04154691f27d22d8379.
- All36 timed runs independently parsed:8 active A/A,8 lowload A/A,12 active candidate,8 lowload candidate;1024CSV rows total. Correct unique36/19-row inventories and15/11case completion receipts; fixed48kHz/128frames; zero nonfinite count, available positive callback clocks, callback counts/work durations/checksums identical across every run of each suite. CPU/audio ratio fields agree with raw seconds/durations.
- Every per-case ratio and aggregate ratio recomputed from CSV equals saved analysis; all analysis CSV hashes independently checked. Source-held subsets are9 source_performance held active rows and4 isolation nonidle held lowload rows.
- Active calibration E=0.006338957644054233; lowload E=0.0034489245301458915. All frozen aggregate±2% and row±10% gates passed. This is this-run empirical usability, not universal precision.
- Active total median candidate/baseline0.9982867669953539; source-held0.9967627011725226. Both fail frozen2% material improvement, also fail improvement>2E. Lowload total0.9986966297316751; isolation-held0.9962195446134847. Independently checked per-case/aggregate caution and hard-loss absence. No material signal; no promotion or extra rounds.
- All top-level terminal receipts0. Fresh baseline/candidate Engine/history/dispatch corpora byte-identical; immutable history SHA08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195. Reach receipt reports6224868 DAZ+FTZ-mode3 admissions and0other modes; no timed binary instrumentation.
- Frozen tooling hashes match reviewed local tooling; normalized Engine/FactoryPresets/Batch/benchmark compile commands identical across arms. All timed command records pin already-allowedCPU0 and use baseline executable for both calibration labels. Per-run source metadata matches immutable baseline/candidate identifiers. Recorded benchmark hashes8d48a98e…/5efdf6e9… are runner assertions; executables were not included in the artifact, so their bytes cannot be independently rehashed here. This limitation does not invalidate raw-ratio audit; do not describe it as an independent executable rehash.

## Absolute callback-thread CPU context

Candidate source-performance held per-row callback CPU/audio ratios span0.535656786621–0.748101408691 (53.57–74.81% of one callback thread's real-time budget). Candidate active-suite summed CPU/audio ratios span0.326861614977–0.328325854921 across six runs; these combine heterogeneous held/pedal/release cases and are not a particular user workload.

Candidate lowload isolation-held ratios span0.5306082854–0.621029099854. Idle quiet isolation0.136940867432–0.137935209961; idle quiet Legacy0.0268642683105–0.0270691625977; idle noise Legacy0.0299284511719–0.0300255058594.

These are instrumented core callback-thread CPU seconds/rendered audio seconds on the hosted runner, including callback timing reads and setControlFrame/control/render work. They are NOT system CPU utilization, JUCE/DAW/driver CPU, or proof of the user's under2% target. No measured row above establishes that target.
