#!/usr/bin/env python3
"""Independent signed filter-EG closed forms, IO safety, and dynamic-chain tests."""
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
SCRIPT = ROOT / 'scripts/render_filter_envelope_reference.py'
SPEC = importlib.util.spec_from_file_location('filter_reference', SCRIPT)
REF = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REF)
FILTER = VCA = None
EPS = Q(13743895, 17179869184)  # Exact independent binary32 encoding of 0.0008f.


def attack(start, peak, elapsed, duration):
    return start + 1.5 * (peak - start) * (1 - 3 ** (-float(elapsed) / float(duration)))


def settle(start, target, elapsed, duration):
    return target + (start - target) * 1000 ** (-float(elapsed) / float(duration))


def rows(data):
    return list(csv.DictReader(io.StringIO(data.decode('ascii'))))


class AnalyticTests(unittest.TestCase):
    def env(self, il='-1', al='1', a='.1', d='1', r='1'):
        return REF.Envelope(*map(Q, (il, al, a, d, r)))

    def test_idle_fresh_jump_retained_on_and_reset(self):
        env = self.env()
        self.assertEqual(env.advance(Q(1)), 0)
        self.assertEqual(env.event(Q(1), 'note_off')['after']['stage'], 'idle')
        onset = env.event(Q(1), 'fresh_on')
        self.assertEqual(onset['before']['envelope'], 0)
        self.assertEqual(onset['after_action']['envelope'], -1)
        self.assertEqual(onset['after']['envelope'], -1)
        self.assertTrue(env.gate)
        retained = env.event(Q('1.03'), 'retained_retrigger')
        self.assertEqual(retained['before']['envelope'], retained['after']['envelope'])
        fresh = env.event(Q('1.04'), 'fresh_on')
        self.assertNotEqual(fresh['before']['envelope'], -1)
        self.assertEqual(fresh['after']['envelope'], -1)
        reset = env.event(Q('1.05'), 'reset')
        self.assertEqual(reset['after']['envelope'], 0)
        self.assertFalse(env.gate)
        self.assertEqual(env.stage, 'idle')
        env = self.env()
        self.assertEqual(env.event(Q(0), 'retained_retrigger')['after']['envelope'], 0)

    def test_bidirectional_attack_duration_curvature_and_negative_levels(self):
        for start, peak in ((-2, 3), (3, -2), (-3, -1), (-1, -3), (0, -3), (-8, 8), (8, -8)):
            for duration in ('.001', '.3333333', '1'):
                env = self.env(il=str(start), al=str(peak), a=duration)
                env.event(Q(0), 'fresh_on')
                values = []
                for p in (Q(0), Q(1, 4), Q(1, 2), Q(3, 4), Q(999, 1000)):
                    value = env.advance(Q(duration) * p)
                    self.assertAlmostEqual(value, attack(start, peak, p * Q(duration), Q(duration)), delta=4e-15)
                    self.assertTrue(min(start, peak) <= value <= max(start, peak))
                    self.assertEqual(env.stage, 'attack')
                    values.append(value)
                self.assertGreater(abs(values[1] - values[0]), abs(values[2] - values[1]))
                self.assertEqual(env.advance(Q(duration)), peak)
                self.assertEqual(env.stage, 'decay')

    def test_reused_attack_above_below_equal_target_and_boundary_neighbors(self):
        for start, peak in ((-3, 2), (3, 2), (-1, -2), (-3, -2), (0, 0), (2, 2), (-2, -2),
                            (math.nextafter(-2, -math.inf), -2), (math.nextafter(-2, math.inf), -2),
                            (math.nextafter(2, -math.inf), 2), (math.nextafter(2, math.inf), 2)):
            for duration in ('.001', '.01', '1'):
                env = self.env(al=str(peak), a=duration)
                onset = Q('.00002')
                env.begin('idle', onset, start)
                audit = env.event(onset, 'retained_retrigger')
                if start == peak:
                    self.assertEqual(audit['after']['stage'], 'sustain' if peak == 0 else 'decay')
                    continue
                end = onset + Q(duration)
                probes = sorted(set([Q(math.nextafter(float(end), 0)),
                                     end - Q(1, 10**20), end - Q(1, 10**100)]))
                for t in probes:
                    if t < end:
                        self.assertTrue(min(start, peak) <= env.advance(t) <= max(start, peak))
                        self.assertEqual(env.stage, 'attack')
                self.assertEqual(env.advance(end), peak)
                self.assertEqual(env.stage, 'decay')
                self.assertEqual(env.advance(end + Q(1, 10**100)), peak)
                self.assertEqual(env.stage, 'decay')
        # Piecewise numerically stable forms agree with the separate power oracle.
        for start, peak in ((3, -2), (-2, 3), (-3, -1), (-1, -3)):
            env = self.env(il=str(start), al=str(peak), a='.01')
            env.event(Q(0), 'fresh_on')
            for t in (Q('.00499999999999999999'), Q('.005'), Q('.00500000000000000001')):
                self.assertAlmostEqual(env.advance(t), attack(start, peak, t, '.01'), delta=2e-15)

    def test_degenerate_levels_grounded_sustain_and_immediate_audit(self):
        for level in (-2, 0, 2):
            env = self.env(il=str(level), al=str(level))
            on = env.event(Q(0), 'fresh_on')
            self.assertEqual(on['after_action']['envelope'], level)
            self.assertEqual(on['after_action']['stage'], 'attack')
            self.assertEqual(on['after']['stage'], 'sustain' if level == 0 else 'decay')
            self.assertEqual(env.advance(Q(9)), 0)
            self.assertEqual(env.stage, 'sustain')
            self.assertTrue(env.gate)
            env.event(Q(9), 'note_off')
            self.assertEqual(env.advance(Q(12)), level)
            self.assertEqual(env.stage, 'idle')
            idle = env.event(Q(12), 'note_off')
            self.assertEqual(idle['before'], idle['after'])
        env = self.env(il='-1', al='0')
        env.event(Q(0), 'fresh_on')
        self.assertEqual(env.advance(Q('.1')), 0)
        self.assertEqual(env.stage, 'sustain')
        self.assertEqual([t['time'] for t in env.transitions], [REF.io.exact_time(Q('.1'))] * 2)
        env = self.env(il='.0001', al='.0001')
        on = env.event(Q(0), 'fresh_on')
        self.assertEqual(on['after_action']['envelope'], .0001)
        self.assertEqual(on['after']['envelope'], 0)
        self.assertEqual(env.transitions[-1]['before_value'], .0001)

    def test_minus_60_db_signed_settle_and_fractional_remainder(self):
        for time in ('.01', '.7', '10'):
            for peak in (-3, 3):
                env = self.env(il='0', al=str(peak), a='.013', d=time)
                env.event(Q(0), 'fresh_on')
                self.assertAlmostEqual(env.advance(Q('.013') + Q(time)), peak * .001, delta=3e-18)
                env = self.env(il=str(-peak), al=str(peak), r=time)
                env.begin('sustain', Q(0), 0)
                env.event(Q(0), 'note_off')
                self.assertAlmostEqual(env.advance(Q(time)), -peak + peak * .001, delta=1e-15)
        env = self.env(il='-1', al='2', a='.001', d='.01')
        env.event(Q(0), 'fresh_on')
        self.assertAlmostEqual(env.advance(Q('.001125')), settle(2, 0, .000125, .01), delta=4e-16)
        self.assertEqual(env.stage, 'decay')
        self.assertEqual(env.advance(Q(1)), 0)
        self.assertEqual(env.stage, 'sustain')

    def test_float_threshold_and_signed_continuous_boundary_snap(self):
        self.assertEqual(Q(REF.SETTLE_EPSILON), EPS)
        self.assertNotEqual(REF.SETTLE_EPSILON, .0008)
        for peak in (-2, 2):
            env = self.env(il=str(-peak), al=str(peak), a='.1', d='.7', r='.9')
            env.event(Q(0), 'fresh_on')
            env.advance(Q('.1'))
            boundary = env.end
            self.assertAlmostEqual(float(boundary - Q('.1')), .7 * math.log(2 / float(EPS), 1000), delta=2e-16)
            self.assertGreater(abs(env.advance(boundary - Q(1, 10**10))), float(EPS))
            for t in (boundary - Q(1, 10**20), boundary - Q(1, 10**100)):
                self.assertTrue(math.isfinite(env.advance(t)))
                self.assertEqual(env.stage, 'decay')
            self.assertEqual(env.advance(boundary), 0)
            self.assertEqual(env.stage, 'sustain')
            self.assertEqual(env.transitions[-1]['before_value'], math.copysign(float(EPS), peak))
            env.event(boundary, 'note_off')
            end = env.end
            self.assertAlmostEqual(float(end - boundary), .9 * math.log(2 / float(EPS), 1000), delta=3e-16)
            self.assertEqual(env.advance(end), -peak)
            self.assertEqual(env.stage, 'idle')
            self.assertEqual(env.transitions[-1]['before_value'], -peak + math.copysign(float(EPS), peak))
        # Immediate equality/below versus a positive duration one float above.
        for sign in (-1, 1):
            for magnitude in (math.nextafter(float(EPS), 0), float(EPS), math.nextafter(float(EPS), math.inf)):
                env = self.env(il='0')
                env.begin('decay', Q(0), sign * magnitude)
                boundary = env.end
                self.assertEqual(boundary > 0, magnitude > float(EPS))
                if boundary:
                    env.advance(boundary / 2)
                    self.assertEqual(env.stage, 'decay')
                self.assertEqual(env.advance(boundary), 0)
                self.assertEqual(env.stage, 'sustain')

    def test_off_attack_and_retrigger_release_with_equal_time_events(self):
        env = self.env(il='-1', al='2', a='.1', r='.2')
        env.event(Q(0), 'fresh_on')
        first = attack(-1, 2, .025, .1)
        off = env.event(Q('.025'), 'note_off')
        self.assertAlmostEqual(off['before']['envelope'], first, delta=5e-16)
        self.assertEqual(off['after']['envelope'], off['before']['envelope'])
        start = settle(first, -1, .0125, .2)
        on = env.event(Q('.0375'), 'retained_retrigger')
        self.assertAlmostEqual(on['after']['envelope'], start, delta=5e-16)
        self.assertAlmostEqual(env.advance(Q('.0875')), attack(start, 2, .05, .1), delta=6e-16)
        self.assertEqual(env.advance(Q('.1375')), 2)
        off = env.event(Q('.1375'), 'note_off')
        self.assertEqual(off['before']['stage'], 'decay')
        self.assertEqual(off['before']['envelope'], 2)
        self.assertEqual(env.event(Q('.1375'), 'retained_retrigger')['after']['stage'], 'decay')
        self.assertEqual(env.event(Q('.1375'), 'reset')['after']['envelope'], 0)
        env.event(Q('.1375'), 'fresh_on')
        self.assertEqual(env.event(Q('.1375'), 'note_off')['after']['stage'], 'idle')
        self.assertEqual(env.state()['envelope'], -1)

    def test_sample_queries_and_rate_do_not_change_trajectory(self):
        events = [(Q(0), 'fresh_on'), (Q('.0170123'), 'note_off'),
                  (Q('.0190037'), 'retained_retrigger'), (Q('.031'), 'note_off')]
        results = []
        for rate in (8000, 16000, 48000, 192000, 384000):
            env, cursor, common = self.env(a='.01', d='.1', r='.2'), 0, []
            for i in range(rate // 20 + 1):
                t = Q(i, rate)
                while cursor < len(events) and events[cursor][0] <= t:
                    env.event(*events[cursor]); cursor += 1
                value = env.advance(t)
                if i % (rate // 8000) == 0:
                    common.append((value, env.stage, env.gate))
            results.append((common, env.transitions))
        self.assertTrue(all(result == results[0] for result in results))


class CliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.input, self.out = self.root / 'events.csv', self.root / 'result'
        self.input.write_text('event,time_s,action\n0,0,fresh_on\n1,.06,note_off\n'
                              '2,.065,retained_retrigger\n3,.08,note_off\n')

    def args(self, mapping=False, **options):
        values = {'events': self.input, 'out': self.out, 'rate': '16000', 'samples': '1601',
                  'attack': '.01', 'decay': '.1', 'il': '-1', 'al': '2', 'release': '.02'}
        if mapping:
            values.update({'hp-base-hz': '200', 'lp-base-hz': '2000', 'hp-depth': '.25', 'lp-depth': '-.5', 'q': '1'})
        values.update(options)
        return [str(item) for key, value in values.items() for item in ('--' + key, value)]

    def run_cli(self, success=True, **options):
        proc = subprocess.run([sys.executable, str(SCRIPT), *self.args(**options)], capture_output=True, timeout=30)
        self.assertEqual(proc.returncode == 0, success, proc.stderr.decode())
        return proc

    def test_exports_hashes_helper_replay_and_mapping(self):
        self.run_cli(mapping=True)
        manifest = json.loads((self.out / 'manifest.json').read_bytes())
        self.assertTrue(manifest['complete'])
        self.assertEqual(manifest['event_count'], 4)
        self.assertEqual(manifest['snap_policy']['absolute_threshold'], float(EPS))
        self.assertEqual((self.out / 'input_events.csv').read_bytes(), self.input.read_bytes())
        self.assertEqual((self.out / 'renderer.py').read_bytes(), SCRIPT.read_bytes())
        self.assertEqual((self.out / REF.HELPER.name).read_bytes(), REF.HELPER.read_bytes())
        for name, meta in manifest['artifacts'].items():
            data = (self.out / name).read_bytes()
            self.assertEqual(meta, {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        envelope = rows((self.out / 'envelope.csv').read_bytes())
        controls = rows((self.out / 'filter_controls.csv').read_bytes())
        self.assertEqual(len(envelope), 1601)
        self.assertEqual(envelope[0]['envelope'], '-1')
        for i, (e, c) in enumerate(zip(envelope, controls)):
            self.assertEqual(e['sample'], c['sample'])
            self.assertEqual(e['sample'], str(i))
            self.assertEqual(float(e['time_s']), i / 16000)
            v = float(e['envelope'])
            self.assertEqual(float(c['hp_hz']), 200 * 2 ** (.25 * v))
            self.assertEqual(float(c['lp_hz']), 2000 * 2 ** (-.5 * v))
            self.assertEqual(float(c['q']), 1)
        replay = self.root / 'replay'
        proc = subprocess.run([sys.executable, str(self.out / 'renderer.py'), *self.args(mapping=True, out=replay)], capture_output=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual({p.name: p.read_bytes() for p in self.out.iterdir()},
                         {p.name: p.read_bytes() for p in replay.iterdir()})

    def test_endpoint_ties_subsample_and_final_event(self):
        self.input.write_text('event,time_s,action\n0,.000062499999999999999,fresh_on\n'
                              '1,.0000625,note_off\n2,.0000625,retained_retrigger\n'
                              '3,.000062500000000000001,reset\n4,.1,fresh_on\n')
        self.run_cli()
        samples = rows((self.out / 'envelope.csv').read_bytes())
        self.assertEqual(samples[0]['stage'], 'idle')
        self.assertEqual(samples[1]['last_event'], '2')
        self.assertEqual(samples[1]['stage'], 'attack')
        self.assertEqual(samples[2]['stage'], 'idle')
        self.assertEqual(samples[-1]['last_event'], '4')
        self.assertEqual(samples[-1]['envelope'], '-1')
        audit = json.loads((self.out / 'events.json').read_bytes())
        self.assertEqual(audit[1]['before']['stage'], 'attack')
        self.assertEqual(audit[1]['after']['envelope'], -1)
        self.assertEqual(audit[2]['before'], audit[1]['after'])

    def test_empty_events_crlf_and_dense_events(self):
        self.input.write_bytes(b'event,time_s,action\r\n')
        self.run_cli()
        self.assertFalse((self.out / 'filter_controls.csv').exists())
        self.assertIsNone(json.loads((self.out / 'manifest.json').read_bytes())['control_export'])
        self.assertTrue(all(r['envelope'] == '0' for r in rows((self.out / 'envelope.csv').read_bytes())))
        self.input.write_bytes(('event,time_s,action\r\n' + ''.join(
            f'{i},0,{"fresh_on" if i % 2 == 0 else "reset"}\r\n' for i in range(REF.MAX_EVENTS))).encode())
        self.run_cli(out=self.root / 'dense')
        self.assertEqual(len(json.loads((self.root / 'dense/events.json').read_bytes())), REF.MAX_EVENTS)

    def test_malformed_and_oversized_event_input(self):
        header = b'event,time_s,action\n'
        cases = [b'', b'event,time_s,action', b'event,time,action\n', header + b'\n',
                 header + b'00,0,fresh_on\n', header + b'1,0,fresh_on\n', header + b'0,-.1,fresh_on\n',
                 header + b'0,.100000001,fresh_on\n', header + b'0,0,note_on\n',
                 header + b'0,0,fresh_on,extra\n', header + b'0, 0,fresh_on\n', header + b'0,"0",fresh_on\n',
                 header + b'0,0,fresh_on\r', header + b'0,\xff,fresh_on\n',
                 header + b'0,0,fresh_on\n2,0,reset\n', header + b'0,.05,fresh_on\n1,.04,reset\n']
        cases += [header + f'0,{value},fresh_on\n'.encode() for value in (
            'nan', 'inf', '1_0', '0x0', '1e999', '1e-999', '0e1000000000', '1e-301', '.', '+')]
        cases += [header + b'0,' + b'0' * 129 + b',fresh_on\n', b'x' * (REF.MAX_INPUT_BYTES + 1)]
        cases += [header + b''.join(f'{i},0,reset\n'.encode() for i in range(REF.MAX_EVENTS + 1))]
        for data in cases:
            with self.subTest(data=data[:90]):
                self.input.write_bytes(data)
                self.run_cli(False)
                self.assertFalse(self.out.exists())

    def test_bad_arguments_budgets_and_partial_or_out_of_range_mapping(self):
        cases = {'attack': ('0', '.00099999', '1.00001'), 'decay': ('.0099', '10.01'),
                 'release': ('-.1', '.009', '10.01'), 'il': ('-8.0001', '8.0001'), 'al': ('-8.1', '8.1'),
                 'rate': ('7999', '384001', '08000', '8000.0'), 'samples': ('1', '100002', '1e2')}
        for key, values in cases.items():
            for value in values:
                self.run_cli(False, **{key: value})
        for key, values in {'hp-base-hz': ('24.9999', '20000.01'), 'lp-base-hz': ('0', '1e300'),
                            'hp-depth': ('8.01', '-8.01'), 'lp-depth': ('nan', '1e300'),
                            'q': ('.34999999999', '9.0001')}.items():
            for value in values:
                self.run_cli(False, mapping=True, **{key: value})
        self.run_cli(False, **{'hp-base-hz': '200'})
        self.run_cli(False, mapping=True, **{'hp-base-hz': '25', 'hp-depth': '1'})
        self.run_cli(False, mapping=True, **{'lp-base-hz': '20000', 'lp-depth': '1'})
        self.run_cli(False, rate='8000', samples='80002')
        for extra in (['--rate', '16000'], ['--attack=.01'], ['--att', '.01'], ['--surprise', 'true']):
            proc = subprocess.run([sys.executable, str(SCRIPT), *self.args(), *extra], capture_output=True)
            self.assertNotEqual(proc.returncode, 0)
        for budget in ('MAX_WORK', 'MAX_BUNDLE_BYTES'):
            with patch.object(REF, budget, 1), self.assertRaisesRegex(ValueError, 'budget'):
                REF.render(REF.parse(self.args()))
        self.assertFalse(self.out.exists())

    def test_numeric_mapping_boundaries_and_no_amplitude_or_production_calls(self):
        self.input.write_text('event,time_s,action\n0,0,fresh_on\n')
        self.run_cli(mapping=True, il='0', al='0', **{'hp-base-hz': '25', 'lp-base-hz': '20000',
                                                   'hp-depth': '-8', 'lp-depth': '8', 'q': '.35'})
        control = rows((self.out / 'filter_controls.csv').read_bytes())
        self.assertTrue(all((r['hp_hz'], r['lp_hz'], r['q']) == ('25', '20000', '0.34999999999999998') for r in control))
        with patch.object(REF.io, 'Envelope', side_effect=AssertionError('amplitude law used')):
            REF.render(REF.parse(self.args(out=self.root / 'independent')))
        source = SCRIPT.read_text()
        self.assertNotIn('src/core', source)
        self.assertNotIn('subprocess', source)

    def test_max_samples_time_and_level_bounds(self):
        self.input.write_text('event,time_s,action\n0,0,fresh_on\n')
        self.run_cli(samples='100001', rate='10000', attack='1', decay='10', release='10', il='-8', al='8')
        data = rows((self.out / 'envelope.csv').read_bytes())
        self.assertEqual(len(data), 100001)
        self.assertAlmostEqual(float(data[-1]['envelope']), 8 * 1000 ** (-.9), delta=3e-17)
        self.run_cli(out=self.root / 'short', samples='2', attack='.001', decay='.01', release='.01', il='8', al='-8')

    def test_existing_outputs_and_input_protected(self):
        for kind in ('file', 'directory', 'dangling_symlink', 'input'):
            target = self.root / kind
            if kind == 'file': target.write_text('keep')
            elif kind == 'directory': target.mkdir(); (target / 'keep').write_text('keep')
            elif kind == 'dangling_symlink': target.symlink_to(self.root / 'missing')
            else: target = self.input
            self.run_cli(False, out=target)
            self.assertTrue(os.path.lexists(target))
        self.assertEqual((self.root / 'file').read_text(), 'keep')
        self.assertEqual((self.root / 'directory/keep').read_text(), 'keep')
        self.run_cli(False, out=self.root / 'missing_parent/result')
        self.run_cli(False, events=self.root)
        fifo = self.root / 'fifo'
        os.mkfifo(fifo)
        self.run_cli(False, events=fifo)

    def test_failure_cleanup_preserves_unowned_and_replaced_files(self):
        original = REF.io.Owned.write
        for foreign in (False, True):
            out = self.root / ('foreign' if foreign else 'clean')
            def fail(owner, path, data):
                if path.name == 'events.json':
                    if foreign:
                        (out / 'unrelated').write_text('keep')
                        replacement = out / 'replacement'
                        replacement.write_text('different inode')
                        os.replace(replacement, out / 'envelope.csv')
                    raise OSError('injected write failure')
                return original(owner, path, data)
            with patch.object(REF.io.Owned, 'write', fail), self.assertRaisesRegex(OSError, 'injected'):
                REF.render(REF.parse(self.args(out=out)))
            if foreign:
                self.assertEqual({p.name for p in out.iterdir()}, {'unrelated', 'envelope.csv'})
                self.assertEqual((out / 'envelope.csv').read_text(), 'different inode')
            else:
                self.assertFalse(out.exists())
        original_generate = REF.generate
        def race(*args):
            result = original_generate(*args)
            self.out.write_text('created concurrently')
            return result
        with patch.object(REF, 'generate', race), self.assertRaises(FileExistsError):
            REF.render(REF.parse(self.args()))
        self.assertEqual(self.out.read_text(), 'created concurrently')

    def test_actual_dynamic_filter_amplitude_chain_and_explicit_gain_wav(self):
        if not FILTER or not VCA:
            self.skipTest('Pass --filter and --vca to exercise native dynamic chain integration')
        self.run_cli(mapping=True)
        amp_events = self.root / 'amp_events.csv'
        amp_events.write_text(self.input.read_text().replace('fresh_on', 'note_on').replace('retained_retrigger', 'retrigger'))
        amp = self.root / 'amplitude'
        proc = subprocess.run([sys.executable, str(REF.HELPER), '--events', str(amp_events), '--out', str(amp),
                               '--attack', '.01', '--decay', '.1', '--sustain', '.3', '--release', '.02',
                               '--lc', '.8', '--ei', '-.5', '--rate', '16000', '--samples', '1601'], capture_output=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        signal = self.root / 'signal.csv'
        signal.write_text('sample,value\n' + ''.join(
            f'{i},{.1 * math.sin(2 * math.pi * 440 * i / 16000):.17g}\n' for i in range(1601)))
        chain = self.root / 'chain'
        proc = subprocess.run([sys.executable, str(ROOT / 'scripts/render_reference_chain.py'), '--filter', FILTER,
                   '--vca', VCA, '--input', str(signal), '--controls', str(amp / 'controls.csv'),
                   '--filter-controls', str(self.out / 'filter_controls.csv'), '--rate', '16000', '--samples', '1601',
                   '--ota-level', '2.5', '--gain', 'unity', '--wav-gain', '.1', '--out', str(chain)],
                   capture_output=True, timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stderr.decode())
        self.assertEqual((chain / 'inputs/filter_controls.csv').read_bytes(), (self.out / 'filter_controls.csv').read_bytes())
        self.assertEqual((chain / 'inputs/controls.csv').read_bytes(), (amp / 'controls.csv').read_bytes())
        expected = rows((self.out / 'filter_controls.csv').read_bytes())
        actual = rows((chain / 'filter/control_endpoints.csv').read_bytes())
        self.assertEqual(len(actual), 1601)
        self.assertEqual(len(expected), 1601)
        for a, b in zip(expected, actual):
            self.assertEqual(tuple(float(a[k]) for k in ('hp_hz', 'lp_hz', 'q')),
                             tuple(float(b[k]) for k in ('hp_hz', 'lp_hz', 'q')))
        expected = rows((amp / 'controls.csv').read_bytes())
        actual = rows((chain / 'vca_reference_input/trace.csv').read_bytes())
        self.assertEqual(len(actual), 1601)
        self.assertEqual(len(expected), 1601)
        for a, b in zip(expected, actual):
            self.assertEqual(tuple(float(a[k]) for k in ('lc', 'ei', 'envelope')),
                             tuple(float(b[k]) for k in ('lc', 'ei', 'envelope')))
        manifest = json.loads((chain / 'manifest.json').read_bytes())
        self.assertEqual(manifest['wav_gain'], .1)
        self.assertTrue(manifest['complete'])
        outputs = [float(r['reference_output']) for r in rows((chain / 'chain.csv').read_bytes())]
        self.assertEqual(len(outputs), 1601)
        self.assertTrue(all(math.isfinite(v) for v in outputs))
        self.assertGreater(max(map(abs, outputs)), .001)
        wav = (chain / 'reference.wav').read_bytes()
        self.assertEqual(wav[:4], b'RIFF'); self.assertEqual(wav[8:12], b'WAVE')
        offset, chunks = 12, {}
        while offset < len(wav):
            tag, size = struct.unpack_from('<4sI', wav, offset)
            chunks[tag] = wav[offset + 8: offset + 8 + size]
            offset += 8 + size + size % 2
        fmt = struct.unpack_from('<HHIIHH', chunks[b'fmt '])
        self.assertEqual((fmt[0], fmt[1], fmt[2], fmt[5]), (3, 1, 16000, 32))
        self.assertEqual(chunks[b'data'], b''.join(struct.pack('<f', value * .1) for value in outputs))
        self.assertTrue(all(math.isfinite(x[0]) for x in struct.iter_unpack('<f', chunks[b'data'])))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--filter')
    parser.add_argument('--vca')
    args, remaining = parser.parse_known_args()
    if bool(args.filter) != bool(args.vca):
        parser.error('Supply both --filter and --vca, or neither')
    FILTER = str(Path(args.filter).resolve()) if args.filter else None
    VCA = str(Path(args.vca).resolve()) if args.vca else None
    unittest.main(argv=[__file__, *remaining])
