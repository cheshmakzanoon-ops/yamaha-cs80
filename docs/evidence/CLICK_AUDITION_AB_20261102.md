# Deterministic click-event A/B audition pack — 2026-11-02

**Status: diagnostic artifacts verified; no independent human listening
verdict; original acceptance scores remain FAIL.** This research does not
alter the CS-80 audio engine, fixed hardware control frames, audit scores,
MIDI events, renderer, sample-rate settings, original gates or their limits.

## What was rendered

The self-contained `ClickAuditionAB.cpp` includes the unchanged audit source
translation unit and generates original stereo **IEEE-754 float32** PCM at
the *native* 44,100, 48,000 or 96,000 Hz host rate. Every clip contains
75 ms before the exact 64-frame-rounded MIDI event, then 300 ms afterward:
0.375 seconds total, with the cue at local timestamp **0.075 s**. There is
**no fade, resampling, normalization, limiter, or sample edit**. PCM amplitude
matches the deterministic original engine, and original event/omission prefixes
are bit-identical. All eight source-policy scenarios omit only the specified
event in their B control:

| Fixture | Host rates | Event | Original score >1? | Clip stems |
| --- | --- | --- | --- | --- |
| Bass Stack original physical PANEL | 48, 96 kHz | First strike at 6.00 s; separately retrigger at 6.25 s | Both rates, both strikes | `bass_panel_first_*`, `bass_panel_retrigger_*` |
| Funky 1 + Funky 2 tone tabs | 44.1, 48, 96 kHz | Note-off at 1.40 s | All three | `funky_tabs_noteoff_*` |
| Clavichord 1 + Clavichord 2 tone tabs | 96 kHz | Four-note steal onset at 8.65 s | 1.03039 | `clav_tabs_steal_96000` |

Each stem has exactly three matching WAV files, all with original PCM scale:

- `_A_event.wav`: event present; original failing score.
- `_B_omission.wav`: only that event removed; all earlier PCM unchanged.
- `_C_difference.wav`: pointwise A − B (no gain applied). This is a **causal
  difference signal**, not an isolated perceptual click. In particular,
  note-off differences persist because a held tone and a released tone differ.

The eight rows in `click-audition-manifest-20261102.csv` retain the frozen
original/omission scores, clip RMS/peaks, frame bounds, event counts and
pre-event PCM bit mismatches (all **zero**). The 24 rows in
`click-audition-wav-inventory-20261102.csv` retain each WAV's exact SHA-256,
byte size, rate, duration, channel count and float32 codec. `ffprobe`
independently recognized `pcm_f32le`, stereo, 48,000 Hz and 0.375 s for the
Bass 48 kHz exemplar; the inventory/parser verifies all 24 native formats.

## Objective timing and spectral interpretation

`ClickAuditionAnalysis.py` recomputes A − B at float32 precision and checks
it equals every unmodified C sample **exactly**. It also enforces identical
pre-event audio and reports how the event-dependent signal's **squared PCM
energy** distributes through independently fixed post-MIDI windows (0–1,
1–2, 2–4, 4–8, 8–20, 20–100, 100–300 ms). This is descriptive energy,
**not** a psychoacoustic audibility or click threshold.

| Event-only signal | 0–1 ms energy / 0–100 ms energy | 20–100 ms energy / 0–100 ms energy |
| --- | ---: | ---: |
| Bass first strike, 48 / 96 kHz | 1.17e−13 / 1.09e−5 | 79.01% / 79.04% |
| Bass retrigger, 48 / 96 kHz | 1.05e−13 / 5.78e−7 | 75.73% / 74.82% |
| Funky note-off, 44.1 / 48 / 96 kHz | 2.20e−15 / 4.62e−13 / 2.29e−6 | 96.00% / 95.90% / 95.76% |
| Clavichord voice-steal onset, 96 kHz | 2.97e−5 | 72.60% |

For each fixture, the immediate first millisecond contains less than
`3e-5` of the 0–100 ms event-only energy; most of the difference continues
after 20 ms. The separately archived zero-time/8-ms studies found **no tested
instantaneous oscillator, filter, envelope or VCA state reset**, and the
Bass/Clav worst derivative peaks occur several milliseconds after key-on.
Funky note-off is *not necessary* for any of its >1 score failures: omission
controls still score >1, including higher than baseline at 48 and 96 kHz.

