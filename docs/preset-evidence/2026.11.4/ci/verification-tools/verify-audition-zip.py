#!/usr/bin/env python3
"""Independently qualify actual 2026.11.4 audition ZIP bytes; no extraction."""
from __future__ import annotations
import argparse
from array import array
import csv
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import re
import struct
import subprocess
import sys
import types
import zipfile

REPO = Path.cwd()  # Run from the repository root or pass --repo explicitly.
RATE = 48000

def require(value, message):
    if not value:
        raise ValueError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def parameter_digest(values):
    result = 14695981039346656037
    for value in values:
        for byte in struct.pack('<f', value):
            result = ((result ^ byte) * 1099511628211) & 0xffffffffffffffff
    return result

def inspect_wav(data, expected_frames):
    require(len(data) >= 58 and data[:4] == b'RIFF' and data[8:12] == b'WAVE', 'Invalid RIFF/WAVE header')
    require(struct.unpack_from('<I', data, 4)[0] == len(data) - 8, 'RIFF size differs from actual bytes')
    chunks = {}
    offset = 12
    while offset < len(data):
        require(offset + 8 <= len(data), 'Truncated chunk header')
        kind, length = struct.unpack_from('<4sI', data, offset)
        begin, end = offset + 8, offset + 8 + length
        require(kind not in chunks and end <= len(data), 'Duplicate or truncated RIFF chunk')
        chunks[kind] = data[begin:end]
        offset = end + (length & 1)
    require(offset == len(data) and set(chunks) == {b'fmt ', b'fact', b'data'}, 'Unexpected RIFF chunk layout')
    require(len(chunks[b'fmt ']) == 18, 'Expected producer IEEE-float fmt chunk')
    fmt = struct.unpack('<HHIIHHH', chunks[b'fmt '])
    require(fmt == (3, 2, RATE, RATE * 8, 8, 32, 0), 'Expected 48 kHz stereo 32-bit IEEE float')
    require(len(chunks[b'fact']) == 4 and struct.unpack('<I', chunks[b'fact'])[0] == expected_frames,
            'fact frame count differs from requested duration')
    require(len(chunks[b'data']) == expected_frames * 8, 'Actual audio frame count differs from requested duration')
    samples = array('f')
    samples.frombytes(chunks[b'data'])
    if sys.byteorder != 'little':
        samples.byteswap()
    peak = 0.0
    energy = 0.0
    for sample in samples:
        require(math.isfinite(sample), 'Nonfinite float WAV sample')
        absolute = abs(sample)
        require(absolute <= 1.0, 'Float WAV sample exceeds full scale')
        peak = max(peak, absolute)
        energy += sample * sample
    rms = math.sqrt(energy / len(samples))
    db = lambda value: 20 * math.log10(max(value, 1e-12))
    return dict(frames=expected_frames, seconds=expected_frames / RATE,
                peak=peak, peak_dbfs=db(peak), rms_dbfs=db(rms),
                nonfinite_samples=0, samples_over_full_scale=0)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--source', required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--attempt', required=True)
    parser.add_argument('--expected-zip-sha256', required=True)
    parser.add_argument('--repo', type=Path, default=REPO)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    require(re.fullmatch('[0-9a-f]{40}', args.source), 'Expected full source commit')
    require(re.fullmatch('[0-9a-f]{64}', args.expected_zip_sha256), 'Expected API ZIP SHA256 digest')
    archive_data = args.archive.read_bytes()
    require(sha(archive_data) == args.expected_zip_sha256, 'ZIP SHA256 differs from GitHub artifact digest')
    git = lambda *parts: subprocess.check_output(['git', '-C', str(args.repo), *parts])
    tree = git('rev-parse', args.source + '^{tree}').decode().strip()
    script_path = 'scripts/capture-factory-auditions.py'
    script_data = git('show', args.source + ':' + script_path)
    module = types.ModuleType('source_bound_auditions')
    module.__file__ = str(args.repo / script_path)
    exec(compile(script_data, module.__file__, 'exec'), module.__dict__)
    groups = module.selections()
    expected_summary = module.roster_summary(groups)
    require(expected_summary == dict(wav_count=138, selected_program_count=74, audio_seconds=870), 'Unexpected source roster')
    require(len(groups) == 30, 'Expected 30 audition folders')
    with zipfile.ZipFile(io.BytesIO(archive_data)) as archive:
        infos = [info for info in archive.infolist() if not info.is_dir()]
        names = [info.filename for info in infos]
        require(len(names) == len(set(names)) and len(names) < 500, 'Duplicate or excessive ZIP members')
        require(sum(info.file_size for info in infos) < 512 * 1024 * 1024, 'ZIP expanded size exceeds expected bounds')
        require(all(info.file_size < 16 * 1024 * 1024 for info in infos), 'Excessive member size')
        require(all(not PurePosixPath(name).is_absolute() and '..' not in PurePosixPath(name).parts
                    and '\\' not in name for name in names), 'Unsafe ZIP member name')
        manifests = [name for name in names if PurePosixPath(name).name == 'provenance.json']
        require(len(manifests) == 1, 'Expected exactly one provenance manifest')
        prefix = manifests[0][:-len('provenance.json')]
        require(all(name.startswith(prefix) for name in names), 'Unexpected files outside audition root')
        files = {name[len(prefix):]: archive.read(name) for name in names}
    sums = {}
    for line in files['SHA256SUMS'].decode().splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        require(match and match[2] not in sums, 'Malformed or duplicate SHA256SUMS entry')
        sums[match[2]] = match[1]
    require(set(files) == set(sums) | {'SHA256SUMS'}, 'Checksum inventory differs from ZIP members')
    require(all(sha(files[name]) == digest for name, digest in sums.items()), 'Member checksum mismatch')
    provenance = json.loads(files['provenance.json'])
    require(provenance['schema'] == 3 and provenance['capture_status'] == 'complete', 'Capture did not complete')
    require(provenance['source_commit'] == args.source and provenance['source_tree'] == tree, 'Source identity mismatch')
    require(provenance['repository'] == 'cheshmakzanoon-ops/yamaha-cs80', 'Repository identity mismatch')
    require(str(provenance['run_id']) == args.run_id and str(provenance['run_attempt']) == args.attempt, 'CI run identity mismatch')
    require(provenance['tracked_source_clean_before_capture'] is True
            and provenance['tracked_source_clean_after_capture'] is True, 'Capture source was not clean')
    require(re.fullmatch('[0-9a-f]{64}', provenance['capture_binary_sha256']), 'Missing executable digest')
    require(files['source-commit.txt'].decode().strip() == args.source, 'source-commit.txt mismatch')
    require(provenance['selections'] == groups, 'Manifest selections differ from source roster')
    for key, value in [('expected_wav_count', 138), ('wav_count', 138), ('selected_program_count', 74),
                       ('expected_audio_seconds', 870), ('audio_seconds', 870)]:
        require(provenance[key] == value, 'Unexpected manifest summary: ' + key)
    inventory = json.loads(files['inventory.json'])
    require(inventory['schema'] == 1 and inventory['library_revision'] == '2026.11.4'
            and provenance['library_revision'] == '2026.11.4', 'Unexpected library revision or schema')
    ids = inventory['parameter_ids']
    require(len(ids) == len(set(ids)) == 200, 'Expected 200 unique core parameters')
    require(ids[190:198] == [f'body{rank}_{control}' for rank in (1, 2)
                        for control in ('model', 'mix', 'brightness', 'decay')], 'Body parameter order changed')
    require(ids[198:] == ['body1_size', 'body2_size'], 'Unexpected appended size IDs')
    programs = inventory['programs']
    require(len(programs) == 523 and [p['host_index'] for p in programs] == list(range(523))
            and [p['index'] for p in programs] == list(range(523)), 'Incomplete factory address inventory')
    by_name = {p['name']: p for p in programs}
    require(len(by_name) == 523, 'Duplicate factory names')
    require(all(len(p['values']) == 200 and all(math.isfinite(value) for value in p['values'])
                for p in programs), 'Invalid inventory parameter values')
    expected_files = {'SHA256SUMS', 'README.txt', 'source-commit.txt', 'provenance.json', 'inventory.json'}
    wave_reports = []
    models = set()
    for group in groups:
        metrics_name = group['folder'] + '/metrics.csv'
        expected_files.add(metrics_name)
        rows = list(csv.DictReader(io.StringIO(files[metrics_name].decode())))
        require(len(rows) == len(group['names']) and {r['name'] for r in rows} == set(group['names']),
                'Metrics roster mismatch: ' + group['folder'])
        for row in rows:
            program = by_name[row['name']]
            require(int(row['index']) == program['host_index'] and row['category'] == program['category'], 'Metrics program identity mismatch')
            default_note = 48 if program['category'] == 'Bass' else 67 if program['category'] in ('Winds', 'Leads') else 72 if program['category'] == 'Bells' else 60
            expected_settings = dict(sample_rate=RATE, velocity=group['velocity'], pressure=group['pressure'],
                                     hold_seconds=group['hold'], tail_seconds=group['tail'], rank=0,
                                     note=group.get('note', default_note), nonfinite=0, samples_over_full_scale=0)
            require(all(math.isfinite(float(row[key])) and abs(float(row[key]) - value) <= 1e-6
                        for key, value in expected_settings.items()), 'Metrics settings mismatch')
            require(int(row['parameter_digest'], 16) == parameter_digest(program['values']), 'Rendered parameter digest differs from inventory')
            filename = str(program['host_index']) + '-' + re.sub('[^A-Za-z0-9]', '-', program['name']) + '.wav'
            path = group['folder'] + '/' + filename
            expected_files.add(path)
            frames = round(group['hold'] * RATE) + round(group['tail'] * RATE)
            measurement = inspect_wav(files[path], frames)
            require(abs(measurement['peak_dbfs'] - float(row['peak_dbfs'])) < 1e-5
                    and abs(measurement['rms_dbfs'] - float(row['rms_dbfs'])) < 1e-5, 'Actual audio level differs from metrics')
            wave_reports.append(dict(path=path, program=program['name'], host_index=program['host_index'],
                                     sha256=sha(files[path]), **measurement))
            values = dict(zip(ids, program['values']))
            for rank in (1, 2):
                weight = 1 - values['mix'] if rank == 1 else values['mix']
                if values[f'body{rank}_mix'] * weight > 0 and values[f'body{rank}_model'] > 0:
                    models.add(int(values[f'body{rank}_model']))
    require(set(files) == expected_files, 'Unexpected or missing audition files')
    require(len(wave_reports) == 138 and sum(w['frames'] for w in wave_reports) == 870 * RATE, 'Actual audio duration/count mismatch')
    require(models == set(range(1, 20)), 'Auditions no longer cover every body model')
    report = dict(schema=1, accepted=True, source_commit=args.source, source_tree=tree,
                  repository=provenance['repository'], run_id=args.run_id, run_attempt=args.attempt,
                  zip_sha256=sha(archive_data), zip_bytes=len(archive_data), member_count=len(files),
                  checksum_count=len(sums), wav_count=len(wave_reports), actual_audio_seconds=870,
                  selected_program_count=74, inventory_program_count=523, parameter_count=200,
                  body_models=sorted(models), max_peak_dbfs=max(w['peak_dbfs'] for w in wave_reports),
                  nonfinite_samples=0, samples_over_full_scale=0,
                  capture_binary_sha256=provenance['capture_binary_sha256'],
                  capture_script_sha256=sha(script_data), verifier_sha256=sha(Path(__file__).read_bytes()),
                  verification_scope='Actual downloaded ZIP and all audio payloads; single-note auditions do not establish acoustic equivalence or host validation.',
                  wavs=wave_reports)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'wavs'}, indent=2))

if __name__ == '__main__':
    main()
