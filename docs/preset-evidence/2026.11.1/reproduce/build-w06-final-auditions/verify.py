#!/usr/bin/env python3
"""Independently verify final audition bytes, identities, timing and levels."""
from pathlib import Path
import csv
import hashlib
import json
import math
import struct

import numpy as np

TASK = Path(__file__).resolve().parent
OUT = TASK / 'preset-audition'
ROOT = TASK.parent
EXPECTED_SOURCE = '99f5ebd6b71283a56810d801df89c49ef62f3512'
EXPECTED_INVENTORY = 'fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fnv(values):
    value = 14695981039346656037
    for byte in struct.pack('<' + 'f' * len(values), *values):
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return format(value, 'x')


def db(value):
    return 20 * math.log10(max(float(value), 1e-12))


def read_wav(path):
    data = path.read_bytes()
    assert data[:4] == b'RIFF' and data[8:12] == b'WAVE', path
    assert struct.unpack_from('<I', data, 4)[0] == len(data) - 8, path
    chunks = {}
    pos = 12
    while pos < len(data):
        name, size = struct.unpack_from('<4sI', data, pos)
        assert name not in chunks, (path, name)
        pos += 8
        chunks[name] = data[pos:pos+size]
        assert len(chunks[name]) == size, path
        pos += size + (size & 1)
    assert pos == len(data), path
    assert set(chunks) == {b'fmt ', b'fact', b'data'}, path
    assert struct.unpack('<HHIIHHH', chunks[b'fmt ']) == (3, 2, 48000, 384000, 8, 32, 0), path
    values = np.frombuffer(chunks[b'data'], dtype='<f4').reshape((-1, 2)).astype(np.float64)
    assert struct.unpack('<I', chunks[b'fact'])[0] == len(values), path
    return values


