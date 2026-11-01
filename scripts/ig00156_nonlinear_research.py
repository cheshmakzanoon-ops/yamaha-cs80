#!/usr/bin/env python3
"""Optional, read-only IG00156 coupled nonlinear discretization counterexample.

Run: python3 scripts/ig00156_nonlinear_research.py [--include-16] [--level-sweep]
Needs NumPy/SciPy, NOT core/plugin/CI dependencies. Tested with NumPy 2.3.5,
SciPy 1.17.0. Never changes tests, circuit numerics, files or tolerances.

The continuous 4+4-state matrices are those of the declared M41 inference in
reference/IG00156Network.h (CS-80 service p41, control anchors p42); the
two OTA currents per filter retain the declared in-loop gm*L*tanh(v/L)
(CS20M manual pp53-54 supports integrators but NOT the inferred tanh level).
This is a numerical validation of that *assumed* equation, no measured audio.
"""
import argparse
import math

import numpy as np
from scipy import integrate, linalg
from scipy.special import roots_legendre

from ig00156_discretization_prototype import stage_system


LEVEL = 2.5  # Same declared kIg00156OtaLevel as realtime, engine units.
FF_RATE = 1. / (4.7e6 * 1.5e-9)
RATE, CUTOFF, FREQ, Q, DURATION = 88200., 12000., 6000., 9., .012


class Coupled:
    """8-state exact linear exp(A*T), cubic source and nonlinear 4D collocation."""
    def __init__(self, rate, cutoff, q, steps):
        ah, bh, ch = stage_system('hp', cutoff, q)
        al, bl, _ = stage_system('lp', cutoff, q)
        self.a = np.block([[ah, np.zeros((4, 4))], [bl @ ch, al]])
        self.b = np.vstack((bh, np.zeros((4, 1))))
        self.gm = np.array([-ah[3, 2]] * 2 + [-al[3, 2]] * 2) - FF_RATE
        self.state = np.zeros(8)
        self.history = np.zeros(4)
        self.steps = steps
        self.newton_evaluations = 0
        dt = 1. / (rate * steps)
        self.transition = linalg.expm(self.a * dt)
        self.source = np.zeros((steps, 8, 4))
        self.old_force, self.new_force = np.zeros((8, 4)), np.zeros((8, 4))
        quadrature, weights = roots_legendre(16)
        for tau, weight in zip((quadrature + 1.) / 2., weights / 2.):
            flow = linalg.expm(self.a * (1. - tau) * dt) * (dt * weight)
            self.old_force += flow[:, [2, 3, 6, 7]] * (1. - tau)
            self.new_force += flow[:, [2, 3, 6, 7]] * tau
            source = (flow @ self.b).ravel()
            for step in range(steps):
                x = (step + tau) / steps - 1.
                for j in range(4):
                    basis = math.prod((x + k) / (k - j) for k in range(4) if k != j)
                    self.source[step, :, j] += source * basis

    def nonlinear(self, state):
        hp = 2. * (state[1] + state[2]) - state[3]
        lp_hp = 2. * (state[5] + state[6]) - state[7]
        ports = np.array([hp, state[2], lp_hp, state[6]])
        tangent = np.tanh(ports / LEVEL)
        residual = self.gm * (ports - LEVEL * tangent)
        slopes = self.gm * tangent * tangent
        jac = np.zeros((4, 8))
        jac[0, 1] = jac[0, 2] = 2. * slopes[0]
        jac[0, 3] = -slopes[0]
        jac[1, 2] = slopes[1]
        jac[2, 5] = jac[2, 6] = 2. * slopes[2]
        jac[2, 7] = -slopes[2]
        jac[3, 6] = slopes[3]
        return residual, jac

    def ode(self, time, state, source):
        force, _ = self.nonlinear(state)
        derivative = self.a @ state + self.b[:, 0] * source(time)
        derivative[[2, 3, 6, 7]] += force
        return derivative

    def sample(self, input_value):
        self.history[1:] = self.history[:-1]
        self.history[0] = input_value
        for step in range(self.steps):
            previous, _ = self.nonlinear(self.state)
            known = (self.transition @ self.state + self.source[step] @ self.history
                     + self.old_force @ previous)
            value, _ = self.nonlinear(known)
            for iteration in range(30):
                current = known + self.new_force @ value
                correct, jac = self.nonlinear(current)
                delta = value - correct
                if np.max(np.abs(delta)) < 1e-10 * max(1., np.max(np.abs(value))):
                    self.newton_evaluations += iteration + 1
                    self.state = current
                    break
                value -= np.linalg.solve(np.eye(4) - jac @ self.new_force, delta)
            else:
                raise RuntimeError('8-state nonlinear Newton failed to converge')
        return self.state[7]


