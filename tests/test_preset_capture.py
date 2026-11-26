#!/usr/bin/env python3
"""Factory capture file/measurement contracts, not an acoustic similarity test."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import unittest


parser = argparse.ArgumentParser()
parser.add_argument('--capture', required=True)
args, remaining = parser.parse_known_args()
CAPTURE = str(Path(args.capture).resolve())
RATE, HOLD, TAIL = 44100, .10001, .01001
HELD, FRAMES = 4410, 4851  # Round each duration separately, at a non-block boundary.
PRESET = 'Init Circuit'


def run(*arguments, **kwargs):
    return subprocess.run([CAPTURE, *map(str, arguments)], capture_output=True,
                          text=True, encoding='utf-8', timeout=30, **kwargs)


def read_wav(path):
    """Walk RIFF chunks independently; float WAVE is not PCM16/PCM32."""
    raw = path.read_bytes()
    magic, size, kind = struct.unpack_from('<4sI4s', raw)
    assert (magic, kind, size) == (b'RIFF', b'WAVE', len(raw) - 8)
    chunks, at = {}, 12
    while at < len(raw):
        tag, size = struct.unpack_from('<4sI', raw, at)
        assert tag not in chunks and at + 8 + size <= len(raw)
        chunks[tag] = raw[at + 8:at + 8 + size]
        at += 8 + size + size % 2
    assert at == len(raw)
    fmt = struct.unpack('<HHIIHHH', chunks[b'fmt '])
    assert fmt[:2] == (3, 2)  # WAVE_FORMAT_IEEE_FLOAT, stereo.
    assert fmt[3:] == (fmt[2] * 8, 8, 32, 0)
    frames, = struct.unpack('<I', chunks[b'fact'])
    data = chunks[b'data']
    assert len(data) == frames * 8
    values = struct.unpack('<' + 'f' * (frames * 2), data)
    assert all(map(math.isfinite, values))
    return fmt[2], frames, data, values


def read_row(path):
    with path.open(encoding='utf-8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 1
    return rows[0]


def parameter_digest(values):
    # FNV-1a over the actual little-endian binary32 parameter representation.
    digest = 14695981039346656037
    for byte in struct.pack('<' + 'f' * len(values), *values):
        digest = ((digest ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return format(digest, 'x')


class PresetCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        result = run('--inventory', cls.root / 'inventory.json')
        if result.returncode:
            raise RuntimeError(result.stderr)
        cls.inventory = json.loads((cls.root / 'inventory.json').read_text(encoding='utf-8'))
        cls.options = ['--preset', PRESET, '--rate', str(RATE), '--note', '69',
                       '--velocity', '.75', '--pressure', '.25', '--hold', str(HOLD),
                       '--tail', str(TAIL)]
        for name in ('a', 'b'):
            result = run('--out', cls.root / name, '--wav', *cls.options)
            if result.returncode:
                raise RuntimeError(result.stdout + result.stderr)
        cls.a = cls.root / 'a'
        cls.wav_path, = cls.a.glob('*.wav')
        cls.rate, cls.frames, cls.data, cls.values = read_wav(cls.wav_path)
        cls.row = read_row(cls.a / 'metrics.csv')

    def assert_metric(self, field, expected):
        actual = float(self.row[field])
        self.assertTrue(math.isfinite(actual), field)
        self.assertTrue(math.isclose(actual, expected, rel_tol=2e-9, abs_tol=1e-9),
                        f'{field}: {actual} != {expected}')

    def test_inventory_listing_and_rendered_parameter_identity(self):
        inventory = self.inventory
        self.assertEqual(inventory['schema'], 1)
        self.assertTrue(inventory['library_revision'])
        ids = inventory['parameter_ids']
        self.assertEqual(len(ids), len(set(ids)))
        programs = inventory['programs']
        self.assertEqual(len(programs), 301)
        self.assertEqual([p['index'] for p in programs], list(range(301)))
        self.assertEqual([p['host_index'] for p in programs], list(range(222, 523)))
        self.assertEqual(len({p['name'] for p in programs}), 301)
        for program in programs:
            self.assertEqual(len(program['values']), len(ids))
            self.assertTrue(all(map(math.isfinite, program['values'])))
        listed = run('--list')
        self.assertEqual(listed.returncode, 0, listed.stderr)
        self.assertEqual(listed.stdout.splitlines(),
                         [f"{p['index']}\t{p['category']}\t{p['name']}" for p in programs])
        program = next(p for p in programs if p['name'] == PRESET)
        for field in ('index', 'name', 'category'):
            self.assertEqual(self.row[field], str(program[field]))
        self.assertEqual(self.row['parameter_digest'], parameter_digest(program['values']))
        self.assertEqual(self.row['rank'], '0')
        self.assertEqual([float(self.row[k]) for k in ('note', 'sample_rate', 'velocity', 'pressure')],
                         [69, RATE, .75, .25])
        result = run('--out', self.root / 'rank', '--rank', '2', *self.options)
        self.assertEqual(result.returncode, 0, result.stderr)
        isolated = list(program['values'])
        isolated[ids.index('mix')] = 1.0
        row = read_row(self.root / 'rank' / 'metrics.csv')
        self.assertEqual(row['rank'], '2')
        self.assertEqual(row['parameter_digest'], parameter_digest(isolated))

    def test_complete_factory_inventory_keeps_existing_addresses(self):
        for library, count, offset in [('all', 1023, 0), ('original', 222, 0)]:
            path = self.root / (library + '.json')
            result = run('--inventory', path, '--library', library)
            self.assertEqual(result.returncode, 0, result.stderr)
            programs = json.loads(path.read_text(encoding='utf-8'))['programs']
            self.assertEqual(len(programs), count)
            self.assertEqual([p['host_index'] for p in programs], list(range(offset, offset + count)))
            self.assertEqual(programs[8]['name'], 'Tone I: E. Piano')
            self.assertEqual(programs[9]['name'], 'Tone II: Bass')
        result = run('--out', self.root / 'original-note', '--library', 'original',
                     '--preset', 'Tone I: E. Piano', '--hold', '.15', '--tail', '.05')
        self.assertEqual(result.returncode, 0, result.stderr)
        row = read_row(self.root / 'original-note' / 'metrics.csv')
        self.assertEqual(row['name'], 'Tone I: E. Piano')
        self.assertEqual(row['nonfinite'], '0')
        self.assertEqual(row['samples_over_full_scale'], '0')

    def test_float_riff_frames_raw_metrics_and_determinism(self):
        self.assertEqual((self.rate, self.frames), (RATE, FRAMES))
        self.assertEqual(float(self.row['hold_seconds']), HOLD)
        self.assertEqual(float(self.row['tail_seconds']), TAIL)
        files = {p.name for p in self.a.iterdir()}
        self.assertEqual(files, {'metrics.csv', self.wav_path.name})
        self.assertEqual(files, {p.name for p in (self.root / 'b').iterdir()})
        for name in files:
            self.assertEqual((self.a / name).read_bytes(), (self.root / 'b' / name).read_bytes())
        values = self.values
        def db_rms(samples):
            return 20 * math.log10(max(math.sqrt(math.fsum(x*x for x in samples) / len(samples)), 1e-12))
        self.assertGreater(max(map(abs, values)), 0)  # Presence, not a timbre threshold.
        self.assert_metric('peak_dbfs', 20 * math.log10(max(max(map(abs, values)), 1e-12)))
        self.assert_metric('rms_dbfs', db_rms(values))
        self.assert_metric('early_rms_dbfs', db_rms(values[:2 * HELD]))
        self.assert_metric('body_rms_dbfs', db_rms(values[:2 * HELD]))
        self.assert_metric('tail_rms_dbfs', db_rms(values[2 * HELD:]))
        self.assertEqual(int(self.row['nonfinite']), 0)
        self.assertEqual(int(self.row['samples_over_full_scale']), sum(abs(x) > 1 for x in values))

    def test_note_off_boundary_does_not_change_held_prefix(self):
        out = self.root / 'no-tail'
        result = run('--out', out, '--wav', *self.options, '--tail', '0')
        self.assertEqual(result.returncode, 0, result.stderr)
        path, = out.glob('*.wav')
        rate, frames, data, _ = read_wav(path)
        self.assertEqual((rate, frames), (RATE, HELD))
        self.assertEqual(data, self.data[:HELD * 8])
        self.assertEqual(float(read_row(out / 'metrics.csv')['tail_rms_dbfs']), -240)

    def test_spectrum_against_direct_dft(self):
        # Direct summation is intentionally independent of the C++ radix-2 FFT.
        width = RATE // 20
        held = list(zip(self.values[:2 * HELD:2], self.values[1:2 * HELD:2]))
        starts = range(0, HELD, width)
        peak = max(starts, key=lambda at: math.fsum(l*l + r*r for l, r in held[at:at+width])
                   / len(held[at:at+width]))
        self.assert_metric('peak_window_seconds', peak / RATE)
        first = min(max(peak - 2048 + width // 2, 0), HELD - 4096)
        windowed = [(l * (.5 - .5 * math.cos(2 * math.pi * i / 4095)),
                     r * (.5 - .5 * math.cos(2 * math.pi * i / 4095)))
                    for i, (l, r) in enumerate(held[first:first+4096])]
        spectrum = []
        for k in range(1, 2049):
            angle = -2 * math.pi * k / 4096
            step, phase = complex(math.cos(angle), math.sin(angle)), 1+0j
            left = right = 0j
            for l, r in windowed:
                left += l * phase
                right += r * phase
                phase *= step
            spectrum.append((k * RATE / 4096, (abs(left)**2 + abs(right)**2) * (1 if k == 2048 else 2)))
        total = math.fsum(p for _, p in spectrum)
        self.assert_metric('centroid_hz', math.fsum(hz*p for hz, p in spectrum) / total)
        for name, lo, hi in [('low_power', 0, 500), ('mid_power', 500, 2000),
                             ('high_power', 2000, float('inf'))]:
            self.assert_metric(name, math.fsum(p for hz, p in spectrum if lo <= hz < hi) / total)

    def test_unknown_selection_creates_and_overwrites_nothing(self):
        for exists in (False, True):
            out = self.root / f'unknown-{exists}'
            if exists:
                out.mkdir()
                (out / 'metrics.csv').write_bytes(b'existing evidence')
            before = {p.name: p.read_bytes() for p in out.iterdir()} if exists else None
            result = run('--out', out, '--wav', '--preset', PRESET, '--preset', 'No such Circuit program')
            self.assertEqual(result.returncode, 2, result.stderr)
            if exists:
                self.assertEqual({p.name: p.read_bytes() for p in out.iterdir()}, before)
            else:
                self.assertFalse(out.exists())

    def test_invalid_inputs_fail_before_artifacts(self):
        cases = [['--unknown'], ['--hold'], ['--all'], ['--preset', ''], ['--note', '-1'],
                 ['--out', ''], ['--inventory', ''],
                 ['--note', '128'], ['--note', '60.5'], ['--rank', '0'], ['--rank', '3'],
                 ['--rate', '48000.5'], ['--rate', '8000'], ['--hold', '.049999'],
                 ['--hold', '30.0001'], ['--tail', '-.01'], ['--tail', '30.0001']]
        for option in ('--velocity', '--pressure'):
            cases += [[option, value] for value in ('1.00000001', '-1e-50', '1e300')]
        for option in ('--rate', '--hold', '--tail', '--velocity', '--pressure'):
            cases += [[option, value] for value in ('nan', 'inf', '1e999', '1junk', '')]
        out = self.root / 'invalid'
        for extra in cases:
            with self.subTest(arguments=extra):
                result = run('--out', out, '--preset', PRESET, *extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertFalse(out.exists())

    def test_exclusive_output_modes_preserve_existing_files(self):
        for exists in (False, True):
            out = self.root / f'exclusive-{exists}'
            inventory = self.root / f'exclusive-{exists}.json'
            if exists:
                out.mkdir()
                (out / 'metrics.csv').write_bytes(b'existing metrics')
                inventory.write_bytes(b'existing inventory')
            cases = [('--list', '--inventory', inventory),
                     ('--list', '--out', out, '--preset', PRESET),
                     ('--inventory', inventory, '--out', out, '--preset', PRESET)]
            for arguments in cases:
                with self.subTest(existing=exists, arguments=arguments):
                    result = run(*arguments)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertIn('choose exactly one', result.stderr)
                    if exists:
                        self.assertEqual({p.name: p.read_bytes() for p in out.iterdir()},
                                         {'metrics.csv': b'existing metrics'})
                        self.assertEqual(inventory.read_bytes(), b'existing inventory')
                    else:
                        self.assertFalse(out.exists())
                        self.assertFalse(inventory.exists())

    def test_unicode_output_paths(self):
        out = self.root / 'capture-\u65e5\u672c\u8a9e-\u00e9 space'
        out.mkdir()
        inventory = out / '\u97f3\u8272.json'
        result = run('--inventory', inventory)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(inventory.read_text(encoding='utf-8')), self.inventory)
        result = run('--out', out, '--wav', *self.options)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(read_row(out / 'metrics.csv'), self.row)
        path, = out.glob('*.wav')
        self.assertEqual(path.read_bytes(), self.wav_path.read_bytes())

    @unittest.skipUnless(os.name == 'posix' and hasattr(signal, 'SIGXFSZ'), 'POSIX file-size limits')
    def test_buffered_output_failures_are_reported(self):
        import resource
        for wav, limit in [(False, 64), (True, 2205 * 8 - 1)]:
            def restrict():
                signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
                resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))
            out = self.root / f'write-failure-{wav}'
            result = run('--out', out, '--preset', PRESET, '--rate', RATE, '--hold', '.05',
                         '--tail', '0', *(['--wav'] if wav else []), preexec_fn=restrict)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn('WAV write failed' if wav else 'metrics report write failed', result.stderr)
            self.assertFalse(list(out.glob('*.wav')), 'A truncated WAV must not look like a completed capture')


if __name__ == '__main__':
    unittest.main(argv=[__file__, *remaining])
