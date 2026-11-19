# Separate native-interface work diagnostic after varied release

This is the five-arm follow-up, not part of the four shorter scenes in
the sibling package. The actual unmodified Linux VST3 host plays eight
different velocities, renders 1,024 held callbacks, releases the notes
and renders 8,192 tail callbacks before the analyzed quiet window.
Measure four 64-frame calls, all five, or independently the last one.
Every arm rendered all 9,222 audio callbacks.

Instructions within the actual plugin edge: 6,568,831 to 6,234,367
(5.092% fewer translated guest instructions). Data references fell
1.231% reads and 5.697% writes. No stopwatch or native CPU claim.
Native and Callgrind stock/candidate pairs agreed within their respective
runtime on PCM, FP records and public states; the runtimes differed
from each other on small release-tail float words (absolute maximum
about 1.187e-38). The complete [decision](decision.json) records the
cross-runtime mismatch. It is not removed from evidence or compared to
the cloud's native stopwatch.

Run `python3 -B check.py` to replay every raw measured Callgrind dump,
exact genuine entry counts and both count controls. The compact artifact
retains per-arm public/FP/PCM hashes but does not include all original
audio/state bytes or independently recheck them. The full losslessly
deduplicated local evidence archive used for the original replay was
SHA-256
`5104da5e34aeb6ce37971c3c8b1cfb4e53cda80fd5ec5b4cde701266acf1b398`.
A fresh full artifact run uses the trusted stock and candidate modules,
the pinned JUCE SDK and the included run script.
