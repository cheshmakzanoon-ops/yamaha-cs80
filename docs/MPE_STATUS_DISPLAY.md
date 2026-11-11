# Applied MPE status display

The Keyboard and Tuning page shows a read-only observation of the primary engine's last completed audio callback: whether MPE is enabled, lower/upper zone managers and member-channel ranges, and each zone's effective manager/member bend sensitivity. Incoming MCM/RPN messages can override the stored fallback knobs; this display reports the applied values rather than inferring routing from those knobs.

Before an audio callback, and after prepare/release resets, the display says it is waiting. It reports disabled when MPE is off or PURE masks it. An enabled engine with no configured zones shows both zones disabled. Parameter changes and incoming messages become visible only after audio processing applies them. Zero-frame callbacks can apply messages and publish a new reading. The existing UI refresh polls four times per second while the page is showing.

Publication packs both member counts and four cent-rounded range values into one lock-free 64-bit atomic word. Impossible simultaneous 15/15-member counts distinguish not-ready and disabled readings. There are no audio-thread strings, allocations, locks or retry loops, and the UI never reads mutable engine state. Ranges are rounded to 0.01 semitone for display; no DSP values are quantized or altered. This is a primary-engine status display, not independent telemetry from every voice or extension engine.

This feature does not change controller admission, MIDI/MPE compatibility policy, negotiated ranges, sound generation or parameter persistence. It is an observation, not a host or external-controller connection indicator. It does not establish full MPE conformance, physical UI/DAW qualification, deadline safety or a CPU-target pass.

## Validation

The corrected native build completed 25 steps. The first invocation named a nonexistent allocation target and stopped before compilation; correcting that target name required no source workaround. The two focused registrations passed 2/2 in 1.89 seconds: 155 status checks and 204 allocation checks, all passing. Status coverage includes initial/reset states, requested versus applied changes, Legacy/Circuit and eight/sixteen-voice settings, zero/32-frame callbacks, fallback updates, two-zone negotiation, overlap shrink, disabled zones, fractional RPN ranges through 128.27 semitones, PURE masking, and coherent concurrent telemetry-only reads. UI assertions directly refresh and inspect page text; they do not exercise the actual timer, physical window layout or an external controller/DAW.

The unchanged historical capture executable compared the preserved published `c7d30c63ba8fe255262323e6de83a1aecd10980c` module with the candidate. A local contract copy changed only the baseline pin, not the zero-differing-byte gate. All 336 cases and all 15,105,216 capture bytes matched, including 2,378 ordered parameter IDs and normalized values per case. Scope remains 48 kHz float32 realtime, blocks 64/256/1024, eight/sixteen-note inputs, three policies, and the historical recall/queue cases. The capture retains the original eighteen queue lanes, not the newly added pan trajectories, and does not serialize every session metadata field.

- Baseline module SHA256: `9246fbb3c58c0dd6ff0ccd3a7891fc86f98ba6c0d1f7f72188031417a2eee95f`.
- Candidate module SHA256: `c6d58e1a1c8215a65e21e49545b674b63a72f7ee867f77556900fc11e16bfff7`.
- Shared capture executable SHA256: `40581933ff88c4dea2139928d8c20abcb751b4e4d03415ab1181accbb8c4dc59`.
- Both complete capture SHA256 values: `6353ce8f0af03f9d2debb0d5e9699a0bae1e545c92f8923c8595968580d3211f`.

Independent source and evidence review approved the snapshot and verified complete capture byte identity and artifact hashes. A separate focused repeat passed 3/3 tests in 25.07 seconds: status 155/0, allocation 204/0, and the actual Linux VST3 48 kHz realtime automation registration, including its sixteen pan cases. This is not a new six-rate/mode matrix run or a full native-suite result. Historical offline-render limitations remain; this observation feature is not a universal render-determinism guarantee.
