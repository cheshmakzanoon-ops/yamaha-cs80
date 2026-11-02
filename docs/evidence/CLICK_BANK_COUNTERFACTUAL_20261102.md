# Full-bank click counterfactual — 2026-11-02

**Disposition: diagnostic replay PASS; original full-engine click acceptance remains FAIL/OPEN.**
No audio engine implementation, source audit, score limit, score window, reporter,
or full-audit qualification request was changed.

## Provenance and locked method

- This diagnostic was preregistered as `tests/parity/tolerances-v2/click-bank-causality.json`
  in local commit `987bcdc`, before the counterfactual implementation.
- Original full-engine audit: evidence branch
  `evidence/full-engine-audit-20261102`, commit `0151b869`;
  `audit/phrase.csv` (566 data rows, 283 source-policy rows), SHA-256
  `2330ba8ead63ad1f3322bd5764a9c112b5cec3ea015696b9e720f36668444914`.
- Unmodified production `src/` Git tree:
  `7dd159d491012fd47b46c0aa651e86ea959ca289`.
- Additive test-only executable `cs80_click_bank_causality_tests` includes the
  unchanged `src/tools/engine_audit.cpp` in its own translation unit, so it
  uses precisely the original factory/tone setup, phrase events, host-block
  quantization, render path and sample-to-sample jump normalization.
- The original numerical target remains **score <= 1**; a score >1 is a
  flagged jump and is *not* proof of a MIDI-triggered discontinuity. The new
  output is strictly explanatory and does not replace any required gate.

For each original source-policy score >1, the diagnostic replays the baseline
and a control that omits *only* the registered target event: one MIDI note-off
at 1.40 s, four chord note-offs at 4.40 s, one retrigger note-on at 6.25 s,
or four voice-steal note-ons at 8.65 s. Events round up to the next 64-sample
host block, and the original 8 ms post-event window and 60 ms pre-note-off
reference are untouched. Scores round-trip against the six-significant-digit
original CSV with preregistered relative error at most `1e-5`; pre-event PCM
must be bit-for-bit identical. Event-only derivative strength is reported as
an attribution *measurement*, with no acceptance limit.

Reproduction from this branch, using the original archived CSV:

```bash
cmake -S . -B build-core -G Ninja -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --target cs80_click_bank_causality_tests
ctest --test-dir build-core --output-on-failure -R '^cs80_click_bank_causality_tests$'
./build-core/cs80_click_bank_causality_tests \
  --phrase-csv /path/to/original/audit/phrase.csv \
  --out docs/evidence/click-bank-counterfactual-20261102.csv
```

## Measured results

| Original failed event category | Failed rows | Omitted-event control still >1 | No post-event PCM change within 20 ms |
| --- | ---: | ---: | ---: |
| Single note-off | 22 | 22 | 1 |
| Chord note-off | 37 | 37 | 1 |
| Retrigger note-on | 1 | 1 | 0 |
| Voice-steal note-on | 1 | 0 | 0 |
| **Total** | **61** | **60** | **2** |

- All **61/61** failing original scores reproduced with maximum relative
  difference `4.801858e-6`; all 61 controls omitted exactly the registered
  one or four events; **zero pre-event PCM bit mismatches**.
- **60/61** event-only maximum derivative scores are below 1 against the
  *unchanged* original reference. In **50/61** cases the event contribution
  at the baseline worst-derivative sample is below 1% (descriptive, not a new
  threshold or an acceptance reclassification).
- The two 20 ms bit-identical omitted-event cases are `Frozen Sequences 163`
  (44.1 kHz single note-off) and `Midnight Sequences 165` (48 kHz chord
  note-off). `FactoryPresets.cpp` gives the generated Sequences family an
  arpeggiator; `Engine::applyNoteOff` forwards the key-up to the arpeggiator
  key ledger rather than immediately releasing a voice. A tick outside this
  window may still respond later. The over-limit samples in these windows
  are not consequences of the omitted note-offs.

Selected exact independent counterfactuals:

| Original source row | Event | Original score | Score without event | Attribution |
| --- | --- | ---: | ---: | --- |
| Funky 1 + Funky 2, 44.1 kHz | Single off | 1.62689 | 1.62688764 | Baseline worst jump changes ~0.000095% at the same sample |
| Funky 1 + Funky 2, 48 kHz | Single off | 1.29131 | 1.38050307 | Omission produces a *larger* jump |
| Funky 1 + Funky 2, 96 kHz | Single off | 1.22096 | 1.23890355 | Omission still fails |
| Distant FX 139, 44.1 kHz | Chord off | 1.84628 | 1.85317004 | Omission still fails |
| Velvet Sequences 172, 44.1 kHz | Retrigger on | 1.23689 | 1.23704028 | Omission still fails; arpeggiator-controlled fixture |
| Clavichord 1 + Clavichord 2, 96 kHz | Steal on | 1.03039 | 0.00249765 | See unresolved interpretation below |