def compare(amplitude, steps, restart_at=0.):
    model = Coupled(RATE, CUTOFF, Q, steps)
    times = np.arange(round(DURATION * RATE) + 1) / RATE
    source = lambda t: amplitude * math.sin(2. * math.pi * FREQ * t)
    # DOP853 is independent of the exponential/cubic integration rule.
    truth = integrate.solve_ivp(lambda t, x: model.ode(t, x, source),
                                (0., times[-1]), np.zeros(8), t_eval=times,
                                method='DOP853', rtol=1e-9, atol=1e-11,
                                max_step=1. / (32. * max(CUTOFF, FREQ)))
    if not truth.success:
        raise RuntimeError(truth.message)
    y_true = truth.y[7]
    if restart_at:
        # Diagnostic counterfactual ONLY, impossible as a plugin technique:
        # inject the analog oracle's *entire* physical state after startup.
        i0 = round(restart_at * RATE)
        model.state = np.copy(truth.y[:, i0])
        model.history = np.array([source((i0-j) / RATE) for j in range(4)])
        audio = np.copy(y_true)
        for i in range(i0 + 1, len(times)):
            audio[i] = model.sample(source(times[i]))
    else:
        # Physical capacitor states begin at zero, while an analytically
        # smooth sine supplies historical Lagrange nodes before t=0.
        model.history = np.array([source(-j / RATE) for j in range(4)])
        audio = np.zeros(len(times))
        for i in range(1, len(times)):
            audio[i] = model.sample(source(times[i]))
    mask = times > .6 * DURATION
    rms = lambda z: math.sqrt(float(np.mean(z[mask] ** 2)))
    result = rms(audio - y_true) / rms(y_true)
    return result, rms(y_true), rms(audio), model.newton_evaluations / len(times)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--include-16', action='store_true', help='include expensive 16x trial')
    ap.add_argument('--level-sweep', action='store_true', help='also test inferred branch onset')
    args = ap.parse_args()
    print('Research ONLY: unchanged 8-state M41 inference, in-loop OTA tanh, DOP853 truth.')
    print('Existing nonlinear reference limit 0.03 is informational here, NOT an official new gate.')
    cases = [(8., 1, 0.), (8., 4, 0.), (8., 4, .004)]
    if args.include_16:
        cases.append((8., 16, 0.))
    if args.level_sweep:
        cases.extend((level, 4, 0.) for level in (2.5, 5., 6., 7.))
    for amplitude, steps, restart in cases:
        error, ref_rms, trial_rms, evals = compare(amplitude, steps, restart)
        print(f'fs={RATE:.0f} fc={CUTOFF:.0f} q={Q:g} amp={amplitude:g} '
              f'internal={steps}x oracle_restart={restart:g}s '
              f'rms_rel={error:.6g} truth_rms={ref_rms:.5g} '
              f'candidate_rms={trial_rms:.5g} newton_evals/output={evals:.2f} '
              f'under_0.03={error <= .03}', flush=True)
    print('ADJ p11 HP remains 1.69 Vpp versus 4.3 ± 1; top-octave release gate OPEN.')


if __name__ == '__main__':
    main()
