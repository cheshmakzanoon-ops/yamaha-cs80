# REAPER tour dismissal: failed V2 and bounded input correction

Actual Actions run [41510793290](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41510793290)
used source commit f65197491c69c206e42329ae9b88ec5b1aa4eef6, unchanged production
src 7dd159d491012fd47b46c0aa651e86ea959ca289. Artifact13173685819 ZIP SHA-256
275d1da9ac4124281c45b48444dc89dacce014c9adb88d5e4e60756a2e048fc6 remains a
failed dry V2 result, not plugin/audio acceptance.

Actual raw pixels were inspected independently. Before Escape, the instrument
renders Tour1/9 and Close Tour; the REAPER evaluation window also remains open.
After one Escape to wrapper0x40011e, the instrument editor is closed while REAPER
remains visible, its FX stays enabled, and the host command label says Close FX
config. The host's pre-existing audio-device error dialog is then exposed.
Host API editor_open changes true to false; all24,036 previous setup checks
are true. No render or audio verifier was reached. This does not establish a
plugin crash or a new audio defect. The live-device absence is unchanged and
excluded from this offline smoke.

## Registered correction, before implementation

Add one regression to the existing mock-only dispatch controls: no Escape may
be sent through the host wrapper. Current runner should fail this new regression
and retain the three original mock controls passing. Preserve the failed run and
original V1/V2 contract bytes, every render, numeric bound, and evaluation date.

Replace only the dismissal input: find exactly one visible named instrument
window, require its observed1600x1028 client geometry, activate it and capture
actual root pixels. OCR the raw screenshot with official Tesseract TSV output;
require one visible Welcome to the CS-80 heading, one Tour1/9 step label, and
one Close Tour label, all inside the source-defined centred first-tour card.
Only high-confidence heading/button words and the actual observed label centre
may choose the one ordinary left click. Fail closed on missing/ambiguous labels,
wrong geometry, stale geometry, failed activation/OCR/input, or out-of-card text.
Never send Escape, use guessed screen coordinates, suppress the evaluation UI,
write tourSeen, or modify production code.

Retain raw before/after screenshots and TSV. After the click require absence of
the first-tour heading/button in the observed editor and a newly created true
tourSeen setting written by the actual callback; retain that settings XML as
output evidence. The actual host API must still report the editor open after
acknowledgement. Those checks do not replace human inspection of the new pixels.
Add negative/mock controls for recognition, ambiguity, geometry, failed input,
post-click persistence and host editor survival. No local display is attempted;
only the authorized Actions path can execute a native host.

The earlier raw screenshot's Tesseract5 TSV recognizes the exact heading and
Close Tour at80+ confidence. This is read-only diagnosis of existing pixels, not
a new host result. Add only official Ubuntu tesseract-ocr to the existing CI
free-dependency installation. New actual native input and audio remain pending.
