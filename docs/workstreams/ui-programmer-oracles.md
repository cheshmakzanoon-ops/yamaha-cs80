# Host UI gates after electrical programmer integration

2026-10-31. Test-oracle correction and state-snapshot discipline. No production DSP or plugin behavior changes in this workstream; no hardware measurements claimed.

## Reproduced failure and cause

The integrated `2acd64a` UI smoke failed 385 assertions. The original complete log is retained alongside this report, with SHA-256 `864afb85616d92f89812e22147e367ad09659e69dae8b7d7fab28363d21d5e15`. The failures came from fixtures that still assumed the old physical-unit PANEL/memory input or the old cold-envelope initialization, plus two fractional inputs to the newly discrete chorus-tab parameter.

| Existing assertion family | Failed assertions before correction | Contradictory fixture assumption |
| --- | ---: | --- |
| Live filter envelope history | 138 | PANEL/memory targets and times were taken directly from the legacy-effective `Params` snapshot; cold assignment reset to IL. |
| Live amplitude envelope history | 134 | Raw physical-unit timings and sustain were used instead of the selected loaded electrical programmer source. |
| PANEL/memory receiver ownership | 66 | Leaving a factory tab was expected to clear readouts to zero, although PANEL/memory now drives the same receiver network. |
| Factory first-tick envelope history | 33 | The independent filter primitive was reset to IL despite the committed retained-capacitor-charge law. |
| Host output meter byte identity | 10 | The core oracle received an unannotated physical-unit frame; the host received `ControlFrame::programmer`. Only PURE PANEL mono/stereo cases differed. |
| Physical parameter roundtrip | 2 | `P_chorusTabs` was missing from the test's discrete-parameter list; its two fractional illegal values were compared with the host's snapped choices. |
| Held sustain response | 2 | The bounds used raw 0.25/0.65 rather than the corresponding selected electrical sustain targets. |

The corrections do not change the numerical envelope limits (`2e-5` value, `2e-6` elapsed time), zero-frame state preservation, original scenario/sample counts, host/core audio bit identity, physical-parameter skew tolerance, or any existing assertion family. Receiver ownership is now stricter: stored-bit identity for every pin, effective CV and loaded bus voltage, plus the saturation mask.

## Independent truth and ownership boundaries

Acceptance was registered first in `tests/parity/tolerances-v2/ui-electrical-programmer-oracles.json` (commit `1bc691f`). The UI tests answer who owns the controls and whether time/state is continuous. They do not re-calibrate the programmer circuit.

For PANEL and memory sources, the host fixture independently selects the stored APVTS bank, compiles a fresh stateless source and drives separate `Adsr` and `FilterEnv` instances. It never reads Engine's cached tones or envelope targets. Fresh selection distinguishes PANEL, M1/M2 and M3/M4 and preserves the original inactive-bank/hidden-PANEL edit scenarios. The factory-target oracle remains its original raw-cell and explicitly labelled hypothesis calculation.

Absolute electrical accuracy remains in `ProgrammerIntegrationTests.cpp` against `reference::memory_panel::MemoryPanelNetwork`, including its separate row transcription and preregistered `programmer-integration.json` tolerances. That full nodal reference is independent of the compiled audio source. Its physical tolerance must not be silently substituted for an envelope-routing tolerance.

Receiver ownership uses the same fresh selected source and the observable input drives (filter charge and TWS touch nodes), then compares the resulting memoryless receiver values with the Engine cache. This receiver gate detects stale factory state, blank readouts and dropped programmer annotations. The independent live envelope histories and legacy-effective selected-source snapshots additionally reject wrong memory-bank routing. It is an integration gate; the receiver's absolute numerical laws remain in their separate circuit truth gates.

The cold initial condition follows the already committed [cold filter-envelope law](../COLD_FILTER_ENVELOPE.md): M41/IC12 and IC guide PDF page 33 separate the selected drive from the buffered capacitor state. No rendered idle interval has elapsed after prepare in these first-strike fixtures, so the retained charge starts at zero. Legacy keeps its historical assignment reset. No output fade, added time constant or curve fit is involved.

## APVTS snapshots

