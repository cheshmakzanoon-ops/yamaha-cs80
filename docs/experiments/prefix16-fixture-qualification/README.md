# Prefix16 fixture qualification

Original frozen99fc0c2a full plugin build succeeded. Original557 non-window tests:549pass/8fail,11677.58s;7display tests excluded. Original failures and environment dependency setup logs retained, not replaced by passing retests.

Default VST3 automation failure reproduced exactly on c511201 baseline. Expanded120-parameter fixture generated out-of-range normalized knots, and its scalar reference interpolated raw values while host clamps endpoints. Corrected valid-input generation passed both unchanged modules across all6rates/modes (12runs), byte-identical paired logs. Existing invalidinput tests untouched. Historical key_parameter_count18 retained; generated_parameter_count120 added, numeric tolerances unchanged.

Both state-test failures reproduced on baseline; unchanged candidate repeat passed2/2. Reviewed test-only synchronization now observes actual callbacks and timer barrier, retains exact coalescing/reentrant/negative/thread/state assertions. No production timer edits. Polling deadline is not a hard latency guarantee.

Integrated registered retest9/9passed574.22s: all8prior failures plus unchanged invalidinput control.192production file hashes recorded. Full557suite rerun pending, not yet claimed passed. No remote writes or promotion.
