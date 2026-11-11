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
        paths = {'renderer': 'src/tools/truth_render.cpp', 'input_sha256_helper': 'src/tools/TruthInputSha256.h', 'network': 'src/reference/IG00156Network.h',
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
        fixtures = {'arbitrary': [0.3, -0.7, .1, .8, -.5, -.5, .2, 0.],
                    'constant': [.25]*12, 'zero': [0.]*8,
                    'multitone': [.2*math.sin(i*.2)+.3*math.cos(i*.7) for i in range(49)]}
        for stimulus in ['sine', 'impulse', *fixtures]:
            if stimulus in fixtures:
                path = self.write_input(stimulus+'.csv', fixtures[stimulus])
                out = self.run_tool(stimulus, '--input', path, '--input-rate', 48000,
                                    '--input-samples', len(fixtures[stimulus]), '--ota-level', 0, '--gain', 'physical')
            else:
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
                    if stimulus in fixtures:
                        left, right = fixtures[stimulus][sample:sample+2]
                        u0 = left + (right-left)*sub/m['export_substeps']
                        u1 = left + (right-left)*(sub+1)/m['export_substeps']
                    rhs = [state[i]+h*.5*(sum(a[i][j]*state[j] for j in range(8))+b[i]*(u0+u1)) for i in range(8)]
                    state = solve(lhs, rhs)
                for j, name in enumerate(['hp_vin', 'hp_x', 'hp_bp', 'hp_lp', 'lp_vin', 'lp_x', 'lp_bp', 'lp_lp']):
                    self.assertAlmostEqual(row[name], state[j], delta=2e-12)

    def write_input(self, name, values):
        path = self.root / name
        path.write_text('sample,value\n'+''.join(f'{i},{v:.17g}\n' for i, v in enumerate(values)))
        return path

    def test_sample_provenance_and_endpoints(self):
        values = [.2*math.sin(i*.3) for i in range(65)]
        path = self.write_input('oscillator.csv', values)
        args = ['--input', path, '--input-rate', 48000, '--input-samples', len(values)]
        a = self.run_tool('input-a', *args)
        b = self.run_tool('input-b', *args)
        for name in ['trace.csv', 'manifest.json', 'stimulus.csv']:
            self.assertEqual((a/name).read_bytes(), (b/name).read_bytes())
        m, rows = self.read(a)
        self.assertEqual([r['input'] for r in rows], values)
        self.assertEqual(rows[0]['hp'], 0)
        self.assertEqual(rows[0]['lp'], 0)
        self.assertEqual(m['intervals'], len(values)-1)
        self.assertEqual(m['actual_duration_s'], (len(values)-1)/48000)
        self.assertEqual(m['stimulus'], 'samples')
        self.assertIsNone(m['frequency_hz'])
        self.assertIsNone(m['amplitude'])
        self.assertIsNone(m['impulse_definition'])
        self.assertEqual(m['input']['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertEqual(m['input']['bytes'], path.stat().st_size)
        self.assertEqual(m['input']['sample_count'], len(values))
        self.assertEqual(m['input']['rate_hz'], 48000)
        self.assertIn('not calibrated', m['input']['units'])
        self.assertEqual((a/'stimulus.csv').read_bytes(), path.read_bytes())
        # Exact-byte hash/export includes CRLF, explicit plus sign and decimal spelling.
        # Exercise SHA padding on both sides of 56/64-byte boundaries and multiblock input.
        for size in [55, 56, 63, 64, 65, 119, 120, 127, 128, 129]:
            raw = b'sample,value\r\n0,+0.5\r\n1,-0.' + b'0'*(size-30) + b'5\r\n'
            self.assertEqual(len(raw), size)
            path.write_bytes(raw)
            out = self.run_tool('hash'+str(size), '--input', path, '--input-rate', 8000,
                                '--input-samples', 2, '--ota-level', 0, '--substeps', 64)
            meta, _ = self.read(out)
            self.assertEqual(meta['input']['sha256'], hashlib.sha256(raw).hexdigest())
            self.assertEqual((out/'stimulus.csv').read_bytes(), raw)

    def test_sample_rates_and_signed_bounds(self):
        path = self.write_input('bounds.csv', [-8, 8])
        for rate in [8001, 44100, 384000]:
            m, rows = self.read(self.run_tool('rate'+str(rate), '--input', path,
                                '--input-rate', rate, '--input-samples', 2,
                                '--ota-level', 0, '--substeps', 128))
            self.assertEqual([r['input'] for r in rows], [-8, 8])
            self.assertEqual(m['rate_hz'], rate)
            self.assertEqual(m['actual_duration_s'], 1/rate)

    def test_sample_invalid_inputs_and_options(self):
        path = self.write_input('bad.csv', [0, .1, -.1])
        args = ['--input', path, '--input-rate', 48000, '--input-samples', 3]
        malformed = [b'', b'sample,value\n', b'input\n0\n1\n2\n',
                     b'sample,value\n0,0\n1,.1\n', b'sample,value\n0,0\n1,.1\n2,-.1',
                     b'sample,value\n0,0\n1,.1\n2,-.1\n3,0\n',
                     b'sample,value\n0,0\n1,.1\n2,-.1\n\n']
        for value in ['nan', 'inf', '-inf', '1e999', '1e-999', '8.01', '-8.01', '0x1p0',
                      '', '1,2', ' 1', '1 ', '.', '+', '1e+', '1x', '\x00']:
            malformed.append(f'sample,value\n0,0\n1,{value}\n2,0\n'.encode())
        for index in ['-1', '+1', '01', '2', '99999999999999999999999999']:
            malformed.append(f'sample,value\n0,0\n{index},0\n2,0\n'.encode())
        malformed.append(b'sample,value\n0,0\n1,'+b'0'*129+b'\n2,0\n')
        for i, raw in enumerate(malformed):
            path.write_bytes(raw)
            self.run_tool('invalid-input'+str(i), *args, success=False)
            self.assertEqual(path.read_bytes(), raw)
        path = self.write_input('bad.csv', [0, .1, -.1])
        for i, extra in enumerate([['--stimulus', 'sine'], ['--frequency', 440], ['--amplitude', 1],
                                    ['--duration', .1], ['--rate', 48000], ['--input', path],
                                    ['--tolerance', '1e-20']]):
            self.run_tool('input-options'+str(i), *args, *extra, success=False)
        for i, options in enumerate([['--input', path], ['--input-rate', 48000], ['--input-samples', 3],
                                    ['--stimulus', 'samples']]):
            self.run_tool('missing-options'+str(i), *options, success=False)
        for i, rate in enumerate([0, -1, 7999, 384001, 48000.5, 'nan', 'inf']):
            self.run_tool('invalid-rate'+str(i), '--input', path, '--input-rate', rate,
                          '--input-samples', 3, success=False)
        for i, count in enumerate([0, 1, 2, 4, 3.5, 3840002, 'nan', '1e999', 80002]):
            self.run_tool('invalid-count'+str(i), '--input', path, '--input-rate', 8000,
                          '--input-samples', count, success=False)
        self.run_tool('work-limit', '--input', path, '--input-rate', 48000,
                      '--input-samples', 200000, '--substeps', 256, success=False)
        for name, target in [('missing-file', self.root/'absent'), ('directory-file', self.root)]:
            self.run_tool(name, '--input', target, '--input-rate', 48000, '--input-samples', 3, success=False)
        with path.open('wb') as f:
            f.truncate(64*1024*1024+1)
        self.run_tool('oversize', *args, success=False)
        path = self.write_input('bad.csv', [0, .1, -.1])
        self.run_tool('missing-parent/child', *args, success=False)
        # File, directory, and dangling-link collisions never overwrite content.
        for name in ['occupied.csv', 'occupied-dir', 'dangling']:
            target = self.root/name
            if name == 'occupied.csv': target.write_bytes(b'keep')
            elif name == 'occupied-dir': target.mkdir(); (target/'keep').write_bytes(b'keep')
            else: target.symlink_to(self.root/'absent')
            result = subprocess.run([RENDERER, '--out', str(target), *map(str,args)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            if name == 'occupied.csv': self.assertEqual(target.read_bytes(), b'keep')
            elif name == 'occupied-dir': self.assertEqual((target/'keep').read_bytes(), b'keep')
            else: self.assertTrue(target.is_symlink())

    @unittest.skipUnless(__import__('os').name == 'posix', 'POSIX file-size failure injection')
    def test_input_write_failure_cleanup(self):
        import resource
        import signal
        path = self.write_input('source.csv', [.1*math.sin(i) for i in range(65)])
        original = path.read_bytes()
        out = self.root/'failed-write'
        def limit_file_size():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (1024, 1024))
        result = subprocess.run([RENDERER, '--out', str(out), '--input', str(path),
                                 '--input-rate', '48000', '--input-samples', '65'],
                                capture_output=True, text=True, preexec_fn=limit_file_size)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('Filter reference exported', result.stdout)
        self.assertFalse(out.exists())
        self.assertEqual(path.read_bytes(), original)

    def comparison_rows(self, out):
        with (out / 'realtime.csv').open() as f:
            rows = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(f)]
        self.assertTrue(all(math.isfinite(v) for r in rows for v in r.values()))
        return rows

    def test_realtime_metrics_and_provenance(self):
        import struct
        def f32(x):
            return struct.unpack('f', struct.pack('f', x))[0]
        fixtures = {'zero': [0.]*65,
                    'driven': [2.5*math.sin(i*.3)+.25*math.sin(i*.07) for i in range(257)]}
        for name, values in fixtures.items():
            path = self.write_input(name+'.csv', values)
            args = ['--input', path, '--input-rate', 96000, '--input-samples', len(values),
                    '--hp-hz', 1000.123456, '--lp-hz', 1700.234567, '--q', 1.23456789]
            plain = self.run_tool(name+'-plain', *args)
            out = self.run_tool(name+'-rt', *args, '--compare-realtime', 'true', '--realtime-rate', 96000)
            again = self.run_tool(name+'-again', *args, '--compare-realtime', 'true', '--realtime-rate', 96000)
            m, reference = self.read(out)
            rt = self.comparison_rows(out)
            self.assertEqual((plain/'trace.csv').read_bytes(), (out/'trace.csv').read_bytes())
            plain_m, _ = self.read(plain)
            c = m.pop('realtime_comparison')
            self.assertEqual(plain_m, m)
            self.assertFalse((plain/'realtime.csv').exists())
            for file in ['trace.csv', 'manifest.json', 'stimulus.csv', 'realtime.csv']:
                self.assertEqual((out/file).read_bytes(), (again/file).read_bytes())
            self.assertEqual(len(rt), len(reference))
            self.assertEqual(c['sample_count'], len(rt))
            self.assertEqual(c['internal_rate_hz'], 96000)
            self.assertEqual(c['q_float'], f32(1.23456789))
            self.assertEqual(c['ota_level'], 2.5)
            source = Path(__file__).resolve().parent.parent
            paths = {'filter': 'Ig00156Filter.h', 'dsp': 'Dsp.h', 'fast_math': 'FastMath.h'}
            self.assertEqual(c['source_sha256'], {k: hashlib.sha256((source/'src/core'/p).read_bytes()).hexdigest()
                                                for k, p in paths.items()})
            self.assertEqual([r['input_float'] for r in rt], [f32(v) for v in values])
            self.assertEqual([r['sample'] for r in rt], [r['sample'] for r in reference])
            self.assertEqual([r['time_s'] for r in rt], [r['time_s'] for r in reference])
            ff = 1/(2*math.pi*4.7e6*1.5e-9)
            for stage, cutoff in [('hp', 1000.123456), ('lp', 1700.234567)]:
                self.assertEqual(c[stage+'_gm_cutoff_hz_float'], f32(cutoff-ff))
                self.assertEqual(c[stage+'_effective_total_hz'], f32(cutoff-ff)+ff)
                n = m[stage]
                g = 1/n['source_ohms'] + 1/n['ground_ohms'] + 1/n['feedback_ohms']
                self.assertEqual(c[stage+'_q_siemens'], max(0, g/(2*f32(1.23456789))-1/n['feedback_ohms']))
                diffs = [r[stage+'_realtime']-ref[stage] for r, ref in zip(rt, reference)]
                self.assertEqual(diffs, [r[stage+'_difference'] for r in rt])
                energy = sum(r[stage]**2 for r in reference)
                error = sum(d*d for d in diffs)
                self.assertAlmostEqual(c[stage]['absolute_rms'], math.sqrt(error/len(rt)), delta=1e-15)
                self.assertEqual(c[stage]['absolute_max'], max(map(abs, diffs)))
                self.assertAlmostEqual(c[stage]['reference_rms'], math.sqrt(energy/len(rt)), delta=1e-15)
                if energy == 0:
                    self.assertIsNone(c[stage]['relative_rms'])
                    self.assertEqual(error, 0)
                else:
                    self.assertAlmostEqual(c[stage]['relative_rms'], math.sqrt(error/energy), delta=1e-15)
                    self.assertGreater(error, 0)  # non-vacuous: outputs are not copied from the reference
                    self.assertGreater(max(abs(r[stage+'_realtime']) for r in rt), .01)

    def test_realtime_options_and_clamp_boundaries(self):
        base = ['--duration', '.001', '--compare-realtime', 'true', '--realtime-rate', 48000]
        for i, args in enumerate([['--compare-realtime', 'true'], ['--realtime-rate', 48000],
                                 ['--compare-realtime', 'false', '--realtime-rate', 48000],
                                 ['--compare-realtime', 'true', '--realtime-rate', 96000],
                                 ['--compare-realtime', 'true', '--realtime-rate', 'nan'],
                                 base+['--gain', 'physical'], base+['--ota-level', 0],
                                 base+['--ota-level', 2.50000001], base+['--stimulus', 'impulse'],
                                 base+['--compare-realtime', 'true']]):
            self.run_tool('rt-invalid'+str(i), *args, success=False)
        nonzero = self.write_input('nonzero.csv', [1e-20, .2, 0])
        self.run_tool('rt-nonzero', '--input', nonzero, '--input-rate', 48000, '--input-samples', 3,
                      '--compare-realtime', 'true', '--realtime-rate', 48000, success=False)
        # Float gm control is checked before production's clamp, not only the requested double total.
        for stage in ['hp', 'lp']:
            for cutoff, ok in [(3599.999, True), (3600.001, False), (20000, False), (25, True)]:
                self.run_tool('boundary-'+stage+str(cutoff), '--rate', 8000, '--duration', '.001',
                              '--amplitude', 0, '--'+stage+'-hz', cutoff, '--compare-realtime', 'true',
                              '--realtime-rate', 8000, success=ok)
        # All supported endpoint clocks can also be declared as the internal clock.
        for rate in [8000, 8001, 44100, 384000]:
            self.run_tool('rt-rate'+str(rate), '--rate', rate, '--duration', '.001', '--amplitude', 0,
                          '--compare-realtime', 'true', '--realtime-rate', rate)

    def test_realtime_synthetic_reference_unchanged(self):
        args = ['--duration', '.005', '--amplitude', 2.5, '--q', 5]
        plain = self.run_tool('sine-plain', *args)
        out = self.run_tool('sine-compare', *args, '--compare-realtime', 'true', '--realtime-rate', 48000)
        self.assertEqual((plain/'trace.csv').read_bytes(), (out/'trace.csv').read_bytes())
        self.assertGreater(self.read(out)[0]['realtime_comparison']['lp']['absolute_rms'], 0)

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
