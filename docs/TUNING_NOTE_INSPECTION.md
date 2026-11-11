# Applied tuning-table note inspection

The Keyboard and Tuning page can inspect one MIDI note (0–127) and channel (1–16) in the primary engine's applied tuning table. The display reports the effective source (12-TET, Scala/KBM or MTS/MTS-ESP), whether the key is mapped, and its fractional MIDI-semitone table pitch. This lets players check Scala keyboard exclusions and channel-specific MIDI tuning without playing a note or accessing engine state from the UI.

This is a current table lookup, not a measurement of sounding audio. It is before master tuning, rank offsets, pitch bend, glide and modulation. In particular, a non-real-time MTS update may change the table while existing notes retain their earlier pitch. Unmapped is distinct from waiting or an unavailable nonfinite pitch. The source label identifies the effective table slot; a cleared Scala slot or untouched MTS slot can still contain default equal temperament, so it is not proof of a loaded file or connected master. Channel zero, the engine's application-level fallback convention, is deliberately not a selectable MIDI channel.

The older pitch panel now explicitly labels its equal-tempered calculation as a 12-TET reference instead of claiming it is sounding A4 or the active temperament. Its help points to the inspector rather than incorrectly saying tuning imports are unimplemented.

## Thread and lifecycle contract

The requested note/channel share one atomic word. At the end of a callback, the audio thread echoes that selection alongside the effective mode, mapped flag and exact float pitch bits in one lock-free 64-bit atomic response. UI requests reject invalid ranges without changing the current selection. No UI read touches mutable engine state, and no callback string, allocation, lock or retry loop is introduced.

The UI shows waiting until a matching response exists. It polls through the page's existing four-Hz refresh while showing. Prepare/release invalidate the response but retain the requested selection. A reopened page restores that selection. The controls are diagnostic only: they are not automated sound parameters and do not change notes, tuning, presets or DSP. The response describes the primary engine, not independent extension/voice measurements. Display decimal formatting does not quantize DSP state.

## Validation

The native build completed 27 steps. Focused tests passed 3/3 in 2.01 seconds: inspector 292 checks, MPE status 155 checks and allocation 204 checks, all passing. Inspector cases cover Legacy/Circuit, eight/sixteen-voice settings, zero/32-frame callbacks, malformed and pending selections, lifecycle invalidation, concurrent coherent request/response words, Scala/KBM exclusions and 432 Hz reference, clear/reload, channel-specific MTS recipient isolation, real-time/non-real-time table updates, PURE and direct UI selector/text behavior. The NRT case asserts table state, not held-oscillator pitch. UI checks call existing constructors/selector callbacks; they do not exercise the physical timer or window layout.

The unchanged historical compatibility helper compared the preserved published `4443d8e123f77b3c3ca599e31da2d2121f5ff136` module against the candidate. Only the local contract's baseline pin changed; the zero-byte difference gate did not. All 336 cases, 2,378 ordered parameter IDs/normalized values per case and 15,105,216 total capture bytes matched. This is the historical 48 kHz float32 realtime matrix, blocks 64/256/1024, eight/sixteen-note inputs, three policies and existing recall/queue cases. It retains the original eighteen queue lanes, excludes new pan trajectories, and is not a complete session-metadata serialization test.

- Baseline module SHA256: `c6d58e1a1c8215a65e21e49545b674b63a72f7ee867f77556900fc11e16bfff7`.
- Candidate module SHA256: `46103c4df8ca2bcb84e78dc111132c4e4430d2f364907e86caeede889733c4db`.
- Shared capture executable SHA256: `40581933ff88c4dea2139928d8c20abcb751b4e4d03415ab1181accbb8c4dc59`.
- Both complete capture SHA256 values: `6353ce8f0af03f9d2debb0d5e9699a0bae1e545c92f8923c8595968580d3211f`.

Independent source and evidence review approved the implementation and verified complete capture bytes and artifact hashes. A separate focused repeat passed 4/4 in 25.01 seconds: tuning inspection, applied MPE status, allocation, and the actual Linux VST3 48 kHz realtime automation registration. This is not a new full native suite or six-rate/mode matrix. Historical offline-render limitations remain. No physical-window, external DAW/controller, live MTS-ESP service, full plugin matrix, CPU/deadline or universal render-determinism claim follows.
