# A13 evidence and reproduction

The complete classification is `keys-structure.csv`: **all 45 Keys programs**,
with exact name, zero-based host address, zero-based MIDI bank MSB/LSB/Program
Change, source recipe, intended role, affected mechanism, body mix, filters,
footage, sustain, release, and touch controls. Curated index = host index − 222;
original addresses use MSB 1; curated addresses use MSB 0. This distinction is
material: **Soft Keys 110** (host 153) is a vibes recipe, whereas **Soft Keys**
(host 350) is a felt-style piano recipe.

All numerical comparisons use actual complete patches, with no parameter
replacement, rank isolation, output normalization or clipping. The official
capture utility renders all ten piano-body Keys at MIDI 36 and 60, velocity
0.85, holding for 12 and 6 seconds respectively, followed by 1.2-second
release. Stage/Soft receive additional 0.25-velocity and 0.2-second early-key-off
cases. Electric Piano I/II, Tine Piano and Reed Piano receive 3-second C4 holds
at both velocities. These are 36 paired conditions / 72 full-output WAVs.
`capture-manifest.json` records every exact command and successful return code.
`capture-metrics.csv` retains the utility's measurements. All PCM remains in
`/tmp/cs80-a13`; it is intentionally not committed.

The spectral analysis sums power from the **two channels separately** rather
than summing their waveforms. It Hann-windows each explicitly recorded time
interval. The upper/lower ratio integrates 6.5–30.5 fundamental frequencies
versus 0.7–6.5; upper limit is capped below Nyquist. The h7/h6 diagnostic uses
±0.22-fundamental bands. These finite bands allow the authored stretch,
detuning, course beating and filter response. `window-metrics.csv` preserves
absolute RMS alongside relative bands, so a quieter or nearly vanished tail
cannot be mistaken for an improved spectrum. Long-window results well below
normal playback noise are numerical observations, not listening evidence.

`comparisons.csv` supplies paired changes, full-output peaks, nonfinite/full-
scale checks and exact EP-control sample differences. `touch-metrics.csv`
records firm/soft onset RMS and upper/lower spectral differences. No subjective
listening assessment or acoustic-recording similarity is claimed.

## Reproduce the complete-patch matrix

From the repository root, using the separately built baseline and candidate
executables indicated in the manifest:

```sh
python docs/preset-evidence/2026.11.7/verification/a13/capture_matrix.py
python docs/preset-evidence/2026.11.7/verification/a13/analyze.py
```

The baseline is `18711f93a39aa49f3d6f9a41784be456224facbe`. Current captured
inventory revision is `.7`; every program value and metadata item equals the
saved `.6` inventory. `summary.json` retains executable, library and pertinent
source hashes. The matrix uses 48 kHz host / default 2× internal processing,
where A05's compensation is unity. It does not re-run A05's sample-rate matrix
or A06's string-bend suite. Structural coverage identifies the 12 Keys touched
by A06 without claiming new bend-render coverage for those programs.

## Pedal and key damping probe

`pedal_probe.cpp` renders Stage Keys and Soft Keys at MIDI 36, and Electric
Piano I and Reed Piano at MIDI 60, all at velocity 0.85. It compares three
three-second gestures: key-off at 0.2 s; CC64 down at 0.1 s, key-off at 0.2 s,
pedal-up at 2 s; and a direct key hold until 2 s. The latter two should retain
identical body history and release output. The 24 PCM captures are temporary
raw interleaved float stereo at 48 kHz. `pedal-metrics.csv` retains comparisons.

**ABI requirement:** export old headers for the old library. The changed body
object changes the containing Engine's layout; compiling the current header
against the old library is invalid.

```sh
mkdir -p /tmp/cs80-a13/oldheaders
git archive 18711f93 src | tar -x -C /tmp/cs80-a13/oldheaders
c++ -std=c++20 -O2 -I/tmp/cs80-a13/oldheaders/src docs/preset-evidence/2026.11.7/verification/a13/pedal_probe.cpp /tmp/cs80-reference-1cee5a/libcs80_core.a -lpthread -o /tmp/cs80-a13/pedal-before
c++ -std=c++20 -O2 -Isrc docs/preset-evidence/2026.11.7/verification/a13/pedal_probe.cpp /tmp/cs80-build-1cee5a/libcs80_core.a -lpthread -o /tmp/cs80-a13/pedal-after
/tmp/cs80-a13/pedal-before /tmp/cs80-a13/pedal/before
/tmp/cs80-a13/pedal-after /tmp/cs80-a13/pedal/after
python docs/preset-evidence/2026.11.7/verification/a13/analyze_pedal.py
```

No production source, preset, or test registration was changed by A13.