The separate original 9-row click-causality audit also reproduces: for
physical-`PANEL` Bass Stack the 48 kHz retrigger is `1.440924` against an
omission control `0.0314353`, but the *first* ordinary strike already scores
`1.123291`. The retrigger's worst derivative occurs 374 frames (7.79 ms)
after event dispatch, near the end of the 8 ms window, rather than at a
reset boundary. The production *electrical programmer* Bass coordinate
is a different experiment and passes at all three rates. Do not equate those
two coordinate systems or infer an arbitrary attack fade.

### Sole event-dependent full-bank over-limit row remains open

For **Clavichord 1 + Clavichord 2, 96 kHz**, the voice-steal MIDI note-on
causally produces the failed jump: omitted-event score `0.002497647`,
event-only peak score `1.032885816`, original score `1.030388169`.
Its worst sample is host frame `830925`, 525 frames (**5.46875 ms**) *after*
the note-on block at frame `830400`. The same preset's initial four-note
chord-on score is `0.660992` relative to its earlier chord window.

The source-policy `Engine::startVoice` preserves free-running oscillator
phase, filter integrator and VCA coupling when reassigning released circuits;
it restarts envelope attack rather than immediately clearing those audio
memories. A silent circuit is caught up to the current time before reuse.

An **additional exploratory zero-time probe** now confirms that code-path
in this exact fixture, without amending the preregistered diagnostic or
creating a new acceptance gate. It replays the same 96 kHz electrical-tone
frame and phrase to frame `830400`, captures all eight voices and both ranks,
dispatches the four note-ons *without advancing audio time*, then compares
all 72 exposed oscillator/residual, nodal, envelope, VCA coupling and phase
fields per rank using the existing bit-exact comparator. Exactly four active
but key-released voices are reassigned (slots `0, 1, 2, 7`); **0 / 1,152
zero-time audio-state fields change** across the entire eight-voice engine.
This excludes an immediate reset of those exposed states at MIDI dispatch,
though not every unexposed/global modeled quantity.

The same fixture's event-only maximum sample derivative, normalized to the
**original unmodified** prior-chord reference, increases across the post-event
waveform instead of peaking at the dispatch boundary:

| Host frames after note-on block | Event-only max normalized derivative |
| --- | ---: |
| 0–62 | 0.0010021 |
| 63–127 | 0.0140448 |
| 128–255 | 0.135741 |
| 256–383 | 0.222980 |
| 384–511 | 0.545148 |
| 512–767 | 1.032886 |

The first 63 frames contribute just 0.1% of the ordinary attack reference,
whereas the worst event-dependent derivative is more than 1,000 times larger
525 frames after dispatch. This **supports an evolving note attack rather
than an instantaneous oscillator/filter/envelope/VCA reset**. However it
does not determine whether the full level or waveform is an accurate physical
Clavichord-tone CS-80 attack; a measured hardware trace or independent
circuit evidence is still required to exclude a wrong sound law or unexposed
transient. **The original 1.03039 score remains FAIL and the physical
attack-law inference remains OPEN. No DSP patch, attack fade or score waiver.**

The standalone probe is archived solely for reproducibility; it is not
registered with CTest, does not run in production, and is not a substitute for
the original qualification gate. After configuring/building `cs80_core`:

```bash
g++ -std=c++20 -O3 -DNDEBUG -Isrc -Itests \
  docs/evidence/ClavichordStealZeroTimeProbe.cpp build-core/libcs80_core.a \
  -o build-core/clav-steal-zero-time-probe
./build-core/clav-steal-zero-time-probe
```

The exact probe code has SHA-256
`7aa4b94de726faf32913e098d366a814a894a7cc5b59340620400ce39c304326`;
its archived stdout `docs/evidence/clavichord-steal-zero-time-20261102.txt`
has SHA-256
`19a753bfa21c40d7d0d40f0d11d66b89c8c765b22245ed96291e0f2a750bb115`.

## Verification and archived output

- New CTest (`--self-test`), including the original Funky and physical-panel
  Bass score replay and four synthetic mutation rejections (wrong pre-event
  PCM, vacuous control on known-active fixtures, wrong omission count,
  tampered phrase score): **PASS**, 5.02 s.
- Existing `cs80_note_off_continuity_tests`,
  `cs80_click_event_continuation_tests`, and
  `cs80_ig00151_idle_tail_tests`: all **PASS**, 70.38 s, 91.14 s,
  and 11.93 s respectively (parallel execution).
- Full 61-row output: `docs/evidence/click-bank-counterfactual-20261102.csv`,
  SHA-256
  `9a91a0827a441ce1b34a29715c2fde38246d81a98458fba6c9c59f04852259a5`.
- Original frozen full-audit score rows, failures, windows and limits remain
  unchanged; this research is not a release sign-off.
