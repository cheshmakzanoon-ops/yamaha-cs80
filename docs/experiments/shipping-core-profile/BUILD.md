# Method reconstruction, not replay

Do not run these commands merely to verify this export. `verify.py` and `test_negative.py` use saved evidence and never execute DSP.

The original method linked three normal production core objects: `Engine.cpp`, `FactoryPresets.cpp`, and `Ig00156Batch.cpp`. Source hashes are provided, with project paths relative to `src`. Use the frozen baseline source, not a rolling experimental checkout.

Normal compile flags:

```text
-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic
```

Only `Ig00156Batch.cpp` additionally uses `-ffp-contract=off`. Debug objects add only `-g`; do not alter optimization, frame-pointer, ISA, floating-point or interposition flags. The host `profile.cpp` is compiled with the normal flags plus `-g` and an include root pointing to the baseline `src`. It includes the existing source factory fixture under a renamed main. Link that same host object first to the normal core archive and then to the debug archive, preserving archive order Engine / FactoryPresets / Ig00156Batch. The recorded compiler was GNU 14.2.0, Debian 14.2.0-19.

Before any new sampling, prove all three object pairs have identical allocatable sections and normalized relocations, and both linked executable images agree in every allocatable section/address except `.note.gnu.build-id`. The `elf_evidence.py` reader and `verify.py::authenticate_build` document this comparison. Never infer instruction identity merely because flags look similar.

`collect.py` expects the verified pair named `profile-debug` and `profile-stock` alongside the script, plus a freshly verified `build-identity.json`, `profile.cpp` and `PLAN.txt`. Starting it requires the explicit `--run-new-study` argument. It creates a fresh `samples` directory, pins an allowed CPU and applies process-local CPU/wall bounds. It deliberately refuses to reuse an existing output directory. Its six-slot order and stopping rules are in `PLAN.txt`. A new run produces raw local maps and process receipts; **those files are not public-portable exports**. Normalize them to module-relative counted PCs before sharing. No new run was performed to construct this bundle.

The exported collector differs from the original only by this explicit-start guard and the portable identity-receipt filename. Both source hashes are recorded. The compiled sampling method `profile.cpp` is unchanged byte for byte. These packaging changes do not rewrite the historical acquisition or authorize a new one.

For reproduction, full binary SHA-256 values may change with compiler, linker, environment or debug paths. Such a rebuild is a distinct run, not the exact recorded evidence. A same-code claim requires fresh identity proofs and cannot be granted by editing this export's pins.