def main():
    checksums = {}
    for line in (OUT / 'SHA256SUMS').read_text().splitlines():
        digest, relative = line.split('  ', 1)
        assert relative not in checksums
        checksums[relative] = digest
        assert sha(OUT / relative) == digest, relative
    actual_files = {p.relative_to(OUT).as_posix() for p in OUT.rglob('*') if p.is_file()}
    assert actual_files == set(checksums) | {'SHA256SUMS'}
    assert len(checksums) == 32
    inventory = json.loads((OUT / 'inventory.json').read_text())
    assert sha(OUT / 'inventory.json') == EXPECTED_INVENTORY
    assert inventory['library_revision'] == '2026.11.1'
    assert len(inventory['programs']) == 301
    assert len(inventory['parameter_ids']) == 190
    programs = {p['name']: p for p in inventory['programs']}
    assert len(programs) == 301
    provenance = json.loads((OUT / 'provenance.json').read_text())
    assert provenance['source_commit'] == EXPECTED_SOURCE
    assert (OUT / 'source-commit.txt').read_text().strip() == EXPECTED_SOURCE
    assert provenance['run_id'] == 'local-assignment-32'
    assert provenance['capture_status'] == 'complete'
    assert provenance['wav_count'] == provenance['expected_wav_count'] == 24
    assert sha(ROOT / 'build-audit/cs80_preset_capture') == provenance['capture_binary_sha256']
    original = (TASK / 'workflow-audition-original.py').read_text()
    local = (TASK / 'workflow-audition-local.py').read_text()
    execution = json.loads((TASK / 'execution.json').read_text())
    substitution = execution['sole_block_substitution']
    assert original.replace(substitution['before'], substitution['after']) == local

    seen = set()
    measurements = []
    checked_wavs = set()
    max_peak_error = max_rms_error = 0.0
    for selection in provenance['selections']:
        folder = OUT / selection['folder']
        rows = list(csv.DictReader((folder / 'metrics.csv').open()))
        assert {row['name'] for row in rows} == set(selection['presets'])
        assert len(rows) == len(selection['presets'])
        for row in rows:
            name = row['name']
            assert name not in seen
            seen.add(name)
            preset = programs[name]
            assert int(row['index']) == preset['index']
            assert row['category'] == preset['category']
            assert row['parameter_digest'] == fnv(preset['values']), name
            assert int(row['rank']) == 0
            assert struct.pack('<f', float(row['pressure'])) == struct.pack('<f', selection['pressure'])
            assert struct.pack('<f', float(row['velocity'])) == struct.pack('<f', .8)
            assert float(row['hold_seconds']) == selection['hold_seconds']
            assert float(row['tail_seconds']) == selection['tail_seconds']
            assert int(row['sample_rate']) == 48000
            expected_note = 48 if preset['category'] == 'Bass' else 67 if preset['category'] in ('Winds', 'Leads') else 72 if preset['category'] == 'Bells' else 60
            assert int(row['note']) == expected_note
            matches = list(folder.glob(str(preset['index']) + '-*.wav'))
            assert len(matches) == 1
            wav = matches[0]
            checked_wavs.add(wav)
            samples = read_wav(wav)
            duration = selection['hold_seconds'] + selection['tail_seconds']
            assert len(samples) == round(48000 * duration)
            assert np.isfinite(samples).all()
            peak = float(np.max(np.abs(samples)))
            rms = float(np.sqrt(np.mean(samples * samples)))
            assert 0 < peak < 1
            assert int(row['nonfinite']) == int(row['samples_over_full_scale']) == 0
            peak_error = abs(db(peak) - float(row['peak_dbfs']))
            rms_error = abs(db(rms) - float(row['rms_dbfs']))
            assert peak_error < 1e-7 and rms_error < 1e-7
            max_peak_error = max(max_peak_error, peak_error)
            max_rms_error = max(max_rms_error, rms_error)
            measurements.append({'name':name, 'category':preset['category'], 'index':preset['index'], 'note':expected_note,
                                 'pressure':selection['pressure'], 'seconds':duration, 'frames':len(samples),
                                 'peak_dbfs':db(peak), 'rms_dbfs':db(rms), 'parameter_digest':row['parameter_digest'],
                                 'wav':wav.relative_to(OUT).as_posix(), 'wav_sha256':sha(wav)})
    assert len(seen) == 24 and len(checked_wavs) == 24
    assert set(OUT.rglob('*.wav')) == checked_wavs
    assert sum(row['seconds'] for row in measurements) == 117
    summary = {
        'assignment':32, 'status':'PASS', 'source_commit':EXPECTED_SOURCE,
        'scope':'Final calibrated bank through the committed CI audition block; local executable path substitution only.',
        'qualification':'Local prepublication executable built before the source commit; not a GitHub Actions-produced or downloadable platform artifact.',
        'library_revision':inventory['library_revision'], 'inventory_sha256':sha(OUT/'inventory.json'),
        'capture_binary_sha256':provenance['capture_binary_sha256'],
        'manifest_entries_verified':len(checksums), 'inventory_programs':301, 'unique_wavs':24,
        'generated_seconds':117, 'stereo_frames':sum(row['frames'] for row in measurements),
        'format':'Stereo 48 kHz IEEE float32 WAV, RIFF/fmt/fact/data chunks independently checked.',
        'nonfinite_samples':0, 'over_full_scale_samples':0,
        'all_factory_parameter_digests_match_inventory':True,
        'all_csv_peak_and_rms_values_match_wav_samples':True,
        'max_peak_db_error':max_peak_error, 'max_rms_db_error':max_rms_error,
        'highest_peak_dbfs':max(row['peak_dbfs'] for row in measurements),
        'lowest_peak_dbfs':min(row['peak_dbfs'] for row in measurements),
        'normalization_evidence':'WAV peak/RMS match the capture metrics from unchanged factory parameter digests. The exporter writes the original IEEE samples without normalization or clipping; levels vary across excerpts.',
        'limitations':['Single-note demonstrations are not chord, all-register, or listening-panel validation.',
                       'Release excerpts are deliberately bounded and may end while a long release continues.',
                       'The actual CI audition artifact still needs source/archive verification when available.',
                       'A subsequent packaging revision-parser correction may change the final source commit without changing this bank; this local provenance must not be relabeled.'],
        'files':{name:sha(TASK/name) for name in ['workflow-audition-original.py','workflow-audition-local.py','execution.json','capture.log','verify.py']},
        'artifact_manifest_sha256':sha(OUT/'SHA256SUMS'),
        'measurements':measurements,
    }
    (TASK/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({key:summary[key] for key in ['status','unique_wavs','generated_seconds','manifest_entries_verified','highest_peak_dbfs','max_peak_db_error','max_rms_db_error']}))


if __name__ == '__main__':
    main()
