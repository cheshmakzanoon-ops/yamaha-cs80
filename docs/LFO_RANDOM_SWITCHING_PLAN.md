# Variable-rate random-shape live-selection oracle

This reviewed design completed its bounded run. See the
[verified results and limits](experiments/lfo-random-switching/README.md).

Standalone core extension, no production changes or host waveform-automation
claim. Preserve variable-rate 24-case matrix:two initial shapes 5/6,three Engine
seeds, four internal rates, 0.25 s. Six toggles apply BEFORE that sample's tick:
floor(.13N), first reference wrap, wrap+1, floor(.61N), third wrap, floor(.87N).
Wrap indices come only from independent reference integration. Assert unique,
in-range events and exactly two wrap coincidences; never deduplicate collisions.

Phase and RNG continue across selection. A new selection initializes before any
normal wrap advance on that sample:hold consumes one, smooth consumes two, with no
additional wrap draw. Next-sample toggle initializes again from the retained
stream. Reference draw counts are 12/13 by initial shape; six switches/case, 144 total.
Compare whole sequences, exact hold samples and reset replay using the same saved
waveform timeline. Existing 1e-6 waveform gate remains. New predeclared phase error
gate 1e-10 uses production getter only as observed output, never as reference;
reference boundary clearance remains >1e-10, with no skipped samples.

Save 48 actual/reference sequences, 4 rate arrays, 8 waveform arrays: 60 captures, not
cross-build pairs. Independent exact-integer saved-data audit before publication.
Finite/bounded complete samples, phase and all inputs; four wraps per case; 638
retained source/test identities unchanged. No Engine smoothing, depth/routing,
statistical randomness, host discrete sample-accuracy, GUI/DAW, hardware, performance,
release or cross-platform claims. No broad suites, malformed inputs or benchmark
replays. Independent design and exact source/script review before execution.
