# Sample-accurate VST3 rank pan

## Capability and compatibility boundary

The two rank pan controls now preserve VST3 host sample offsets in modern circuit policy. They are appended after the original eighteen lanes, with their existing parameter IDs, normalization and DSP pan law. This is an intentional capability expansion: modern queued pan previously used the block's final value. It is not a claim that all parameters, AU automation or every host are now sample-accurate.

Pan is a software control independent of the programmer's PANEL, factory-tab and MEMORY selection. The automation adapter therefore exempts only `CP_pan` from its selected-source suppression. The other rank lanes remain subordinate to the selected programmer source. Pan is still recalled by ordinary preset changes; it is not added to the physical-setup parameters that survive recall. Existing curve-versus-program ordering remains unchanged.

Legacy and PURE retain final-point-at-block-start handling, and no-queue behavior remains unchanged. The actual product format selector still patches only VST3; AU and Standalone paths are not expanded. Fixed-size lane arrays gain two entries, with no arbitrary point cap, queue copy or new dynamic allocation in this feature. Timing still applies host normalized interpolation before the existing DSP behavior, not a new pan smoothing or circuit law.

The frozen `host-automation.json` remains byte-for-byte unchanged. `host-automation-pan-v1.json` registers the twenty-lane extension separately. The scalar test explicitly checks the original eighteen-key prefix/order and the two appended keys. Existing interpolation and audio-error tolerances are not relaxed.

## Reproduction and focused evidence

A clean no-work native build at `b8c66edf` was copied before the feature and retained as the baseline module. The final expanded actual-VST3 helper against that module exited 1 with 53 failed checks, including missing within-block pan movement and scalar/partition disagreement. This negative control uses the new capability expectations; it does not label the old documented block-rate behavior an unexplained regression.

The first candidate 48 kHz realtime registration passed in 22.90 seconds. Sixteen pan scenarios cover each rank independently, PANEL/factory tab 0/both MEMORY selections, and eight/sixteen-voice settings (two sounding notes in eight-voice mode, sixteen in sixteen-voice mode). Each compares actual native queues at 7/64/256/1024/4096-frame partitions against independently evaluated one-sample native updates. All sixteen pan maximum differences were zero in this run. Finite nonzero PCM, left-then-right stereo energy inside one process call, and silent-rank isolation are positive controls; they are numerical signal checks, not perceptual listening evidence.

No-queue and empty-queue PCM are byte-compared within each module. The two protected policies are byte-compared with flattened native queues. All 48 retained no-queue/Legacy/PURE PCM hashes also matched the preserved baseline at this configuration. The first expanded integration run rebuilt 51 steps and completed 46/47 tests in 342.35 seconds. All six actual-module configurations (44.1/48/96 kHz, realtime/offline) passed, totaling 96 pan scenarios with zero measured pan partition error and zero silent-rank error. The smallest pan-versus-neutral PCM difference was 0.0147179. Existing combined-lane, policy/program, zero-frame/readback and MTS checks also ran within those registrations. These are observed values in this run, not a universal exactness guarantee.

The single failure was the old direct multi-recall fixture's frozen eighteen-lane roster assertion, before any of its state cases executed. That historical fixture now explicitly retains its original eighteen queue lanes, arrays and exact state contract. The historical 336-case compatibility capture similarly retains its original eighteen queued lanes. Neither frozen JSON contract or numerical threshold is changed; the new native boundary fixtures retain the dynamic twenty-lane roster. The corrected historical direct witness passed 1/1 in 0.73 seconds after a five-step capture/witness rebuild. The final same capture executable was then run against both independently preserved modules with a local copy of the strict historical contract, changing only its required baseline pin to `b8c66edf`. All 336 cases, complete 2,378-parameter ID rosters and normalized values, and all 15,105,216 binary capture bytes matched. This historical capture is 48 kHz float32 realtime, with 64/256/1024-frame blocks, eight/sixteen-note inputs, three policies and its existing recall/queue variants. The historical capture queues only its original eighteen lanes; modern pan trajectories are deliberately outside this byte-compatibility gate.

Artifact identities for that strict comparison:

- Pre-feature module: `14fa449975b32fe2f75d1518a84a2485429fd192b9ce00cafa66f611e2ca9df2`.
- Candidate module from the feature source now committed as `635772b6`: `9246fbb3c58c0dd6ff0ccd3a7891fc86f98ba6c0d1f7f72188031417a2eee95f`.
- Same capture executable for both modules: `40581933ff88c4dea2139928d8c20abcb751b4e4d03415ab1181accbb8c4dc59`.
- Both complete captures: `6353ce8f0af03f9d2debb0d5e9699a0bae1e545c92f8923c8595968580d3211f`.

The independent final integration repeat passed all 47 selected tests in 348.36 seconds on the corrected source, without changing criteria. All 96 pan scenarios across the six rate/mode configurations again had zero measured pan partition and silent-rank errors; the allocation fixture passed 204 checks and the frozen multi-recall witness passed 65 checks. The module hash remained the candidate identity above. A separate focused repeat passed 3/3 tests in 26.98 seconds: scalar curve checks, the actual 48 kHz realtime module registration (16 pan scenarios), and allocation checks. Independent reviewers also compared the complete strict captures and verified the original frozen contracts and workloads. These repeats qualify this bounded feature, not a general guarantee of exact offline rendering.

The native snapshot build emitted two pre-existing enum-conditional warnings; no warning-free-build claim is made.

## Limits

The source selector for both ranks is configured to the same selected source in each case; independent mixed-source combinations are not added by this fixture. The six rate/mode configurations of the host infrastructure use float32 processing and an actual Linux VST3 module, not an external DAW or GUI. No hardware equivalence, subjective stereo assessment, automation CPU/deadline target or universal offline-render guarantee follows. Earlier offline comparison failures remain a separate historical limitation.
