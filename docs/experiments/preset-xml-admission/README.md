# XML admission evidence

- `manifest.json`: exact final source/binary hashes, limits and result summary.
- `baseline-tests.patch`, `baseline.log`: test-only main-1726b935 reproduction,
  180 failures in 639 checks. The nine valid XML cases pass this baseline.
- `initial-harness.log`: pre-correction exact-float comparison diagnostic.
- `initial-unicode-fixture.log`: Unicode fixture accidentally constructed through
  JUCE's ASCII constructor. Corrected to explicit UTF-8 before final testing.
- `big-endian-before-fix.log`: three UTF-16BE candidate failures, corrected by
  selecting the explicit decoder from the BOM.
- `stale-harness.log`: an in-flight compile missed the newly added CLI handler.
  The harness was explicitly recompiled; `repeat-build.log` is the final no-op.
- `final-ctest.log`, `final-LastTest.log`: ten final CTests, exit 0.
- `independent-review.json`, `independent-*.log`: independent review and repeat.
- `dependency-check.json`, `snapshot-dynamic.txt`: static hidden-symbol check.
- `core-only.log`, `offline-configure.log`: dependency isolation/offline checks.
- `xxe-test.log`, `xxe-evidence.json`: blocked syscall-trace attempt, not a
  successful test trace. No retry or security change was made. Final XML tests
  include the separately permitted plain-sentinel and owned-FIFO controls.
- `packaging/`: current installer/archive notice tests and license-scope audit.

Raw logs intentionally retain malformed XML inputs with trailing whitespace.
The unified baseline patch also retains its required blank context line.
These evidence files are not reformatted to satisfy whitespace lint. Source
whitespace was checked separately, excluding this evidence directory and the
verbatim/provenance notice directory; there are no product-source warnings.
