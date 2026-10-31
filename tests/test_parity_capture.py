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


def parse_metadata(text):
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate JSON key")
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValueError("Nonfinite JSON constant: " + value)

    return json.loads(text, object_pairs_hook=unique_pairs, parse_constant=invalid_constant)


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
    assert metadata['parameter_count'] == len(metadata['settings']) == 139
    assert all(type(x) in (int, float) and math.isfinite(x) for x in metadata['settings'].values())
    assert metadata['settings']['parity_mode'] == 1
    assert metadata['settings']['source_performance'] == 0
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
        cls.metadata = parse_metadata((cls.a / 'capture.json').read_text())

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


# This fixture checks the existing software law, not an analog envelope model.
def f32(value):
    import struct
    return struct.unpack('f', struct.pack('f', value))[0]


LIVE_BATCHES = {1: (8640, 'a', (0.2, 0.25), 1),
                2: (28800, 'd', (0.8, 1.2), 2),
                4: (52800, 'r', (0.4, 0.6), 4)}


def validate_live_metadata(metadata, default_settings):
    assert set(metadata) == {
        "schema_version", "fixture_id", "capture_kind", "analog_evidence", "sample_rate_hz",
        "sample_count", "internal_sample_rate_hz", "latency_samples", "latency_compensated",
        "channels", "envelope_trace", "event_timing", "render_blocks", "parameter_application",
        "initial_state", "units", "postprocessing", "engine_output_processing", "external_input",
        "parameter_count", "initial_settings", "final_settings", "events"}
    assert metadata['schema_version'] == 2
    assert metadata['fixture_id'] == 'live-adr-v1'
    assert metadata['capture_kind'] == 'engine_candidate_only'
    assert metadata['analog_evidence'] is False
    assert metadata['sample_rate_hz'] == 48000
    assert metadata['internal_sample_rate_hz'] == 96000
    assert metadata['sample_count'] == 96000
    assert metadata['latency_samples'] == 63
    assert metadata['latency_compensated'] is False
    assert metadata['parameter_count'] == 139
    assert metadata['channels'] == {'left': 'left.csv', 'right': 'right.csv'}
    assert metadata['envelope_trace'] == 'envelope-events.csv'
    assert metadata['event_timing'] == 'zero-based host sample; sequence order; applied before rendering that sample'
    assert metadata['render_blocks'] == 'split at events, one-host-frame probes and capture end'
    assert metadata['parameter_application'] == 'one full setParams per parameter_batch; old/new are submitted core values'
    assert default_settings['source_performance'] == 0
    expected_initial = dict(default_settings)
    for name in ('master_sustain', 'key_release', 'sustain_mode', 'sustain_assign'):
        expected_initial[name] = 0
    for rank in (1, 2):
        for envelope in ('v', 'f'):
            for phase, value in (('a', 1), ('d', 2), ('r', 2)):
                expected_initial[f'ch{rank}_{envelope}_{phase}'] = value
        expected_initial[f'ch{rank}_v_s'] = .25
        expected_initial[f'ch{rank}_f_il'] = 0
        expected_initial[f'ch{rank}_f_al'] = f32(.8)
    for name in ('initial_settings', 'final_settings'):
        assert set(metadata[name]) == set(default_settings)
        assert all(type(value) in (int, float) and math.isfinite(value)
                   for value in metadata[name].values())
    assert {key: f32(value) for key, value in metadata['initial_settings'].items()} == {
        key: f32(value) for key, value in expected_initial.items()}
    settings = dict(metadata['initial_settings'])
    assert len(metadata['events']) == 5
    for sequence, event in enumerate(metadata['events']):
        assert type(event['sequence']) is int and event['sequence'] == sequence
        assert type(event['sample']) is int
        if sequence in (0, 3):
            assert event == {'sequence': sequence, 'sample': 4800 if sequence == 0 else 48000,
                             'type': 'note_on' if sequence == 0 else 'note_off',
                             'note': 60, 'channel': 1, 'value': .75 if sequence == 0 else 0}
            continue
        frame, phase, values, _ = LIVE_BATCHES[sequence]
        assert set(event) == {'sequence', 'sample', 'type', 'changes'}
        assert event['sample'] == frame and event['type'] == 'parameter_batch'
        expected_ids = [f'ch{rank}_{envelope}_{phase}' for rank in (1, 2) for envelope in ('v', 'f')]
        assert [change['id'] for change in event['changes']] == expected_ids
        for index, change in enumerate(event['changes']):
            assert set(change) == {'id', 'old', 'new'}
            assert all(type(change[key]) in (int, float) and math.isfinite(change[key]) for key in ('old', 'new'))
            assert change['old'] == settings[change['id']]
            assert f32(change['new']) == f32(values[index // 2])
            settings[change['id']] = change['new']
    assert metadata['final_settings'] == settings
    for field in ('initial_state', 'units', 'postprocessing', 'engine_output_processing', 'external_input'):
        assert type(metadata[field]) is str and metadata[field]
    assert metadata['postprocessing'] == 'none: no clipping, normalization, resampling or trimming'
    assert metadata['external_input'] == 'disconnected'
    assert metadata['units'] == 'raw engine floats; software envelope state proxies, not calibrated volts'
    assert metadata['initial_state'] == 'fresh Engine; setParams; prepare; reset; no warmup or discarded samples'
    assert metadata['engine_output_processing'] == (
        'parity mode bypasses output DC blocker and output soft clip; '
        'production decimator and provisional filter saturation retained')


def read_live_probes(path):
    with path.open(newline='') as stream:
        reader = csv.DictReader(stream)
        assert reader.fieldnames == ['sequence', 'sample', 'point', 'voice', 'note', 'rank',
                                     'envelope', 'stage', 'value', 'elapsed', 'active', 'held']
        rows = list(reader)
    validate_live_probes(rows)
    return rows


def half_ulp32(value):
    """Maximum round-to-nearest error for a finite binary32 snapshot."""
    return 2**-150 if abs(value) < 2**-126 else 2.0**(math.frexp(abs(value))[1] - 25)


def validate_live_probes(rows):
    assert len(rows) == 36
    cursor = 0
    for sequence, (frame, phase, times, stage) in LIVE_BATCHES.items():
        before = {}
        for point in ('before', 'after', 'after_one_host_frame'):
            for rank in (0, 1):
                for envelope in ('amplitude', 'filter'):
                    row = rows[cursor]
                    cursor += 1
                    assert row['sequence'] == str(sequence)
                    assert row['sample'] == str(frame + (point == 'after_one_host_frame'))
                    assert row['point'] == point
                    assert row['voice'] == '0' and row['note'] == '60'
                    assert row['rank'] == str(rank) and row['envelope'] == envelope
                    assert row['stage'] == str(stage) and row['active'] == '1'
                    assert row['held'] == ('0' if phase == 'r' else '1')
                    value, elapsed = float(row['value']), float(row['elapsed'])
                    assert math.isfinite(value) and math.isfinite(elapsed)
                    assert elapsed > .06  # Beyond every provisional fast-bend window.
                    key = (rank, envelope)
                    if point == 'before':
                        before[key] = (value, elapsed)
                    elif point == 'after':
                        assert (value, elapsed) == before[key]
                    else:
                        expected, old_elapsed = before[key]
                        target = (1 if envelope == 'amplitude' else f32(3 * f32(.8))) if phase == 'a' else (
                            .25 if phase == 'd' and envelope == 'amplitude' else 0)
                        assert abs(expected - target) > .005
                        if phase == "a":
                            assert old_elapsed < times[rank] * 1.2 - .01
                        # Reconstruction retains double state/clock; CSV contains
                        # round-trippable binary32 snapshots, not that hidden state.
                        # Propagate the prior half-ULP uncertainty through the
                        # contractive two-tick law, then allow final snapshot rounding.
                        # Circuit-engine RC law (docs/ENVELOPE_RC_MODEL.md): decay and
                        # release settle to -60 dB at T; attack charges from its start
                        # level (0 here, both IL and the amplitude onset) toward
                        # 1.5 x the peak and reaches the peak at T.
                        if phase == 'a':
                            coefficient = math.exp(-math.log(3) / (f32(times[rank]) * 96000))
                            target = 1.5 * target
                        else:
                            coefficient = math.exp(-math.log(1000) / (f32(times[rank]) * 96000))
                        expected = f32(expected)
                        initial_error = half_ulp32(expected)
                        for _ in range(2):
                            if phase == 'r' and envelope == 'filter':
                                expected += (target - expected) * (1 - coefficient)
                            elif phase == 'r':
                                expected *= coefficient
                            else:
                                expected = target + (expected - target) * coefficient
                        value = f32(value)
                        # At most 16 double arithmetic rounding operations; this
                        # term is negligible next to the two float snapshots.
                        arithmetic_error = 16 * 2**-52 * (1 + abs(expected) + abs(target))
                        value_bound = initial_error * coefficient**2 + half_ulp32(value) + arithmetic_error
                        assert abs(value - expected) <= value_bound
                        old_elapsed = f32(old_elapsed)
                        expected_elapsed = old_elapsed + 2 / 96000
                        elapsed = f32(elapsed)
                        time_bound = half_ulp32(old_elapsed) + half_ulp32(elapsed) + 2 * math.ulp(expected_elapsed)
                        assert abs(elapsed - expected_elapsed) <= time_bound
                        assert abs(value - before[key][0]) > 1e-6


class LiveCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        cls.a, cls.b = cls.directory / 'a', cls.directory / 'b'
        for directory in (cls.a, cls.b):
            subprocess.run([str(CAPTURE), '--out', str(directory), '--fixture', 'live-adr-v1'], check=True, capture_output=True)
        default = cls.directory / 'default'
        subprocess.run([str(CAPTURE), '--out', str(default)], check=True, capture_output=True)
        cls.default_settings = parse_metadata((default / 'capture.json').read_text())['settings']
        cls.metadata = parse_metadata((cls.a / 'capture.json').read_text())

    def test_live_metadata_replay_and_probe_law(self):
        validate_live_metadata(self.metadata, self.default_settings)
        read_live_probes(self.a / 'envelope-events.csv')

    def test_live_independent_process_determinism_and_audio(self):
        expected = {'left.csv', 'right.csv', 'capture.json', 'envelope-events.csv'}
        for directory in (self.a, self.b):
            self.assertEqual({p.name for p in directory.iterdir()}, expected)
        for name in expected:
            self.assertEqual((self.a / name).read_bytes(), (self.b / name).read_bytes())
        for channel in ('left.csv', 'right.csv'):
            samples = read_samples(self.a / channel)
            self.assertGreater(max(abs(x) for x in samples[48000:57600]), 1e-5)

    def test_strict_json_and_failed_rerun_manifest(self):
        for text in ('{"schema_version":2,"schema_version":2}', '{"value":NaN}', '{"value":Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_metadata(text)
        for fixture in ([], ['--fixture', 'live-adr-v1']):
            directory = self.directory / ('failed-live' if fixture else 'failed-default')
            directory.mkdir()
            (directory / 'capture.json').write_text('{"old_success":true}')
            (directory / 'envelope-events.csv').mkdir()
            (directory / 'envelope-events.csv' / 'keep.txt').write_text('unrelated')
            result = subprocess.run([str(CAPTURE), '--out', str(directory), *fixture], capture_output=True)
            self.assertEqual(result.returncode, 2)
            self.assertFalse((directory / 'capture.json').exists())
            self.assertEqual((directory / 'envelope-events.csv' / 'keep.txt').read_text(), 'unrelated')

    def test_live_metadata_mutations(self):
        mutations = [lambda m: m['events'].pop(),
                     lambda m: m['events'].append(copy.deepcopy(m['events'][1])),
                     lambda m: m['events'].reverse(),
                     lambda m: m['events'][1].update(sample=8641),
                     lambda m: m['events'][1]['changes'][0].update(id='ch2_v_a'),
                     lambda m: m['events'][1]['changes'][0].update(new=.5),
                     lambda m: m['events'][1]['changes'][0].update(old=0),
                     lambda m: m['events'][1]['changes'][0].update(new=float('nan')),
                     lambda m: m['events'][1]['changes'].pop(),
                     lambda m: m['final_settings'].update(ch1_v_s=.9),
                     lambda m: m.update(analog_evidence=True),
                     lambda m: m.update(engine_output_processing='production clipping retained'),
                     lambda m: m.update(units='calibrated volts'),
                     lambda m: m.update(initial_state='warmup discarded')]
        for index, mutate in enumerate(mutations):
            modified = copy.deepcopy(self.metadata)
            mutate(modified)
            with self.subTest(index=index), self.assertRaises(AssertionError):
                validate_live_metadata(modified, self.default_settings)

    def test_live_probe_mutations(self):
        original = read_live_probes(self.a / 'envelope-events.csv')
        mutations = [lambda rows: rows.pop(), lambda rows: rows.reverse(),
                     lambda rows: rows[4].update(value='0'),
                     lambda rows: rows[4].update(elapsed='0'),
                     lambda rows: rows[8].update(value=rows[0]['value']),
                     lambda rows: rows[8].update(sample='8642'),
                     lambda rows: rows[8].update(elapsed=rows[0]['elapsed']),
                     # A stale attack coefficient (1 second instead of .2) must fail.
                     lambda rows: rows[8].update(value=str(f32(1 + (float(rows[0]['value']) - 1)
                                                                  * math.exp(-12 / 96000)))),
                     lambda rows: rows[8].update(value='nan'),
                     lambda rows: rows[0].update(stage='3')]
        for index, mutate in enumerate(mutations):
            modified = copy.deepcopy(original)
            mutate(modified)
            with self.subTest(index=index), self.assertRaises(AssertionError):
                validate_live_probes(modified)

    def test_fixture_switch_removes_only_known_stale_trace(self):
        directory = self.directory / 'switch'
        subprocess.run([str(CAPTURE), '--out', str(directory), '--fixture', 'live-adr-v1'], check=True, capture_output=True)
        (directory / 'keep.txt').write_text('unrelated')
        subprocess.run([str(CAPTURE), '--out', str(directory)], check=True, capture_output=True)
        self.assertFalse((directory / 'envelope-events.csv').exists())
        self.assertEqual((directory / 'keep.txt').read_text(), 'unrelated')
        validate_metadata(parse_metadata((directory / 'capture.json').read_text()))
        before = {p.name: p.read_bytes() for p in directory.iterdir()}
        result = subprocess.run([str(CAPTURE), '--out', str(directory), '--fixture', 'unknown'], capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(before, {p.name: p.read_bytes() for p in directory.iterdir()})

if __name__ == '__main__':
    if not __debug__:
        raise SystemExit('Run without -O: validation assertions must remain enabled')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    arguments, remaining = parser.parse_known_args()
    CAPTURE = arguments.capture.resolve()
    unittest.main(argv=[__file__, *remaining])
