# Baseline VST3 automation failure reproduction

Frozen clean c51120121490854bf088c88ebfd64fca83a6d289 baseline configured/built successfully in a new directory. Same Release, -ffp-contract=off, AVX2-ON options and official dependency environment as candidate. JUCE/libxml sources reused read-only from candidate build; no candidate source edits. Targeted default cs80_vst3_automation_tests at 48k realtime failed, CTest exit8.

All 66 semantic assertion/diagnostic lines compared with the candidate's default run match exactly, including all17 FAIL lines. Partition max .000337488,16voice .000291761; zero-frame max .00165544; program700 .000827415; modern-to-modern .0114804. comparison.json preserves counts. Thus these observed default-configuration failures are pre-existing at c511201, not first introduced by prefix16. This does not qualify the failing test, prove all other configurations equivalent, or establish absence of every candidate regression. 44.1k/offline baseline variants were not independently run in this investigation.

The general fixture emits out-of-range normalized knots after roster expansion and its scalar oracle clamps at a different stage from block interpolation; SOURCE-FINDING.md explains the source-level mechanism. Causality for each failure remains unproven without a narrowly scoped valid-input experiment. No assertions/tolerances changed, no production edits, no failed result hidden.

Commands: ../run-hybrid-baseline-automation.sh and commands.log. Configure/build/test logs, clean source ID/status and artifact hashes retained here.

## Additional state controls

Same isolated baseline and unchanged dependency/cache environment: cs80_modern_state_notification_tests and cs80_studio_state_sync_tests both fail, CTest exit8 (29.38s). Modern test949checks/3failures: exact coalesced dirty count twice and reentrant latest-state save once. Same Fontconfig no-writable-cache warnings present. Studio test25checks/1failure: periodic sync consumes restored preferences. These match candidate failure labels/counts and establish baseline reproduction under this environment, not whether cause is fixture timing, UI runtime configuration or application behavior. Both targets are standalone processor-linked executables, not CS80Snapshots; built directly. No tolerance/assertion/source changes.
