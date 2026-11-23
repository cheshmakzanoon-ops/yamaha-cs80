#!/usr/bin/env python3
"""Verify a downloaded 2026.11.2 audition ZIP offline; never execute its contents.

Requires Python 3.10+ and NumPy. The independent, frozen roster below is the
68-excerpt contract. Paths in recorded commands belong to the CI runner; this
checker does not resolve or execute them and needs no repository-relative root.

Example:
  python verify-audition.py --archive auditions.zip --artifact-json artifact.json \
    --source FULL_COMMIT_SHA --run-id RUN_ID --run-attempt 1 --out proof.json

A successful proof concerns these bytes and their supplied API metadata only.
Native/validator job results and the authenticity/freshness of the supplied
metadata must be checked separately. No output proof is written on failure.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import sys
import zipfile
import zlib

import numpy as np

sys.dont_write_bytecode = True
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
REVISION = '2026.11.2'
RATE = 48000
WAV_COUNT = 68
UNIQUE_PROGRAMS = 56
TOTAL_SECONDS = 431
PARAMETER_COUNT = 190
# SHA256 of newline-joined ordered parameter IDs, with no final newline.
PARAMETER_IDS_SHA256 = '7e03391b29dced09464af746967e799841d23f99bada52d14e935882482b060b'
# SHA256 of compact UTF-8 JSON [[host_index,name,category], ...] for all 523
# existing addresses. Descriptions, tags and deliberately revoiced values are
# excluded; the captured pre-revision address identity must remain unchanged.
PROGRAM_IDENTITY_SHA256 = '153dc26a4cc6777258b4cc9005d81ca51355375c7a3fd7563d4072c51f8f2ce6'
MAX_ARCHIVE_BYTES = 256 * 1024 * 1024
MAX_EXPANDED_BYTES = 256 * 1024 * 1024
MAX_MEMBER_BYTES = 8 * 1024 * 1024
MAX_MEMBERS = 128
KEYS = [
    'Electric Piano I', 'Electric Piano II', 'Tine Piano', 'Clavichord I',
    'Clavichord II', 'Funky Clavichord', 'Harpsichord I', 'Harpsichord II',
    'Lute Harpsichord', 'Toy Piano', 'Honky Keys', 'Vibes Keys', 'Celesta',
    'Saw Pulse Piano', 'Soft Keys', 'Chorus Keys', 'Stage Keys', 'Velocity Keys',
    'Dulcimer', 'Reed Piano', 'Glass Keys', 'Tremolo Keys', 'Contour Keys', 'Pressure Keys',
]
FAMILIES = [
    'Brass Swell I', 'Sforzando Brass', 'Covered Horn', 'Solo Violin', 'Solo Cello',
    'Marcato Strings', 'Recorder', 'Clarinet', 'Flute Organ 8+4', 'Tremolo Organ',
    'Rubber Bass', 'Picked Bass', 'Nylon Pluck', 'Mandolin', 'Vibraphone', 'Marimba',
    'Tremolo Brass', 'Tremolo Pad', 'Tremolo Poly', 'Touch Lead',
    'Aftertouch Vibrato Lead', 'Glide Lead',
]
ORIGINAL = ['Tone I: E. Piano', 'Tone II: Bass', 'Analog Piano', 'Bright Keys',
            'Amber Keys 97', 'Slow Keys 106', 'Replicant Brass', 'Bass Stack']
PRESSURE = ['Aftertouch Vibrato Lead', 'Pressure Lead', 'Wah Lead']
METRIC_FIELDS = (
    'index,name,category,note,sample_rate,velocity,pressure,hold_seconds,tail_seconds,rank,'
    'parameter_digest,peak_dbfs,rms_dbfs,early_rms_dbfs,body_rms_dbfs,tail_rms_dbfs,'
    'peak_window_seconds,centroid_hz,low_power,mid_power,high_power,tilt,'
    'stereo_correlation,nonfinite,samples_over_full_scale'
).split(',')


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def json_load(data):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, f'Duplicate JSON key: {key}')
            result[key] = value
        return result
    def invalid(value):
        raise ValueError(f'Nonfinite JSON value: {value}')
    return json.loads(data, object_pairs_hook=pairs, parse_constant=invalid)


def selections():
    groups = [
        dict(folder='keyboards', names=KEYS, velocity=.85, pressure=0, hold=4, tail=1),
        dict(folder='soft-keyboards', names=['Electric Piano I', 'Electric Piano II',
             'Tine Piano', 'Reed Piano', 'Stage Keys'], velocity=.3, pressure=0, hold=4, tail=1),
        dict(folder='families', names=FAMILIES, velocity=.85, pressure=0, hold=7, tail=2),
        dict(folder='original-addresses', names=ORIGINAL, velocity=.85, pressure=0, hold=4, tail=1),
        dict(folder='pressure-zero', names=PRESSURE, velocity=.85, pressure=0, hold=5, tail=1),
        dict(folder='pressure-full', names=PRESSURE, velocity=.85, pressure=1, hold=5, tail=1),
    ]
    groups += [dict(folder=f'tine-register-{note}', names=['Tine Piano'], velocity=.85,
                   pressure=0, hold=3, tail=1, note=note) for note in (48, 60, 84)]
    require(sum(len(g['names']) for g in groups) == WAV_COUNT, 'Invalid verifier roster count')
    require(len({n for g in groups for n in g['names']}) == UNIQUE_PROGRAMS, 'Invalid verifier unique count')
    require(sum(len(g['names']) * (g['hold'] + g['tail']) for g in groups) == TOTAL_SECONDS,
            'Invalid verifier duration contract')
    return groups


def validate_api(meta, source, run_id, archive_hash, archive_size):
    require(meta['name'] == 'cs80-curated-preset-audition-' + source, 'Wrong artifact name')
    require(meta['expired'] is False, 'Artifact API record reports expiration')
    require(type(meta['id']) is int and meta['id'] > 0, 'Invalid artifact ID')
    require(type(meta['size_in_bytes']) is int and 0 < meta['size_in_bytes'] <= MAX_ARCHIVE_BYTES,
            'Artifact size exceeds bound')
    require(meta['size_in_bytes'] == archive_size, 'Archive size differs from API')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', meta.get('digest', '')) is not None,
            'Missing or malformed API SHA256')
    require(meta['digest'] == 'sha256:' + archive_hash, 'Archive SHA256 differs from API')
    run = meta['workflow_run']
    require(type(run['id']) is int and run['id'] == run_id and run['head_sha'] == source,
            'Artifact source/run differs from requested build')
    require(run.get('head_branch') == 'main', 'Artifact does not originate from main')


def safe_members(archive):
    infos = archive.infolist()
    require(0 < len(infos) <= MAX_MEMBERS, 'Unexpected ZIP member count')
    names, files, total = set(), {}, 0
    directories = {g['folder'] + '/' for g in selections()}
    for info in infos:
        name = info.filename
        require(name and name == info.orig_filename and '\x00' not in name and
                '\\' not in name and not name.startswith('/'),
                f'Unsafe ZIP path: {name!r}')
        path = PurePosixPath(name)
        require('..' not in path.parts and ':' not in name and
                path.as_posix() == (name[:-1] if info.is_dir() else name), f'Noncanonical ZIP path: {name!r}')
        require(name not in names, f'Duplicate ZIP member: {name}')
        names.add(name)
        kind = stat.S_IFMT(info.external_attr >> 16)
        require(kind in (0, stat.S_IFDIR if info.is_dir() else stat.S_IFREG), f'Special ZIP member: {name}')
        require(not (info.flag_bits & 1), f'Encrypted ZIP member: {name}')
        require(info.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED), 'Unexpected ZIP compression')
        require(0 <= info.file_size <= MAX_MEMBER_BYTES and info.compress_size >= 0, 'Oversized ZIP member')
        total += info.file_size
        require(total <= MAX_EXPANDED_BYTES, 'Expanded ZIP exceeds bound')
        if info.is_dir():
            require(name in directories and info.file_size == 0, f'Unexpected ZIP directory: {name}')
        else:
            files[name] = info
    return files, total


def validate_checksums(archive, files):
    require('SHA256SUMS' in files, 'Missing checksum manifest')
    records = {}
    for line in archive.read('SHA256SUMS').decode('utf-8').splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        require(match is not None, 'Malformed checksum line')
        sha, name = match.groups()
        require(name not in records and name != 'SHA256SUMS', 'Duplicate/self-referential checksum entry')
        records[name] = sha
    require(set(records) == set(files) - {'SHA256SUMS'}, 'Checksum inventory differs from ZIP files')
    for name, sha in records.items():
        require(digest(archive.read(name)) == sha, f'Member SHA256 mismatch: {name}')
    return records


def validate_inventory(inventory):
    require(inventory['schema'] == 1 and inventory['library_revision'] == REVISION, 'Wrong inventory schema/revision')
    ids = inventory['parameter_ids']
    require(isinstance(ids, list) and len(ids) == PARAMETER_COUNT and all(isinstance(i, str) for i in ids),
            'Wrong parameter roster')
    require(digest('\n'.join(ids).encode()) == PARAMETER_IDS_SHA256, 'Parameter ID order differs')
    programs = inventory['programs']
    require(isinstance(programs, list) and len(programs) == 523, 'Inventory must contain all 523 programs')
    for index, p in enumerate(programs):
        require(type(p['index']) is int and type(p['host_index']) is int and
                p['index'] == p['host_index'] == index, 'Program addresses differ from ordered full library')
        values = p['values']
        require(len(values) == PARAMETER_COUNT and all(type(v) in (float, int) and math.isfinite(v) for v in values),
                f'Invalid parameter vector: {p["name"]}')
        for value in values:
            require(math.isfinite(struct.unpack('<f', struct.pack('<f', value))[0]), 'Parameter exceeds finite float32')
        require(values[ids.index('source_performance')] == 1 and values[ids.index('parity_mode')] == 0,
                f'Noncanonical circuit policy: {p["name"]}')
        require(values[ids.index('sub_original_mode')] == 1, f'Non-original factory SUB setting: {p["name"]}')
    identity = [[p['host_index'], p['name'], p['category']] for p in programs]
    require(digest(json.dumps(identity, ensure_ascii=False, separators=(',', ':')).encode()) == PROGRAM_IDENTITY_SHA256,
            'Factory names/categories/address identity differs')
    result = {p['name']: p for p in programs}
    require(len(result) == 523, 'Repeated program name')
    return result


def validate_provenance(provenance, source, run_id, attempt, groups):
    require(provenance['schema'] == 2 and provenance['capture_status'] == 'complete', 'Capture was not completed')
    require(provenance['source_commit'] == source and provenance['repository'] == REPOSITORY,
            'Capture source/repository differs')
    require(provenance['run_id'] == str(run_id) and provenance['run_attempt'] == str(attempt),
            'Capture run/attempt differs')
    require(provenance['library_revision'] == REVISION and provenance['wav_count'] == WAV_COUNT and
            provenance['expected_wav_count'] == WAV_COUNT, 'Wrong capture revision/count')
    require(provenance['selections'] == groups, 'Selections differ from the independent audition contract')
    require(re.fullmatch(r'[0-9a-f]{64}', provenance['capture_binary_sha256']) is not None,
            'Invalid recorded capture-binary hash')
    require(isinstance(provenance['platform'], str) and provenance['platform'], 'Missing capture platform')
    command = provenance['inventory_command']
    require(isinstance(command, list) and len(command) == 5 and all(isinstance(a, str) for a in command),
            'Invalid inventory argv')
    executable, inventory_path = PurePosixPath(command[0]), PurePosixPath(command[4])
    require(executable.is_absolute() and executable.name == 'cs80_preset_capture' and '..' not in executable.parts,
            'Invalid recorded capture executable path')
    require(inventory_path.is_absolute() and inventory_path.name == 'inventory.json' and '..' not in inventory_path.parts,
            'Invalid recorded inventory output path')
    require(command[1:4] == ['--library', 'all', '--inventory'], 'Inventory command is not the complete library')
    out = inventory_path.parent
    expected = []
    for g in groups:
        cmd = [str(executable), '--library', 'all', '--out', str(out / g['folder']), '--wav',
               '--rate', str(RATE), '--velocity', str(g['velocity']), '--pressure', str(g['pressure']),
               '--hold', str(g['hold']), '--tail', str(g['tail'])]
        if 'note' in g:
            cmd += ['--note', str(g['note'])]
        for name in g['names']:
            cmd += ['--preset', name]
        expected.append(cmd)
    require(provenance['capture_commands'] == expected, 'Recorded capture argv/settings differ')


def fnv_float32(values):
    h = 14695981039346656037
    for value in values:
        for byte in struct.pack('<f', value):
            h = ((h ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f'{h:x}'


def db(value):
    return 20.0 * math.log10(max(float(value), 1e-12))


def decode_wav(data, frames, allow_silence=False):
    require(len(data) >= 58, 'Truncated WAV')
    riff, size, wave = struct.unpack_from('<4sI4s', data)
    require((riff, wave) == (b'RIFF', b'WAVE') and size == len(data) - 8, 'Invalid RIFF envelope')
    at, parts = 12, {}
    while at < len(data):
        require(at + 8 <= len(data), 'Truncated chunk header')
        name, length = struct.unpack_from('<4sI', data, at)
        at += 8
        require(name not in parts and at + length <= len(data), 'Repeated/truncated WAV chunk')
        parts[name] = memoryview(data)[at:at + length]
        at += length + (length & 1)
        require(at <= len(data), 'Missing WAV chunk padding')
    require(set(parts) == {b'fmt ', b'fact', b'data'}, 'Unexpected WAV chunks')
    require(len(parts[b'fmt ']) == 18 and struct.unpack('<HHIIHHH', parts[b'fmt ']) ==
            (3, 2, RATE, RATE * 8, 8, 32, 0), 'Not stereo 48 kHz IEEE float32')
    require(len(parts[b'fact']) == 4 and struct.unpack('<I', parts[b'fact'])[0] == frames and
            len(parts[b'data']) == frames * 8, 'WAV frame count differs from requested duration')
    audio = np.frombuffer(parts[b'data'], dtype='<f4').astype(np.float64).reshape((-1, 2))
    require(bool(np.isfinite(audio).all()), 'Nonfinite WAV samples')
    peak = float(np.max(np.abs(audio)))
    require(peak < 1.0, 'Samples at or above full scale')
    if allow_silence:
        require(peak == 0.0, 'Zero-pressure Pressure Lead is not silent')
    else:
        require(peak > 1e-6, 'Unexpected silent audition')
    return audio


def raw_metrics(audio, held):
    def rms(start, end):
        segment = audio[max(0, start):min(len(audio), end)]
        return float(np.sqrt(np.mean(segment * segment))) if len(segment) else 0.0
    body = audio[:held]
    ll, rr = np.sum(body * body, axis=0)
    lr = float(np.sum(body[:, 0] * body[:, 1]))
    differences = np.diff(body, axis=0)
    window = RATE // 20
    energies = [rms(start, min(start + window, held)) for start in range(0, held, window)]
    peak_start = int(np.argmax(energies)) * window
    first = min(max(peak_start - 2048 + window // 2, 0), max(0, held - 4096))
    segment = np.zeros((4096, 2), dtype=np.float64)
    count = min(4096, held - first)
    segment[:count] = audio[first:first + count]
    spectrum = np.sum(np.abs(np.fft.rfft(segment * np.hanning(4096)[:, None], axis=0)) ** 2, axis=1)
    spectrum[1:-1] *= 2.0
    spectrum[0] = 0.0
    hz = np.fft.rfftfreq(4096, 1.0 / RATE)
    total = float(np.sum(spectrum))
    centroid = float(np.dot(hz, spectrum))
    low, mid, high = (float(np.sum(spectrum[mask])) for mask in (hz < 500, (hz >= 500) & (hz < 2000), hz >= 2000))
    if total > 1e-24:
        centroid, low, mid, high = (v / total for v in (centroid, low, mid, high))
    return dict(peak_dbfs=db(np.max(np.abs(audio))), rms_dbfs=db(rms(0, len(audio))),
                early_rms_dbfs=db(rms(0, min(held, RATE // 5))),
                body_rms_dbfs=db(rms(max(0, held - RATE // 2), held)),
                tail_rms_dbfs=db(rms(max(held, len(audio) - RATE // 2), len(audio))),
                peak_window_seconds=peak_start / RATE, centroid_hz=centroid,
                low_power=low, mid_power=mid, high_power=high,
                tilt=float(np.sum(differences * differences)) / max(1e-24, float(ll + rr)),
                stereo_correlation=lr / max(1e-24, math.sqrt(float(ll * rr))),
                nonfinite=0, samples_over_full_scale=0)


def expected_filename(program):
    safe = ''.join(chr(b) if 65 <= b <= 90 or 97 <= b <= 122 or 48 <= b <= 57 else '-'
                   for b in program['name'].encode('utf-8'))
    return f'{program["index"]}-{safe}.wav'


def verify(archive_path, meta, source, run_id, attempt):
    require(re.fullmatch(r'[0-9a-f]{40}', source) is not None, 'Expected full lowercase source SHA')
    require(type(run_id) is int and run_id > 0 and type(attempt) is int and attempt > 0, 'Invalid run/attempt')
    size = archive_path.stat().st_size
    require(0 < size <= MAX_ARCHIVE_BYTES, 'Archive exceeds bound')
    sha = file_digest(archive_path)
    validate_api(meta, source, run_id, sha, size)
    groups = selections()
    measurements = []
    with zipfile.ZipFile(archive_path) as archive:
        files, expanded = safe_members(archive)
        checksums = validate_checksums(archive, files)
        require(archive.read('source-commit.txt').decode('utf-8') == source + '\n', 'Source text differs')
        provenance = json_load(archive.read('provenance.json'))
        validate_provenance(provenance, source, run_id, attempt, groups)
        programs = validate_inventory(json_load(archive.read('inventory.json')))
        expected_files = {'inventory.json', 'provenance.json', 'source-commit.txt', 'README.txt', 'SHA256SUMS'}
        for group in groups:
            metrics_path = group['folder'] + '/metrics.csv'
            expected_files.add(metrics_path)
            reader = csv.DictReader(io.StringIO(archive.read(metrics_path).decode('utf-8')))
            require(reader.fieldnames == METRIC_FIELDS, f'Wrong metrics columns: {metrics_path}')
            rows = list(reader)
            require([r['name'] for r in rows] == group['names'], f'Metrics selection/order differs: {metrics_path}')
            for row in rows:
                program = programs[row['name']]
                default_note = 48 if program['category'] == 'Bass' else 67 if program['category'] in ('Winds', 'Leads') else 72 if program['category'] == 'Bells' else 60
                note = group.get('note', default_note)
                require(int(row['index']) == program['index'] and row['category'] == program['category'] and
                        int(row['note']) == note and int(row['sample_rate']) == RATE and int(row['rank']) == 0,
                        f'Capture identity/note/rank differs: {row["name"]}')
                require(row['parameter_digest'] == fnv_float32(program['values']), 'Capture parameter digest differs')
                for key in ('velocity', 'pressure'):
                    require(struct.pack('<f', float(row[key])) == struct.pack('<f', group[key]), f'Wrong {key}')
                require(float(row['hold_seconds']) == group['hold'] and float(row['tail_seconds']) == group['tail'],
                        'Wrong hold/tail duration')
                path = group['folder'] + '/' + expected_filename(program)
                expected_files.add(path)
                frames = RATE * (group['hold'] + group['tail'])
                silent = group['folder'] == 'pressure-zero' and row['name'] == 'Pressure Lead'
                audio = decode_wav(archive.read(path), frames, silent)
                measured = raw_metrics(audio, RATE * group['hold'])
                errors = {}
                for key, value in measured.items():
                    reported = float(row[key])
                    require(math.isfinite(reported), f'Nonfinite CSV metric: {key}')
                    error = abs(value - reported)
                    # CSV uses ten significant digits; NumPy reduction/FFT order
                    # is independent of the C++ implementation. These bounds
                    # cover printed rounding, not a change in audio or settings.
                    tolerance = 1e-6 if key.endswith('_dbfs') else 1e-5 if key == 'centroid_hz' else 1e-8
                    require(error <= max(tolerance, abs(value) * 6e-10), f'WAV/CSV {key} differs: {path}')
                    errors[key] = error
                require(int(row['nonfinite']) == int(row['samples_over_full_scale']) == 0, 'CSV invalid sample counts')
                measurements.append(dict(path=path, name=row['name'], index=program['index'], category=program['category'],
                    note=note, velocity=group['velocity'], pressure=group['pressure'], frames=frames,
                    intentional_silence=silent, parameter_digest=row['parameter_digest'], wav_sha256=checksums[path],
                    metrics=measured, metric_absolute_errors=errors))
        require(set(files) == expected_files, 'ZIP has missing or unexamined files')
        require(len(measurements) == WAV_COUNT and len({m['name'] for m in measurements}) == UNIQUE_PROGRAMS,
                'Wrong actual excerpt/program total')
        require(sum(m['frames'] for m in measurements) == TOTAL_SECONDS * RATE, 'Wrong total audition duration')
        require(sum(m['intentional_silence'] for m in measurements) == 1, 'Wrong intentional silence count')
        inventory_sha = checksums['inventory.json']
        checksum_sha = digest(archive.read('SHA256SUMS'))
    return dict(status='ARTIFACT_BYTES_VERIFIED', verified_at_utc=datetime.now(timezone.utc).isoformat(),
        verifier_sha256=file_digest(Path(__file__)), source_commit=source, run_id=run_id, run_attempt=attempt,
        repository=REPOSITORY, library_revision=REVISION, artifact_id=meta['id'], artifact_name=meta['name'],
        artifact_expires_at=meta.get('expires_at'), outer_bytes=size, outer_sha256=sha,
        expanded_bytes=expanded, checksum_files_verified=len(checksums), inventory_sha256=inventory_sha,
        checksum_manifest_sha256=checksum_sha, program_identity_sha256=PROGRAM_IDENTITY_SHA256,
        capture_binary_sha256_as_recorded=provenance['capture_binary_sha256'], capture_platform=provenance['platform'],
        wav_count=WAV_COUNT, unique_programs=UNIQUE_PROGRAMS, inventory_programs=523,
        sample_rate=RATE, stereo_frames=TOTAL_SECONDS * RATE, seconds=TOTAL_SECONDS,
        intentional_silent_files=1, nonfinite_samples=0, samples_at_or_above_full_scale=0,
        highest_peak_dbfs=max(m['metrics']['peak_dbfs'] for m in measurements), measurements=measurements,
        limitations='Offline byte, inventory, command/provenance and sample/CSV consistency checks against supplied API metadata. '
                    'The capture executable is not present for independent hash verification. No payload was executed or published. '
                    'This does not qualify native jobs, validators, arbitrary hosts, listening quality or acoustic/hardware equivalence.')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--artifact-json', required=True, type=Path, help='One complete GitHub artifact API object')
    parser.add_argument('--source', required=True)
    parser.add_argument('--run-id', required=True, type=int)
    parser.add_argument('--run-attempt', type=int, default=1)
    parser.add_argument('--out', type=Path, help='New proof file; existing files are never overwritten')
    args = parser.parse_args()
    try:
        require(args.out is None or not args.out.exists(), 'Refusing to overwrite an existing proof')
        proof = verify(args.archive, json_load(args.artifact_json.read_bytes()), args.source, args.run_id, args.run_attempt)
        if args.out is not None:
            with args.out.open('x', encoding='utf-8') as stream:
                stream.write(json.dumps(proof, indent=2, allow_nan=False) + '\n')
        print(json.dumps({k: proof[k] for k in ('status', 'artifact_id', 'source_commit', 'run_id', 'wav_count',
              'unique_programs', 'inventory_programs', 'seconds', 'outer_sha256', 'highest_peak_dbfs')}))
        return 0
    except (ValueError, KeyError, TypeError, OSError, OverflowError, EOFError, csv.Error,
            struct.error, zipfile.BadZipFile, zlib.error) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
