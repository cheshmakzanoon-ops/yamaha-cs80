#!/usr/bin/env python3
"""W0 acceptance and deliberate corruption/mutation tests (stdlib only)."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('parity_harness', ROOT / 'scripts/parity_harness.py')
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)


class HarnessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.path = h.synthetic(self.directory)
        self.manifest = json.loads(self.path.read_text())

    def save(self):
        h.write_json(self.path, self.manifest)

    def compare(self):
        self.save()
        return h.compare(self.path)

    def edit_capture(self, transform):
        descriptor = self.manifest['stages'][0]['candidate']
        path = self.directory / descriptor['path']
        rows = path.read_text().splitlines()
        transform(rows)
        path.write_text('\n'.join(rows) + '\n')
        descriptor['sha256'] = h.sha256(path.read_bytes())

    def test_end_to_end_reports_all_stages_and_provenance(self):
        report = self.compare()
        self.assertTrue(report['passed'])
        self.assertFalse(report['analog_evidence'])
        self.assertEqual(len(report['stages']), 4)
        self.assertEqual(report['manifest_sha256'], h.sha256(self.path.read_bytes()))
        self.assertEqual(report['tolerances_sha256'], h.sha256(h.DEFAULT_TOLERANCES.read_bytes()))
        for stage in report['stages']:
            self.assertEqual(stage['alignment']['latency_samples'], 63)
            self.assertGreater(stage['unaligned_rms_error'], 1e-3)
            self.assertAlmostEqual(stage['metrics']['rms_error'], 1e-10, delta=1e-16)
            self.assertLess(stage['metrics']['abs_bias_error'], 1e-16)

    def test_generator_is_byte_deterministic(self):
        before = {p.name: p.read_bytes() for p in self.directory.iterdir()}
        h.synthetic(self.directory)
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.directory.iterdir()})

    def test_missing_provenance_fields_fail_closed(self):
        original = copy.deepcopy(self.manifest)
        for key in original['provenance']:
            with self.subTest(field=key):
                self.manifest = copy.deepcopy(original)
                del self.manifest['provenance'][key]
                with self.assertRaises(h.InvalidEvidence):
                    self.compare()

    def test_hardware_cannot_use_synthetic_acceptance(self):
        self.manifest['evidence_kind'] = 'hardware'
        with self.assertRaisesRegex(h.InvalidEvidence, 'hardware acceptance'):
            self.compare()

    def test_removed_stage_cannot_silently_pass(self):
        self.manifest['stages'].pop()
        with self.assertRaises(h.InvalidEvidence):
            self.compare()

    def test_duplicate_stage_rejected(self):
        self.manifest['stages'].append(copy.deepcopy(self.manifest['stages'][0]))
        with self.assertRaises(h.InvalidEvidence):
            self.compare()

    def test_hash_mutation_rejected(self):
        descriptor = self.manifest['stages'][0]['reference']
        (self.directory / descriptor['path']).write_text('tampered')
        with self.assertRaisesRegex(h.InvalidEvidence, 'hash mismatch'):
            self.compare()

    def test_wrong_latency_rejected_not_truncated(self):
        self.manifest['stages'][0]['latency_samples'] = 62
        with self.assertRaisesRegex(h.InvalidEvidence, 'complete registered window'):
            self.compare()

    def test_shift_mutation_fails_metrics_even_with_updated_hash(self):
        def shift(rows):
            values = [row.split(',')[1] for row in rows[1:]]
            values = [values[-1]] + values[:-1]
            rows[1:] = [f'{i},{value}' for i, value in enumerate(values)]
        self.edit_capture(shift)
        self.assertFalse(self.compare()['passed'])

    def test_gain_mutation_fails_metrics(self):
        def gain(rows):
            for i in range(1, len(rows)):
                index, value = rows[i].split(',')
                rows[i] = f'{index},{float(value) * 1.01:.17g}'
        self.edit_capture(gain)
        self.assertFalse(self.compare()['passed'])

    def test_dc_mutation_fails_bias_metric(self):
        def bias(rows):
            for i in range(64, len(rows)):
                index, value = rows[i].split(',')
                rows[i] = f'{index},{float(value) + 0.001:.17g}'
        self.edit_capture(bias)
        report = self.compare()
        self.assertFalse(report['passed'])
        self.assertAlmostEqual(report['stages'][0]['metrics']['abs_bias_error'], .001)

    def test_nonfinite_capture_rejected_after_hash_update(self):
        original = (self.directory / 'oscillator-candidate.csv').read_bytes()
        for value in ('nan', 'inf', '-inf', '1e999'):
            with self.subTest(value=value):
                (self.directory / 'oscillator-candidate.csv').write_bytes(original)
                self.edit_capture(lambda rows: rows.__setitem__(64, '63,' + value))
                with self.assertRaisesRegex(h.InvalidEvidence, 'Nonfinite sample'):
                    self.compare()

    def test_sample_index_mutation_rejected(self):
        self.edit_capture(lambda rows: rows.__setitem__(64, '64,0'))
        with self.assertRaises(h.InvalidEvidence):
            self.compare()

    def test_empty_or_truncated_captures_rejected(self):
        self.edit_capture(lambda rows: rows.clear())
        with self.assertRaises(h.InvalidEvidence):
            self.compare()

    def test_path_traversal_rejected(self):
        self.manifest['stages'][0]['reference']['path'] = '../outside.csv'
        with self.assertRaisesRegex(h.InvalidEvidence, 'escapes corpus'):
            self.compare()

    def test_symlink_escape_rejected(self):
        with tempfile.TemporaryDirectory() as outside:
            destination = Path(outside) / 'trace.csv'
            destination.write_bytes((self.directory / 'oscillator-reference.csv').read_bytes())
            link = self.directory / 'escape.csv'
            try:
                link.symlink_to(destination)
            except (OSError, NotImplementedError):
                self.skipTest('symlink creation unavailable on this platform')
            self.manifest['stages'][0]['reference']['path'] = link.name
            with self.assertRaisesRegex(h.InvalidEvidence, 'symlink escapes'):
                self.compare()

    def test_malformed_top_level_and_boolean_schema_rejected(self):
        original = copy.deepcopy(self.manifest)
        for value in (None, [], {}, True):
            h.write_json(self.path, value)
            with self.subTest(value=value), self.assertRaises(h.InvalidEvidence):
                h.compare(self.path)
        self.manifest = original
        self.manifest['schema_version'] = True
        with self.assertRaises(h.InvalidEvidence):
            self.compare()

    def test_missing_capture_rejected(self):
        (self.directory / 'filter-reference.csv').unlink()
        with self.assertRaises(OSError):
            self.compare()

    def test_invalid_metadata_rejected(self):
        original = copy.deepcopy(self.manifest)
        for key, value in [('sample_rate_hz', True), ('latency_samples', -1),
                           ('latency_samples', .5), ('settings', {}), ('split', 'unknown'),
                           ('source_row', ''), ('comparison_samples', 0), ('units', None)]:
            with self.subTest(key=key, value=value):
                self.manifest = copy.deepcopy(original)
                self.manifest['stages'][0][key] = value
                with self.assertRaises(h.InvalidEvidence):
                    self.compare()

    def test_nonfinite_and_duplicate_json_rejected(self):
        for contents in ('{"x":NaN}', '{"x":1e999}', '{"x":1,"x":2}'):
            self.path.write_text(contents)
            with self.assertRaises(h.InvalidEvidence):
                h.compare(self.path)

    def test_invalid_tolerances_rejected(self):
        original = json.loads(h.DEFAULT_TOLERANCES.read_text())
        tolerance_path = self.directory / 'limits.json'
        for value in (-1, True, '1e-8', None):
            tolerance = copy.deepcopy(original)
            tolerance['stages']['oscillator']['rms_error'] = value
            h.write_json(tolerance_path, tolerance)
            with self.subTest(value=value), self.assertRaises(h.InvalidEvidence):
                h.compare(self.path, tolerance_path)
        del original['stages']['output']['max_abs_error']
        h.write_json(tolerance_path, original)
        with self.assertRaises(h.InvalidEvidence):
            h.compare(self.path, tolerance_path)

    def test_cli_failure_replaces_stale_success_and_exit_codes(self):
        report_path = self.directory / 'report.json'
        command = [sys.executable, str(ROOT / 'scripts/parity_harness.py'), 'compare',
                   '--manifest', str(self.path), '--report', str(report_path)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.edit_capture(lambda rows: rows.__setitem__(64, '63,0.5'))
        self.save()
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertFalse(json.loads(report_path.read_text())['passed'])
        del self.manifest['provenance']
        self.save()
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertFalse(json.loads(report_path.read_text())['valid'])


if __name__ == '__main__':
    unittest.main()
