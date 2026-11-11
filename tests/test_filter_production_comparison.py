#!/usr/bin/env python3
"""Actual-FilterEnv oracle, independent segment/clock controls, and strict bundle tests."""
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
SCRIPT = ROOT / 'scripts/compare_filter_envelope_production.py'
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
        self.events.write_text('event,time_s,action\n0,0,fresh_on\n1,.025,note_off\n2,.035,retained_retrigger\n3,.075,note_off\n')
        self.out = self.root / 'output'

    def args(self, **changes):
        values = dict(production=RENDERER, events=self.events, out=self.out, rate=8000, samples=801,
                      il='-.7', al='1.3', attack='.0137', decay='.02', release='.03')
        values.update(changes)
        return [str(v) for k, value in values.items() for v in ('--' + k, value)]

    def render(self, **changes):
        C.render(C.parse(self.args(**changes)))
        return json.loads((self.out / 'manifest.json').read_bytes())

    def native(self, events, il='-.7', al='1.3', a='.0137', d='.02', r='.03', rate=8000, n=801):
        data = ('event,sample,action\n' + ''.join(f'{i},{sample},{action}\n' for i, (sample, action) in enumerate(events))).encode()
        proc = subprocess.run([str(RENDERER), str(rate), str(n), il, al, a, d, r], input=data,
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

    def test_optional_mapping_is_unchanged_reference_only(self):
        mapping = {'hp-base-hz': '200', 'lp-base-hz': '1000', 'hp-depth': '.2', 'lp-depth': '-.3', 'q': '.7'}
        report = self.render(**mapping)
        standalone = self.root / 'mapped'
        proc = subprocess.run([sys.executable, str(C.REF.__file__), *self.args(out=standalone, **mapping)[2:]],
                              capture_output=True, timeout=20)
        self.assertEqual(proc.returncode, 0, proc.stderr.decode())
        for name in C.NAMES + C.OPTIONAL_NAMES:
            self.assertEqual((standalone / name).read_bytes(), (self.out / 'reference' / name).read_bytes())
        self.assertIn('reference/filter_controls.csv', report['artifacts'])
        self.assertNotIn('filter_controls.csv', report['artifacts'])
        self.out = self.root / 'invalid-mapped'
        with self.assertRaises(ValueError):
            self.render(**{**mapping, 'hp-depth': '8'})
        self.assertFalse(self.out.exists())

    def test_reference_artifacts_and_bundle_budget_fail_closed(self):
        original = C.execute
        def tamper(command, directory, input_path=None):
            result = original(command, directory, input_path)
            if input_path is None:
                path = directory / 'reference' / 'events.json'
                path.write_bytes(b'[]')
            return result
        with patch.object(C, 'execute', tamper), self.assertRaisesRegex(ValueError, 'hash mismatch'):
            self.render()
        self.assertFalse(self.out.exists())
        # Each source/trace is admitted, but aggregate published copies exceed it.
        bound = Path(RENDERER).stat().st_size + 110000
        with patch.object(C, 'MAX_BYTES', bound), self.assertRaisesRegex(ValueError, 'bundle exceeds'):
            self.render()
        self.assertFalse(self.out.exists())
        for data in (b'{"x":1,"x":2}', b'{"x":NaN}', b'{"x":1e999}'):
            with self.assertRaises(ValueError): C.read_json(data)

    def test_zero_reference_rms_is_null_and_no_tick_at_zero(self):
        self.events.write_text('event,time_s,action\n')
        report = self.render(samples=2)
        self.assertEqual(report['metrics']['reference_rms'], 0)
        self.assertEqual(report['metrics']['rms_error'], 0)
        self.assertIsNone(report['metrics']['relative_rms_error'])
        data = rows((self.out / 'production.csv').read_bytes())
        self.assertEqual(data[0]['elapsed'], '0')
        self.assertEqual(float(data[1]['elapsed']), f32(1/8000))

    def test_actual_public_filter_direct_oracle_all_rows(self):
        events = [(0, 'note_off'), (0, 'fresh_on'), (7, 'note_off'), (11, 'retained_retrigger'),
                  (61, 'fresh_on'), (200, 'note_off'), (200, 'retained_retrigger'),
                  (400, 'fresh_on'), (400, 'note_off'), (600, 'reset'), (800, 'fresh_on')]
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
        self.assertEqual(float(actual[800]['envelope']), f32(-.7))
        self.assertEqual(actual[800]['stage'], 'attack')
        self.assertEqual(actual[800]['elapsed'], '0')

    def test_signed_attack_closed_forms_and_specific_clock_difference(self):
        for il, al in (('-2', '3'), ('3', '-2'), ('-1', '-3'), ('0', '-8'), ('8', '0')):
            _, actual = self.native([(0, 'fresh_on')], il=il, al=al, a='.001', d='.01', n=21)
            origin, target = f32(float(il)), f32(float(al))
            self.assertEqual(actual[0]['stage'], 'attack')
            self.assertEqual(float(actual[0]['envelope']), origin)
            self.assertEqual(actual[8]['stage'], 'attack')
            self.assertEqual(actual[9]['stage'], 'decay')
            self.assertEqual(float(actual[9]['envelope']), target)
            self.assertEqual(float(actual[9]['elapsed']), 0)
            for i in range(1, 9):
                expected = origin + 1.5 * (target-origin) * (1 - 3 ** (-i/(8000*f32(.001))))
                self.assertAlmostEqual(float(actual[i]['envelope']), f32(expected), delta=5e-7)
            self.assertEqual(float(actual[10]['envelope']), f32(target * 1000**(-1/(8000*f32(.01)))))
        reference = C.REF.Envelope(Q('-2'), Q('3'), Q('.001'), Q('.01'), Q('.03'))
        reference.event(Q(0), 'fresh_on')
        self.assertEqual(reference.advance(Q(8,8000)), 3)
        self.assertEqual(reference.stage, 'decay')
        self.assertAlmostEqual(reference.advance(Q(9,8000)), 3*1000**(-.000125/.01), delta=1e-15)

    def test_noninteger_attack_duration_discards_remainder(self):
        _, actual = self.native([(0, 'fresh_on')], il='-1', al='2', a='.00107', d='.01', n=15)
        self.assertEqual(actual[8]['stage'], 'attack')
        self.assertEqual(actual[9]['stage'], 'decay')
        self.assertEqual(float(actual[9]['envelope']), 2)
        ref = C.REF.Envelope(Q('-1'), Q('2'), Q('.00107'), Q('.01'), Q('.03'))
        ref.event(Q(0), 'fresh_on')
        expected = 2 * 1000 ** (-float(Q(9,8000)-Q('.00107'))/.01)
        self.assertAlmostEqual(ref.advance(Q(9,8000)), expected, delta=5e-16)
        self.assertLess(expected, 2)

    def test_off_during_attack_release_and_completed_idle(self):
        _, data = self.native([(0, 'fresh_on'), (7, 'note_off'), (200, 'note_off'), (210, 'reset')],
                              il='-2', al='3', a='.0137', r='.01', n=220)
        origin = -2 + 7.5*(1-3**(-7/(8000*f32(.0137))))
        self.assertEqual(data[7]['stage'], 'release')
        self.assertAlmostEqual(float(data[7]['envelope']), f32(origin), delta=1e-7)
        ticks = math.floor(8000*f32(.01)*math.log(abs(origin+2)/f32(.0008),1000))+1
        for k in (1, 10, ticks-1):
            self.assertEqual(float(data[7+k]['envelope']), f32(-2+(origin+2)*1000**(-k/(8000*f32(.01)))))
        self.assertEqual(data[7+ticks-1]['stage'], 'release')
        self.assertEqual(data[7+ticks]['stage'], 'idle')
        self.assertEqual(float(data[7+ticks]['envelope']), -2)
        self.assertEqual(float(data[200]['envelope']), -2)
        self.assertGreater(float(data[200]['elapsed']), 0)  # off on Idle is no-op
        self.assertEqual(float(data[210]['envelope']), 0)
        self.assertEqual(data[210]['elapsed'], '0')
        self.assertTrue(all(row['stage']=='idle' and float(row['envelope'])==0 for row in data[210:]))

    def test_threshold_neighbor_decay_and_release_crossings(self):
        # Independent inverse exponential yields the one-tick crossing. Choose
        # adjacent binary32 levels bracketing it, not a fitted output tolerance.
        boundary = f32(.0008) * 1000**(1/(384000*f32(10)))
        bits = struct.unpack('I', struct.pack('f', boundary))[0]
        candidates = [struct.unpack('f', struct.pack('I', n))[0] for n in (bits-1,bits,bits+1)]
        low = max(x for x in candidates if x < boundary)
        high = min(x for x in candidates if x > boundary)
        for sign in (-1, 1):
            for level, expected in ((low, 'sustain'), (high, 'decay')):
                token = repr(sign*level)
                _, data = self.native([(0,'fresh_on')], il=token, al=token, d='10', rate=384000, n=4)
                self.assertEqual(data[1]['stage'], 'decay')
                self.assertEqual(data[2]['stage'], expected)
            for level, expected in ((low,'idle'), (high,'release')):
                # A zero-origin retained attack followed by off at t=0 gives
                # exactly the signed IL span without an earlier tick.
                _, data = self.native([(0,'retained_retrigger'), (0,'note_off')], il=repr(sign*level), al='0',
                                      r='10', rate=384000, n=2)
                self.assertEqual(data[0]['stage'], 'release')
                self.assertEqual(data[1]['stage'], expected)

    def test_equal_targets_reference_immediate_but_no_extra_production_tick(self):
        for value in ('0','-2','3'):
            _, actual = self.native([(0,'fresh_on')], il=value, al=value, n=3)
            self.assertEqual(actual[0]['stage'], 'attack')
            self.assertEqual(float(actual[0]['envelope']), float(value))
            self.assertEqual(actual[0]['elapsed'], '0')
            self.assertEqual(actual[1]['stage'], 'decay')
            ref = C.REF.Envelope(Q(value),Q(value),Q('.0137'),Q('.02'),Q('.03'))
            ref.event(Q(0), 'fresh_on')
            self.assertEqual(ref.stage, 'sustain' if value=='0' else 'decay')
        self.events.write_text('event,time_s,action\n0,0,fresh_on\n')
        report = self.render(il='0',al='0',samples=3)
        self.assertEqual(report['metrics']['stage_mismatch_rows'], 2)
        self.assertEqual(report['metrics']['maximum_absolute_error'], 0)

    def test_retained_retrigger_preserves_private_double_and_fresh_resets(self):
        # Closed forms, not a copy of the tick recurrence. A retrigger at row7
        # retains private binary64 state; public f32 roundtrip is detectably wrong.
        _, data = self.native([(0,'fresh_on'),(7,'retained_retrigger')], n=100)
        il, al, a = f32(-.7), f32(1.3), f32(.0137)
        origin = il+1.5*(al-il)*(1-3**(-7/(8000*a)))
        distinguish = 0
        for k in range(1,90):
            factor = 1.5*(1-3**(-k/(8000*a)))
            expected = f32(origin+(al-origin)*factor)
            wrong = f32(f32(origin)+(al-f32(origin))*factor)
            self.assertAlmostEqual(float(data[7+k]['envelope']), expected, delta=3e-8)
            distinguish += expected != wrong and float(data[7+k]['envelope']) == expected
        self.assertGreater(distinguish, 0)
        _, fresh = self.native([(0,'fresh_on'),(7,'fresh_on')], n=9)
        self.assertEqual(float(fresh[7]['envelope']), il)
        self.assertNotEqual(fresh[7]['envelope'], data[7]['envelope'])
        # Retained from reset starts at 0, not IL; below/equal/above current.
        for al in ('-2','0','3'):
            _, retained = self.native([(0,'retained_retrigger')], il='-1', al=al, n=2)
            self.assertEqual(float(retained[0]['envelope']), 0)
            self.assertEqual(retained[0]['stage'], 'attack')
            self.assertEqual(retained[1]['stage'], 'decay' if al=='0' else 'attack')

    def test_event_order_ties_repeated_off_and_final_endpoint(self):
        _, off_on = self.native([(0,'fresh_on'),(40,'note_off'),(40,'retained_retrigger')],n=41)
        _, on_off = self.native([(0,'fresh_on'),(40,'retained_retrigger'),(40,'note_off')],n=41)
        self.assertEqual(off_on[-1]['stage'],'attack'); self.assertEqual(on_off[-1]['stage'],'release')
        self.assertEqual(off_on[-1]['gate'],'1'); self.assertEqual(on_off[-1]['gate'],'0')
        self.assertEqual(off_on[-1]['envelope'],on_off[-1]['envelope'])
        self.assertEqual(off_on[-1]['elapsed'],'0')
        _, zero = self.native([(0,'note_off'),(0,'fresh_on'),(0,'note_off')],n=2)
        self.assertEqual(zero[0]['stage'],'release'); self.assertEqual(zero[1]['stage'],'idle')
        self.assertEqual(float(zero[1]['envelope']), f32(-.7))
        _, repeated = self.native([(0,'fresh_on'),(7,'note_off'),(9,'note_off')],n=11)
        self.assertEqual(repeated[9]['elapsed'],'0')
        self.assertEqual(repeated[9]['stage'],'release')

    def test_fast_slow_signed_bounds_rates_and_maximum_rows(self):
        for rate in (8000,384000):
            for il,al,a,d,r in (('-8','8','.001','.01','.01'),('8','-8','1','10','10')):
                _, data=self.native([(0,'fresh_on'),(31,'note_off'),(47,'retained_retrigger')],
                                    il=il,al=al,a=a,d=d,r=r,rate=rate,n=101)
                self.assertTrue(all(-8<=float(row['envelope'])<=8 for row in data))
        _,data=self.native([(0,'fresh_on')],rate=10000,n=100001,a='1',d='10',r='10')
        self.assertEqual(len(data),100001); self.assertEqual(float(data[-1]['time_s']),10)

    def test_exact_alignment_rejects_subsamples_but_standalone_does_not(self):
        for time in ('.00001', '.0001250000000000000000000001', '.0001249999999999999999999999'):
            self.events.write_text(f'event,time_s,action\n0,{time},fresh_on\n')
            with self.assertRaisesRegex(ValueError, 'exactly endpoint-aligned'):
                self.render()
            self.assertFalse(self.out.exists())
            args = C.REF.parse(self.args()[2:])
            C.REF.generate(args, C.REF.parse_events(self.events.read_bytes(), Q(1,10)))

    def test_signed_float_quantization_exact_decimal_halfway(self):
        token = '0.500000029802322387695312500000000000001'
        expected = float.fromhex('0x1.000002p-1')
        for sign in ('','-'):
            requested = Q(sign+token)
            value = -expected if sign else expected
            self.assertEqual(C.passed_float(requested),value)
            meta,_=self.native([],il=sign+token,al=sign+token,n=2)
            self.assertEqual(meta['parameters']['il'],value)
            self.assertEqual(meta['parameters']['al'],value)
        meta,_=self.native([],il='-1e-40',al='+1e-40',a='+.001',d='1.e-2',r='+.01',n=2)
        self.assertEqual(meta['parameters']['il'],f32(-1e-40))
        self.assertEqual(meta['parameters']['al'],f32(1e-40))

    def test_maximum_events_and_duplicate_options(self):
        events = [(0, 'fresh_on' if i % 2 == 0 else 'note_off') for i in range(4096)]
        meta, data = self.native(events, n=2)
        self.assertEqual(meta['events'], 4096)
        self.assertEqual(data[0]['last_event'], '4095')
        self.assertEqual(data[0]['stage'], 'release')
        self.assertEqual(data[1]['stage'], 'idle')
        too_many = 'event,sample,action\n' + ''.join(f'{i},0,fresh_on\n' for i in range(4097))
        p = subprocess.run([str(RENDERER), '8000', '2', '-.7', '1.3', '.001', '.01', '.01'],
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
        for key, value in (('attack', 'nan'), ('decay', 'inf'), ('al', '8.00000000000000001'),
                           ('release', '.00999999999999999'), ('rate', '7999'), ('samples', '100002')):
            with self.assertRaises(ValueError):
                self.render(**{key: value})
            self.assertFalse(self.out.exists())
        for data in (b'event,time_s,action\n0,NaN,fresh_on\n', b'event,time_s,action\n0,0,unknown\n',
                     b'event,time_s,action\n0,0,fresh_on', b'event,time_s,action\n0,.2,fresh_on\n'):
            self.events.write_bytes(data)
            with self.assertRaises(ValueError): self.render()
            self.assertFalse(self.out.exists())
        self.events.write_text('event,time_s,action\n')
        self.out.symlink_to(self.root / 'missing')
        with self.assertRaises(ValueError): self.render()
        self.assertTrue(self.out.is_symlink())

    def test_native_rejects_invalid_data_without_partial_stdout(self):
        good = ['8000', '2', '-.7', '1.3', '.001', '.01', '.01']
        for index, value in ((0, '08000'), (0, 'inf'), (1, '100002'), (2, 'nan'),
                             (4, '.00099999999999999999999'), (2, '-8.00000000000000001'),
                             (3, '8.00000000000000001'), (3, '1e-300'), (2, '-1e-300'),
                             (6, '10.000000000000000001')):
            args = good.copy(); args[index] = value
            p = subprocess.run([str(RENDERER), *args], input=b'event,sample,action\n', capture_output=True, timeout=10)
            self.assertNotEqual(p.returncode, 0, args)
            self.assertEqual(p.stdout, b'')
        for data in (b'', b'event,sample,action', b'event,sample,action\n0,2,fresh_on\n',
                     b'event,sample,action\n0,0.5,fresh_on\n', b'event,sample,action\n0,0,unknown\n',
                     b'event,sample,action\n1,0,fresh_on\n', b'x' * (1024*1024+1)):
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
            if mode == 'decimal': value['source_sha256']['src/tools/DecimalConversion.h'] = '0'*64
            if mode == 'input': value['input_sha256'] = '0'*64
            if mode == 'float': value['parameters']['attack'] = .5
            if mode == 'nonfinite': value['parameters']['attack'] = float('nan')
            if mode == 'trace': trace = trace.replace(b'0,0,-0.69999998807907104,', b'0,0,nan,', 1)
            if mode == 'partial': trace = trace[:-3]
            return json.dumps(value).encode() + b'\n' + trace
        for mode in ('source', 'decimal', 'input', 'float', 'nonfinite', 'trace', 'partial'):
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
