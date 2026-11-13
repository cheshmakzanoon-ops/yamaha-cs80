# Fixed modern FX mix automation with outer gain compensation enabled

Test-only gate on the existing 115-lane FX module. All 5,373 base tracked paths
remain unchanged. Fixed slot 1 BBD ensemble (model 1), slot 2 Drive (model 5),
slot 3 BBD ensemble (model 1), separately and together. Classic types 0; A/B 0.5;
oversampling 2; outer gain compensation fixed ON throughout each render.
No type/model/bypass/compensation switching and no A/B automation.

## Source grounding

`FxSlot::liveTypeFor` selects nonzero modern models even with classic type 0.
`FxSlot::process` runs the optional slot-level `LoudnessMatcher` on slot input
and already wet/dry-mixed output, then multiplies both output channels by its
returned gain. All three slots have independent matcher state. A change in the
compensation enable flag resets matcher measurement/history, not the effect's
stored tail. This fixture fixes the flag before processing, rather than testing
that switching behavior.

`FxLoudness.h` defines K-weighted stereo energy averaging (1 second), a recent
input peak with 3-second decay, target updates gated above 1e-7 mean square and
within 20 dB of the recent peak, a +/-18 dB target range, and 150 ms gain slew.
When the input gate closes the target is held; the gain continues slewing toward
that target. These are existing modern engineering laws, not hardware claims.
`FxModels.h` keeps ensemble/Drive's linear wet/dry mixing. Drive's intrinsic wet
loudness matcher remains active in addition to the enabled outer slot matcher.
The gate qualifies mix timing with these existing stateful paths enabled; it
does not isolate their individual response or prove loudness convergence. A
matched-OFF scalar control uses identical active curves in a separate render;
only the outer compensation flag differs, while Drive's intrinsic matcher remains
active in both. An above-threshold output difference demonstrates that enabling
the outer matcher changes the output for these histories, not loudness calibration
or standards compliance. It does not test live flag toggling.

## Ordinary histories and frozen gates

Six configurations: 44.1/48/96 kHz, realtime/offline Linux VST3 float32. Same
bounded 16,384-frame two-note history as the reviewed modern-mix gate: notes
48/49 on frames 17/18, off 14336/14337; manual/manual, neutral macros/matrix,
no drift/dispersion/noise. Selected mixes start 0.10 through frame 1023, then
0.35/0.15/0.45/0.20/0.10 at 2047/4095/8191/12287/16383. Combined interior knots
have offsets 37 times slot index. All selected mixes stay above bypass threshold.

Four scenarios times 14 renders plus seven combined captures = 63/configuration,
378 retained finite stereo PCM files of 131,072 bytes each. 96 scalar/block
comparisons (7/64/256/1024) retain the historical 2e-6 bound. 24 reversed-queue
checks and 24 initial 1024-frame holds must be byte-identical. 24 overall and
18 combined per-lane witnesses must exceed the unchanged 1e-5 threshold over
frames 1024..14335. Each scenario also retains `outer-match-off.pcm`; all 24
matched-ON versus matched-OFF scalar differences must exceed the separately
predeclared 1e-5 outer-match threshold in the same held window. Empty/null, Legacy final-point flattening and PURE moving/
constant controls must be exact. PURE disables the optional outer compensation
through existing policy; no claim it remains enabled under PURE. No cross-build
compatibility claim or new automation lane is added.

## Execution and retention

Independent source audit precedes helper build/runtime. Freeze the four new
source files and pin known-qualified module SHA256
`8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`.
Eight exclusive stages: helper build, six explicit runs, collection. Verify all
old tracked paths and source, module, helper, preceding evidence/capture hashes
before and after stages. Never overwrite or retry an incomplete stage; preserve
errors and investigate first. Collector recomputes every active/control metric
from all retained PCM using frozen contract thresholds.

No production, DSP, protected UI, metadata, source authority or historical
artifact edits; no broad suites, malformed/parser/state reproduction, scanner,
GUI, benchmark or privileged workaround. No modern reverb/echo, all-model/range,
source-mask, native DAW, AU, cross-platform, CPU or hardware-fidelity claim.
