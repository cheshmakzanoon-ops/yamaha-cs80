# Independent combined coverage audit

Verified from raw CTest result lines, not only the reconciliation JSON:
- Interrupted rerun: 297 completed, 295 passes and two failures (LFO switching and variable-rate fresh-directory guards); case 298 started without a recorded completion.
- Separate unchanged fresh-directory LFO retest: two passes, exit 0.
- Continuation: 260 passes, exit 0, 6013.86 seconds. Includes unfinished case 298.
- These passing name sets are disjoint and their union exactly equals all 557 selected inventory entries. Their order/names agree with combined-coverage.json. No duplicate passing results are needed to reach 557.
- All 192 recorded production source hashes match the current tree, HEAD 3f4fa7085d954cde8c25ed9584e8e8828044ab8f. This verifies the recorded production manifest, not every build input or historical binary identity.

The full registered inventory has 564 entries. The seven excluded names exactly match the recorded exclusion regex: cs80_locale_editor, cs80_locale_editor_mixed, cs80_locale_query, cs80_locale_query_mixed, cs80_teardown_probe, cs80_ui_smoke, cs80_voice_activity_ui_tests.

## Internal skips and provenance

Preserved the interrupted run's still-available detailed temporary CTest log as interrupted-LastTest.log. The continuation's preserved LastTest.log is byte-identical to the build's latest detailed log. SHA-256 receipts for both detailed logs and all three summary logs are in independent-audit.json.

The interrupted detailed log reports three internal Python unittest skips inside passing CTest entries: package_binary_identity skips its native Windows junction case (13 tests, one skipped); windows_installer_preflight skips native Windows directory-junction and native Windows/Inno Setup compilation cases (12 tests, two skipped). Source decorators confirm the platform prerequisites. No additional explicit skip/not-run reports were found in the continuation detailed log. Therefore 557 passing CTest registrations does not imply every nested platform-specific assertion executed.

The original full run's 549/557 result and eight failures remain preserved separately in hybrid-full-evidence/nonwindow.log. The interrupted rerun and two stale-directory failures are not erased by this reconciliation. This is complete combined selected-test passing coverage after fixture corrections and retests, NOT a single uninterrupted green full-suite run. It does not qualify the seven display-dependent tests, a real DAW, other operating systems, audio hardware, or real-time deadlines. Reused capture directories also retain the provenance limitations described in the prior fresh-path audit.

Audit writes only evidence files; no source edits, active-directory changes, or test executions. Reproduction script: independent-audit.py.
