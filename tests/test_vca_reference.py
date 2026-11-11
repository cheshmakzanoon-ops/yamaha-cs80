#!/usr/bin/env python3
"""Independent analytic/convolution oracles and hostile-input CLI contracts."""
import argparse
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import unittest

PARSER = argparse.ArgumentParser()
PARSER.add_argument('--renderer', required=True)
ARGS, REST = PARSER.parse_known_args()
RENDERER = str(Path(ARGS.renderer).resolve())
ROOT = Path(__file__).resolve().parents[1]
TAU = 1 / (1 / 180000 + 1 / 560000 + 3 / 270000) * 1e-8
AC = 0.1
F = lambda x: struct.unpack('f', struct.pack('f', x))[0]
OFFSET, FEED = F(.0015), F(.001)


def exact_li(initial, lc, slope, t):
    return lc + slope * t - slope * TAU + (initial - lc + slope * TAU) * math.exp(-t / TAU)


def oracle(drives, rate, initial):
    """Exact LI plus integrating-factor convolution, composite Simpson quadrature.

    Independent of RK4: only scalar quadrature of a known analytic LI trajectory.
    Splits a possible LI=0 crossing to avoid Simpson's nonsmooth gain integrand.
    """
    li, coupling = initial, 0.0
    result = []
    def raw(l, d):
        g = max(0, l) * 2 ** d[2] * d[3]
        return g * (d[0] + OFFSET) + FEED * g
    result.append((li, coupling, raw(li, drives[0]) - coupling))
    h = 1 / rate
    for a, b in zip(drives, drives[1:]):
        slope = (b[1] - a[1]) / h
        old_li = li
        def level(t):
            return exact_li(old_li, a[1], slope, t)
        def integrand(t):
            d = [x + (y - x) * t / h for x, y in zip(a, b)]
            return math.exp(-(h-t)/AC) * raw(level(t), d) / AC
        knots = [0.0, h]
        # Tests use monotonic LI within each crossing interval.
        if level(0) * level(h) < 0:
            left, right = 0.0, h
            for _ in range(60):
                middle = (left + right) / 2
                if level(left) * level(middle) <= 0:
                    right = middle
                else:
                    left = middle
            knots.insert(1, (left + right) / 2)
        integral = 0.0
        for left, right in zip(knots, knots[1:]):
            step = (right - left) / 256
            total = integrand(left) + integrand(right)
            total += sum((4 if i % 2 else 2) * integrand(left + i * step) for i in range(1, 256))
            integral += total * step / 3
        li = level(h)
        coupling = coupling * math.exp(-h / AC) + integral
        result.append((li, coupling, raw(li, b)-coupling))
    return result


class VcaReferenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / 'input.csv'
        self.out = self.root / 'render'

    def tearDown(self):
        self.temp.cleanup()

    def write(self, drives, crlf=False):
        newline = '\r\n' if crlf else '\n'
        data = 'sample,input,lc,ei,envelope' + newline
        data += ''.join(str(i) + ',' + ','.join(format(v, '.17g') for v in d) + newline for i, d in enumerate(drives))
        self.source.write_bytes(data.encode())
        return len(drives)

    def command(self, count, rate=48000, extra=(), out=None):
        return [RENDERER, '--input', str(self.source), '--rate', str(rate), '--samples', str(count),
                '--out', str(out or self.out), *extra]

    def run_ok(self, drives, rate=48000, extra=(), out=None):
        count = self.write(drives)
        dest = out or self.out
        p = subprocess.run(self.command(count, rate, extra, dest), capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr)
        rows = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(io.StringIO((dest / 'trace.csv').read_text()))]
        return rows, json.loads((dest / 'manifest.json').read_text())

    def reject(self, command):
        p = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(p.returncode, 0)
        self.assertNotIn('Wrote', p.stdout)
        self.assertFalse(self.out.exists())

    def test_analytical_dc_and_initialization(self):
        drives = [[.7, .8, -.5, .9]] * 1201
        rows, manifest = self.run_ok(drives)
        gain = .8 * 2 ** -.5 * .9
        raw = gain * (.7 + OFFSET) + FEED * gain
        self.assertNotEqual(rows[0]['output'], 0)
        for row in rows:
            t = row['time']
            self.assertAlmostEqual(row['li'], .8, delta=2e-15)
            self.assertAlmostEqual(row['gain'], gain, delta=2e-15)
            self.assertAlmostEqual(row['coupling'], raw * (1-math.exp(-t/AC)), delta=1e-14)
            self.assertAlmostEqual(row['output'], raw * math.exp(-t/AC), delta=1e-14)
        self.assertEqual(manifest['input_offset'], OFFSET)
        self.assertEqual(manifest['feedthrough'], FEED)
        self.assertEqual(manifest['li_seconds'], TAU)
        self.assertEqual(manifest['ac_seconds'], 1e-6 * 100000)
        self.assertIsNone(manifest['max_abs_realtime_difference'])

    def test_analytical_lc_step_and_ramp(self):
        for ramp in (False, True):
            with self.subTest(ramp=ramp):
                dest = self.root / str(ramp)
                rate = 8000
                drives = [[.3, 1 + (i / rate * 70 if ramp else 0), 0, 1] for i in range(81)]
                rows, _ = self.run_ok(drives, rate, ('--initial-li', '0'), dest)
                for row in rows:
                    t = row['time']
                    self.assertAlmostEqual(row['li'], exact_li(0, 1, 70 if ramp else 0, t), delta=2e-10)
                    if not ramp:
                        a, b = 1 / TAU, 1 / AC
                        k = .3 + OFFSET + FEED
                        coupling = k * ((1-math.exp(-b*t)) - b*(math.exp(-a*t)-math.exp(-b*t))/(b-a))
                        self.assertAlmostEqual(row['coupling'], coupling, delta=2e-12)

    def test_independent_convolution_varying_all_controls(self):
        for rate in (8000, 48000, 384000):
            drives = [[2*math.sin(i*.43), -.4 + .07*i, .7*math.sin(i*.29), .5+.4*math.cos(i*.31)] for i in range(40)]
            rows, _ = self.run_ok(drives, rate, ('--steps','64'), self.root / str(rate))
            expected = oracle(drives, rate, drives[0][1])
            for row, (li, coupling, output) in zip(rows, expected):
                self.assertAlmostEqual(row['li'], li, delta=2e-12)
                self.assertAlmostEqual(row['coupling'], coupling, delta=2e-9)
                self.assertAlmostEqual(row['output'], output, delta=2e-9)

    def test_realtime_clock_and_float_recurrence(self):
        drives = [[math.sin(i*.1), .8+.1*math.sin(i*.05), .3*math.cos(i*.1), 0 if i==0 else .7] for i in range(80)]
        rows, manifest = self.run_ok(drives, extra=('--compare-realtime','true'))
        self.assertEqual(rows[0]['output'], 0)
        self.assertEqual(rows[0]['realtime_output'], 0)
        node, coupling = F(drives[0][1]), 0
        alpha = F(-math.expm1(-1/(TAU*48000)))
        beta = -math.expm1(-1/(AC*48000))
        for row, d in zip(rows, drives):
            inp, lc, ei, env = map(F, d)
            node = F(node + F(F(lc-node)*alpha))
            control = F(F(max(0, node)*env)*F(2**ei))
            raw = F(F(control*F(inp+OFFSET)) + F(FEED*control))
            coupling += (raw-coupling)*beta
            self.assertAlmostEqual(row['realtime_li'], node, delta=1e-7)
            self.assertAlmostEqual(row['realtime_output'], F(raw-coupling), delta=2e-7)
        for key in ('li','output'):
            expected = max(abs(r['realtime_'+key]-r[key]) for r in rows)
            self.assertEqual(manifest['max_abs_realtime_difference'][key], expected)

    def test_refinement_reports_actual_differences_and_guard(self):
        drives = [[1, 1, 0, 1]] * 30
        coarse, _ = self.run_ok(drives, 8000, ('--steps','1','--initial-li','0'), self.root/'coarse')
        fine, m = self.run_ok(drives, 8000, ('--steps','2','--initial-li','0'), self.root/'fine')
        for key in ('li','coupling','output'):
            self.assertEqual(m['max_abs_h_h2_difference'][key], max(abs(a[key]-b[key]) for a,b in zip(coarse,fine)))
        self.reject(self.command(len(drives),8000,('--steps','1','--initial-li','0','--tolerance','1e-15')))

    def test_snapshot_hash_provenance_and_determinism(self):
        drives = [[0,1,0,0],[.1,.8,.2,.7],[0,0,0,0]]
        count = self.write(drives, crlf=True)
        original = self.source.read_bytes()
        for dest in (self.out, self.root/'again'):
            p = subprocess.run(self.command(count, out=dest), capture_output=True)
            self.assertEqual(p.returncode,0,p.stderr)
        for name in ('stimulus.csv','trace.csv','manifest.json'):
            self.assertEqual((self.out/name).read_bytes(),(self.root/'again'/name).read_bytes())
        self.source.write_text('later mutation')
        self.assertEqual((self.out/'stimulus.csv').read_bytes(),original)
        m = json.loads((self.out/'manifest.json').read_text())
        self.assertEqual(m['stimulus_bytes'],len(original))
        self.assertEqual(m['stimulus_sha256'],hashlib.sha256(original).hexdigest())
        files = {'renderer':'src/tools/vca_reference.cpp','reference':'src/reference/IG00151Reference.h',
                 'decimal_conversion':'src/tools/DecimalConversion.h',
                 'hash_helper':'src/tools/TruthInputSha256.h','declared_core_model':'src/core/Vca.h',
                 'model_document':'docs/IG00151_VCA_MODEL.md','workflow_document':'docs/VCA_REFERENCE.md'}
        for key,path in files.items():
            self.assertEqual(m['source_sha256'][key],hashlib.sha256((ROOT/path).read_bytes()).hexdigest())

    def test_decimal_portability(self):
        tokens = ['+0.5', '-0', '1.', '-.25E+1', '5e-324', '-5e-324', '0e999999', '0e-999999']
        self.source.write_text('sample,input,lc,ei,envelope\n' + ''.join(
            f'{i},{v},0,0,0\n' for i, v in enumerate(tokens)))
        p = subprocess.run(self.command(len(tokens)), capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr)
        rows = list(csv.DictReader(io.StringIO((self.out / 'trace.csv').read_text())))
        for token, row in zip(tokens, rows):
            value = float(row['input'])
            self.assertEqual(value, float(token))
            self.assertEqual(math.copysign(1, value), math.copysign(1, float(token)))

    def test_malformed_csv(self):
        good = 'sample,input,lc,ei,envelope\n0,0,1,0,0\n1,0,1,0,1\n'
        bad = [good[:-1],good+'\n',good.replace('1,0,1,0,1','2,0,1,0,1'),good.replace('0,0,1,0,0','00,0,1,0,0'),
               good.replace('sample,','index,'),good.replace('1,0,1,0,1','1,0,1,0'),good.replace('1,0,1,0,1','1,0,1,0,1,0'),
               good.replace('1,0,1,0,1','1,0,1,0,1\x00'),good.replace('0,0,1,0,0\n','')]
        for token in ('nan','inf','-inf','0x1p0',' 0','0 ','1e9999','1e-9999','2.4e-324','-2.4e-324','+','1e','.', '9','-9','"0"'):
            bad.append(good.replace('1,0,1,0,1',f'1,{token},1,0,1'))
        for column in (2,3,4):
            parts=['1','0','1','0','1']; parts[column]='9'
            bad.append(good.replace('1,0,1,0,1',','.join(parts)))
        bad += [good.replace('1,0,1,0,1','1,0,1,0,-.1'),good.replace('1,0,1,0,1','1,'+'0'*260+',1,0,1')]
        for data in bad:
            with self.subTest(data=data):
                self.source.write_bytes(data.encode())
                self.reject(self.command(2))

    def test_argument_bounds_conflicts_and_counts(self):
        self.write([[0,1,0,0],[0,1,0,1]])
        for extra in (('--unknown','1'),('--rate','48000'),('--steps','0'),('--steps','129'),('--steps','1.0'),
                      ('--initial-li','nan'),('--initial-li','9'),('--tolerance','0'),('--tolerance','2'),
                      ('--compare-realtime','false'),('--compare-realtime','true','--initial-li','0')):
            with self.subTest(extra=extra): self.reject(self.command(2,extra=extra))
        for rate in ('7999','384001','48000.0','8e3','-8000','nan'):
            self.reject(self.command(2,rate))
        for count in (0,1,3,3840002,'2.0'):
            self.reject(self.command(count))
        self.reject(self.command(80002,8000))  # duration
        self.reject(self.command(30000,48000,('--steps','128')))  # work budget
        self.reject([RENDERER])
        self.reject([RENDERER,'--input'])
        self.write([[0,1,0,.1],[0,1,0,1]])
        self.reject(self.command(2,extra=('--compare-realtime','true')))

    def test_file_size_and_boundary_values(self):
        self.source.write_bytes(b'')
        with self.source.open('r+b') as f:
            f.truncate(64 * 1024 * 1024 + 1)
        self.reject(self.command(2))
        drives = [[-8,-8,-8,0],[8,8,8,1]]
        rows, _ = self.run_ok(drives,8000,('--initial-li','-8','--steps','128'))
        self.assertEqual(len(rows),2)
        self.assertEqual(rows[0]['gain'],0)
        self.assertEqual(rows[0]['output'],0)
        self.assertLess(rows[1]['li'],0)
        self.assertEqual(rows[1]['output'],0)

    def test_collisions_and_output_failures(self):
        self.write([[0,1,0,0],[0,1,0,1]])
        for kind in ('file','dir','symlink'):
            if kind=='file': self.out.write_text('preserve')
            elif kind=='dir': self.out.mkdir()
            else: self.out.symlink_to(self.root/'missing')
            p=subprocess.run(self.command(2),capture_output=True)
            self.assertNotEqual(p.returncode,0)
            if kind=='file': self.assertEqual(self.out.read_text(),'preserve')
            if kind=='dir': self.out.rmdir()
            else: self.out.unlink()
        self.reject(self.command(2,out=self.root/'absent'/'child'))
        self.source.unlink()
        self.reject(self.command(2))
        self.source.mkdir()
        self.reject(self.command(2))

    @unittest.skipUnless(os.name=='posix','POSIX file-size failure injection')
    def test_failed_write_rolls_back(self):
        import resource
        self.write([[0,1,0,0],[0,1,0,1]])
        def restrict():
            signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE,(128,128))
        p=subprocess.run(self.command(2),capture_output=True,text=True,preexec_fn=restrict)
        self.assertNotEqual(p.returncode,0)
        self.assertIn('Output write failed',p.stderr)
        self.assertFalse(self.out.exists())
        self.assertNotIn('Wrote',p.stdout)


if __name__ == '__main__':
    unittest.main(argv=[__file__,*REST])
