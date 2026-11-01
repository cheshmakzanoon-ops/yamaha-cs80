#!/usr/bin/env python3
"""Research-only IG00156 continuous four-state/discrete integrator comparison.

Usage: python3 scripts/ig00156_discretization_prototype.py
Optional dependencies: NumPy and SciPy (tested: 2.3.5, 1.17.0). They are NOT
build or CI dependencies. This script reads no project files, changes none,
and never changes an audio law or a registered tolerance. Do not construe its
linear-model score as an official nonlinear/realtime gate pass.

Equations copied explicitly from reference/IG00156Network.h::stageRows;
matrix exponential with causal source interpolation is an investigated
numerical method, not a newly inferred hardware circuit.
"""
import math

import numpy as np
from scipy import linalg, signal
from scipy.special import roots_legendre


PI = math.pi
RATES = (88200., 96000., 192000., 384000.)
CUTOFFS = (60., 250., 1000., 4000., 12000.)
QS = (0.5, 1., 2., 5., 9.)
CORE_LIMIT = 0.03
LOW_RATE_BAND_LIMIT = 0.08
HIGH_RATE_BAND_LIMIT = 0.02


def stage_system(stage, cutoff, q):
    """The existing reference's SI-unit [input,FB,BP,LP] small-signal rows."""
    source_ohms = 75000. if stage == "hp" else 150000.
    gs, gg, gf = 1. / source_ohms, 1. / 33000., 1. / 1.2e6
    total = gs + gg + gf
    q_si = max(0., total / (2. * q) - gf)
    alpha = total / 180e-12
    omega = 2. * PI * min(cutoff, 20000.)
    beta = omega * omega / alpha
    # ig00156IntegratorRate, including the documented 180 pF compensation.
    rate = (beta + math.sqrt(beta * beta + omega * omega)) / omega * 2. * PI * cutoff
    ki, kf = gs / 1e-6, gs / 180e-12
    a = np.array([
        [-ki, -ki, -ki, 0.],
        [-kf, -total / 180e-12, (-gs - gg + q_si) / 180e-12, 0.],
        [0., -2. * rate, -2. * rate, rate],
        [0., 0., -rate, 0.],
    ])
    # The production stage normalizes physical passband gain to one.
    gain = total / (2. * gs)
    b = np.array([[ki], [kf], [0.], [0.]]) * gain
    c = np.array([[0., 2., 2., -1.] if stage == "hp" else [0., 0., 0., 1.]])
    return a, b, c


def continuous_response(stage, cutoff, q):
    a, b, c = stage_system(stage, cutoff, q)
    num, den = signal.ss2tf(a, b, c, np.zeros((1, 1)))

    def at(freq):
        s = 2j * PI * np.asarray(freq)
        return np.polyval(num[0], s) / np.polyval(den, s)

    return at


def prewarped_trapezoid(stage, cutoff, q, sample_rate):
    """Identical linear bilinear frequency map to the existing realtime filter."""
    a, b, c = stage_system(stage, cutoff, q)
    num, den = signal.ss2tf(a, b, c, np.zeros((1, 1)))
    theta = PI * cutoff / sample_rate
    warp = math.tan(theta) / theta

    def at(freq):
        s = 2j * sample_rate * np.tan(PI * np.asarray(freq) / sample_rate) / warp
        return np.polyval(num[0], s) / np.polyval(den, s)

    return at


def causal_polynomial_integrator(stage, cutoff, q, sample_rate, taps):
    """Exp(A*T), plus B_j for causal Lagrange source through current/past taps.

    x[n]=Ad*x[n-1]+sum(B_j*u[n-j]), where B_j is the exact-form
    integral of exp(A*(T-t))*B*L_j(t/T-1), approximated here via 16-point
    Gauss-Legendre quadrature. taps=4 means a cubic interpolant.
    """
    a, b, c = stage_system(stage, cutoff, q)
    seconds = 1. / sample_rate
    ad = linalg.expm(a * seconds)
    roots, weights = roots_legendre(16)
    roots = (roots + 1.) / 2.
    weights = weights / 2.
    lagrange_b = np.zeros((taps, 4))
    for sample, weight in zip(roots, weights):
        tangent = (linalg.expm(a * (1. - sample) * seconds) @ b).ravel()
        interpolation_offset = sample - 1.
        for j in range(taps):
            basis = math.prod((interpolation_offset + k) / (k - j)
                              for k in range(taps) if k != j)
            lagrange_b[j] += seconds * weight * basis * tangent
    # Exact rational digital transfer of the cubic-interpolated source.
    sections = [signal.ss2tf(ad, bj.reshape(4, 1), c, np.zeros((1, 1)))
                for bj in lagrange_b]

    def at(freq):
        z = np.exp(2j * PI * np.asarray(freq) / sample_rate)
        return sum(z ** (1 - j) * np.polyval(num[0], z) / np.polyval(den, z)
                   for j, (num, den) in enumerate(sections))

    return at


def log_grid(start, factor, stop, inclusive=False):
    """Copy the published C++ test's iterated-frequency grid, not geomspace."""
    points = []
    f = start
    while f <= stop if inclusive else f < stop:
        points.append(f)
        f *= factor
    return np.array(points)


def score(rate, cutoff, method):
    peak_f = log_grid(5., 1.01, min(24000., .45 * rate))
    band_f = log_grid(20., 1.02, min(20000., rate / 5.), inclusive=True)
    core_mask = ((band_f >= .5 * cutoff) & (band_f <= 2. * cutoff)
                 & (band_f <= rate / 8.))
    max_core, max_band = 0., 0.
    for q in QS:
        for config in range(3):
            hp = 0. if config == 1 else (max(60., cutoff / 4.) if config == 2 else cutoff)
            lp = 0. if config == 0 else cutoff
            stages = [(name, hz) for name, hz in (("hp", hp), ("lp", lp)) if hz > 0.]
            analog = [continuous_response(name, hz, q) for name, hz in stages]
            if method == "prewarped":
                sampled = [prewarped_trapezoid(name, hz, q, rate) for name, hz in stages]
            else:
                sampled = [causal_polynomial_integrator(name, hz, q, rate, int(method[-1]))
                           for name, hz in stages]
            def multiply(parts, frequencies):
                return np.prod([part(frequencies) for part in parts], axis=0)
            peak = np.max(np.abs(multiply(analog, peak_f)))
            error = np.abs(multiply(analog, band_f) - multiply(sampled, band_f)) / peak
            max_band = max(max_band, float(np.max(error)))
            if np.any(core_mask):
                max_core = max(max_core, float(np.max(error[core_mask])))
    return max_core, max_band


def main():
    print("IG00156 research-only LINEAR truth; 5 Q x 3 configs x 5 cutoffs", flush=True)
    print("core limit 0.03; band limit 0.08 below 192 kHz, else 0.02", flush=True)
    for rate in RATES:
        band_limit = HIGH_RATE_BAND_LIMIT if rate >= 192000 else LOW_RATE_BAND_LIMIT
        print(f"rate {rate:.0f} Hz:", flush=True)
        for name in ("prewarped", "causal3", "causal4"):
            results = [score(rate, cutoff, name) for cutoff in CUTOFFS]
            fails = sum(core > CORE_LIMIT or band > band_limit for core, band in results)
            numbers = " | ".join(f"{cutoff:.0f}: {core:.4f}/{band:.4f}"
                                 for cutoff, (core, band) in zip(CUTOFFS, results))
            print(f"  {name:>10s} {fails} over: {numbers}", flush=True)
    print("No production DSP, nonlinear, CPU or official gate is assessed.")


if __name__ == "__main__":
    main()
