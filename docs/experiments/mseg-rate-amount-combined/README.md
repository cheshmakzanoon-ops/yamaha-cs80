# Simultaneous MSEG rate and matrix amount histories

Qualified on the unchanged 112-lane native Linux module from local source
21ad03f35b05c066a1fb67b7410dfaf4e800d610. This is test-only evidence, not a new
production capability or a baseline compatibility comparison.

All nine explicit stages completed once without interruption or retry: narrow
helper build, public canonical generation, six runs and collection. Independent
source audit preceded execution. The retained inventory pins all 5,287 prior
tracked files; six new source hashes and four runtime binary identities are
verified by the staged qualifier. No historical suite was rerun.

At 44.1, 48 and 96 kHz in realtime/offline modes:
- 132 finite stereo PCM captures retained locally, 22 per configuration.
- 24 scalar/block comparisons (7, 64, 256, 1024), maximum absolute error
  1.862645149230957e-9, below unchanged 2e-6 tolerance.
- 48 reverse/rotate queue-order comparisons, all byte-identical.
- 36 held-one-lane witnesses, effects 0.004938058089464903–0.04273952730000019,
  all above the predeclared 1e-5 threshold.
- Six initial 1,024-frame holds and six no-route controls, all byte-identical.
- Two public canonical session roundtrips and 12 exact native config readbacks.

The collector independently recomputes every audio comparison and witness from
retained PCM, not merely log output. Completion manifests record capture and
state hashes; PCM resides in the local build-mseg-rate-amount-combined directory.
This repository evidence contains logs, source identities, manifests and metrics,
not the compiled binaries or PCM payloads.

Scope is six concurrent lanes: three MSEG octave rates plus their first three
matrix amounts. Fixed unsynced poly bipolar looping shapes feed both-rank
pitch/cutoff/level routes. Manual/manual sources, two notes, neutral macros,
fixed conservative trajectories and no via sources are used. Holding one lane
shows its contribution under this combined history, not isolation of every DSP
constituent. No all-routing, full-range, source-mask, sync/edit, native DAW,
cross-platform, hardware-fidelity or CPU-budget claim is made. No production,
protected UI, historical contract or historical evidence changes.
