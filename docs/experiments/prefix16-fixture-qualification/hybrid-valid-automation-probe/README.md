# Valid normalized-knot diagnostic (scratch host only)

Original registered suite failures remain unchanged and preserved. This separate executable changes only constructed test inputs in three copied test sources: clamp generated normalized curve endpoints to[0,1] BEFORE interpolation/subdivision. No module/DSP source, assertions or tolerances changed. General curves, zero-frame prior/flush/target values, policy-transition endpoints and program-transition endpoints are bounded. All original first18 lane values that were already in bounds remain unchanged.

Reason: expanded roster made lane-linear formulas emit negative or >1 queue endpoints. Production bridge clamps endpoints before interpolation; original per-sample oracle interpolated raw endpoints then resubmitted/clamped each sample, yielding a different curve. The patch makes both sides exercise the same valid normalized input curve. This is diagnostic evidence, not retrospective approval of original invalid-input expectations or a production test integration.

Compile/link commands preserved; scratch host uses the baseline SDK objects and original compiler flags. Same scratch executable is run against original frozen baseline and candidate VST3 modules with unchanged official dependency environment. Scope is48k realtime default only, not all six rates/modes. fixture-only.patch contains every scratch source change; no repository source modified.

Result: both baseline and candidate runs exited0. Entire stdout/stderr logs are byte-identical. Previously failing partition,16voice,zero-frame,policy/program and same-policy modern recall diagnostics now report zero error in this bounded scratch experiment. This supports the expanded invalid-knot fixture explanation for these default48k failures. Original fullsuite remains549/557; this experiment does not change its outcome or qualify unrun configurations. Source/host/module hashes retained in hashes.txt.

## Expanded guarded matrix

Revised scratch host adds finite/range checks on generated normalized points before subdivision and a runtime roster-count assertion120. All12 direct runs (baseline+candidate ×44.1/48/96k ×realtime/offline) exited0. Paired baseline/candidate logs are byte-identical in every configuration; all-results.json verifies counts/configuration/invalid-bank control. All frozen input/module hashes verified afterward. No numeric thresholds changed. Original first48k experiment preserved under initial-48k.

The intended malformed-input HostAutomationCurveTests source is unchanged and retains nonfinite prior values, INT_MAX offsets, regressing/negative points, explicit ±2 endpoint clamps and failed-read checks; its registered binary was rerun successfully (1/1). Invalid bank/program control remains in every full host run.

Metadata compatibility correction: original18 is a deliberately frozen-prefix field used by HostAutomationCurveTests, so permanent proposal retains it and adds generated_parameter_count120 for this expanded host fixture. proposal/ and proposed-valid-input.patch reflect this safe separation. The tested scratch matrix uses its isolated key_parameter_count120 field; proposal changes only which metadata key supplies the same120 check and still needs integration rebuild/test. No production or repository test changes applied.