Native signal spectrograms of representative event-only clips were also
generated and inspected using one fixed FFmpeg transform
(`showspectrumpic`, logarithmic frequency/color, 100 dB range, no source
PCM change). Bass shows a growing pitched harmonic onset after the event;
Funky shows a prolonged harmonic difference as the release departs from the
held tone; Clav shows broad pitched attack energy after the four-note chord
onset. The short-time Fourier window smears sub-millisecond onsets, so these
images **cannot** prove a click absent or establish an accurate hardware
attack law. They are derived diagnostics, not included in the native WAV
archive or acceptance gates.

**No subjective audition was performed through a listening-capable
interface.** A human reviewer can now conduct one at a comfortable fixed
monitor level, blind-randomizing A/B filenames *externally*, focusing on the
event at 0.075 s and ignoring ordinary cut-boundary artifacts at clip start
and end. Do not level-normalize A/B, as their real note-on/release loudness
difference is part of the control. The unscaled C track may aid localization,
but hearing it alone is not a click verdict. Keep separate labels for
"new note sounds", "abrupt nonmusical discontinuity" and "cannot tell";
repeat at both Bass rates and all three Funky rates.

## Reproduction and archive integrity

Input production `src/` tree:
`7dd159d491012fd47b46c0aa651e86ea959ca289`. Build `cs80_core` in the
normal release configuration, then from the repository root:

```bash
g++ -std=c++20 -O3 -DNDEBUG -Isrc -Itests \
  docs/evidence/ClickAuditionAB.cpp build-core/libcs80_core.a \
  -o build-core/click-audition-ab
./build-core/click-audition-ab /path/to/new-empty/clip-directory
python3 docs/evidence/ClickAuditionAnalysis.py /path/to/clip-directory \
  --out /path/to/new-energy.csv
python3 docs/evidence/ClickAuditionAnalysis.py /path/to/clip-directory \
  --inventory /path/to/new-inventory.csv
```

The eight original WAV sets were **independently regenerated** from the
archived C++ source at a separate path: every one of the 24 SHA-256 inventory
rows and the entire renderer manifest were byte-for-byte identical (two
successful `cmp` checks). The original `src` tree was not edited.

One separate compressed, lossless native-WAV archive of the 24 A/B/C files
was generated in a deterministic filename order, with fixed timestamps and
owner/group, using only these WAVs (not the derived images or manifests):

```bash
tar --sort=name --mtime='2026-11-02 00:00:00 UTC' \
  --owner=0 --group=0 --numeric-owner --format=ustar \
  -I 'zstd -19 -T1' -cf click-audition-native-wavs-20261102.tar.zst \
  -C /path/to/clip-directory --exclude='*.png' --exclude=manifest.csv .
```

Archive: **3,974,131 bytes**, SHA-256
`65d7d2feebac84e48fdebbe0732698e1e4831c8c111abf5bc87af40239650d23`.
Two separate compressions reproduced that SHA exactly. To list/extract the
archive without resampling or altering WAV samples:

```bash
tar -I zstd -tf click-audition-native-wavs-20261102.tar.zst
mkdir extracted-native-clips
tar -I zstd -xf click-audition-native-wavs-20261102.tar.zst \
  -C extracted-native-clips
```

This binary archive is intentionally **not committed alongside the text-only
diagnostic**; it was separately saved for owner review and durable
publication, with its digest above. The original 61-row click audit remains
unchanged and the Bass, Funky and Clav scores **remain FAIL/OPEN**. No
release sign-off or arbitrary fade/score-waiver is implied.

### Possible principled future diagnostic — not adopted

An owner could preregister a separate *causal onset localization* study
combining (1) omitted-event bit-exact prehistory, (2) all-rank zero-time
charge continuity after legitimate idle wake, (3) matched-rate first-strike
and retrigger controls, and (4) time-localized event-only derivative and
energy profiles. The numeric values above can inform fixture choice but
**must not be turned post hoc into a new acceptance threshold**. To validate
any real click classifier, first inject known abrupt discontinuities and
realistic legitimate attacks under a separately locked positive/negative
control protocol, compare measured original hardware when available, and
conduct genuinely blinded listening evaluation. Otherwise retain the
unchanged original score failures and mark the hardware sound law unresolved.
