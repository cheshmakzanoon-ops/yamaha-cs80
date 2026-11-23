# A26 — bowed-response state and host compatibility review

Reviewed `c4fe30987a5242305be60bd6cfddb144facd6643`; instrument-body header SHA256 `d33f34f5cff6341abc9bc2c85056ce381b492d141a0fad856e0c7cbd4f06422e`.

**No stale-cache or new host-state defect found.** This is a focused static review plus a header-only lifecycle probe, not a replacement for native host qualification.

## State paths

| Path | Finding |
|---|---|
| `InstrumentBody::reset()` | Clears modes/count, disables the model, resets control scheduling and pitch/colour history. Bowed arrays are not cleared, but are inaccessible while Off. This is safe rather than an omitted audible reset. |
| Bowed `noteOn()` | Resets first, then fully writes each of the 24 active inverse responses and gains; resets the response bound and common scale. Every future bowed use therefore has fresh note/size geometry. |
| Model changes | Non-bowed `updateControls()` never reads bowed arrays. Changing from another model to bowed runs the full initialization before any tick. Invalid model numbers clamp to the existing supported range. |
| Array bounds | Bowed excitation uses the shared `harmonicModeCount = 24` constant; it never takes the piano-specific 42-mode path. Its positive formant/source weights give exactly 24 active entries. |
| Response normalization | Body response has a positive 0.32 floor, so cached inverses are finite. Bounded note, size and live frequency inputs keep the formant arithmetic finite. |
| Copy/reassignment | `Engine::startVoice()` value-copies the entire outgoing body before resetting the incoming one. The new arrays, reference geometry, bound, ramps, RNG and control countdown are copied automatically; no pointers or external cache ownership were introduced. |
| Three-millisecond handoff | Outgoing `previousBodyFrequency`, colour and pressure remain frozen. The copied bowed body therefore retains its own geometry and current gain state; new-note initialization cannot overwrite it. |
| Prepare/rate reset | `Engine::prepare()` prepares both current and outgoing bodies/filters and clears handoff count. A later note reconstructs all coefficients and body-response entries. |
| Panic/all-sound-off | Both current/outgoing bodies and filters reset, and handoff count becomes zero. Stale cache values cannot produce output. |
| Saved state | The arrays are transient DSP state, not APVTS parameters. `getStateInformation()` saves parameter state; `setStateInformation()` restores normalized parameter state. No body-model range, parameter ID, host slot, version hint or serialization field changes in this correction. Existing totals remain 200 core / 306 host parameters. |

## Focused runtime check

Compiled the header-only probe with `-fsanitize=address,undefined -fno-omit-frame-pointer` and ran with leak scanning disabled. It compares reused/copied bodies to fresh objects at 44.1, 48 and 96 kHz through all 20 model choices:

- copy a bowed body after a large moving-pitch segment, then continue both copies;
- change a previously bent bowed body to each model;
- reset and retrigger a differently sized bowed body;
- prepare and retrigger after prior notes.

**240 lifecycle cases; 983,100 compared float samples; zero byte differences and zero non-finite results.** ASan/UBSan emitted no diagnostics. Reset produced immediate exact silence.

Evidence: `a26/probe.cpp`, `a26/probe.log`, `a26/summary.json` in the continuation scratch directory. No production files, tests, commits or refs were changed.
