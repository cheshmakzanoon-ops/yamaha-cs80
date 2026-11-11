#!/usr/bin/env python3
"""Independent endpoint-clock, nodal/convolution and failure-contract checks.

The reported deltas describe a three-run numerical sensitivity experiment.
These tests deliberately do not turn them into an error bound or parity gate.
"""
import argparse
import contextlib
import csv
from fractions import Fraction
import hashlib
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from test_reference_chain import ac_oracle, dynamic_lp, envelope_oracle, linear_lp

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts/study_reference_chain_clock.py'
CHAIN_SCRIPT = ROOT / 'scripts/render_reference_chain.py'
FILTER = VCA = None
STUDY = None
RUNS = ('baseline', 'matched_step', 'finer_step')
COMPARISONS = (
    ('matched_step_minus_baseline', 'matched_step', 'baseline'),
    ('finer_step_minus_baseline', 'finer_step', 'baseline'),
    ('finer_step_minus_matched_step', 'finer_step', 'matched_step'),
)
COMMON_COLUMNS = [
    'sample', 'time_s', 'baseline_index', 'matched_step_index', 'finer_step_index',
    'baseline_output', 'matched_step_output', 'finer_step_output',
    'matched_step_minus_baseline', 'finer_step_minus_baseline',
    'finer_step_minus_matched_step',
]


def rows(path):
    with path.open(newline='') as stream:
        return [{key: float(value) for key, value in row.items()}
                for row in csv.DictReader(stream)]


def midpoint_endpoints(values):
    """Test-owned interpolation: no production refinement/serialization helpers."""
    result = []
    for left, right in zip(values, values[1:]):
        result.extend((left, tuple(a + (b-a)/2 for a, b in zip(left, right))))
    result.append(values[-1])
    return result


def metrics(candidate, reference):
    differences = [candidate[i] - reference[i] for i in range(len(reference))]
    energy = math.fsum(value*value for value in reference)
    error = math.fsum(value*value for value in differences)
    return {'max_abs': max(map(abs, differences)),
            'rms_abs': math.sqrt(error/len(reference)),
            'relative_rms': math.sqrt(error/energy) if energy else None}


class ChainClockTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.signal = self.root/'signal.csv'
        self.controls = self.root/'controls.csv'
        self.filter_controls = self.root/'filter-controls.csv'
        self.out = self.root/'out'
        self.write([.2*math.sin(.37*i)-.03 for i in range(17)],
                   [(.8, -.5, .9)]*17)

    def write_csv(self, path, header, values, crlf=False):
        newline = '\r\n' if crlf else '\n'
        text = header+newline
        text += ''.join(str(i)+','+','.join(format(value, '.17g') for value in row)+newline
                        for i, row in enumerate(values))
        path.write_bytes(text.encode('ascii'))

    def write(self, signal, controls, crlf=False):
        self.count = len(signal)
        self.write_csv(self.signal, 'sample,value', [(value,) for value in signal], crlf)
        self.write_csv(self.controls, 'sample,lc,ei,envelope', controls, crlf)

    def dynamic(self, values, crlf=False):
        self.write_csv(self.filter_controls, 'sample,hp_hz,lp_hz,q', values, crlf)
        return {'hp-hz': None, 'lp-hz': None, 'q': None,
                'filter-controls': self.filter_controls}

    def args(self, **changes):
        options = {'filter': FILTER, 'vca': VCA, 'input': self.signal,
                   'controls': self.controls, 'out': self.out, 'rate': 48000,
                   'samples': self.count, 'hp-hz': 200, 'lp-hz': 2000,
                   'q': 1, 'ota-level': 0, 'gain': 'physical',
                   'filter-substeps': 16, 'vca-steps': 8}
        options.update(changes)
        return [str(item) for key, value in options.items() if value is not None
                for item in ('--'+key, value)]

    def execute(self, script=SCRIPT, tail=(), **changes):
        return subprocess.run([sys.executable, str(script), *self.args(**changes), *tail],
                              capture_output=True, text=True, timeout=30)

    def assert_clean(self):
        self.assertEqual([path.name for path in self.root.iterdir()
                          if path.name.startswith('.cs80-')], [])

    def run_ok(self, **changes):
        process = self.execute(**changes)
        self.assertEqual(process.returncode, 0, process.stdout+process.stderr)
        self.assert_clean()
        output = Path(changes.get('out', self.out))
        manifest = json.loads((output/'manifest.json').read_text(),
                              parse_constant=lambda value: self.fail(value))
        self.assertTrue(manifest['complete'])
        self.assertEqual(manifest['schema'], 'cs80-chain-clock-study-v1')
        return output, manifest

    def reject(self, tail=(), **changes):
        process = self.execute(tail=tail, **changes)
        self.assertNotEqual(process.returncode, 0, process.stdout+process.stderr)
        self.assertNotIn('Wrote', process.stdout)
        self.assertFalse(Path(changes.get('out', self.out)).exists())
        self.assert_clean()
        return process

    def shim(self, name, code):
        path = self.root/name
        path.write_text('#!'+sys.executable+'\n'+code)
        path.chmod(0o700)
        return path

    def assert_common_clock(self, output, manifest, rate=48000):
        with (output/'common_clock.csv').open(newline='') as stream:
            self.assertEqual(next(csv.reader(stream)), COMMON_COLUMNS)
        common = rows(output/'common_clock.csv')
        self.assertEqual(len(common), self.count)
        selected = {}
        for name in RUNS:
            native = rows(output/'runs'/name/'chain.csv')
            stride = 1 if name == 'baseline' else 2
            self.assertEqual(len(native), 1+(self.count-1)*stride)
            selected[name] = [row['reference_output'] for row in native[::stride]]
            for i, row in enumerate(common):
                self.assertEqual(row['sample'], i)
                self.assertEqual(row['time_s'], i/rate)
                self.assertEqual(row[name+'_index'], i*stride)
                self.assertEqual(row[name+'_output'], selected[name][i])
                self.assertEqual(native[i*stride]['time_s'], i/rate)
        for key, candidate, reference in COMPARISONS:
            expected = metrics(selected[candidate], selected[reference])
            for i, row in enumerate(common):
                self.assertEqual(row[key], selected[candidate][i]-selected[reference][i])
            actual = manifest['comparisons'][key]
            self.assertEqual(actual['sample_count'], self.count)
            for metric, value in expected.items():
                self.assertEqual(actual[metric], value, (key, metric))
        return common

    def assert_oracles(self, output, signal, controls, filter_controls=None, rate=48000,
                       filter_substeps=16):
        for name in RUNS:
            refined = name != 'baseline'
            native_rate = rate*(2 if refined else 1)
            native_steps = filter_substeps//2 if name == 'matched_step' else filter_substeps
            source = [row[0] for row in midpoint_endpoints([(value,) for value in signal])] if refined else signal
            vca_controls = midpoint_endpoints(controls) if refined else controls
            if filter_controls is None:
                expected_lp = linear_lp(source, native_rate, 2*native_steps)
            else:
                fc = midpoint_endpoints(filter_controls) if refined else filter_controls
                expected_lp = dynamic_lp(source, fc, native_rate, 2*native_steps)
            envelopes = [row[2] for row in vca_controls]
            expected = envelope_oracle(expected_lp, envelopes, native_rate)
            filtered = rows(output/'runs'/name/'filter/trace.csv')
            chain = rows(output/'runs'/name/'chain.csv')
            self.assertEqual(len(filtered), len(expected_lp))
            self.assertEqual(len(chain), len(expected))
            for index, (filt, actual, lp, value) in enumerate(zip(filtered, chain, expected_lp, expected)):
                self.assertAlmostEqual(filt['lp'], lp, delta=3e-12, msg=(name, index, 'LP'))
                self.assertAlmostEqual(actual['reference_output'], value, delta=3e-12,
                                       msg=(name, index, 'VCA'))

    def test_independent_linear_constant_signed_and_zero_source_oracles(self):
        for name, signal in [('constant', [.25]*17),
                             ('signed', [.3*math.sin(i*.61)-.12 for i in range(19)]),
                             ('zero', [0.]*9)]:
            with self.subTest(source=name):
                controls = [(.8, -.5, .9)]*len(signal)
                self.write(signal, controls)
                output, manifest = self.run_ok(out=self.root/name, **{'filter-substeps': 64})
                self.assert_oracles(output, signal, controls, filter_substeps=64)
                common = self.assert_common_clock(output, manifest)
                # Filter zero state does not imply a muted VCA offset at t=0.
                self.assertNotEqual(common[0]['baseline_output'], 0)
                if name == 'zero':
                    expected = ac_oracle([0.]*len(signal), 48000)
                    for row, value in zip(common, expected):
                        self.assertAlmostEqual(row['baseline_output'], value, delta=1e-15)

    def test_dynamic_nodal_and_convolution_oracle_at_each_clock(self):
        signal = [.3*math.sin(.39*i)-.09 for i in range(21)]
        envelopes = [max(0., min(i/6, (20-i)/9, 1.)) for i in range(21)]
        controls = [(.8, -.5, value) for value in envelopes]
        fc = [(200+43*i, 2800-61*i, .6+.11*i) for i in range(21)]
        self.write(signal, controls)
        output, manifest = self.run_ok(**self.dynamic(fc))
        self.assert_oracles(output, signal, controls, fc)
        common = self.assert_common_clock(output, manifest)
        # This fixture actually distinguishes added LP knots from baseline PWL.
        self.assertGreater(max(abs(row['matched_step_minus_baseline']) for row in common), 1e-9)
        self.assertGreater(max(abs(row['finer_step_minus_matched_step']) for row in common), 1e-12)

    def test_exact_original_endpoints_midpoints_provenance_and_configuration(self):
        # Include binary64 values for which computing an even endpoint as
        # a+(b-a) does not recover b exactly, and noncanonical decimal spellings.
        signal = [1., -1e-17, -.31, .7000000000000001, -.0]
        controls = [(.8+.037*i, -.75+.13*i, (.17, .93, .41, .02, .71)[i]) for i in range(5)]
        fc = [(200+.1*i, 2800-137*i, .7+.3*i) for i in range(5)]
        self.write(signal, controls, crlf=True)
        self.signal.write_bytes(self.signal.read_bytes().replace(b'0,1\r\n', b'0,+1.00000000000000000\r\n'))
        output, manifest = self.run_ok(**self.dynamic(fc, crlf=True),
                                       **{'initial-li': .12, 'filter-substeps': 64})
        for name, path, values, columns in [
                ('signal', self.signal, [(value,) for value in signal], ('value',)),
                ('controls', self.controls, controls, ('lc', 'ei', 'envelope')),
                ('filter_controls', self.filter_controls, fc, ('hp_hz', 'lp_hz', 'q'))]:
            self.assertEqual((output/'inputs'/f'{name}.csv').read_bytes(), path.read_bytes())
            refined = rows(output/'inputs'/f'refined_{name}.csv')
            expected = midpoint_endpoints(values)
            self.assertEqual(len(refined), 2*len(values)-1)
            self.assertEqual([row['sample'] for row in refined], list(range(2*len(values)-1)))
            for row, want in zip(refined, expected):
                self.assertEqual(tuple(row[column] for column in columns), want)
            self.assertEqual([tuple(row[column].hex() for column in columns) for row in refined[::2]],
                             [tuple(float(value).hex() for value in row) for row in values])
            # Rational arithmetic is independent of the driver's floating
            # interpolation expression. Check both midpoint values and the
            # represented function at quarter points, with an explicit small
            # rounding allowance in the original input's units.
            for i, (left, right) in enumerate(zip(values, values[1:])):
                for column, a, b in zip(columns, left, right):
                    a, b = float(a), float(b)
                    exact_a, exact_b = Fraction.from_float(a), Fraction.from_float(b)
                    actual_mid = Fraction.from_float(refined[2*i+1][column])
                    tolerance = Fraction.from_float(4*math.ulp(max(1., abs(a), abs(b))))
                    self.assertLessEqual(abs(actual_mid-(exact_a+exact_b)/2), tolerance)
                    for numerator in (1, 3):
                        phase = Fraction(numerator, 4)
                        original_pwl = (1-phase)*exact_a+phase*exact_b
                        segment = 0 if numerator == 1 else 1
                        refined_left = Fraction.from_float(refined[2*i+segment][column])
                        refined_right = Fraction.from_float(refined[2*i+segment+1][column])
                        refined_pwl = (refined_left+refined_right)/2
                        self.assertLessEqual(abs(refined_pwl-original_pwl), tolerance)
            for run in RUNS:
                source = path.read_bytes() if run == 'baseline' else (output/'inputs'/f'refined_{name}.csv').read_bytes()
                self.assertEqual((output/'runs'/run/'inputs'/f'{name}.csv').read_bytes(), source)
        expected_runs = [('baseline', 48000, 5, 64, 8),
                         ('matched_step', 96000, 9, 32, 4), ('finer_step', 96000, 9, 64, 8)]
        for name, rate, count, fsteps, vsteps in expected_runs:
            metadata = manifest['runs'][name]
            for key, expected in [('rate_hz', rate), ('samples', count),
                                  ('filter_substeps', fsteps), ('vca_steps', vsteps)]:
                self.assertEqual(metadata[key], expected)
            run = output/'runs'/name
            cm = json.loads((run/'manifest.json').read_text())
            vm = json.loads((run/'vca_reference_input/manifest.json').read_text())
            fm = json.loads((run/'filter/manifest.json').read_text())
            self.assertEqual(cm['endpoint_span_s'], 4/48000)
            self.assertFalse(cm['compare_realtime'])
            self.assertIsNone(cm['wav_gain'])
            self.assertIsNone(cm['comparison'])
            self.assertEqual(vm['initial_li'], .12)
            self.assertEqual(vm['coarse_steps_per_interval'], vsteps)
            self.assertEqual(fm['export_substeps'], 2*fsteps)
            self.assertEqual(rows(run/'vca_reference_input/trace.csv')[0]['li'], .12)
        self.assert_common_clock(output, manifest)
        artifacts = manifest['artifacts']
        expected_paths = {str(path.relative_to(output)) for path in output.rglob('*') if path.is_file()}
        self.assertEqual(set(artifacts), expected_paths-{'manifest.json'})
        for relative, metadata in artifacts.items():
            data = (output/relative).read_bytes()
            self.assertEqual(metadata['bytes'], len(data))
            self.assertEqual(metadata['sha256'], hashlib.sha256(data).hexdigest())
        for name, source in [('filter', Path(FILTER)), ('vca', Path(VCA)),
                             ('render_reference_chain.py', CHAIN_SCRIPT),
                             ('study_reference_chain_clock.py', SCRIPT)]:
            self.assertEqual((output/'tools'/name).read_bytes(), source.read_bytes())
        self.assertFalse(list(output.rglob('*.wav')))
        self.assertFalse(list(output.rglob('realtime.csv')))
        self.assertFalse(list(output.rglob('vca_production_input')))

    def test_zero_output_metrics_have_null_relative_denominators(self):
        self.write([.2, -.1, .3, 0.], [(.8, -.5, 0.)]*4)
        output, manifest = self.run_ok(**{'filter-substeps': 64})
        common = self.assert_common_clock(output, manifest)
        self.assertTrue(all(row[name+'_output'] == 0 for row in common for name in RUNS))
        for key, _, _ in COMPARISONS:
            self.assertEqual(manifest['comparisons'][key]['max_abs'], 0)
            self.assertEqual(manifest['comparisons'][key]['rms_abs'], 0)
            self.assertIsNone(manifest['comparisons'][key]['relative_rms'])

    def test_metric_denominator_and_all_endpoint_formula(self):
        # Unequal endpoint values expose a mean subtraction, normalization by
        # candidate energy, omitted t=0/final values, or relative-error averaging.
        candidate, reference = [3., -1., 6., 4.], [1., 2., 2., -2.]
        actual = STUDY.metrics(candidate, reference)
        self.assertEqual(actual['max_abs'], 6.)
        self.assertEqual(actual['rms_abs'], math.sqrt(65/4))
        self.assertEqual(actual['relative_rms'], math.sqrt(65/13))
        self.assertEqual(actual['sample_count'], 4)
        zero_reference = STUDY.metrics(candidate, [0.]*4)
        self.assertEqual(zero_reference['max_abs'], 6.)
        self.assertEqual(zero_reference['rms_abs'], math.sqrt(62/4))
        self.assertIsNone(zero_reference['relative_rms'])

    def test_minimum_endpoint_and_maximum_rate_boundary(self):
        self.write([.1, .11], [(.8, -.5, .9)]*2)
        output, manifest = self.run_ok(rate=192000)
        self.assert_common_clock(output, manifest, rate=192000)
        self.assertEqual(len(rows(output/'runs/finer_step/chain.csv')), 3)
        self.assertEqual(rows(output/'runs/finer_step/chain.csv')[-1]['time_s'], 1/192000)

    def test_determinism_and_original_fixed_chain_bytes_unchanged(self):
        direct = self.root/'direct'
        process = self.execute(script=CHAIN_SCRIPT, out=direct)
        self.assertEqual(process.returncode, 0, process.stderr)
        first, first_manifest = self.run_ok()
        second, second_manifest = self.run_ok(out=self.root/'again')
        self.assertEqual(first_manifest, second_manifest)
        for path in first.rglob('*'):
            if path.is_file():
                self.assertEqual(path.read_bytes(), (second/path.relative_to(first)).read_bytes())
        for path in direct.rglob('*'):
            if path.is_file():
                self.assertEqual(path.read_bytes(), (first/'runs/baseline'/path.relative_to(direct)).read_bytes())
        self.assert_clean()

    def test_reference_only_flags_duplicates_and_control_conflicts_rejected(self):
        for changes in [{'compare-realtime': 'true'}, {'compare-realtime': 'false'},
                        {'wav-gain': 0}, {'wav-gain': .1}, {'hp-hz': None},
                        {'lp-hz': None}, {'q': None}]:
            with self.subTest(changes=changes):
                self.reject(**changes)
        for tail in [('--rate', '48000'), ('--unknown', '1'), ('--rat', '48000')]:
            self.reject(tail=tail)
        options = self.dynamic([(200, 2000, 1)]*self.count)
        for key, value in [('hp-hz', 200), ('lp-hz', 2000), ('q', 1)]:
            self.reject(**{**options, key: value})
        self.reject(tail=('--filter-controls', str(self.filter_controls)), **options)

    def test_preflight_even_steps_refined_bounds_and_aggregate_work(self):
        invalid = [{'rate': 192001}, {'rate': 384000}, {'rate': '48000.0'},
                   {'samples': 50002}, {'samples': 100001}, {'samples': 1},
                   {'filter-substeps': 1}, {'filter-substeps': 3}, {'filter-substeps': 255},
                   {'filter-substeps': 258}, {'vca-steps': 1}, {'vca-steps': 3},
                   {'vca-steps': 127}, {'vca-steps': 130},
                   {'samples': 34724}, {'samples': 50001},
                   {'ota-level': 'nan'}, {'filter-tolerance': 'inf'},
                   {'vca-tolerance': 0}, {'initial-li': '1e-999'}, {'initial-li': 9}]
        with contextlib.redirect_stderr(io.StringIO()):
            for changes in invalid:
                with self.subTest(changes=changes), self.assertRaises((ValueError, SystemExit)):
                    STUDY.parse(self.args(**changes))
            # Numerical limits are checked before reading source rows or tools.
            for changes in [{'samples': 34723},
                            {'samples': 50001, 'filter-substeps': 2, 'vca-steps': 2},
                            {'samples': 2, 'rate': 192000, 'filter-substeps': 256, 'vca-steps': 128}]:
                STUDY.parse(self.args(**changes))
        marker = self.root/'executed'
        fail = self.shim('must-not-run', f"from pathlib import Path\nPath({str(marker)!r}).write_text('ran')\n")
        self.reject(filter=fail, vca=fail, samples=34724)
        self.assertFalse(marker.exists())

    def test_nonfinite_malformed_and_nonregular_inputs_rejected(self):
        options = self.dynamic([(200, 2000, 1)]*self.count)
        for path, field in [(self.signal, 1), (self.controls, 2), (self.filter_controls, 3)]:
            original = path.read_bytes()
            lines = original.splitlines(keepends=True)
            first = lines[1].rstrip(b'\n').split(b',')
            malformed = [original[:-1], original+b'\n', b''.join(lines[:1]+lines[2:])]
            for value in (b'nan', b'inf', b'1e-999', b' 1', b'"1"', b'1_0', b'0x1', b'\xff'):
                row = first.copy(); row[field] = value
                malformed.append(lines[0]+b','.join(row)+b'\n'+b''.join(lines[2:]))
            row = first.copy(); row[0] = b'00'
            malformed.append(lines[0]+b','.join(row)+b'\n'+b''.join(lines[2:]))
            for index, data in enumerate(malformed):
                with self.subTest(input=path.name, variant=index):
                    path.write_bytes(data)
                    self.reject(**options)
            path.write_bytes(original)
        self.reject(input=self.root)
        fifo = self.root/'fifo'
        os.mkfifo(fifo)
        self.reject(controls=fifo)

    def test_child_failures_and_corrupt_artifacts_do_not_publish(self):
        failure = self.shim('failed-vca', "import sys\nsys.stderr.write('x'*50000+'meaningful clock-study failure')\nsys.exit(7)\n")
        result = self.reject(vca=failure)
        self.assertIn('meaningful clock-study failure', result.stderr)
        self.assertLess(len(result.stderr), 18000)
        for name, mutation in [
                ('schema', "m['schema']=999"),
                ('clock', "m['rate_hz']=8000"),
                ('hash', "m['input']['sha256']='0'*64"),
                ('nan', "p=out/'trace.csv';p.write_text(p.read_text().replace(',0,0,',',0,nan,',1))"),
                ('truncated', "p=out/'trace.csv';p.write_bytes(p.read_bytes().rsplit(b'\\n',2)[0]+b'\\n')"),
                ('later', "\nif int(sys.argv[sys.argv.index('--input-rate')+1]) == 96000: sys.exit(7)"),
                ('third', "\nif (int(sys.argv[sys.argv.index('--input-rate')+1]) == 96000 and int(sys.argv[sys.argv.index('--substeps')+1]) == 16): sys.exit(7)")]:
            with self.subTest(mutation=name):
                shim = self.shim('filter-'+name, f'''import json,pathlib,subprocess,sys
subprocess.run([{FILTER!r},*sys.argv[1:]],check=True)
out=pathlib.Path(sys.argv[sys.argv.index('--out')+1])
p=out/'manifest.json';m=json.loads(p.read_text())
{mutation}
(out/'manifest.json').write_text(json.dumps(m))
''')
                self.reject(filter=shim)

    def test_completed_run_workflow_binary_and_hash_corruption_rejected(self):
        verify = STUDY.verify_run
        for mutation in ('workflow', 'binary', 'trace', 'artifact_hash'):
            visited = []
            def corrupt(directory, *args, **kwargs):
                visited.append(directory.name)
                # Fail only after the third component bundle has completed,
                # exercising adoption/cleanup of all three successful runs.
                if directory.name == 'finer_step':
                    if mutation == 'artifact_hash':
                        path = directory/'manifest.json'
                        manifest = json.loads(path.read_text())
                        manifest['artifacts']['chain.csv']['sha256'] = '0'*64
                        path.write_text(json.dumps(manifest))
                    else:
                        path = directory/{'workflow': 'tools/workflow.py',
                                          'binary': 'tools/filter',
                                          'trace': 'chain.csv'}[mutation]
                        path.chmod(0o600)
                        path.write_bytes(path.read_bytes()+b'corrupted after completion\n')
                return verify(directory, *args, **kwargs)
            errors = io.StringIO()
            with self.subTest(mutation=mutation), \
                    patch.object(STUDY, 'verify_run', side_effect=corrupt), \
                    contextlib.redirect_stderr(errors), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(STUDY.main(self.args()), 1)
            self.assertEqual(visited, list(RUNS))
            self.assertIn('mismatch', errors.getvalue())
            self.assertFalse(self.out.exists())
            self.assert_clean()

    def test_later_run_cannot_change_previously_verified_baseline(self):
        verify = STUDY.verify_run
        original_paths = {path.name for path in self.root.iterdir()}
        for relative in ('chain.csv', 'filter/trace.csv'):
            verified, changed = [], []
            def corrupt_earlier_run(directory, *args, **kwargs):
                result = verify(directory, *args, **kwargs)
                verified.append(directory.name)
                # Baseline was verified and its output selected before this
                # point. Corrupt it only after the third run also succeeds.
                if directory.name == 'finer_step' and not changed:
                    self.assertEqual(verified[:3], list(RUNS))
                    path = directory.parent/'baseline'/relative
                    path.write_bytes(path.read_bytes()+b'changed by a later run\n')
                    changed.append(relative)
                return result
            errors = io.StringIO()
            with self.subTest(artifact=relative), \
                    patch.object(STUDY, 'verify_run', side_effect=corrupt_earlier_run), \
                    contextlib.redirect_stderr(errors), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(STUDY.main(self.args()), 1)
            self.assertEqual(changed, [relative])
            self.assertIn('mismatch', errors.getvalue())
            self.assertFalse(self.out.exists())
            self.assert_clean()
            self.assertEqual({path.name for path in self.root.iterdir()}, original_paths)

    def test_snapshots_remain_common_when_original_inputs_change(self):
        options = self.dynamic([(200+5*i, 2000-17*i, 1+.02*i) for i in range(self.count)])
        originals = {name: path.read_bytes() for name, path in
                     [('signal', self.signal), ('controls', self.controls),
                      ('filter_controls', self.filter_controls)]}
        shim = self.shim('source-changing-filter', f'''import pathlib,subprocess,sys
for name in {[str(self.signal), str(self.controls), str(self.filter_controls)]!r}:
    pathlib.Path(name).write_text('changed original after study preflight\\n')
subprocess.run([{FILTER!r},*sys.argv[1:]],check=True)
''')
        output, manifest = self.run_ok(filter=shim, **options)
        for name, original in originals.items():
            self.assertEqual((output/'inputs'/f'{name}.csv').read_bytes(), original)
            self.assertEqual((output/'runs/baseline/inputs'/f'{name}.csv').read_bytes(), original)
            refined = (output/'inputs'/f'refined_{name}.csv').read_bytes()
            for run in ('matched_step', 'finer_step'):
                self.assertEqual((output/'runs'/run/'inputs'/f'{name}.csv').read_bytes(), refined)
        self.assertNotEqual(self.signal.read_bytes(), originals['signal'])
        self.assert_common_clock(output, manifest)

    def test_snapshot_artifact_and_time_budgets_fail_closed(self):
        # Every individual input/tool fits this cap, but their aggregate does
        # not. Reject before launching even the first component.
        cap = max(Path(FILTER).stat().st_size, Path(VCA).stat().st_size,
                  SCRIPT.stat().st_size, CHAIN_SCRIPT.stat().st_size)+1024
        with patch.object(STUDY, 'MAX_SNAPSHOT_BYTES', cap), \
                patch.object(STUDY, 'execute') as execute, \
                contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(STUDY.main(self.args()), 1)
            execute.assert_not_called()
        self.assertFalse(self.out.exists())
        self.assert_clean()
        # Exercise artifact accounting after successful stages as well.
        with patch.object(STUDY, 'MAX_BUNDLE_BYTES', 1024), \
                contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(STUDY.main(self.args()), 1)
        self.assertFalse(self.out.exists())
        self.assert_clean()
        slow = self.shim('slow-filter', 'import time\ntime.sleep(2)\n')
        errors = io.StringIO()
        with patch.object(STUDY, 'TOTAL_TIMEOUT', .05), \
                contextlib.redirect_stderr(errors), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(STUDY.main(self.args(filter=slow)), 1)
        self.assertIn('timeout', errors.getvalue().lower())
        self.assertFalse(self.out.exists())
        self.assert_clean()

    def test_existing_directory_and_dangling_symlink_are_preserved(self):
        self.out.mkdir()
        marker = self.out/'keep'
        marker.write_bytes(b'original bytes')
        result = self.execute()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(list(self.out.iterdir()), [marker])
        self.assertEqual(marker.read_bytes(), b'original bytes')
        marker.unlink(); self.out.rmdir()
        target = self.root/'missing'
        self.out.symlink_to(target)
        result = self.execute()
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(self.out.is_symlink())
        self.assertEqual(self.out.readlink(), target)
        self.assertFalse(target.exists())
        self.assert_clean()

    def test_publication_failure_preserves_unrelated_file(self):
        real_link = STUDY.os.link
        publications = 0
        def fail_publication(source, target, *args, **kwargs):
            nonlocal publications
            target = Path(target)
            if target.is_relative_to(self.out):
                publications += 1
                if publications == 3:
                    (self.out/'unrelated').write_text('keep me')
                    raise OSError('injected clock-study publication failure')
            return real_link(source, target, *args, **kwargs)
        with patch.object(STUDY.os, 'link', side_effect=fail_publication), \
                contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(STUDY.main(self.args()), 1)
        self.assertEqual(publications, 3)
        self.assertEqual(list(self.out.iterdir()), [self.out/'unrelated'])
        self.assertEqual((self.out/'unrelated').read_text(), 'keep me')
        self.assert_clean()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--filter', required=True)
    parser.add_argument('--vca', required=True)
    args, rest = parser.parse_known_args()
    FILTER = str(Path(args.filter).resolve())
    VCA = str(Path(args.vca).resolve())
    spec = importlib.util.spec_from_file_location('chain_clock_study', SCRIPT)
    STUDY = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(STUDY)
    unittest.main(argv=[__file__, *rest])
