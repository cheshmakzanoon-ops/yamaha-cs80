# UI completion and native tone-source review

2026-11-01. Read-only qualification of
`bcd51d3a9f8ed962b250cf1ae2113c0003e6b47f`, following the handover PDF,
STATUS, workstream handovers and original improvement plan. This review adds
source identities and evidence provenance. It changes no production source,
parameter, sound, tolerance or test.

## UI implementation findings

| Handover item | Reviewed behavior | Existing regression evidence |
| --- | --- | --- |
| Context menus and MIDI learn | `ParameterContext` binds the actual host parameter ID on main and Studio switches, host-parameter combo boxes, tone/memory selectors, and memory-lid sliders/switches. A miniature's shared display formatter does not redirect its write to the panel parameter. Popup interception precedes the normal button/selector handler. | `ParameterContextTests.h` covers seven widget families, independent memory writes, visible choice text, incompatible units, right-click handling and balanced typed-entry gestures. Replacing every binding with volume rejects all seven families. |
| Host undo gestures | `ParameterChangeAction` emits begin/end around the synchronous APVTS callback only for each changed parameter during undo/redo. Coalescing and the existing classification of external changes remain in place. State snapshots/replacements use the shared tree lock and explicit synchronization. | `StudioUiTests.h` checks changed-parameter replay gestures. Omitting the replay gesture branch rejects three checks. The later state synchronization evidence retains the full UI suite and an omitted-gesture mutation. |
| Voice monitor | The audio thread publishes a coherent atomic frame containing actual `getVoiceInfo()` and `getRankInfo()` values. The drawer selects a physical slot and rank, clears history at reassignment, and retains the previous frame if publication overlaps its bounded read. Telemetry never feeds audio. | `VoiceMonitorHostTests.h` compares exact engine taps in Legacy, Circuit and PURE and compares monitored/unmonitored output bytes. Blank taps reject six checks. Focus/capacity mutations independently reject the selector assertions. |
| Sixteen-voice meter and pressure ownership | Slots 8–15 contribute activity bits, counts and diagnostics. The level meter reads the final combined host buffer. Pressure ownership snapshots both instruments and rejects any active host-owned alias of the same note before permitting the owned UI strike. | Second-instrument meter omission rejects two checks; second-instrument ownership omission rejects two. The original eight-slot pressure audit keeps all 151 checks and its storage bound. |

The completion commits are
`2c0866e93de5b83140058c11ce74e99e7437fdfa` (parameter contexts),
`0d9f522ba23aff29e7e31e238f0d551e9b632146` (undo gestures), and
`3c08e8e8d25f0ee1427f2467a199f37940a2ea3d` (engine taps and second-instrument
ownership). Their prerequisites, seven mutation results and initial complete
smoke are recorded in [ui-completion.md](../../../workstreams/ui-completion.md).

The complete integrated smoke at
`61dad944b46e1fe9ff80afe988343f5c24880181` is retained in
[the full root log](../../continuation-2026-10-31/ui-integrated-au-root-smoke.log):
`UI SMOKE TEST PASSED`, both original goldens at **0.0000%** changed pixels,
**314** modern controls with **0** undersized, and the existing **26** documented
hardware-layout exceptions. The later programmer integration oracle correction
retains the original 385-failure log, unchanged limits, and four independently
rejected mutations in
[ui-programmer-oracles.md](../../../workstreams/ui-programmer-oracles.md).
These are dated qualification records; the final integrated tree still receives
its full build, smoke and remote CI gates.

The scope remains explicit: nonparameter session configuration, such as MSEG
loop/trigger configuration and modulation-matrix routing, is not a newly exposed
host parameter. MIDI learn is disabled and its stored mappings are inert in
PURE. PURE uses eight engine slots. The monitor curves show control-tap history
sampled by the 15 Hz view, not measured audio waveforms or a modeled transfer
response. Source inspection found no additional defect requiring a production
patch in this workstream.

## Native source provenance

A fresh GitHub API read on 2026-11-01 confirms that all ten jobs in
[Circuit source inspection run 41224106605](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605)
completed successfully at
`0281af873389a13452ac5dca2ef9b90dcec07c72`.
[workflow-jobs.json](workflow-jobs.json) records the individual job IDs, source
commit, success state and URLs.

