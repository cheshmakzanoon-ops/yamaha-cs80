# MSEG rate automation qualification

Three existing octave-rate controls are appended at indices 89–91, for 92 lanes.
The literal 89-lane prefix, parameter metadata, DSP, routing, serialization and
protected UI sources are unchanged. This evidence covers three isolated canonical
MSEG routes to both-rank pitch, cutoff and level, fixed amount 0.15, unsynced poly
always-looping bipolar shapes, manual/manual source mode, and two held notes.

## Results

- 18 full and 18 compatibility scenarios across 44.1/48/96 kHz, realtime/offline.
- 306 candidate and 126 frozen-baseline renders, 16,384 stereo frames each.
- All 72 scalar/block comparisons (blocks 7/64/256/1024) have maximum error zero.
- All initial 1,024-frame hold checks and unrouted/zero-amount/PURE/Legacy controls
  pass. Positive held-note rate effects span 0.0113993–0.0389594, above the
  predeclared 0.00001 threshold.
- 72 actual candidate/frozen-89 PCM pairs are byte-identical (144 capture files).
  These cover no-queue, four representative prior-lane histories, Legacy and PURE
  cases, not a requalification of every preceding lane.
- 36 native configuration readbacks match canonical routing via the known helper.
- Source, canonical-session and five runtime-binary identities match before/after.

## Interrupted build recovery

The original attempt configured and linked the candidate module and fixture, then
was interrupted during baseline-runner compilation. Its session became unavailable;
no baseline executable, runtime captures or pre-runtime binary manifest existed
at recovery. Original logs remain in `../mseg-rate-automation/`. They are not a
failed or completed qualification run. The resumed pass retained the linked
candidate, compiled a fresh baseline runner, and used distinct fresh capture/log
paths. `interruption.json` records original log identities; the separately frozen
resume script and original source freeze were independently reviewed before run.
All runtime comparisons above belong only to this completed resumed pass.

## Limits

No combined-route, source-mask, sync, shape-switching, full-range, native DAW,
cross-platform, CPU-target or hardware-fidelity qualification is claimed.
Historical fixture guard changes were source-reviewed, not rerun. No broad,
malformed-state, GUI/window, privilege/display workaround or benchmark run was
performed. Candidate CMake target build passed; compatibility helper used the
recorded standalone compile against isolated frozen-89 headers/module. PCM and
native-state captures remain in their build directories with hashes recorded here.
