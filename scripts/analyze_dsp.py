#!/usr/bin/env python3
"""Analyze actual probe CSVs. Numerical evidence, never a hardware-match score."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

os.environ.setdefault("MPLCONFIGDIR", "/tmp/cs80-matplotlib")
os.environ.setdefault("XDG_CACHE_HOME", "/tmp/cs80-analysis-cache")
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(path: Path):
    return np.genfromtxt(path, delimiter=",", names=True)


def db(value):
    return 20.0 * np.log10(max(float(value), 1.0e-15))


def ideal_saw_reference(n: int, fundamental_bin: int, oversampling: int = 16):
    """16x periodic Fourier source, ideal host-Nyquist lowpass, then sample.

    Only harmonics strictly below host Nyquist are retained before decimation.
    This is an exact ideal reference, not another BLEP or an analog CS-80 model.
    Oscillator::tick advances phase before output, so shift reference one sample.
    """
    high_n = n * oversampling
    coefficients = np.zeros(high_n // 2 + 1, dtype=np.complex128)
    harmonics = np.arange(1, (n // 2 - 1) // fundamental_bin + 1)
    coefficients[harmonics * fundamental_bin] = -1j * high_n / (np.pi * harmonics)
    high_rate = np.fft.irfft(coefficients, n=high_n)
    return np.roll(high_rate[::oversampling], -1), harmonics * fundamental_bin



def decimator_analysis(directory, finish):
    """Actual C++ impulse and tone response; comparisons are mathematical designs."""
    path = directory / "decimator-impulse.csv"
    if not path.exists():
        return None  # Archived schema-1 probes did not include this measurement.
    config = json.loads((directory / "decimator.json").read_text())
    impulse = load(path)["impulse_response"]
    support = np.flatnonzero(np.abs(impulse) > 1e-12)
    if len(support) == 0:
        raise SystemExit("Decimator impulse is empty")
    measured = impulse[:support[-1]+1]
    def design(taps, cutoff, window):
        h = 2*cutoff*np.sinc(2*cutoff*(np.arange(taps)-(taps-1)/2))*window
        return (h/h.sum()).astype(np.float32).astype(np.float64)
    legacy = design(31, .22, np.hamming(31))
    lower_latency = design(127, (18000/88200+.25)/2, np.kaiser(127, 8.6))
    designs = [("Captured production FIR", measured, "#59d4b5"),
               ("Legacy 31-tap design (reconstructed)", legacy, "#f19c77"),
               ("127-tap / 18 kHz alternative", lower_latency, "#7fafff")]
    fig, axes = plt.subplots(3, 1, figsize=(12, 10))
    nfft = 262144
    freq = np.fft.rfftfreq(nfft)
    results = []
    for label, h, color in designs:
        response = np.fft.rfft(h, nfft)
        gain = 20*np.log10(np.maximum(np.abs(response), 1e-15))
        phase = config["retained_internal_phase"]
        effective_phase = np.unwrap(np.angle(response)) + 2*np.pi*freq*phase
        lowband = (freq > 0) & (freq < .1)
        delay = -np.polyfit(4*np.pi*freq[lowband], effective_phase[lowband], 1)[0]
        centroid = float(np.dot(np.arange(len(h)), h)/np.sum(h))
        if abs(delay-(centroid-phase)/2) > 1e-5:
            raise SystemExit("Measured phase slope disagrees with FIR impulse centroid")
        passband = gain[freq <= 20000/88200]
        row = {"label":label, "taps":len(h), "dc_gain":float(np.sum(h)),
               "fir_group_delay_internal_samples":centroid,
               "indexed_source_to_host_delay_samples":float(delay),
               "passband_through_20khz_at_44100_min_db":float(passband.min()),
               "passband_through_20khz_at_44100_max_db":float(passband.max()),
               "stopband_from_internal_0p25_max_db":float(gain[freq>=.25].max())}
        results.append(row)
        axes[0].plot(freq*88200, gain, label=label, color=color, lw=1.2)
        axes[1].plot(freq*88200, gain, color=color, lw=1.2)
        axes[2].plot(np.arange(len(h[phase::2])), h[phase::2], ".-", color=color,
                     label=f"{label}: {delay:.3f} host samples", lw=1)
    actual = results[0]
    if abs(actual["dc_gain"]-1) > 2e-6 or np.max(np.abs(measured-measured[::-1])) > 2e-6:
        raise SystemExit("Actual FIR lost unity DC gain or linear-phase symmetry")
    if actual["passband_through_20khz_at_44100_min_db"] < -.1 or actual["passband_through_20khz_at_44100_max_db"] > .1:
        raise SystemExit("Actual FIR exceeds 0.1 dB passband target")
    if actual["stopband_from_internal_0p25_max_db"] > -80:
        raise SystemExit("Actual FIR misses 80 dB stopband target")
    if abs(actual["indexed_source_to_host_delay_samples"]-config["declared_host_latency"]) > 1e-5:
        raise SystemExit("Declared host latency disagrees with retained-phase response")
    tones = load(directory / "decimator-tones.csv")
    axes[0].scatter(tones["cycles_per_internal_sample"]*88200,
                    tones["quadrature_rms_gain_db"], s=25, facecolors="none",
                    edgecolors="#ffffff", label="Actual C++ quadrature-tone gain", zorder=5)
    axes[0].axhline(-80, color="#bbb", ls=":", lw=1)
    axes[0].axvline(22050, color="#bbb", ls="--", lw=1)
    axes[0].set(title="2× decimator • measured float kernel vs alternatives",
                ylabel="Gain (dB)", xlim=(0,44100), ylim=(-130,2))
    axes[1].axhline(-.1, color="#bbb", ls=":", lw=1)
    axes[1].set(title="Passband detail • 44.1 kHz host / 88.2 kHz internal",
                xlabel="Internal signal frequency (Hz)", ylabel="Gain (dB)",
                xlim=(0,20000), ylim=(-.12,.02))
    axes[2].set(title="Retained odd-phase impulse • internal impulse at index 0",
                xlabel="Host output index (first pair is index 0)", ylabel="Amplitude")
    for ax in axes:
        ax.grid(alpha=.25)
    for ax in (axes[0],axes[2]):
        ax.legend(facecolor="#18232e", labelcolor="#d6e0e5", fontsize=8)
    finish(fig, "decimator-response-latency")
    return {"convention":"Internal x[n] starts at zero; host y[m]=filtered[2*m+1]. Not a complete instrument impulse response.",
            "config":config, "fft_size":nfft,
            "response_units":"cycles per internal sample, twice host rate",
            "captured_and_design_comparisons":results,
            "actual_quadrature_tones":[{"cycles_per_internal_sample":float(t["cycles_per_internal_sample"]),
                "gain_db":float(t["quadrature_rms_gain_db"])} for t in tones],
            "response_and_declared_latency_gates_passed":True}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", default="artifacts/dsp-probe")
    args = parser.parse_args()
    directory = Path(args.directory)
    meta = json.loads((directory / "probe.json").read_text())
    if meta["failures"]:
        raise SystemExit("C++ probe reported failures; refusing to publish clean-looking charts")
    out = directory / "plots"
    out.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"figure.facecolor": "#111821", "axes.facecolor": "#18232e",
        "axes.edgecolor": "#697a87", "text.color": "#e8edf1", "axes.labelcolor": "#d6e0e5",
        "xtick.color": "#b7c6cf", "ytick.color": "#b7c6cf", "grid.color": "#4a5965",
        "font.size": 11, "savefig.facecolor": "#111821", "axes.titleweight": "bold"})
    colors = {"fixed": "#59d4b5", "legacy": "#f19c77", "reference": "#c5d2e0"}
    def finish(fig, name):
        fig.text(.015, .015, "SOFTWARE NUMERICAL EVIDENCE • NOT A MEASURED CS-80 COMPARISON", fontsize=8, color="#adbac3")
        fig.tight_layout(rect=(0, .035, 1, 1))
        fig.savefig(out / (name + ".png"), dpi=160)
        fig.savefig(out / (name + ".svg"))
        plt.close(fig)

    metrics = []
    wave_data = {}
    for fundamental_bin in meta["oscillator_bins"]:
        data = load(directory / f"oscillator-{fundamental_bin}.csv")
        n = len(data)
        reference, harmonic_bins = ideal_saw_reference(n, fundamental_bin)
        reference_rms = np.sqrt(np.mean(reference ** 2))
        allowed = np.zeros(n // 2 + 1, dtype=bool)
        allowed[0] = True
        allowed[harmonic_bins] = True
        frequency = fundamental_bin * meta["sample_rate"] / n
        row = {"fundamental_hz": frequency, "fundamental_bin": fundamental_bin}
        for label, key in (("corrected", "saw_ramp"), ("legacy_wrong_sign", "legacy_wrong_sign")):
            signal = data[key]
            spectrum = np.fft.rfft(signal)
            unwanted = spectrum.copy()
            unwanted[allowed] = 0
            unwanted_rms = np.sqrt(np.mean(np.fft.irfft(unwanted, n=n) ** 2))
            error_rms = np.sqrt(np.mean((signal - reference) ** 2))
            row[label] = {"off_harmonic_db_relative_to_reference_rms": db(unwanted_rms / reference_rms),
                          "total_reference_residual_db": db(error_rms / reference_rms),
                          "peak": float(np.max(np.abs(signal)))}
        improvement = row["legacy_wrong_sign"]["off_harmonic_db_relative_to_reference_rms"] - row["corrected"]["off_harmonic_db_relative_to_reference_rms"]
        row["alias_energy_improvement_db"] = improvement
        # A sign reversal should materially reduce rather than reinforce the edge.
        # The 6 dB gate is a conservative comparative regression, not an audible
        # quality threshold or a universal BLEP guarantee across all frequencies.
        if improvement < 6.0:
            raise SystemExit(f"Wrong-sign regression did not improve by 6 dB at {frequency}: {improvement}")
        metrics.append(row)
        wave_data[fundamental_bin] = (data, reference)

    chosen = meta["oscillator_bins"][-1]
    data, reference = wave_data[chosen]
    n = len(data)
    rate = meta["sample_rate"]
    freq = chosen * rate / n
    fig, axes = plt.subplots(2, 1, figsize=(12, 8))
    length = int(np.ceil(4 * rate / freq))
    time = (np.arange(length) + 1) / rate * 1000
    axes[0].plot(time, reference[:length], color=colors["reference"], lw=2, label="16× ideal bandlimited reference")
    axes[0].plot(time, data["saw_ramp"][:length], color=colors["fixed"], lw=1.6, marker=".", label="Corrected PolyBLEP ramp")
    axes[0].plot(time, data["legacy_wrong_sign"][:length], color=colors["legacy"], lw=1.2, alpha=.8, label="Legacy wrong sign")
    axes[0].set(title=f"Isolated falling ramp • {freq:.2f} Hz / 48 kHz", xlabel="Time (ms)", ylabel="Amplitude")
    axes[0].legend(loc="upper right", facecolor="#18232e", labelcolor="#d6e0e5", fontsize=9)
    for signal, label, color in [(reference, "Ideal", colors["reference"]), (data["legacy_wrong_sign"], "Legacy", colors["legacy"]), (data["saw_ramp"], "Corrected", colors["fixed"])]:
        magnitude = np.abs(np.fft.rfft(signal)) * 2 / n
        axes[1].plot(np.fft.rfftfreq(n, 1/rate), 20*np.log10(np.maximum(magnitude, 1e-8)), color=color, lw=.7, label=label)
    axes[1].set(xlim=(0, rate/2), ylim=(-100, 10), xlabel="Frequency (Hz)", ylabel="Magnitude (dBFS)", title="Coherent FFT • no window leakage • reset pulse excluded")
    for ax in axes: ax.grid(alpha=.25)
    finish(fig, "oscillator-waveform-spectrum")

    fig, ax = plt.subplots(figsize=(11, 5))
    x = np.arange(len(metrics))
    for offset, key, label, color in [(-.18, "legacy_wrong_sign", "Legacy wrong sign", colors["legacy"]), (.18, "corrected", "Corrected ramp", colors["fixed"])]:
        values = [r[key]["off_harmonic_db_relative_to_reference_rms"] for r in metrics]
        ax.bar(x+offset, values, width=.36, color=color, label=label)
        for pos, value in zip(x+offset, values): ax.text(pos, value-2, f"{value:.1f}", ha="center", va="top", fontsize=9)
    ax.set_xticks(x, [f"{r['fundamental_hz']:.2f} Hz" for r in metrics])
    ax.set(title="Off-harmonic energy from discontinuity aliasing", ylabel="dB relative to ideal reference RMS (lower is better)", ylim=(-85, 0))
    ax.legend(facecolor="#18232e", labelcolor="#d6e0e5")
    ax.grid(axis="y", alpha=.25)
    finish(fig, "oscillator-alias-comparison")

    fig, ax = plt.subplots(figsize=(11, 5))
    envelope_metrics = []
    for sr, color in zip((44100,48000,96000,192000), ("#f0be62", "#59d4b5", "#7fafff", "#d897ec")):
        trace = load(directory / f"filter-envelope-{sr}.csv")
        ax.plot(trace["time_s"], trace["bent_offset_oct"], label=f"{sr/1000:g} kHz", color=color, lw=1.5)
        envelope_metrics.append({"sample_rate":sr,"peak_offset_oct":float(np.max(trace["bent_offset_oct"])),"final_offset_oct":float(trace["bent_offset_oct"][-1])})
    ax.axvline(1.2, color="#ddd", ls="--", lw=1)
    ax.text(1.22, 1.7, "Note off", fontsize=10)
    ax.axhline(0, color="#aab5bf", ls=":", lw=.8)
    ax.set(title="Filter contour • IL → AL → held cutoff → IL", xlabel="Time (s)", ylabel="Model cutoff offset (octaves; uncalibrated scale)", ylim=(-1.8, 2.5))
    ax.legend(facecolor="#18232e", labelcolor="#d6e0e5")
    ax.grid(alpha=.25)
    finish(fig, "filter-envelope-rate-comparison")

    fig, axes = plt.subplots(2,1,figsize=(11,7),sharex=True)
    for name,color in [(7,"#59d4b5"),(40,"#7fafff"),(200,"#f0be62")]:
        trace=load(directory/f"ring-{name}.csv")
        base=trace["base_hz"][0]
        axes[0].plot(trace["time_s"],trace["frequency_hz"],color=color,label=f"Base {base:.3g} Hz")
    axes[1].plot(trace["time_s"],trace["envelope"],color="#d897ec",label="Actual engine AD envelope")
    for ax in axes:
        for at in (0.3,1.4): ax.axvline(at,color="#ddd",ls="--",lw=.8)
        ax.grid(alpha=.25)
        ax.legend(facecolor="#18232e",labelcolor="#d6e0e5",fontsize=9)
    axes[0].set(title="Ring envelope sweeps frequency, not wet amplitude",ylabel="Carrier frequency (Hz)")
    axes[1].set(xlabel="Time (s) • legato at 0.3; new phrase at 1.4",ylabel="Envelope",ylim=(-.05,1.1))
    axes[1].text(.65,.65,"Provisional: base × (1 + depth × env)\nOne-octave excursion;\nnot hardware-calibrated.",fontsize=9)
    finish(fig,"ring-frequency-envelope")

    decimator_metrics = decimator_analysis(directory, finish)

    repo=Path(__file__).resolve().parents[1]
    sources=["src/core/Dsp.h","src/core/Engine.cpp","src/core/Engine.h","src/tools/dsp_probe.cpp","scripts/analyze_dsp.py"]
    hashes={p:hashlib.sha256((repo/p).read_bytes()).hexdigest() for p in sources}
    try:
        head=subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo,text=True).strip()
        dirty=subprocess.check_output(["git","status","--porcelain"],cwd=repo,text=True).splitlines()
    except (subprocess.CalledProcessError,OSError): head="unavailable";dirty=[]
    result={"schema":1,"description":"Software numerical evidence; no physical CS-80 comparison", "reference":"16x Fourier source ideal-lowpassed below host Nyquist before decimation", "git_head":head,"working_tree_status":dirty,"provenance_note":"Hashes sampled at analysis time; archive compiler/CI exact-source evidence separately. They do not attest the executable was built from these bytes.","analysis_time_source_sha256":hashes,"oscillators":metrics,"filter_envelopes":envelope_metrics,"probe_failures":meta["failures"],"comparative_regression_passed":True}
    result["decimator"] = decimator_metrics
    (directory/"measurements.json").write_text(json.dumps(result,indent=2)+"\n")
    for row in metrics: print(f"{row['fundamental_hz']:8.2f} Hz: off-harmonic improvement {row['alias_energy_improvement_db']:.2f} dB")
    print(f"Wrote {5 if decimator_metrics else 4} PNG/SVG figures and {directory/'measurements.json'}")

if __name__=="__main__": main()
