#!/usr/bin/env python3
"""Independent closed-form ADSR, exact clock, export safety and real-chain tests."""
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
SCRIPT = ROOT / 'scripts/render_amplitude_envelope_reference.py'
SPEC = importlib.util.spec_from_file_location('amplitude_reference', SCRIPT)
REF = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REF)
FILTER = VCA = None


def attack(x, elapsed, duration):
    # Independently expressed power law, not renderer expm1 or a production tick.
    return x + 1.5 * (1 - x) * (1 - 3 ** (-float(elapsed) / float(duration)))


def decay(x, target, elapsed, duration):
    return target + (x - target) * 1000 ** (-float(elapsed) / float(duration))


def rows(data):
    return list(csv.DictReader(io.StringIO(data.decode('ascii'))))


class AnalyticTests(unittest.TestCase):
    def env(self, a='.1', d='1', s='.25', r='1'):
        return REF.Envelope(*map(Q, (a, d, s, r)))

    def test_attack_from_zero_and_nonzero_at_declared_time(self):
        for duration in ('.001', '.01', '.3333333', '1'):
            for start in (0, .25, .7, .999):
                env = self.env(a=duration, s=str(start))
                env.event(Q(0), 'note_on')
                env.advance(Q(duration) + 2)
                self.assertEqual(env.state()['envelope'], start)
                onset = Q(duration) + 2
                audit = env.event(onset, 'retrigger')
                self.assertEqual(audit['before']['envelope'], audit['after']['envelope'])
                last = start
                for p in (Q(0), Q(1, 7), Q(1, 2), Q(999, 1000)):
                    value = env.advance(onset + Q(duration) * p)
                    self.assertAlmostEqual(value, attack(start, p * Q(duration), Q(duration)), delta=5e-16)
                    self.assertGreaterEqual(value, last)
                    self.assertEqual(env.stage, 'attack')
                    last = value
                self.assertEqual(env.advance(onset + Q(duration)), 1)
                self.assertEqual(env.stage, 'decay')

    def test_retained_attack_near_exact_boundary_never_overshoots(self):
        # Independent review's valid-input repro previously rounded above 1.
        env = self.env(a='.01', d='.1', s='.5', r='.1')
        env.event(Q(0), 'note_on')
        env.event(Q('.00002'), 'retrigger')
        audit = env.event(Q('.01001999999999999999'), 'note_off')
        self.assertEqual(audit['before']['stage'], 'attack')
        self.assertLessEqual(audit['before']['envelope'], 1)
        self.assertEqual(audit['after']['stage'], 'release')
        # Check float-adjacent and much closer rational times without allowing a
        # level rounded to one to move the comparator boundary prematurely.
        for duration in ('.001', '.01', '1'):
            for start in (0, .003292218469840, .25, .7, math.nextafter(1.0, 0.0)):
                env = self.env(a=duration, d='.1', s='.3')
                env.begin('attack', Q('.00002'), start)
                end = Q('.00002') + Q(duration)
                probes = sorted(set([Q(math.nextafter(float(end), 0.0)),
                                     end - Q(1, 10**20), end - Q(1, 10**100)]))
                for t in probes:
                    if t < end:
                        self.assertTrue(start <= env.advance(t) <= 1)
                        self.assertEqual(env.stage, 'attack')
                self.assertEqual(env.advance(end), 1)
                self.assertEqual(env.stage, 'decay')
                self.assertLessEqual(env.advance(end + Q(1, 10**20)), 1)
                self.assertEqual(env.stage, 'decay')
        # Both algebraic forms agree with the separate power oracle across the
        # switch, including retained-level and almost-unity attacks.
        for start in (0, .003292218469840, .7, .999999):
            env = self.env(a='.01')
            env.begin('attack', Q(0), start)
            for t in (Q('.00499999999999999999'), Q('.005'), Q('.00500000000000000001')):
                self.assertAlmostEqual(env.advance(t), attack(start, t, '.01'), delta=4e-16)

    def test_minus_60_db_and_phase_remainder(self):
        for time in ('.01', '.7', '10'):
            env = self.env(a='.013', d=time, s='.2', r=time)
            env.event(Q(0), 'note_on')
            self.assertAlmostEqual(env.advance(Q('.013') + Q(time)), .2008, delta=2e-16)
            env = self.env(a='.013', d=time, s='1', r=time)
            env.event(Q(0), 'note_on')
            env.event(Q('.02'), 'note_off')
            self.assertAlmostEqual(env.advance(Q('.02') + Q(time)), .001, delta=2e-18)
        env = self.env(a='.001', d='.01')
        env.event(Q(0), 'note_on')
        self.assertAlmostEqual(env.advance(Q('.001125')), decay(1, .25, .000125, .01), delta=2e-16)
        self.assertEqual(env.stage, 'decay')

    def test_threshold_times_and_right_continuous_snaps(self):
        env = self.env()
        env.event(Q(0), 'note_on')
        env.advance(Q('.1'))
        # Independent base-1000 logarithm rather than renderer expression.
        expected = .1 + math.log(.75 / .0005, 1000)
        boundary = env.end
        self.assertAlmostEqual(float(boundary), expected, delta=3e-16)
        self.assertGreater(env.advance(boundary - Q(1, 10**10)), .2505)
        self.assertEqual(env.advance(boundary), .25)
        self.assertEqual(env.stage, 'sustain')
        env.event(boundary, 'note_off')
        release_end = env.end
        self.assertAlmostEqual(float(release_end - boundary), math.log(.25 / .0002, 1000), delta=3e-16)
        self.assertGreater(env.advance(release_end - Q(1, 10**10)), .0002)
        self.assertEqual(env.advance(release_end), 0)
        self.assertEqual(env.stage, 'idle')
        self.assertEqual([t['before_stage'] for t in env.transitions], ['attack', 'decay', 'release'])

    def test_off_during_attack_retrigger_during_release(self):
        env = self.env(a='.1', r='.2')
        env.event(Q(0), 'note_on')
        first = attack(0, .025, .1)
        off = env.event(Q('.025'), 'note_off')
        self.assertAlmostEqual(off['before']['envelope'], first, delta=2e-16)
        self.assertEqual(off['after']['envelope'], off['before']['envelope'])
        next_start = decay(first, 0, .0125, .2)
        on = env.event(Q('.0375'), 'retrigger')
        self.assertAlmostEqual(on['after']['envelope'], next_start, delta=2e-16)
        self.assertAlmostEqual(env.advance(Q('.0875')), attack(next_start, .05, .1), delta=3e-16)
        self.assertEqual(env.advance(Q('.1375')), 1)

    def test_gate_ties_reset_and_zero_span(self):
        env = self.env(s='1')
        self.assertEqual(env.event(Q(0), 'note_off')['after']['stage'], 'idle')
        env.event(Q(0), 'note_on')
        off = env.event(Q(0), 'note_off')
        self.assertEqual(off['after']['stage'], 'idle')
        self.assertEqual(off['after']['envelope'], 0)
        env.event(Q(0), 'note_on')
        self.assertEqual(env.advance(Q('.1')), 1)
        self.assertEqual(env.stage, 'sustain')
        retrigger = env.event(Q('.1'), 'note_on')
        self.assertEqual(retrigger['after']['stage'], 'sustain')
        self.assertTrue(env.gate)
        env.event(Q('.2'), 'reset')
        self.assertEqual(env.state()['envelope'], 0)
        self.assertFalse(env.gate)
        self.assertEqual(env.stage, 'idle')
        env = self.env(s='0')
        env.event(Q(0), 'note_on')
        self.assertEqual(env.advance(Q(5)), 0)
        self.assertEqual(env.stage, 'sustain')
        self.assertTrue(env.gate)
        self.assertEqual(env.event(Q(5), 'note_off')['after']['stage'], 'idle')

    def test_immediate_small_span_snap_and_subsample_precision(self):
        env = self.env(s='.9998')
        env.event(Q(0), 'note_on')
        self.assertEqual(env.advance(Q('.1')), .9998)
        self.assertEqual(env.stage, 'sustain')
        self.assertEqual(env.transitions[-1]['before_value'], 1)
        env = self.env()
        env.event(Q(0), 'note_on')
        tiny = Q(1, 10**30)
        self.assertGreater(env.advance(tiny), 0)
        self.assertAlmostEqual(env.state()['envelope'] / float(tiny), 15 * math.log(3), delta=1e-14)
        audit = env.event(tiny, 'note_off')
        self.assertGreater(audit['before']['envelope'], 0)
        self.assertEqual(audit['after']['envelope'], 0)

    def test_common_times_independent_of_sample_queries(self):
        events = [(Q(0), 'note_on'), (Q('.0170123'), 'note_off'),
                  (Q('.0190037'), 'retrigger'), (Q('.031'), 'note_off')]
        values = []
        for rate in (8000, 16000, 48000, 192000, 384000):
            env, cursor, common = self.env(a='.01', d='.1', r='.2'), 0, []
            for i in range(rate // 20 + 1):
                t = Q(i, rate)
                while cursor < len(events) and events[cursor][0] <= t:
                    env.event(*events[cursor]); cursor += 1
                v = env.advance(t)
                if i % (rate // 8000) == 0:
                    common.append((v, env.stage, env.gate))
            values.append(common)
        self.assertTrue(all(v == values[0] for v in values))


class CliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.input = self.root / 'events.csv'
        self.out = self.root / 'result'
        self.input.write_text('event,time_s,action\n0,0,note_on\n1,.06,note_off\n2,.065,retrigger\n3,.08,note_off\n')

    def args(self, **options):
        values = {'events': self.input, 'out': self.out, 'rate': '16000', 'samples': '1601',
                  'attack': '.01', 'decay': '.1', 'sustain': '.3', 'release': '.02', 'lc': '.8', 'ei': '-.5'}
        values.update(options)
        return [str(item) for key, value in values.items() for item in ('--' + key, value)]

    def run_cli(self, success=True, **options):
        proc = subprocess.run([sys.executable, str(SCRIPT), *self.args(**options)], capture_output=True, timeout=20)
        self.assertEqual(proc.returncode == 0, success, proc.stderr.decode())
        return proc

    def test_exports_provenance_determinism_and_source_replay(self):
        self.run_cli()
        manifest = json.loads((self.out / 'manifest.json').read_bytes())
        self.assertTrue(manifest['complete'])
        self.assertEqual(manifest['event_count'], 4)
        self.assertEqual((self.out / 'input_events.csv').read_bytes(), self.input.read_bytes())
        self.assertEqual((self.out / 'renderer.py').read_bytes(), SCRIPT.read_bytes())
        for name, meta in manifest['artifacts'].items():
            data = (self.out / name).read_bytes()
            self.assertEqual(meta, {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        env = rows((self.out / 'envelope.csv').read_bytes())
        control = rows((self.out / 'controls.csv').read_bytes())
        self.assertEqual(len(env), 1601)
        self.assertEqual(env[0]['envelope'], '0')
        self.assertEqual(env[0]['stage'], 'attack')
        for i, (e, c) in enumerate(zip(env, control)):
            self.assertEqual(e['sample'], str(i))
            self.assertEqual(float(e['time_s']), i / 16000)
            self.assertEqual(e['envelope'], c['envelope'])
            self.assertEqual(float(c['lc']), .8)
            self.assertEqual(float(c['ei']), -.5)
            self.assertTrue(0 <= float(c['envelope']) <= 1)
        replay = self.root / 'replay'
        proc = subprocess.run([sys.executable, str(self.out / 'renderer.py'), *self.args(out=replay)], capture_output=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual({p.name: p.read_bytes() for p in self.out.iterdir()},
                         {p.name: p.read_bytes() for p in replay.iterdir()})

    def test_endpoint_subsample_event_ties_and_final_event(self):
        self.input.write_text('event,time_s,action\n0,.000062499999999999999,note_on\n'
                              '1,.0000625,note_off\n2,.0000625,retrigger\n'
                              '3,.000062500000000000001,reset\n4,.1,note_on\n')
        self.run_cli()
        samples = rows((self.out / 'envelope.csv').read_bytes())
        self.assertEqual(samples[0]['stage'], 'idle')
        self.assertEqual(samples[1]['last_event'], '2')
        self.assertEqual(samples[1]['stage'], 'attack')
        self.assertEqual(samples[2]['stage'], 'idle')
        self.assertEqual(samples[-1]['last_event'], '4')
        self.assertEqual(samples[-1]['stage'], 'attack')
        self.assertEqual(samples[-1]['envelope'], '0')
        audit = json.loads((self.out / 'events.json').read_bytes())
        self.assertGreater(audit[1]['before']['envelope'], 0)
        self.assertEqual(audit[1]['after']['envelope'], 0)
        self.assertEqual(audit[2]['before'], audit[1]['after'])

    def test_cli_retained_attack_boundary_roundoff_regression(self):
        self.input.write_text('event,time_s,action\n0,0,note_on\n1,.00002,retrigger\n'
                              '2,.01001999999999999999,note_off\n')
        self.run_cli(rate='8000', samples='100', sustain='.5', release='.1', lc='1', ei='0')
        audit = json.loads((self.out / 'events.json').read_bytes())
        self.assertEqual(audit[-1]['before']['stage'], 'attack')
        self.assertLessEqual(audit[-1]['before']['envelope'], 1)
        self.assertEqual(audit[-1]['after']['stage'], 'release')

    def test_empty_events_crlf_and_same_time_dense_events(self):
        self.input.write_bytes(b'event,time_s,action\r\n')
        self.run_cli()
        self.assertTrue(all(r['envelope'] == '0' for r in rows((self.out / 'envelope.csv').read_bytes())))
        self.input.write_bytes(('event,time_s,action\r\n' + ''.join(
            f'{i},0,{"note_on" if i % 2 == 0 else "reset"}\r\n' for i in range(REF.MAX_EVENTS))).encode())
        self.run_cli(out=self.root / 'dense')
        self.assertEqual(len(json.loads((self.root / 'dense/events.json').read_bytes())), REF.MAX_EVENTS)

    def test_bad_event_files(self):
        header = b'event,time_s,action\n'
        cases = [b'', b'event,time_s,action', b'event,time,action\n', header + b'\n',
                 header + b'00,0,note_on\n', header + b'1,0,note_on\n', header + b'0,-.1,note_on\n',
                 header + b'0,.100000001,note_on\n', header + b'0,0,unknown\n',
                 header + b'0,0,note_on,extra\n', header + b'0, 0,note_on\n', header + b'0,"0",note_on\n',
                 header + b'0,0,note_on\n1,-.001,reset\n', header + b'0,0,note_on\r',
                 header + b'0,\xff,note_on\n', header + b'0,0,note_on\n2,0,reset\n',
                 header + b'0,.05,note_on\n1,.04,reset\n']
        cases += [header + f'0,{number},note_on\n'.encode() for number in (
            'nan', 'inf', '1_0', '0x0', '1e999', '1e-999', '0e1000000000', '1e-301', '.','+' )]
        cases += [header + b'0,' + b'0' * 129 + b',note_on\n', b'x' * (REF.MAX_INPUT_BYTES + 1)]
        cases += [header + b''.join(f'{i},0,reset\n'.encode() for i in range(REF.MAX_EVENTS + 1))]
        for data in cases:
            with self.subTest(data=data[:90]):
                self.input.write_bytes(data)
                self.run_cli(False)
                self.assertFalse(self.out.exists())

    def test_bad_arguments_and_budgets(self):
        cases = {'attack': ('0', '.0002', '.00099999', '1.00001'), 'decay': ('0', '.0099', '10.01'),
                 'release': ('-.1', '.009', '10.01'), 'sustain': ('-.0001', '1.0001'),
                 'lc': ('-8.1', '8.1'), 'ei': ('-8.1', '8.1'), 'rate': ('7999', '384001', '08000', '8000.0'),
                 'samples': ('1', '100002', '1e2')}
        for key, values in cases.items():
            for value in values:
                with self.subTest(key=key, value=value):
                    self.run_cli(False, **{key: value})
        self.run_cli(False, rate='8000', samples='80002')
        for extra in (['--rate', '16000'], ['--attack=.01'], ['--att', '.01'], ['--surprise', 'true']):
            proc = subprocess.run([sys.executable, str(SCRIPT), *self.args(), *extra], capture_output=True)
            self.assertNotEqual(proc.returncode, 0)
        self.assertFalse(self.out.exists())
        with patch.object(REF, 'MAX_WORK', 1):
            with self.assertRaisesRegex(ValueError, 'work budget'):
                REF.render(REF.parse(self.args()))
        with patch.object(REF, 'MAX_BUNDLE_BYTES', 1):
            with self.assertRaisesRegex(ValueError, 'byte budget'):
                REF.render(REF.parse(self.args()))
        self.assertFalse(self.out.exists())

    def test_max_samples_short_long_time_bounds(self):
        self.input.write_text('event,time_s,action\n0,0,note_on\n')
        self.run_cli(samples='100001', rate='10000', attack='1', decay='10', release='10', sustain='0', lc='-8', ei='8')
        data = rows((self.out / 'envelope.csv').read_bytes())
        self.assertEqual(len(data), 100001)
        self.assertAlmostEqual(float(data[-1]['envelope']), 1000 ** (-.9), delta=3e-18)
        self.run_cli(out=self.root / 'short', samples='2', attack='.001', decay='.01', release='.01', sustain='1', lc='8', ei='-8')

    def test_existing_outputs_and_input_protected(self):
        for kind in ('file', 'directory', 'dangling_symlink', 'input'):
            with self.subTest(kind=kind):
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

    def test_failure_cleanup_only_owned_files(self):
        original = REF.Owned.write
        for foreign in (False, True):
            out = self.root / ('foreign' if foreign else 'clean')
            def fail(owner, path, data):
                if path.name == 'events.json':
                    if foreign:
                        # Preserve an untracked sibling and a replacement inode.
                        (out / 'unrelated').write_text('keep')
                        replacement = out / 'replacement'
                        replacement.write_text('different inode')
                        os.replace(replacement, out / 'controls.csv')
                    raise OSError('injected write failure')
                return original(owner, path, data)
            with patch.object(REF.Owned, 'write', fail), self.assertRaisesRegex(OSError, 'injected'):
                REF.render(REF.parse(self.args(out=out)))
            if foreign:
                self.assertEqual({p.name for p in out.iterdir()}, {'unrelated', 'controls.csv'})
                self.assertEqual((out / 'controls.csv').read_text(), 'different inode')
            else:
                self.assertFalse(out.exists())
        # A file created between preflight and mkdir is never removed.
        original_generate = REF.generate
        def race(*args):
            result = original_generate(*args)
            self.out.write_text('created concurrently')
            return result
        with patch.object(REF, 'generate', race), self.assertRaises(FileExistsError):
            REF.render(REF.parse(self.args()))
        self.assertEqual(self.out.read_text(), 'created concurrently')

    @unittest.skipUnless(os.name == 'posix', 'Existing chain requires POSIX')
    def test_actual_filter_vca_chain_and_explicit_gain_wav(self):
        if not FILTER or not VCA:
            self.skipTest('Pass --filter and --vca to exercise native chain integration')
        self.run_cli()
        signal = self.root / 'signal.csv'
        signal.write_text('sample,value\n' + ''.join(
            f'{i},{.1 * math.sin(2 * math.pi * 440 * i / 16000):.17g}\n' for i in range(1601)))
        chain = self.root / 'chain'
        command = [sys.executable, str(ROOT / 'scripts/render_reference_chain.py'), '--filter', FILTER,
                   '--vca', VCA, '--input', str(signal), '--controls', str(self.out / 'controls.csv'),
                   '--rate', '16000', '--samples', '1601', '--hp-hz', '200', '--lp-hz', '2000', '--q', '1',
                   '--ota-level', '2.5', '--gain', 'unity', '--wav-gain', '.1', '--out', str(chain)]
        proc = subprocess.run(command, capture_output=True, timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stderr.decode())
        self.assertEqual((chain / 'inputs/controls.csv').read_bytes(), (self.out / 'controls.csv').read_bytes())
        controls = rows((self.out / 'controls.csv').read_bytes())
        vca = rows((chain / 'vca_reference_input/trace.csv').read_bytes())
        for a, b in zip(controls, vca):
            self.assertEqual(tuple(float(a[k]) for k in ('lc', 'ei', 'envelope')),
                             tuple(float(b[k]) for k in ('lc', 'ei', 'envelope')))
        manifest = json.loads((chain / 'manifest.json').read_bytes())
        self.assertEqual(manifest['wav_gain'], .1)
        self.assertTrue(manifest['complete'])
        outputs = [float(r['reference_output']) for r in rows((chain / 'chain.csv').read_bytes())]
        self.assertTrue(all(math.isfinite(v) for v in outputs))
        self.assertGreater(max(map(abs, outputs)), .001)
        # Read RIFF chunks independently; compare every float sample to explicit gain.
        wav = (chain / 'reference.wav').read_bytes()
        self.assertEqual(wav[:4], b'RIFF'); self.assertEqual(wav[8:12], b'WAVE')
        offset, chunks = 12, {}
        while offset < len(wav):
            tag, size = struct.unpack_from('<4sI', wav, offset)
            chunks[tag] = wav[offset + 8: offset + 8 + size]
            offset += 8 + size + size % 2
        fmt = struct.unpack_from('<HHIIHH', chunks[b'fmt '])
        self.assertEqual((fmt[0], fmt[1], fmt[2], fmt[5]), (3, 1, 16000, 32))
        self.assertEqual(len(chunks[b'data']), len(outputs) * 4)
        expected = b''.join(struct.pack('<f', value * .1) for value in outputs)
        self.assertEqual(chunks[b'data'], expected)
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
