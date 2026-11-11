# Preserve valid pending UI controls against malformed input

Ribbon, pitch-wheel and modulation-wheel UI setters previously stored any float
in their single-value mailboxes. The audio callback safely skipped nonfinite
values, but by then a malformed value could have overwritten an earlier valid
pending command. The player’s valid movement was silently lost.

The three setters now reject nonfinite values before storing, matching the
existing expression-pedal setter. The audio callback remains the only DSP writer.
Finite last-value behavior, relaxed atomic ordering and downstream clamping are
unchanged; no extra lock, allocation or callback is introduced.

The real-processor regression covers all three controls, Legacy/Circuit and
8/16-voice operation. It verifies that a valid queued command audibly differs
from omission, valid-then-NaN/positive-infinity/negative-infinity renders exactly
like the valid command alone, and each invalid value alone remains inert.

Exact baseline: **84 checks / 36 failures**. Fixed: **84 checks / zero failures**.
After rebuilding affected native targets, ten registered tests passed **10/10 in
50.31 seconds**: mailbox, Scala-session admission, physical-parameter admission,
deterministic MTS processor, and six actual VST3 automation/SysEx rate-mode tests.
Existing tolerances were unchanged.

This is API-to-audio regression coverage using local Linux Release processing,
not physical pointer/GUI event qualification, concurrent-writer stress, external
DAW/device behavior, full native suite, sanitizer, performance or hardware fidelity.