A separate test-only cleanup routes all 30 former direct test `copyState()` calls through `tests/HostParameterState.h`. The helper takes `getUndoableState().treeLock()`, calls `syncNow()`, then copies the APVTS tree. The returned detached tree remains freely editable by migration/malformed-state fixtures. These test helper calls run on the message thread, including state construction before the stress-test writers start. The helper does not introduce a new cross-thread flush guarantee. The four production save/restore paths were already synchronized and are unchanged here.

## Qualification

The corrected full self-test passes first at `87eee44`, then again on the final `1991fdb` tree after all mutations were restored. Both runs use the original `--self-test` workload under Xvfb/Openbox and exit **0**, with **zero failed assertions**. The final focused host-contract run also passes before mutation and after restoration.

| Registered deliberate regression | Failed assertions | Exit status |
| --- | ---: | ---: |
| Remove processor electrical programmer annotations | 348 | 1 |
| Swap selected memory-bank routing | 594 | 1 |
| Restore cold assignment reset to IL | 67 | 1 |
| Treat the five-choice chorus-tabs parameter as continuous in the roundtrip fixture | 2 | 1 |
| Restore all three source files and rebuild | 0 | 0 |

The first mutation rejects through meter identity (10), live filter history (138), live amplitude history (134) and selected receiver ownership (66). Wrong-bank routing rejects selected legacy-effective snapshots (384) and independent live histories (210). The cold reset rejects live filter history (34) and factory first-tick history (33). Every mutated source file's SHA-256 is verified equal to its saved original after restoration; no mutation is committed.

Final UI qualification also retains:

- Both main-panel goldens: **0.0000% changed pixels**, worst 8×8 block mean **0.00**, under the unchanged 0.30% / 8.0 limits.
- Minimum-size controls: **314 modern targets**, **0 undersized**; **0 other panel targets undersized** and the existing **26 documented exceptions**.
- All **71 final image captures**, including the sixteen-voice engine-tap monitor.
- Complete original failing, corrected full, final full, focused and mutation logs in [the evidence directory](evidence/ui-programmer-oracles-2026-10-31/).

A separate read-only review checked the original and final loops, limits, primitive initialization, snapshot thread ownership and the circuit-truth boundary. It found no removed assertion family or tolerance relaxation. That review ran no additional builds or tests.

The final executable is frozen as `CS80Snapshots`, SHA-256 `8d6d80adfeb4a020bee264e15bb2f22aece56882e96f89188fb216246892d4f7`. [The image/binary manifest](evidence/ui-programmer-oracles-2026-10-31/screenshots-and-binary.json) records source commit, dimensions, sizes and hashes for every capture. [The archive record](evidence/ui-programmer-oracles-2026-10-31/archive.json) identifies the complete local handover archive containing the executable, screenshots, original and final logs, and mutation evidence. The archive is preserved at `/tmp/cs80-evidence/ui-programmer-oracles-1991fdb.tar.gz` for the integration owner; the reviewable logs and manifests are committed here.

The qualification is for this frozen integrated source snapshot. Subsequent root-owned automation changes and the final merged tree still receive their normal full integration/CI gates; this report does not substitute for those runs.

The later root-owned integration also passed the unchanged complete
real-window smoke after both VST3 and Audio Unit adapters were published.
The root test commit is `61dad944b46e1fe9ff80afe988343f5c24880181`.
The run exited **0**, logged `UI SMOKE TEST PASSED`, retained both
**0.0000%** golden comparisons, checked **314 modern targets** with none
undersized, and produced **71 output files**. Its full log is archived at
[ui-integrated-au-root-smoke.log](../evidence/continuation-2026-10-31/ui-integrated-au-root-smoke.log).
The frozen private mutation logs and binary manifest above provide an
independent checkpoint for the registration tests.

## Commit boundary

- `1bc691f`: preregister unchanged limits and required mutations.
- `87eee44`: align the host input, envelope and selected-receiver oracles; extract a focused runner without removing the full self-test calls.
- `f640686`: require stored-bit receiver identity, including signed zero.
- `1991fdb`: synchronize test APVTS snapshots under the undo tree lock.

The dedicated `--host-circuit-contract-test` entry point runs the same output-meter, live-envelope, source-ownership and physical-roundtrip functions used by `--self-test`; it exists to run the registered mutations economically. Release qualification still runs the complete unchanged `--self-test` command under Xvfb, xauth and Openbox.
