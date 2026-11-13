# Three existing FX wet/dry automation lanes

Appended existing FX1/2/3 mix controls at lanes112–114, preserving the literal
112-lane prefix, IDs, metadata, DSP and protected UI. Production changes only the
roster. Historical Macro112 and combined MSEG rate/amount exact-size guards become
literal-prefix guards; those historical suites were source-reviewed, not rerun.

Independent source audit preceded all execution. All nine explicit stages passed
once without interruption or retry: candidate narrow build, frozen112 helper
build, six configuration runs and collection. Nine source hashes, 5,328 frozen
baseline identities and four runtime binary identities are pinned and verified.

Six configurations cover44.1/48/96kHz in realtime/offline modes:
- 24 full and24 compatibility scenarios;354candidate+168baseline=522renders.
- All522 finite stereo PCM captures retained locally, including active controls.
- 168 actual frozen112/candidate PCM pairs, byte-identical.
- 96 scalar/block comparisons(7/64/256/1024), maximum error0.
- 24 combined reversed queue-order comparisons, byte-identical.
- 24 initial1,024-frame holds, byte-identical.
- 24 overall effects0.002080952748656273–0.012495826929807663.
- 18 combined per-lane effects0.001923542469739914–0.013317074626684189.
All effects exceed the predeclared1e-5 threshold. Empty/null, Legacy flattened
and PURE moving/constant controls are byte-identical. Historical host2e-6
scalar tolerance is unchanged. Collector recomputes every active/control
comparison and witness from actual PCM, not only runtime log assertions.

The frozen112 module is the previously qualified macro module with SHA256
`dfcb7c84efa8356b4bf6d3228d2d37816098f6711da21763677238e9c2bbb463`.
This evidence retains logs, manifests and hashes in Git; actual PCM and compiled
binaries remain in the local build-native and build-fx-mix-helpers directories.
Representative preceding-lane compatibility controls are not every prior lane.

Scope: fixed classic chorus/phaser/drive types1/2/5 for respective slots, models0,
gain compensationoff, conservative mix histories, manual/manual two-note audio,
neutral macros and matrix amounts. Selected effects tested separately and together.
No type/model switching, delay/reverb qualification, all-model/full-range coverage,
state readback, constituent-isolation, native DAW, cross-platform, hardware-fidelity
or CPU-target claim. All protected UI and historical evidence remain unchanged.
