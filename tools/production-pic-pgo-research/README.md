# Production-PIC PGO research: configuration/provenance increment

This directory does not enable PGO in ordinary builds and contains no workflow, training command, trial runner or production DSP change. The earlier strict-PGO study remains unchanged. These are prospective guards for a separately reviewed Linux/GCC production-PIC qualification, not a released optimization or plugin CPU result.

`overlay.cmake` is opt-in through CMAKE_PROJECT_INCLUDE. Its root-directory/project guard avoids JUCE subproject application. A deferred function requires the exact GNU13.3.0/Linuxx86-64/Ninja/Release/default-feature configuration and production targets. It verifies core PIC/static type and exact three-source inventory, then adds generation or use options to Engine.cpp only. Baseline mode adds no profile option. Normal code does not include this file. Link maps are enabled only in this research configuration. No profile-generation runtime is linked into the plugin; a future runner must separately link the training executable and build only core objects while generating.

`provenance.py` validates baseline/generate/use compile databases with the exact same object/source/directory inventory. One Engine object is required. Only declared Engine PGO options may differ; every ordinary command must stay identical. PIC/Release flags, fatal missing/mismatch diagnostics, no unsafe math/native-target/LTO/response files, one recursively located CMake Engine.cpp.gcda, and immutable artifact byte hashes are checked. This does not yet prove actual final VST3 linkage or runtime behavior.

Run the synthetic negative controls with:

```
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools/production-pic-pgo-research -p 'test_*.py' -v
```

Still required before any project trial: independent review; generated production Ninja/JUCE command comparison; forced generation→use rebuild at identical object path; manual training link provenance; exact profile/version/identity guards; final map/archive-member/ELF resolution checks with stale/discarded/wrong-object negatives; exact unchanged Sub/Ring host roster/capture comparison; complete retained binary artifacts; full300-minute bounded experiment orchestration with90-minute compatibility/cost milestone and complete unchanged569-test candidate suite plus normal render/UI/direct pluginval before release qualification. No cross-platform claim, profile relocation/retraining, retry, coverage reduction, new internal lifecycle instrumentation or automatic production enablement.
