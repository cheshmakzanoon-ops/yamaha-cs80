# Evidence scope

- `baseline-tests.patch` applies to production base `d5ed3ce3`; only tests/harness
  change. `baseline.log` records 120 checks and the three expected failures for
  duplicate-file loaded marker and navigation.
- `native-peer-attempt.log` records eight passing focused tests and a full UI
  smoke failure before the product test started. `xvfb-startup.log` identifies
  the unavailable local socket listeners. This is not a full UI smoke pass.
- Final source/binary hashes, targeted tests and independent review are recorded
  in the manifest and final logs. This is local Linux qualification only.

`pre-save-adjustment/` retains the previously approved candidate, not the final
source. The final correction stops successful save from reading then rewriting
the live program index; name and file origin are still published together. Final
hashes and all final test results are in the top-level manifest/logs. The native
peer attempt also predates this correction and never reached product code.

Copied compiler/Xvfb diagnostics have trailing whitespace removed for the
repository whitespace gate; diagnostic text and results are unchanged.
