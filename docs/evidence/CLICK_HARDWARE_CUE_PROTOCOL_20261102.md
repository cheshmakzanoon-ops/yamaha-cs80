# Original CS-80 click investigation: source-locked hardware and listening cues

**2026-11-02 — protocol only. No real CS-80, technician, independent listener,
or external recorder was used. The six original musical-click score scopes
remain FAIL/OPEN.** These files cannot establish hardware parity or waive the
original numerical limits.

## Source identity and purpose

The source audio remains the frozen `src` Git tree
`7dd159d491012fd47b46c0aa651e86ea959ca289`. The original 885-row,
three-rate audit and its limits are unchanged. The existing diagnostic evidence
isolates the modeled IG00156 filtered signal as the dominant high-derivative
carrier, while the separately registered 42-row CP3 capacitor screen finds no
high-frequency click correction from one idealized input capacitor alone.

`tests/diagnostics/GenerateClickHardwareCues.py` produces **four standard MIDI
type-0 timing aids**. They are not audio or Yamaha service measurements. No
built-in MIDI interface, gate latency, mechanical contact transfer, or native
CS-80 velocity response is assumed. An owner-authorized controller, retrofit,
or external key actuator would require its *own* calibrated timing record.

The generator refuses an existing output directory and any dirty or mismatched
`src` worktree. Its independent
`tests/diagnostics/test_generate_click_hardware_cues.py` checks exact SMF
format/timing/content hashes, rejects malformed MIDI and source mutations, and
verifies no pre-existing cue is overwritten. It is registered in CTest as
`cs80_click_hardware_cue_guard_tests` when core tests are enabled.

| Cue file | Intended source control | Target time / MIDI tick | SHA-256 |
| --- | --- | --- | --- |
| `bass_panel_phrase_event.mid` | Original physical-PANEL Bass Stack; full phrase and 6.25 s retrigger | 6.25 s / 6000 | `f77c0c93f03b8f34fcd0321853e03bdf06fe8cf58613e554f1adaa54286312e3` |
| `bass_panel_phrase_omit_retrigger_on.mid` | Same Bass history, omit only the target note-on; preserve original later off | 6.25 s / 6000 | `062b8feb1c63195652b69a441e57a9c061539aeb92f179ae9d9e57d412f5ed47` |
| `funky_tabs_single_noteoff.mid` | Funky 1+2 original tone tabs; note-off at 1.40 s | 1.40 s / 1344 | `46be894d42f237abe48d63dcaf07065a98e18abe6c708e051984124338c4b663` |
| `funky_tabs_single_held_no_off_at_1400ms.mid` | Same Funky onset; *no* off at 1.40 s, safety off at 1.90 s | 1.40 s / 1344 | `46ea383dc49d8a2868bebf21bf725897f9ad3914edab2c84333795da30e1332b` |

Fixed cue format: SMF0, one track, PPQN 480, 500,000 μs/quarter, 960 ticks/s,
velocity byte 102 for synthetic note-on and zero for off. The resulting
manifest SHA-256 is
`0e6d196df140530f810109ff4f20cae129490b227ddba5923948b184ada91467`.
The 102 byte is **only a MIDI cue value**, approximately 0.80315 on a 0–1
127-denominator scale; it is not a calibrated analogue touch voltage.

Generate into a **new non-existing** directory from the repository root:

```sh
python3 tests/diagnostics/test_generate_click_hardware_cues.py -v
python3 tests/diagnostics/GenerateClickHardwareCues.py --repo . --out NEW_EMPTY_PATH
```

The source renderer itself rounds events forward to the next 64-host-frame
callback; external analogue key contacts do not share that host clock. The
recording protocol must capture actual physical contact/gate timestamps and
align them independently, rather than assuming the MIDI marker is the real
electrical onset.

## Required evidence before any hardware conclusion

1. A technician or instrument owner identifies the actual unit, board and
   installed parts revisions, warm-up, panel/tone-tab settings, calibration
   positions and output connection. **Physical PANEL Bass is not the same as
   electrical-programmer Bass.** Preserve both identities, not a substituted
   preset. The project currently has zero measured installed-component answers
   for its 25 disputed revision cells.
2. Record the original unit's audio as unprocessed, calibrated, lossless PCM;
   document recorder, gain, converters, clocks, sample rate, headroom, noise
   floor and latency. No AGC, limiting, denoising or level-normalization is
   permitted in the comparison evidence. A separate contact/gate timestamp is
   necessary. Any internal CP3/CP4 measurements require a qualified
   technician and their own safe test-point documentation.
3. Capture repeated ordinary first strikes, retriggers, complete held-note
   controls and note-offs at the original panel settings. Keep the full
   preceding Bass chord/retirement history rather than recording an isolated
   note at time zero. Report variability and defective/ambiguous performances;
   do not silently remove outliers.
4. Record the raw files and immutable SHA-256 digests before any derived
   alignment, conversion or analysis. If converting analogue captures to
   44.1/48/96 kHz for comparison, specify the deterministic resampler,
   passband, stopband and group-delay treatment. Do not substitute a derived
   48 kHz render for a native hardware measurement.
5. Distinguish an event-triggered digital discontinuity from an ordinary
   pitched derivative using event/omission controls, exact prehistory where
   meaningful, and time-localized event-minus-control audio. The original
   maximum-jump score can alert on a mathematically smooth held 1760 Hz tone:
   that specificity issue is documented, **not** an authorization to revise
   its threshold or declare the existing software PASS.

## Separate blinded listening study

The original eight deterministic source A/B sets contain 75 ms before and
300 ms after the event (cue at 0.075 s): `_A_event`, `_B_omission`, and
`_C_difference`, all unscaled native float32 PCM. An independent organizer can
blind and counterbalance A/B labels without altering sample content. Present
the event and omission at a safe fixed playback gain, no normalization. Ask
listeners to distinguish *ordinary new/released note*, *abrupt nonmusical
discontinuity*, and *uncertain*, with optional free text. Known one-sample
impulse positives and C1-smooth pitched controls must be separately
pre-registered; neither source A nor B is intrinsically a certified
click-free ground truth. Track listener, equipment, level, trial order and
responses without re-labeling ambiguous results after inspection.

The source audition archive (24 native WAVs) has verified SHA-256
`65d7d2feebac84e48fdebbe0732698e1e4831c8c111abf5bc87af40239650d23`.
It is not automatically included in a source release, and no listening verdict
has yet been received. See `CLICK_AUDITION_AB_20261102.md` for its separate
signal provenance.

## Release decision boundary

These test-only additions do not change `src/**`, program tables, source V1/V2
pins, the original audit's 8 ms windows, score denominator, numerical limits,
or any production audio. No real hardware transfer or listening verdict is
claimed. **The original source retrigger, steal, single note-off, chord
note-off, physical-PANEL Bass and Funky click gates are still red.** A later
production candidate must independently justify circuit topology and
calibration, pass source-locked compatibility and numerical audits, and leave
the original failed evidence immutable.
