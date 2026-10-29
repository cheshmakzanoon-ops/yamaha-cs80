#!/usr/bin/env python3
"""Actual-engine candidate export contract; no hardware acceptance thresholds."""
import argparse
import copy
import csv
import json
import math
from pathlib import Path
import subprocess
import tempfile
import unittest

CAPTURE = None
EXPECTED_EVENTS = [
    {'sample': 4800, 'type': 'note_on', 'note': 60, 'channel': 1, 'value': .75},
    {'sample': 19200, 'type': 'poly_pressure', 'note': 60, 'channel': 1, 'value': .5},
    {'sample': 33600, 'type': 'poly_pressure', 'note': 60, 'channel': 1, 'value': 0},
    {'sample': 48000, 'type': 'note_off', 'note': 60, 'channel': 1, 'value': 0},
    {'sample': 57600, 'type': 'note_on', 'note': 67, 'channel': 1, 'value': .625},
    {'sample': 72000, 'type': 'note_off', 'note': 67, 'channel': 1, 'value': 0},
]


def validate_metadata(metadata):
    assert metadata['schema_version'] == 1
    assert metadata['capture_kind'] == 'engine_candidate_only'
    assert metadata['analog_evidence'] is False
    assert metadata['fixture_id'] == 'w0-engine-stimulus-v1'
    assert metadata['sample_rate_hz'] == 48000
    assert metadata['internal_sample_rate_hz'] == 96000
    assert metadata['sample_count'] == 96000
    assert metadata['latency_samples'] == 63
    assert metadata['latency_compensated'] is False
    assert metadata['channels'] == {'left': 'left.csv', 'right': 'right.csv'}
    assert metadata['parameter_count'] == len(metadata['settings']) == 138
    assert all(type(x) in (int, float) and math.isfinite(x) for x in metadata['settings'].values())
    assert metadata['settings']['parity_mode'] == 1
    assert metadata['settings']['volume'] == .5
    assert metadata['events'] == EXPECTED_EVENTS
    assert metadata['engine_output_processing'] == (
        'parity mode bypasses output DC blocker and output soft clip; '
        'production decimator and provisional filter saturation retained')
    for field in ('units', 'postprocessing', 'engine_output_processing', 'initial_state',
                  'transients', 'external_input', 'event_timing', 'render_blocks', 'settings_scope'):
        assert isinstance(metadata[field], str) and metadata[field]


def read_samples(path):
    with path.open(newline='') as stream:
        rows = csv.reader(stream)
        assert next(rows) == ['sample', 'value']
        samples = []
        for index, row in enumerate(rows):
            assert len(row) == 2 and row[0] == str(index)
            value = float(row[1])
            assert math.isfinite(value)
            samples.append(value)
    assert len(samples) == 96000
    assert all(x == 0 for x in samples[:4800])
    # Basic signal-presence checks, deliberately not an audio-fidelity threshold.
    assert max(abs(x) for x in samples[4800:48000]) > 1e-5
    assert max(abs(x) for x in samples[57600:72000]) > 1e-5
    return samples


class CaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        cls.a, cls.b = cls.directory / 'a', cls.directory / 'b'
        for directory in (cls.a, cls.b):
            result = subprocess.run([str(CAPTURE), '--out', str(directory)], capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(result.stderr)
        cls.metadata = json.loads((cls.a / 'capture.json').read_text())

    def test_metadata_and_fixed_stimulus(self):
        validate_metadata(self.metadata)

    def test_finite_nonzero_complete_channels(self):
        for channel in ('left.csv', 'right.csv'):
            with self.subTest(channel=channel):
                read_samples(self.a / channel)

    def test_independent_process_byte_determinism(self):
        expected = {'left.csv', 'right.csv', 'capture.json'}
        self.assertEqual({p.name for p in self.a.iterdir()}, expected)
        self.assertEqual({p.name for p in self.b.iterdir()}, expected)
        for name in expected:
            self.assertEqual((self.a / name).read_bytes(), (self.b / name).read_bytes(), name)

    def test_bad_arguments_fail(self):
        for arguments in ([], ['--unknown'], ['--out'], ['--out', str(self.directory), '--extra']):
            with self.subTest(arguments=arguments):
                result = subprocess.run([str(CAPTURE), *arguments], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)

    def test_unwritable_destination_fails(self):
        # A regular file cannot serve as a directory, even under privileged CI.
        path = self.directory / 'not-a-directory'
        path.write_text('occupied')
        result = subprocess.run([str(CAPTURE), '--out', str(path)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)

    def test_metadata_mutations_are_detected(self):
        for field, value in [('events', []), ('sample_count', 95999), ('latency_samples', 62),
                             ('analog_evidence', True), ('sample_rate_hz', 44100),
                             ('engine_output_processing', 'production DC blocker, soft clip and decimator retained')]:
            modified = copy.deepcopy(self.metadata)
            modified[field] = value
            with self.subTest(field=field), self.assertRaises(AssertionError):
                validate_metadata(modified)
        modified = copy.deepcopy(self.metadata)
        del modified['settings']['parity_mode']
        with self.assertRaises(AssertionError):
            validate_metadata(modified)

    def test_sample_mutations_are_detected(self):
        rows = (self.a / 'left.csv').read_text().splitlines()
        path = self.directory / 'mutated.csv'
        for mutation in ('nonfinite', 'index', 'truncated', 'silent'):
            modified = rows.copy()
            if mutation == 'nonfinite':
                modified[6000] = '5999,nan'
            elif mutation == 'index':
                modified[6000] = '5998,0'
            elif mutation == 'truncated':
                modified.pop()
            else:
                modified = ['sample,value'] + [f'{i},0' for i in range(96000)]
            path.write_text('\n'.join(modified) + '\n')
            with self.subTest(mutation=mutation), self.assertRaises(AssertionError):
                read_samples(path)


if __name__ == '__main__':
    if not __debug__:
        raise SystemExit('Run without -O: validation assertions must remain enabled')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    arguments, remaining = parser.parse_known_args()
    CAPTURE = arguments.capture.resolve()
    unittest.main(argv=[__file__, *remaining])
