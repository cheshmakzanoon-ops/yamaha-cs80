# MTS-ESP master scale name in the tuning page

The Keyboard / Tuning page now shows the connected master's scale name beside
its MTS-ESP status, and includes the full bounded status in a tooltip. The existing
4 Hz page timer refreshes the name while the page is showing. Empty, null or
rejected reads remove the previous name. No name is stored in the session.

Retrieval runs synchronously on the message thread. The adapter checks for a
master, copies at most 255 bytes into owned storage, normalizes ASCII control
characters to spaces, and rejects an observed disconnect. The processor validates
UTF-8 before constructing the displayed string. Valid Unicode is preserved;
malformed or cap-truncated UTF-8 uses an ASCII-safe question-mark fallback.
Long names are bounded, not allowed to grow the label or tooltip indefinitely.

The audio callback does not query or copy scale names. No audio-path lock,
allocation, polling change or shared mutable string is added. The page stops its
existing timer on destruction, and the synchronous query does not retain an
external pointer or create an asynchronous lifetime dependency.

## Status and concurrency limits

The existing connected indicator means the last audio poll accepted an MTS table
in the active tuning mode. Name visibility uses that same condition, rather than
claiming that a stopped processor has performed a fresh tuning handshake. An
observed missing master rejects the name even if the last accepted-table status
has not yet been refreshed by audio processing.

The external API supplies a pointer to master-owned text. Bounded copying and
pre/post connection checks do not prove coherent concurrent updates or lifetime
safety of an external implementation that violates its pointer contract. Live
master discovery, concurrent service updates, external DAW behavior and actual
window rendering remain unqualified by the deterministic tests.

## Regression contract

The initial real-page/processor fixture reproduced two missing-name assertions:
5,636 checks with 2 failures. The expanded fixture tests initial and updated names,
existing-page refresh, label/tooltip agreement, controls, Unicode, malformed and
cap-truncated UTF-8, long names, empty/null names, observed disconnect and zero
name queries during an audio callback. The no-client build returns an empty name.
These are direct native component callbacks with a mocked external C API, not an
OS pointer/hover or live-master test.

The final Release/Linux run passed **14/14 focused CTests in 61.97 seconds**,
including all six actual-module VST3 rate/mode registrations. The final real-page
processor fixture passed **5,685 checks / 0 failures**, channel adapter **141,469/0**,
and allocation fixture **193/0**. All affected adapters and native targets were
rebuilt before the final run. These results do not qualify OS tooltip rendering,
external DAW interaction or a live MTS master.

Review caught two intermediate issues: the framework UTF-8 validator accepted an
invalid continuation sequence, so strict scalar validation replaced it; and the
status label initially ignored mouse hits, so it was made a tooltip hit target.
The hit-test fixture explicitly makes its page visible without creating a window.
