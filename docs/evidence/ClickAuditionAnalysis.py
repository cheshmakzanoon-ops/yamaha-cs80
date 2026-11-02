"""Descriptive only: event-only PCM energy in fixed post-event windows.

No psychoacoustic accept/reject threshold, no automatic click classifier.
"""
import csv
import hashlib
import pathlib
import sys

import numpy as np
from scipy.io import wavfile


def energy(segment):
    return float(np.sum(segment.astype(np.float64) ** 2))


def rms(segment):
    return float(np.sqrt(np.mean(segment.astype(np.float64) ** 2)))


def study(directory, stream):
    directory = pathlib.Path(directory)
    with (directory / "manifest.csv").open(newline="") as src:
        rows = list(csv.DictReader(src))
    bands_ms = [(0, 1), (1, 2), (2, 4), (4, 8), (8, 20), (20, 100), (100, 300)]
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(["stem", "rate", "early_1ms_energy_fraction_of_100ms", "early_1ms_rms", "late_20_100ms_rms",
                     "early_1ms_peak_abs", "late_20_100ms_peak_abs", "diff_replay_max_error",
                     *[f"{a}_{b}ms_energy_fraction_of_100ms" for a, b in bands_ms]])
    for row in rows:
        name = row["stem"]
        data = []
        for suffix in ("A_event", "B_omission", "C_difference"):
            rate, wave = wavfile.read(directory / f"{name}_{suffix}.wav")
            assert rate == int(row["rate"]) and wave.dtype == np.float32 and wave.ndim == 2 and wave.shape[1] == 2
            data.append(wave)
        a, b, diff = data
        reconstructed = np.subtract(a, b, dtype=np.float32)
        err = float(np.max(np.abs(reconstructed.astype(np.float64) - diff.astype(np.float64))))
        assert err == 0, "event-only clip differs from exact PCM subtraction"
        cue = round(0.075 * rate)
        assert np.array_equal(a[:cue].view(np.uint32), b[:cue].view(np.uint32)), "pre-event PCM drift"
        windows = [diff[cue + round(a_ms * rate / 1000):cue + round(b_ms * rate / 1000)]
                   for a_ms, b_ms in bands_ms]
        e100 = energy(diff[cue:cue + round(0.100 * rate)])
        fractions = [energy(window) / max(e100, 1e-20) for window in windows]
        writer.writerow([name, rate, f"{fractions[0]:.12g}", f"{rms(windows[0]):.12g}",
                         f"{rms(windows[5]):.12g}", f"{np.max(np.abs(windows[0])):.12g}",
                         f"{np.max(np.abs(windows[5])):.12g}", err,
                         *[f"{x:.12g}" for x in fractions]])


def inventory(directory, stream):
    directory = pathlib.Path(directory)
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(["filename", "bytes", "sha256", "rate_hz", "channels", "format", "frames_per_channel", "duration_seconds"])
    files = sorted(directory.glob("*.wav"))
    assert len(files) == 24, "audition suite requires exactly 24 WAV files"
    for path in files:
        raw = path.read_bytes()
        rate, samples = wavfile.read(path)
        assert rate in (44100, 48000, 96000) and samples.ndim == 2 and samples.shape[1] == 2
        assert samples.dtype == np.float32, "archive must contain native 32-bit floating-point PCM"
        writer.writerow([path.name, len(raw), hashlib.sha256(raw).hexdigest(), rate, 2,
                         "pcm_f32le", len(samples), f"{len(samples) / rate:.9f}"])


if __name__ == "__main__":
    if len(sys.argv) == 2:
        study(sys.argv[1], sys.stdout)
    elif len(sys.argv) == 4 and sys.argv[2] == "--out":
        destination = pathlib.Path(sys.argv[3])
        if destination.exists():
            raise SystemExit("refusing to overwrite existing analysis CSV")
        with destination.open("w", newline="") as out:
            study(sys.argv[1], out)
    elif len(sys.argv) == 4 and sys.argv[2] == "--inventory":
        destination = pathlib.Path(sys.argv[3])
        if destination.exists():
            raise SystemExit("refusing to overwrite existing inventory CSV")
        with destination.open("w", newline="") as out:
            inventory(sys.argv[1], out)
    else:
        raise SystemExit("usage: click-audition-analyze-20261102.py CLIP_DIRECTORY [--out NEW_CSV|--inventory NEW_CSV]")
