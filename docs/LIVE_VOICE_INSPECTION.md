# Live selected-voice inspection

The Voice Monitor now has an opt-in **LIVE** view beside its existing settings
illustrations. Previous/next selects a physical slot (1–16); I/II selects its
synthesis rank. In eight-voice mode, slots 9–16 explicitly show unavailable.
The view follows a slot, not a persistent note: reassignment changes its note.

This is read-only software telemetry, not an audio waveform, calibrated circuit
measurement or saved parameter. Note/channel and held/pedal/tail status describe
the completed callback. Envelope and frequency fields copy the rank's cached
last processed sample, not its requested parameter targets. Inactive slots,
zero-frame callbacks and callbacks ending with an unrendered MIDI/UI event do
not expose old numeric rank values. Settings illustrations remain separate and
retain their existing model/measurement disclaimer.

The UI requests one slot/rank; only the audio callback reads Engine getters.
Request generation travels with the response, so a selection change and return
(ABA) cannot relabel an older answer. Preparation/release invalidate readiness.
LIVE visibility requests inspection; the default settings view does not.

A fixed atomic tuple is published with a sequence counter. Every payload word is
atomic and every operation is sequentially consistent. A reader makes a single
attempt and rejects an odd or changed sequence; it never retries. There is one
UI request owner and one audio/setup writer under the host's stopped-audio
lifecycle contract. Publication has bounded scalar work with no allocation,
lock, waiting or DSP ticking. This does not prove a deadline or whole-engine CPU
budget. Multiple independent UI request owners are not a supported contract.

No DSP equations, audio output, host parameter roster, preset or session schema
change. The inspector does not claim sample-by-sample event recording. Short
states between callbacks or UI refreshes may be missed. Physical GUI, DAW and
cross-platform operation require separate qualification.

## Qualification

Linux component/processor qualification: 791 inspection checks and 133
allocation checks pass. Six focused/adjacent CTests pass. The capture run adds
six PNG export checks (797 total) at 420px and 315px, including a long tone name.
Coverage includes coherent tuples, selection ABA, zero-frame/event-boundary
freshness, 8/16/PURE routing, prepare/release, hide/re-show/destruction, cached
getter equality and identical PCM for an inspection-on/off stimulus. Allocation
checks cover both engine policies and voice capacities; they do not establish
a CPU deadline. Physical GUI, DAW and other platforms are not qualified.

An independent reviewer verified exact source/binary hashes before and after
repeating 791 inspection checks, 133 allocation checks and all six CTests.
All six final images were visually inspected without a blocking layout finding.