The archived logs from those exact jobs were decoded again. For every emitted
crop, this review checked one begin/end pair, contiguous chunk indexes, the
declared base64 length, strict decoding, the emitted PNG SHA-256, PNG signature
and IHDR dimensions, and exact equality with the locally viewed PNG bytes.
All **70** crops passed. The metadata and exact saved-log hashes are retained in
[crop-provenance.json](crop-provenance.json); downloaded scan images are not
committed.

| Source | Verified crops | Image provenance |
| --- | ---: | --- |
| S31–S38, T51–T54 schematics and board artwork | 48 | Six native, unresized tiles per 5100 × 3300 source image |
| M41 | 7 | Three native detail crops and four resized overview tiles |
| ADJ PDF pages 12–14, printed pp10–12 | 15 | Full-page and quadrant crops after the workflow's `pdftoppm -scale-to 1800` render |

The visual reread covered all 25 marked schematic cells, their unprimed output
row labels, and the consequential S36 board candidate. The complete existing
[25-cell transcription](../../continuation-2026-10-31/TONE_CELL_NATIVE_REVIEW.md)
is confirmed without promoting any mark to an established installed value.
In particular:

- T53 B14 has an ambiguous first schematic glyph before `5K` on S35. S36 clearly
  prints `75K(39K)` at the candidate footprint. The proposed T53/T51 pairing,
  supported by neighbouring `20K(13K)` and `750(1K)` pairs, remains a declared
  inference. The frozen 25K-looking alternative is retained separately.
- T51 F22 shows crossed `3.6K` with `12K` below; the leading glyph of crossed
  T51 F25 remains insufficiently clear to strengthen its retained `5.6K`
  candidate. T52 B9 shows crossed `12K` with `10K` below; T52 C22's replacement
  `12K` is clear while its former value remains qualified.
- T53 E23 shows crossed `16K` with `22K` below. T54 C22 shows crossed `8.2K`
  with `12K` below, and C25 shows crossed `56K` and an altered diode symbol.
  Neither visible revision marks nor crossed symbols establish installed-part
  applicability. The remaining marked resistor/diode cells retain the same
  page-cited qualifications in the transcription.

Thus the source-fetch and transcription task is complete, while **all 25 source
cells remain labelled hypotheses**. Further promotion needs evidence resolving
revision/install applicability or obscured glyphs. No hardware measurement is
claimed, and no guessed value is substituted into V1.

## Frozen numerics and current documentation rows

Static comparison with original main
`db24a47170610f81236e8829ef5536af69dfc94d` confirms byte identity of the frozen
**11,526-byte** prefix of `FactoryToneHypotheses.h` and the entire **20,864-byte**
`HardwareControl.h`. The hypothesis fingerprint stays `e5f877a025fc52c6`.
[source-identity.json](source-identity.json) records both SHA-256 values, the
prefix boundary and the reviewed UI/test/evidence file identities. This source
comparison complements the numerical compatibility gates; it does not replace
them.

Recommended wording for the next current-state documentation update:

| Item | Accurate current-state wording |
| --- | --- |
| UI editing and undo | Implemented with host-parameter contexts, exact memory-bank binding, balanced undo replay gestures and mutation-qualified regressions; retain the final integrated CI result alongside the dated evidence. |
| Monitor and sixteen voices | Engine-tap monitor, second-instrument activity/level metering and pressure ownership implemented; PURE remains eight voices. |
| Tone-tab source inspection | All 25 cells transcribed from native S31–S38 crops; source payloads verified. All 25 retain explicit hypotheses because applicability/topology or glyph ambiguity remains unresolved. |
| ADJ HP setup evidence | Preserve the documented **1.692 Vpp** model result against **4.3 ± 1 Vpp** on printed ADJ p11. The setup discrepancy remains open; no fitted correction is justified by this review. |
| M41 LP input topology | Preserve the native-trace conclusion that **150 kΩ is in series from CP7 to AI**, with a separate 33 kΩ shunt. Do not repeat the superseded description of 150 kΩ as a shunt. |

The last two rows refer to
[NATIVE_FILTER_EVIDENCE.md](../../continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md).
CPU, phrase-click score qualification, the proven validator-host fix, host
automation/MTS qualification and release landing have separate evidence owners.
This review does not declare their targets met. No duplicate heavy build or
benchmark was run during the isolated CPU measurements.
