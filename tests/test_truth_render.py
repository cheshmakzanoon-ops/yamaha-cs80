#!/usr/bin/env python3
"""Executable artifact contract and independent linear trapezoid oracle (stdlib only)."""
import argparse
import csv
import json
import hashlib
import math
from pathlib import Path
import subprocess
import tempfile
import unittest

RENDERER = None


def solve(a, b):
    m = [list(row) + [v] for row, v in zip(a, b)]
    n = len(b)
    for k in range(n):
        p = max(range(k, n), key=lambda i: abs(m[i][k]))
        m[k], m[p] = m[p], m[k]
        for i in range(k + 1, n):
            f = m[i][k] / m[k][k]
            for j in range(k, n + 1):
                m[i][j] -= f * m[k][j]
    x = [0.] * n
    for i in reversed(range(n)):
        x[i] = (m[i][n] - sum(m[i][j] * x[j] for j in range(i + 1, n))) / m[i][i]
    return x


class RenderTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def run_tool(self, name, *args, success=True):
        out = self.root / name
        result = subprocess.run([RENDERER, '--out', str(out), *map(str, args)], capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        if not success:
            self.assertFalse(out.exists(), result.stderr)
        return out

    def read(self, out):
        manifest = json.loads((out / 'manifest.json').read_text(), parse_constant=lambda x: self.fail(x))
        with (out / 'trace.csv').open() as f:
            rows = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(f)]
        self.assertTrue(all(math.isfinite(v) for r in rows for v in r.values()))
        return manifest, rows

    def test_deterministic_metadata_and_nonlinear_refinement(self):
        a = self.run_tool('a', '--duration', '.005', '--amplitude', 2.5, '--q', 5)
        b = self.run_tool('b', '--duration', '.005', '--amplitude', 2.5, '--q', 5)
        for name in ['trace.csv', 'manifest.json']:
            self.assertEqual((a / name).read_bytes(), (b / name).read_bytes())
        m, rows = self.read(a)
        self.assertEqual(len(rows), 241)
        self.assertEqual(m['export_substeps'], 32)
        source = Path(__file__).resolve().parent.parent
        paths = {'renderer': 'src/tools/truth_render.cpp', 'network': 'src/reference/IG00156Network.h',
                 'linear': 'src/reference/IG00156Linear.h', 'control': 'src/core/Ig00156Filter.h',
                 'model_document': 'docs/IG00156_NODAL_MODEL.md'}
        self.assertEqual(m['source_sha256'], {k: hashlib.sha256((source / p).read_bytes()).hexdigest()
                                             for k, p in paths.items()})
        self.assertTrue(m['newton_certified'])
        self.assertLessEqual(m['max_relative_residual'], 1e-14)
        self.assertLessEqual(m['time_refinement_relative_max'], .001)
        self.assertEqual(m['hp']['source_ohms'], 75000)
        self.assertEqual(m['lp']['source_ohms'], 150000)
        for stage in ['hp', 'lp']:
            n = m[stage]
            for key, value in [('ground_ohms', 33000), ('input_farads', 1e-6), ('feedback_ohms', 1.2e6),
                               ('feedback_farads', 180e-12), ('c1_farads', 1.5e-9), ('c2_farads', 1.5e-9),
                               ('feedforward1_ohms', 4.7e6), ('feedforward2_ohms', 4.7e6), ('ota_level', 2.5)]:
                self.assertEqual(n[key], value)
            g = 1 / n['source_ohms'] + 1 / 33000 + 1 / 1.2e6
            self.assertAlmostEqual(n['q_siemens'], g / 10 - 1 / 1.2e6, places=16)
            self.assertAlmostEqual(n['source_gain'], g * n['source_ohms'] / 2, places=14)
            hz = m[stage + '_total_hz']
            w = 2 * math.pi * hz
            beta = w*w / (g / 180e-12)
            gm = (beta + math.sqrt(beta*beta + w*w)) * 1.5e-9 - 1 / 4.7e6
            self.assertAlmostEqual(n['gm1_siemens'], gm, places=16)
            self.assertEqual(n['gm1_siemens'], n['gm2_siemens'])

    def test_independent_linear_trapezoid(self):
        for stimulus in ['sine', 'impulse']:
            out = self.run_tool(stimulus, '--duration', '.001', '--ota-level', 0, '--gain', 'physical', '--stimulus', stimulus)
            m, rows = self.read(out)
            def derivative(z, u):
                d = []
                for offset, name, src in [(0, 'hp', u), (4, 'lp', 2*(z[1]+z[2])-z[3])]:
                    n = m[name]
                    vin, x, bp, lp = z[offset:offset+4]
                    ai = x + bp
                    hp = 2*ai-lp
                    current = (src - vin - ai) / n['source_ohms']
                    d.extend([current/n['input_farads'],
                              (current-ai/n['ground_ohms']-x/n['feedback_ohms']+n['q_siemens']*bp)/n['feedback_farads'],
                              -(n['gm1_siemens']+1/n['feedforward1_ohms'])*hp/n['c1_farads'],
                              -(n['gm2_siemens']+1/n['feedforward2_ohms'])*bp/n['c2_farads']])
                return d
            columns = [derivative([float(i == j) for i in range(8)], 0) for j in range(8)]
            a = list(zip(*columns))
            b = derivative([0.]*8, 1)
            h = 1 / (m['rate_hz'] * m['export_substeps'])
            lhs = [[float(i == j)-h*.5*a[i][j] for j in range(8)] for i in range(8)]
            state = [0.]*8
            for sample, row in enumerate(rows[1:]):
                for sub in range(m['export_substeps']):
                    t0 = (sample + sub/m['export_substeps'])/m['rate_hz']
                    t1 = t0+h
                    u0, u1 = ((float(sample == 0),)*2 if stimulus == 'impulse' else
                              (math.sin(2*math.pi*440*t0), math.sin(2*math.pi*440*t1)))
                    rhs = [state[i]+h*.5*(sum(a[i][j]*state[j] for j in range(8))+b[i]*(u0+u1)) for i in range(8)]
                    state = solve(lhs, rhs)
                for j, name in enumerate(['hp_vin', 'hp_x', 'hp_bp', 'hp_lp', 'lp_vin', 'lp_x', 'lp_bp', 'lp_lp']):
                    self.assertAlmostEqual(row[name], state[j], delta=2e-12)

    def test_zero_and_failures(self):
        m, rows = self.read(self.run_tool('zero', '--duration', '.001', '--amplitude', 0))
        self.assertEqual(m['time_refinement_relative_max'], 0)
        self.assertTrue(all(r['lp'] == 0 for r in rows))
        for i, args in enumerate([['--rate', 'nan'], ['--rate', '48000x'], ['--substeps', '1.5'],
                                 ['--q', '-1'], ['--stimulus', 'bad'], ['--rate', '48000', '--rate', '96000'],
                                 ['--unknown', '1'], ['--duration'], ['--duration', '10', '--substeps', '256'],
                                 ['--tolerance', '1e-20'], ['--duration', '0'], ['--amplitude', 'inf']]):
            self.run_tool('bad'+str(i), *args, success=False)
        # A finite-but-exhausted solve must fail the executable before publication.
        failed = subprocess.run([RENDERER, '--out', str(self.root / 'newton'),
                                 '--rate', '8000', '--duration', '.001', '--substeps', '1',
                                 '--hp-hz', '200', '--lp-hz', '20000', '--q', '9',
                                 '--amplitude', '8', '--frequency', '3000'], capture_output=True, text=True)
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn('Newton solve not certified', failed.stderr)
        self.assertFalse((self.root / 'newton').exists())
        existing = self.root / 'existing'
        existing.mkdir()
        marker = existing / 'keep'; marker.write_text('original')
        result = subprocess.run([RENDERER, '--out', str(existing)], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(list(existing.iterdir()), [marker])
        self.assertEqual(marker.read_text(), 'original')
        self.run_tool('missing/child', '--duration', '.001', success=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--renderer', required=True)
    args, rest = parser.parse_known_args()
    RENDERER = args.renderer
    unittest.main(argv=[__file__, *rest])
