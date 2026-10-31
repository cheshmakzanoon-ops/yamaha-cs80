# Exact BBD/PRA work omission

Acceptance was published in `706c673c54772eb45cb9fe41bd7dbf342792c64a`
before candidate evaluation. The frozen implementations, registered zero-bit
limits and existing physical/spectral tests remain unchanged. This candidate
continues every phase, optical recurrence, packet-noise stream and nonzero tail.
It omits only unused readouts or algebra on a proven exact-zero history/state.
Signed-zero profiles and stored subnormals are included in the gate.

The independent continuation gate passes 1,245 checks. Nine actual-source
mutants compile and fail the intended assertions; the restored candidate passes.
The four affected integration tests also pass. This folder retains their raw
logs, actual injected patches, hashes and the complete qualification record.
Run the checked-in mutation driver with `--group idle` and the registration SHA.

Whole-engine compatibility and three-round CPU timing are separate requirements.
The preceding lazy-history snapshot supplies the three before rounds. The new
after rounds are archived in the adjacent CPU qualification directory once
complete. An operation-count reduction alone is not a measured speedup.
