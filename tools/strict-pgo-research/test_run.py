import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('study', Path(__file__).with_name('run.py'))
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)


def rows(scale=1, workload=r.COST):
    result = {}
    for key in sorted(r.expected_performance_keys(workload)):
        result[key] = dict(zip(r.KEY, key), callback_cpu_seconds=str(scale),
                           audio_seconds='2', callback_cpu_rtf=str(scale / 2),
                           callback_cpu_status='available', callback_cpu_call_count=str(workload['callbacks']),
                           callbacks=str(workload['callbacks']), nonfinite_count='0', checksum='7')
    return result


class Tests(unittest.TestCase):
    def test_frozen_rosters(self):
        self.assertEqual(len(rows(workload=r.TRAIN)), 36)
        self.assertEqual(len(rows()), 100)
        primary = r.group_keys(rows(), 'primary')
        self.assertEqual(len(primary), 24)
        self.assertEqual({k[4] for k in primary}, set(map(str, range(2, 10))))

    def test_calibration_pass(self):
        self.assertTrue(r.calibration([(rows(), rows())] * 3)[0])

    def test_failed_calibration_zero_candidates(self):
        calls = []
        def run(name, arm):
            calls.append((name, arm))
            return rows(1.3 if name == 'cal-1-b' else 1)
        d = r.cost_phase(run)
        self.assertEqual(d['decision'], 'TERMINAL_UNUSABLE_CALIBRATION')
        self.assertEqual(d['candidate_cost_executions'], 0)
        self.assertEqual(len(calls), 6)
        self.assertTrue(all(a == 'baseline' for _, a in calls))

    def test_order_and_gain(self):
        calls = []
        def run(name, arm):
            calls.append((name, arm))
            return rows(.99 if arm == 'candidate' else 1)
        d = r.cost_phase(run)
        self.assertEqual(d['decision'], 'SCOPED_COST_PASS')
        self.assertEqual([a for _, a in calls[6:]],
                         ['baseline', 'candidate', 'candidate', 'baseline', 'baseline', 'candidate'])

    def assess(self, candidate, noise=0):
        records = r.calibration([(rows(), rows(1 + noise))] * 3)[1]
        return r.assess([(rows(), candidate)] * 3, records)

    def test_no_gain_and_below_floor(self):
        for scale in (1, .999, .996):
            self.assertEqual(self.assess(rows(scale))['decision'], 'REJECT_GAIN_OR_REGRESSION_GATE')

    def test_noise_relative_gate(self):
        self.assertEqual(self.assess(rows(.985), .01)['decision'], 'REJECT_GAIN_OR_REGRESSION_GATE')
        self.assertEqual(self.assess(rows(.975), .01)['decision'], 'SCOPED_COST_PASS')

    def test_every_pair_must_improve(self):
        records = r.calibration([(rows(), rows())] * 3)[1]
        d = r.assess([(rows(), rows(.97)), (rows(), rows(1)), (rows(), rows(.97))], records)
        self.assertEqual(d['decision'], 'REJECT_GAIN_OR_REGRESSION_GATE')

    def test_regression_guards(self):
        for group in ('legacy', 'isolation'):
            x = rows(.98)
            for key in r.group_keys(x, group):
                x[key]['callback_cpu_seconds'] = '1.03'
            self.assertEqual(self.assess(x)['decision'], 'REJECT_GAIN_OR_REGRESSION_GATE')
        x = rows(.98)
        next(iter(x.values()))['callback_cpu_seconds'] = '1.06'
        self.assertEqual(self.assess(x)['decision'], 'REJECT_GAIN_OR_REGRESSION_GATE')

    def test_work_and_inventory_closed(self):
        for action in ('missing', 'checksum', 'extra'):
            x = rows()
            if action == 'missing': x.pop(next(iter(x)))
            elif action == 'checksum': next(iter(x.values()))['checksum'] = '8'
            else: next(iter(x.values()))['new_field'] = '8'
            with self.assertRaises(ValueError): r.exact(rows(), x)
        x = rows(); x.pop(r.group_keys(x, 'primary')[0])
        with self.assertRaises(ValueError): r.group_keys(x, 'primary')

    def fixture(self, p, field=None, value=None):
        data = list(rows().values())
        if field: data[0][field] = value
        (p / 'completed.json').write_text(json.dumps({'completed': True, 'cases': 39, 'rows': 100}))
        with (p / 'results.csv').open('w') as stream:
            writer = csv.DictWriter(stream, fieldnames=data[0].keys())
            writer.writeheader(); writer.writerows(data)

    def test_valid_clock(self):
        with tempfile.TemporaryDirectory() as t:
            p = Path(t); self.fixture(p)
            self.assertEqual(len(r.read_rows(p, r.COST, True)), 100)

    def test_bad_clock_and_roster(self):
        cases = [('callback_cpu_seconds', v) for v in ('nan', 'inf', '0', '-1')]
        cases += [('audio_seconds', v) for v in ('nan', 'inf', '0', '-1')]
        cases += [('callback_cpu_rtf', 'nan'), ('callback_cpu_rtf', '2'),
                  ('callback_cpu_call_count', '127'), ('callback_cpu_status', 'unavailable'),
                  ('nonfinite_count', '1'), ('rate', '48000'), ('initial_selector', '99')]
        for field, value in cases:
            with self.subTest(field=field, value=value), tempfile.TemporaryDirectory() as t:
                p = Path(t); self.fixture(p, field, value)
                with self.assertRaises(ValueError): r.read_rows(p, r.COST, True)

    def test_profile_manifest(self):
        with tempfile.TemporaryDirectory() as t:
            p = Path(t)
            with self.assertRaises(ValueError): r.profile_manifest(p)
            nested = p / 'a/b'; nested.mkdir(parents=True)
            f = nested / 'Engine.gcda'; f.write_bytes(b'adcg' + b'x' * 20)
            self.assertEqual(list(r.profile_manifest(p)), ['a/b/Engine.gcda'])
            f.write_bytes(b'bad!' + b'x' * 20)
            with self.assertRaises(ValueError): r.profile_manifest(p)
            f.write_bytes(b'adcg' + b'x' * 20)
            (p / 'Other.gcda').write_bytes(b'adcg' + b'x' * 20)
            with self.assertRaises(ValueError): r.profile_manifest(p)

    def test_transcripts(self):
        with tempfile.TemporaryDirectory() as t:
            a, b = Path(t) / 'a', Path(t) / 'b'
            for p in (a, b):
                p.mkdir(); (p / 'qualification-receipt.json').write_text('{"calls":1,"frames":2}')
                (p / 'qualification.pcm').write_bytes(b'0' * 16)
                (p / 'qualification.fp').write_bytes(b'0' * 24)
            r.compare_transcripts(a, b)
            (b / 'qualification.fp').write_bytes(b'1' * 24)
            with self.assertRaises(ValueError): r.compare_transcripts(a, b)
            (b / 'qualification.fp').write_bytes(b'0' * 23)
            with self.assertRaises(ValueError): r.compare_transcripts(a, b)

    def test_qualification_anchor_closed(self):
        with self.assertRaises(ValueError): r.qualification_source('not source')

    def test_optimizer_controls(self):
        text = '\n'.join(n + ' ' + state for n, state in (
            ('-fassociative-math', '[disabled]'), ('-ffinite-math-only', '[disabled]'),
            ('-fmath-errno', '[enabled]'), ('-fsigned-zeros', '[enabled]'),
            ('-funsafe-math-optimizations', '[disabled]'), ('-ffp-contract=[off|on|fast]', 'fast')))
        self.assertEqual(len(r.optimizer_states(text)), 6)
        with self.assertRaises(ValueError): r.optimizer_states(text.replace('[disabled]', '[enabled]'))
        with self.assertRaises(ValueError): r.optimizer_states('')

if __name__ == '__main__': unittest.main()
