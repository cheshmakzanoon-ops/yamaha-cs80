#!/usr/bin/env python3
"""Actual-Adsr oracle, independent segment/clock controls, and strict bundle tests."""
import argparse
import csv
from fractions import Fraction as Q
import hashlib
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts/compare_amplitude_envelope_production.py'
SPEC = importlib.util.spec_from_file_location('comparison', SCRIPT)
C = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(C)
RENDERER = ORACLE = None


def f32(value):
    return struct.unpack('f', struct.pack('f', value))[0]


def rows(data):
    return list(csv.DictReader(io.StringIO(data.decode('ascii'))))


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.events = self.root / 'events.csv'
        self.events.write_text('event,time_s,action\n0,0,note_on\n1,.025,note_off\n2,.035,retrigger\n3,.075,note_off\n')
        self.out = self.root / 'output'

    def args(self, **changes):
        values = dict(production=RENDERER, events=self.events, out=self.out, rate=8000, samples=801,
                      attack='.0137', decay='.02', sustain='.37', release='.03', lc='.8', ei='-.5')
        values.update(changes)
        return [str(v) for k, value in values.items() for v in ('--' + k, value)]

    def render(self, **changes):
        C.render(C.parse(self.args(**changes)))
        return json.loads((self.out / 'manifest.json').read_bytes())

    def native(self, events, a='.0137', d='.02', s='.37', r='.03', rate=8000, n=801):
        data = ('event,sample,action\n' + ''.join(f'{i},{sample},{action}\n' for i, (sample, action) in enumerate(events))).encode()
        proc = subprocess.run([str(RENDERER), str(rate), str(n), a, d, s, r], input=data,
                              capture_output=True, timeout=20)
        self.assertEqual(proc.returncode, 0, proc.stderr.decode())
        metadata, trace = proc.stdout.split(b'\n', 1)
        return json.loads(metadata), rows(trace)

    def test_every_metric_recomputes_from_every_csv_row(self):
        report = self.render()
        data = rows((self.out / 'comparison.csv').read_bytes())
        self.assertEqual(len(data), 801)
        errors, reference = [], []
        for i, row in enumerate(data):
            self.assertEqual(int(row['sample']), i)
            ref, actual, diff = map(float, (row['reference'], row['production'], row['difference']))
            self.assertEqual(actual - ref, diff)
            errors.append(diff); reference.append(ref)
        rms = math.sqrt(math.fsum(x*x for x in errors) / len(data))
        ref_rms = math.sqrt(math.fsum(x*x for x in reference) / len(data))
        metrics = report['metrics']
        self.assertEqual(metrics['rows'], len(data))
        self.assertEqual(metrics['maximum_absolute_error'], max(map(abs, errors)))
        self.assertEqual(metrics['rms_error'], rms)
        self.assertEqual(metrics['reference_rms'], ref_rms)
        self.assertEqual(metrics['relative_rms_error'], rms / ref_rms)
        self.assertEqual(metrics['stage_mismatch_rows'], sum(r['reference_stage'] != r['production_stage'] for r in data))
        self.assertNotIn('pass', metrics)
        self.assertNotIn('tolerance', metrics)
        self.assertGreater(metrics['maximum_absolute_error'], 0)
        for path, metadata in report['artifacts'].items():
            actual = (self.out / path).read_bytes()
            self.assertEqual(metadata, {'bytes': len(actual), 'sha256': hashlib.sha256(actual).hexdigest()})
        self.assertEqual((self.out / 'production_executable').read_bytes(), Path(RENDERER).read_bytes())
        self.assertEqual(report['requested_parameters']['attack']['decimal'], '.0137')
        self.assertEqual(report['requested_parameters']['attack']['production_float'], f32(.0137))
        self.assertNotEqual(report['requested_parameters']['attack']['reference_binary64'], f32(.0137))

    def test_standalone_artifacts_are_byte_identical(self):
        self.render()
        standalone = self.root / 'standalone'
        args = self.args(out=standalone)[2:]  # Remove comparison-only --production.
        proc = subprocess.run([sys.executable, str(C.REF.__file__), *args], capture_output=True, timeout=20)
        self.assertEqual(proc.returncode, 0, proc.stderr.decode())
        for name in C.NAMES:
            self.assertEqual((standalone / name).read_bytes(), (self.out / 'reference' / name).read_bytes(), name)

    def test_zero_reference_rms_is_null_and_no_tick_at_zero(self):
        self.events.write_text('event,time_s,action\n')
        report = self.render(samples=2)
        self.assertEqual(report['metrics']['reference_rms'], 0)
        self.assertEqual(report['metrics']['rms_error'], 0)
        self.assertIsNone(report['metrics']['relative_rms_error'])
        data = rows((self.out / 'production.csv').read_bytes())
        self.assertEqual(data[0]['elapsed'], '0')
        self.assertEqual(float(data[1]['elapsed']), f32(1/8000))

    def test_actual_public_adsr_direct_oracle_all_rows(self):
        events = [(0, 'note_off'), (0, 'note_on'), (7, 'note_off'), (11, 'retrigger'),
                  (61, 'note_on'), (200, 'note_off'), (200, 'note_on'),
                  (400, 'note_on'), (400, 'note_off'), (600, 'reset'), (800, 'note_on')]
        _, actual = self.native(events)
        oracle = subprocess.run([str(ORACLE)], capture_output=True, timeout=20)
        self.assertEqual(oracle.returncode, 0)
        expected = list(csv.reader(io.StringIO(oracle.stdout.decode())))
        self.assertEqual(len(expected), len(actual))
        for values, row in zip(expected, actual):
            i, value, stage, elapsed = values
            self.assertEqual(row['sample'], i)
            self.assertEqual(float(row['envelope']), float(value))
            self.assertEqual(row['stage'], C.STAGES[int(stage)])
            self.assertEqual(float(row['elapsed']), float(elapsed))
        self.assertEqual(actual[800]['envelope'], '0')
        self.assertEqual(actual[800]['stage'], 'attack')
        self.assertEqual(actual[800]['elapsed'], '0')

    def test_analytic_attack_closed_form_and_specific_one_tick_delay(self):
        # .001 is rounded slightly UP as binary32, so its eighth 8kHz tick
        # remains Attack. This is a case-specific assertion, not a delay allowance.
        _, actual = self.native([(0, 'note_on')], a='.001', d='.01', s='.3', n=21)
        self.assertEqual(actual[0]['stage'], 'attack')
        self.assertEqual(float(actual[0]['envelope']), 0)
        self.assertEqual(actual[8]['stage'], 'attack')
        self.assertEqual(actual[9]['stage'], 'decay')
        self.assertEqual(float(actual[9]['envelope']), 1)
        self.assertEqual(float(actual[9]['elapsed']), 0)
        for i in range(1, 9):
            expected = 1.5 * (1 - 3 ** (-i / (8000 * f32(.001))))
            self.assertEqual(float(actual[i]['envelope']), f32(expected))
        expected = f32(.3) + (1-f32(.3)) * 1000 ** (-1 / (8000 * f32(.01)))
        self.assertEqual(float(actual[10]['envelope']), f32(expected))
        reference = C.REF.Envelope(Q('.001'), Q('.01'), Q('.3'), Q('.03'))
        reference.event(Q(0), 'note_on')
        self.assertEqual(reference.advance(Q(8, 8000)), 1)
        self.assertEqual(reference.stage, 'decay')
        self.assertLess(reference.advance(Q(9, 8000)), 1)

    def test_noninteger_attack_duration_discards_remainder(self):
        _, data = self.native([(0, 'note_on')], a='.00107', d='.01', s='.25', n=15)
        self.assertEqual(data[8]['stage'], 'attack')
        self.assertEqual(data[9]['stage'], 'decay')
        self.assertEqual(float(data[9]['envelope']), 1)
        ref = C.REF.Envelope(Q('.00107'), Q('.01'), Q('.25'), Q('.03'))
        ref.event(Q(0), 'note_on')
        expected = .25 + .75 * 1000 ** (-float(Q(9, 8000)-Q('.00107')) / .01)
        self.assertAlmostEqual(ref.advance(Q(9, 8000)), expected, delta=3e-16)
        self.assertLess(expected, 1)

    def test_decay_release_threshold_neighbors(self):
        for sustain in ('0', '.3', '.9994999', '.9995001', '1'):
            _, data = self.native([(0, 'note_on')], a='.001', d='.01', s=sustain, n=201)
            target = f32(float(sustain))
            threshold = f32(.0005)
            span = 1-target
            ticks = max(1, math.floor(8000*f32(.01)*math.log(span/threshold, 1000))+1) if span else 1
            self.assertEqual(data[9 + ticks]['stage'], 'sustain')
            self.assertEqual(float(data[9 + ticks]['envelope']), target)
            self.assertEqual(data[9 + ticks - 1]['stage'], 'decay')
        _, data = self.native([(0, 'note_on'), (200, 'note_off')], a='.001', d='.01', s='1', r='.01', n=401)
        ticks = math.floor(8000*f32(.01)*math.log(1/f32(.0002), 1000))+1
        self.assertEqual(data[200+ticks-1]['stage'], 'release')
        self.assertEqual(data[200+ticks]['stage'], 'idle')
        self.assertEqual(float(data[200+ticks]['envelope']), 0)
        for k in (1, 20, ticks-1):
            self.assertEqual(float(data[200+k]['envelope']), f32(1000**(-k/(8000*f32(.01)))))

    def test_event_order_ties_and_final_endpoint(self):
        _, off_on = self.native([(0, 'note_on'), (40, 'note_off'), (40, 'note_on')], n=41)
        _, on_off = self.native([(0, 'note_on'), (40, 'note_on'), (40, 'note_off')], n=41)
        self.assertEqual(off_on[-1]['stage'], 'attack')
        self.assertEqual(on_off[-1]['stage'], 'release')
        self.assertEqual(off_on[-1]['gate'], '1')
        self.assertEqual(on_off[-1]['gate'], '0')
        self.assertEqual(off_on[-1]['envelope'], on_off[-1]['envelope'])
        self.assertEqual(off_on[-1]['elapsed'], '0')
        _, zero = self.native([(0, 'note_on'), (0, 'note_off')], n=2)
        self.assertEqual(zero[0]['stage'], 'release')
        self.assertEqual(zero[1]['stage'], 'idle')
        _, reverse = self.native([(0, 'note_off'), (0, 'note_on')], n=2)
        self.assertEqual(reverse[0]['stage'], 'attack')
        self.assertGreater(float(reverse[1]['envelope']), 0)

    def test_release_retrigger_roundtrip_and_held_restart(self):
        _, data = self.native([(0, 'note_on'), (7, 'note_off'), (11, 'retrigger'), (61, 'note_on')])
        # noteOn uses the public float output as its new double origin. Compute
        # the next independent power-law segment from that rounded level.
        for start in (11, 61):
            origin = float(data[start]['envelope'])
            for offset in (1, 2, 7):
                expected = origin + 1.5*(1-origin)*(1-3**(-offset/(8000*f32(.0137))))
                self.assertEqual(float(data[start+offset]['envelope']), f32(expected))
        self.assertEqual(data[7]['stage'], 'release')
        self.assertEqual(data[11]['elapsed'], '0')
        self.assertGreater(float(data[11]['envelope']), 0)

    def test_fast_slow_bounds_rates_and_maximum_rows(self):
        for rate in (8000, 384000):
            for a, d, s, r in (('.001', '.01', '0', '.01'), ('1', '10', '1', '10')):
                _, data = self.native([(0, 'note_on'), (31, 'note_off'), (47, 'retrigger')],
                                      a=a, d=d, s=s, r=r, rate=rate, n=101)
                self.assertTrue(all(math.isfinite(float(row['envelope'])) for row in data))
        _, data = self.native([(0, 'note_on')], rate=10000, n=100001, a='1', d='10', s='0', r='10')
        self.assertEqual(len(data), 100001)
        self.assertEqual(float(data[-1]['time_s']), 10)
        self.assertGreater(float(data[-1]['envelope']), 0)

    def test_exact_alignment_rejects_subsamples_but_standalone_does_not(self):
        for time in ('.00001', '.0001250000000000000000000001', '.0001249999999999999999999999'):
            self.events.write_text(f'event,time_s,action\n0,{time},note_on\n')
            with self.assertRaisesRegex(ValueError, 'exactly endpoint-aligned'):
                self.render()
            self.assertFalse(self.out.exists())
            args = C.REF.parse(self.args()[2:])
            C.REF.generate(args, C.REF.parse_events(self.events.read_bytes(), Q(1,10)))

    def test_float_quantization_exact_decimal_halfway(self):
        # Binary64 parsing rounds this just-above-halfway decimal to the tie.
        token = '0.500000029802322387695312500000000000001'
        expected = float.fromhex('0x1.000002p-1')
        self.assertEqual(C.passed_float(Q(token)), expected)
        meta, _ = self.native([], s=token, n=2)
        self.assertEqual(meta['parameters']['sustain'], expected)
        meta, _ = self.native([], a='+.001', d='1.e-2', s='1e-40', r='+.01', n=2)
        self.assertEqual(meta['parameters']['sustain'], f32(1e-40))

    def test_maximum_events_and_duplicate_options(self):
        events = [(0, 'note_on' if i % 2 == 0 else 'note_off') for i in range(4096)]
        meta, data = self.native(events, n=2)
        self.assertEqual(meta['events'], 4096)
        self.assertEqual(data[0]['last_event'], '4095')
        self.assertEqual(data[0]['stage'], 'release')
        self.assertEqual(data[1]['stage'], 'idle')
        too_many = 'event,sample,action\n' + ''.join(f'{i},0,note_on\n' for i in range(4097))
        p = subprocess.run([str(RENDERER), '8000', '2', '.001', '.01', '0', '.01'],
                           input=too_many.encode(), capture_output=True, timeout=10)
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(p.stdout, b'')
        with self.assertRaisesRegex(ValueError, 'Duplicate option'):
            C.parse(self.args() + ['--production', str(RENDERER)])
        with self.assertRaisesRegex(ValueError, 'Duplicate option'):
            C.parse(self.args() + ['--attack=.01'])

    def test_child_timeout_and_both_output_budgets(self):
        with patch.object(C, 'TIMEOUT', .03), self.assertRaisesRegex(ValueError, 'timeout'):
            C.execute([sys.executable, '-c', 'import time; time.sleep(2)'], self.root)
        with patch.object(C, 'MAX_BYTES', 64), self.assertRaisesRegex(ValueError, 'byte budget'):
            C.execute([sys.executable, '-c', 'print("x" * 1000)'], self.root)
        with self.assertRaisesRegex(ValueError, 'byte budget'):
            C.execute([sys.executable, '-c', 'import sys; sys.stderr.write("x" * 20000)'], self.root)
        with self.assertRaisesRegex(ValueError, 'Child failed'):
            C.execute([sys.executable, '-c', 'print("partial"); raise SystemExit(1)'], self.root)

    def test_invalid_parameters_events_and_output_collisions(self):
        for key, value in (('attack', 'nan'), ('decay', 'inf'), ('sustain', '1.00000000000000001'),
                           ('release', '.00999999999999999'), ('rate', '7999'), ('samples', '100002')):
            with self.assertRaises(ValueError):
                self.render(**{key: value})
            self.assertFalse(self.out.exists())
        for data in (b'event,time_s,action\n0,NaN,note_on\n', b'event,time_s,action\n0,0,unknown\n',
                     b'event,time_s,action\n0,0,note_on', b'event,time_s,action\n0,.2,note_on\n'):
            self.events.write_bytes(data)
            with self.assertRaises(ValueError): self.render()
            self.assertFalse(self.out.exists())
        self.events.write_text('event,time_s,action\n')
        self.out.symlink_to(self.root / 'missing')
        with self.assertRaises(ValueError): self.render()
        self.assertTrue(self.out.is_symlink())

    def test_native_rejects_invalid_data_without_partial_stdout(self):
        good = ['8000', '2', '.001', '.01', '.3', '.01']
        for index, value in ((0, '08000'), (0, 'inf'), (1, '100002'), (2, 'nan'),
                             (2, '.00099999999999999999999'), (4, '1.00000000000000001'),
                             (4, '1e-300'), (5, '10.000000000000000001')):
            args = good.copy(); args[index] = value
            p = subprocess.run([str(RENDERER), *args], input=b'event,sample,action\n', capture_output=True, timeout=10)
            self.assertNotEqual(p.returncode, 0, args)
            self.assertEqual(p.stdout, b'')
        for data in (b'', b'event,sample,action', b'event,sample,action\n0,2,note_on\n',
                     b'event,sample,action\n0,0.5,note_on\n', b'event,sample,action\n0,0,unknown\n',
                     b'event,sample,action\n1,0,note_on\n', b'x' * (1024*1024+1)):
            p = subprocess.run([str(RENDERER), *good], input=data, capture_output=True, timeout=10)
            self.assertNotEqual(p.returncode, 0)
            self.assertEqual(p.stdout, b'')

    def test_stale_source_nonfinite_and_truncated_output_fail_closed(self):
        original = C.execute
        def tamper(command, directory, input_path=None):
            data = original(command, directory, input_path)
            if input_path is None: return data
            meta, trace = data.split(b'\n', 1)
            value = json.loads(meta)
            if mode == 'source': value['source_sha256']['src/core/Dsp.h'] = '0'*64
            if mode == 'input': value['input_sha256'] = '0'*64
            if mode == 'float': value['parameters']['attack'] = .5
            if mode == 'nonfinite': value['parameters']['attack'] = float('nan')
            if mode == 'trace': trace = trace.replace(b'0,0,0,', b'0,0,nan,', 1)
            if mode == 'partial': trace = trace[:-3]
            return json.dumps(value).encode() + b'\n' + trace
        for mode in ('source', 'input', 'float', 'nonfinite', 'trace', 'partial'):
            with patch.object(C, 'execute', tamper), self.assertRaises(ValueError): self.render()
            self.assertFalse(self.out.exists())

    def test_snapshot_inputs_execute_and_owned_failure_cleanup(self):
        original = C.execute
        expected = self.events.read_bytes()
        def mutate(command, directory, input_path=None):
            self.events.write_bytes(b'changed external input\n')
            return original(command, directory, input_path)
        with patch.object(C, 'execute', mutate): self.render()
        self.assertEqual((self.out / 'input_events.csv').read_bytes(), expected)
        self.out = self.root / 'failure'
        self.events.write_bytes(expected)
        write = C.Owned.write
        def fail(owned, path, data):
            write(owned, path, data)
            if path == self.out / 'production.csv': raise OSError('injected publication failure')
        with patch.object(C.Owned, 'write', fail), self.assertRaises(OSError): self.render()
        self.assertFalse(self.out.exists())

    def test_cleanup_preserves_foreign_and_replaced_inodes(self):
        write = C.Owned.write
        def fail(owned, path, data):
            write(owned, path, data)
            if path == self.out / 'production.csv':
                replacement = self.out / 'replacement'
                replacement.write_bytes(b'foreign replacement')
                os.replace(replacement, path)
                (self.out / 'foreign').write_bytes(b'unrelated')
                raise OSError('injected publication failure')
        with patch.object(C.Owned, 'write', fail), self.assertRaises(OSError): self.render()
        self.assertEqual((self.out / 'production.csv').read_bytes(), b'foreign replacement')
        self.assertEqual((self.out / 'foreign').read_bytes(), b'unrelated')
        self.assertFalse((self.out / 'manifest.json').exists())

    @unittest.skipUnless(os.name == 'posix', 'POSIX FIFO test')
    def test_fifo_input_rejected_without_blocking(self):
        self.events.unlink(); os.mkfifo(self.events)
        with self.assertRaises(ValueError): self.render()
        self.assertFalse(self.out.exists())


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--renderer', required=True, type=Path)
    parser.add_argument('--oracle', required=True, type=Path)
    args, remaining = parser.parse_known_args()
    RENDERER, ORACLE = args.renderer.resolve(), args.oracle.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
