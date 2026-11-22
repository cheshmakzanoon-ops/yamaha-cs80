#!/usr/bin/env python3
"""Verify this fixed CI audition artifact; never publish or execute its payloads."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import csv
import hashlib
import importlib.util
import io
import json
import math
from pathlib import Path
import re
import shutil
import struct
import urllib.parse
import urllib.request
import zipfile

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
SOURCE = '6d3cc220edef5a4a2c7cd968ea8a786c7c83198c'
RUN = 45044040694
ATTEMPT = 1
REPO = 'cheshmakzanoon-ops/yamaha-cs80'
REVISION = '2026.11.1'
NAME = 'cs80-curated-preset-audition-' + SOURCE
INVENTORY_SHA = 'fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa'
WORKFLOW_SHA = 'a609f613be8876ca0a1b49cf7242723c535603aa419442b4df1f6b128c235e3e'
PUBLISHER_SHA = '72ad916f9843a9019d17833c6722c64493e8875a3e7818e58dee5ed0c8d7a343'
MAX_ZIP = 128 * 1024 * 1024
SELECTIONS = [
    ('standard', 0., 3., 1., ['Brass Swell I', 'Solo Violin', 'Clarinet', 'Electric Piano I', 'Flute Organ 8+4', 'Analog Bass', 'Nylon Pluck', 'Marimba', 'Arp Poly', 'Wind']),
    ('pressure', .65, 3., 1., ['Pressure Lead']),
    ('refined', 0., 3., 1., ['Flute', 'Breathy Flute', 'Harp', 'Kalimba', 'Sine Bass', 'Upright Pluck']),
    ('slow', 0., 5., 2., ['Slow Pad', 'Noir Swell Brass', 'Brass Ensemble 16', 'Neo-Noir Pad', 'Arctic Pad', 'Synthwave Chords', 'Ether Pad']),
]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def fnv_float32(values):
    require(len(values) == 190, 'Wrong core parameter count')
    value = 14695981039346656037
    for number in values:
        for byte in struct.pack('<f', number):
            value ^= byte
            value = (value * 1099511628211) % (1 << 64)
    return f'{value:x}'


def db(amplitude):
    return 20 * math.log10(max(float(amplitude), 1e-12))


def inspect_wav(data):
    stream = io.BytesIO(data)
    riff, length, kind = struct.unpack('<4sI4s', stream.read(12))
    require((riff, kind) == (b'RIFF', b'WAVE') and length == len(data) - 8, 'Invalid RIFF envelope')
    parts = {}
    while stream.tell() < len(data):
        header = stream.read(8)
        require(len(header) == 8, 'Truncated WAV chunk header')
        name, size = struct.unpack('<4sI', header)
        require(name not in parts and size <= len(data) - stream.tell(), 'Repeated or truncated WAV chunk')
        parts[name] = stream.read(size)
        if size % 2:
            require(len(stream.read(1)) == 1, 'Missing WAV chunk padding')
    require(set(parts) == {b'fmt ', b'fact', b'data'}, 'Unexpected WAV chunk set')
    require(struct.unpack('<HHIIHHH', parts[b'fmt ']) == (3, 2, 48000, 384000, 8, 32, 0), 'Not stereo 48 kHz float32')
    require(len(parts[b'data']) % 8 == 0, 'Partial stereo frame')
    frames = len(parts[b'data']) // 8
    require(struct.unpack('<I', parts[b'fact'])[0] == frames and frames > 0, 'WAV fact/frame mismatch')
    samples = np.frombuffer(parts[b'data'], dtype='<f4').astype(np.float64).reshape((-1, 2))
    require(bool(np.isfinite(samples).all()), 'Nonfinite WAV sample')
    peak = float(np.max(np.abs(samples)))
    require(0 < peak < 1., 'Silent or full-scale/clipped audition')
    rms = float(np.sqrt(np.mean(np.square(samples))))
    return {'frames': frames, 'peak_dbfs': db(peak), 'rms_dbfs': db(rms),
            'nonfinite_samples': 0, 'samples_at_or_over_full_scale': 0}


class HttpsRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        require(urllib.parse.urlsplit(newurl).scheme == 'https', 'Refusing non-HTTPS redirect')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def download(meta):
    cache = ROOT / 'build-coordination/artifact-cache'
    cache.mkdir(parents=True, exist_ok=True)
    outer = cache / f'{NAME}-{meta["id"]}.zip'
    url = f'https://nightly.link/{REPO}/actions/artifacts/{meta["id"]}.zip'
    if not outer.exists():
        require(shutil.disk_usage(cache).free > meta['size_in_bytes'] + 64 * 1024 * 1024, 'Insufficient reserved disk space')
        part = outer.with_suffix('.zip.part')
        try:
            request = urllib.request.Request(url, headers={'User-Agent': 'CS80-audition-verification/1.0'})
            with urllib.request.build_opener(HttpsRedirect()).open(request, timeout=45) as response, part.open('wb') as stream:
                require(response.status == 200, 'Public artifact GET did not return 200')
                length = 0
                for chunk in iter(lambda: response.read(1024 * 1024), b''):
                    length += len(chunk)
                    require(length <= meta['size_in_bytes'], 'Download exceeds API size')
                    stream.write(chunk)
            require(length == meta['size_in_bytes'] and file_digest(part) == meta['digest'][7:], 'Download byte identity mismatch')
            part.replace(outer)
        finally:
            part.unlink(missing_ok=True)
    require(outer.stat().st_size == meta['size_in_bytes'] and file_digest(outer) == meta['digest'][7:], 'Cached artifact byte identity mismatch')
    return outer, url


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact-json', type=Path, required=True)
    parser.add_argument('--run-json', type=Path, required=True)
    parser.add_argument('--job-json', type=Path, required=True, help='Raw Linux job object, not the jobs collection')
    args = parser.parse_args()
    meta, run, job = [json.loads(p.read_text()) for p in (args.artifact_json, args.run_json, args.job_json)]
    require(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == ATTEMPT, 'Run identity mismatch')
    require(type(run['run_attempt']) is int and run['name'] == 'build' and run['path'] == '.github/workflows/build.yml', 'Wrong workflow/attempt type')
    require(run['head_branch'] == 'main' and run['event'] == 'push' and run['head_repository']['full_name'] == REPO, 'Wrong run origin')
    require((run['status'] == 'in_progress' and run['conclusion'] is None) or (run['status'] == 'completed' and run['conclusion'] == 'success'), 'Build already failed or cancelled')
    require(job['run_id'] == RUN and job['head_sha'] == SOURCE and job['run_attempt'] == ATTEMPT and type(job['run_attempt']) is int, 'Linux job identity mismatch')
    require(job['name'] == 'Linux / VST3 + Standalone' and job['conclusion'] in (None, 'success'), 'Wrong or failed Linux job')
    steps = {step['name']: step for step in job['steps']}
    required = ['Verify configured JUCE source', 'Build', 'Test', 'Capture curated preset auditions', 'Upload curated preset auditions']
    require(all(steps.get(name, {}).get('conclusion') == 'success' for name in required), 'Required audition-producing step is incomplete')
    require(meta['name'] == NAME and meta['expired'] is False, 'Wrong or expired audition artifact')
    require(type(meta['id']) is int and meta['id'] > 0 and type(meta['size_in_bytes']) is int and 0 < meta['size_in_bytes'] <= MAX_ZIP, 'Invalid artifact ID/size')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', meta.get('digest', '')) is not None, 'Missing artifact API digest')
    require(meta['workflow_run']['id'] == RUN and meta['workflow_run']['head_sha'] == SOURCE, 'Artifact build identity mismatch')
    require(file_digest(ROOT / '.github/workflows/build.yml') == WORKFLOW_SHA, 'Reviewed capture workflow changed')
    outer, url = download(meta)

    publisher_path = ROOT / 'scripts/publish-preset-refinement.py'
    require(file_digest(publisher_path) == PUBLISHER_SHA, 'Reviewed publisher verifier changed')
    spec = importlib.util.spec_from_file_location('cs80_audition_offline_publisher', publisher_path)
    publisher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(publisher)
    require(publisher.BUILD_RUN_ATTEMPT == ATTEMPT and publisher.INVENTORY_SHA == INVENTORY_SHA, 'Trusted verifier contract differs')
    trusted = publisher.verify_auditions(outer, SOURCE, RUN)

    # Independently inspect raw ZIP entries/CSV rows/sample arrays. No extraction.
    with zipfile.ZipFile(outer) as archive:
        read = archive.read
        inventory = json.loads(read('inventory.json'))
        require(digest(read('inventory.json')) == INVENTORY_SHA and len(inventory['programs']) == 301, 'Wrong final inventory')
        programs = {p['name']: p for p in inventory['programs']}
        provenance = json.loads(read('provenance.json'))
        expected_selections = [dict(folder=f, pressure=p, hold_seconds=h, tail_seconds=t, presets=names) for f, p, h, t, names in SELECTIONS]
        require(provenance['selections'] == expected_selections, 'Provenance selections differ from source workflow')
        require(provenance['library_revision'] == REVISION and provenance['expected_wav_count'] == provenance['wav_count'] == 24, 'Wrong revision or requested WAV total')
        require(re.fullmatch(r'[0-9a-f]{64}', provenance['capture_binary_sha256']) is not None, 'Invalid capture binary provenance')
        require(provenance['inventory_command'] == ['build/cs80_preset_capture', '--inventory', 'preset-audition/inventory.json'], 'Inventory command differs from source')
        commands = []
        for folder, pressure, hold, tail, names in SELECTIONS:
            command = ['build/cs80_preset_capture', '--out', 'preset-audition/' + folder, '--wav', '--rate', '48000', '--velocity', '0.8', '--hold', str(int(hold)), '--tail', str(int(tail)), '--pressure', '0.65' if pressure else '0']
            for name in names:
                command += ['--preset', name]
            commands.append(command)
        require(provenance['capture_commands'] == commands, 'Capture argv differs from source')
        measurements, seen = [], set()
        for folder, pressure, hold, tail, names in SELECTIONS:
            rows = list(csv.DictReader(io.StringIO(read(folder + '/metrics.csv').decode())))
            require(len(rows) == len(names) and {row['name'] for row in rows} == set(names), 'Wrong CSV selection')
            for row in rows:
                program = programs[row['name']]
                require(row['name'] not in seen, 'Repeated program')
                seen.add(row['name'])
                expected_note = 48 if program['category'] == 'Bass' else 67 if program['category'] in ('Winds', 'Leads') else 72 if program['category'] == 'Bells' else 60
                require(int(row['index']) == program['index'] and row['category'] == program['category'] and int(row['note']) == expected_note, 'CSV factory identity/note differs')
                require(row['parameter_digest'] == fnv_float32(program['values']), 'Factory parameter digest differs')
                require(int(row['sample_rate']) == 48000 and int(row['rank']) == 0, 'Wrong capture format/rank')
                require(struct.pack('<f', float(row['velocity'])) == struct.pack('<f', .8) and struct.pack('<f', float(row['pressure'])) == struct.pack('<f', pressure), 'Wrong velocity/pressure')
                require(float(row['hold_seconds']) == hold and float(row['tail_seconds']) == tail, 'Wrong hold/release excerpt')
                wavs = [name for name in archive.namelist() if name.startswith(folder + '/' + str(program['index']) + '-') and name.endswith('.wav')]
                require(len(wavs) == 1, 'Missing or duplicate program WAV')
                data = read(wavs[0])
                metrics = inspect_wav(data)
                require(metrics['frames'] == round(48000 * (hold + tail)), 'Wrong WAV frame count')
                require(int(row['nonfinite']) == int(row['samples_over_full_scale']) == 0, 'CSV reports invalid samples')
                peak_error = abs(metrics['peak_dbfs'] - float(row['peak_dbfs']))
                rms_error = abs(metrics['rms_dbfs'] - float(row['rms_dbfs']))
                require(peak_error < 1e-7 and rms_error < 1e-7, 'WAV samples differ from CSV levels')
                measurements.append(dict(name=row['name'], index=program['index'], category=program['category'], note=expected_note, pressure=pressure, hold_seconds=hold, tail_seconds=tail, parameter_digest=row['parameter_digest'], wav=wavs[0], wav_sha256=digest(data), peak_error_db=peak_error, rms_error_db=rms_error, **metrics))
        require(len(seen) == 24 and sum(m['frames'] for m in measurements) == 5616000, 'Wrong total frames/programs')
        require({m['wav'] for m in measurements} == {name for name in archive.namelist() if name.endswith('.wav')}, 'Unexamined WAV exists')
        checksum_sha = digest(read('SHA256SUMS'))
    proof = dict(assignment=55, status='ARTIFACT_VERIFIED', verified_utc=datetime.now(timezone.utc).isoformat(), source_commit=SOURCE, run_id=RUN, run_attempt=ATTEMPT, run_status=run['status'], run_conclusion=run['conclusion'], linux_job_id=job['id'], required_producing_steps=required, artifact_id=meta['id'], artifact_name=NAME, public_url=url, outer_zip=str(outer.relative_to(ROOT)), outer_bytes=outer.stat().st_size, outer_sha256=file_digest(outer), artifact_expires_at=meta['expires_at'], trusted_verification=trusted, trusted_publisher_sha256=file_digest(publisher_path), verifier_sha256=file_digest(Path(__file__)), workflow_sha256=WORKFLOW_SHA, inventory_sha256=INVENTORY_SHA, checksum_manifest_sha256=checksum_sha, capture_binary_sha256=provenance['capture_binary_sha256'], wav_count=24, seconds=117, stereo_frames=5616000, parameter_digests_verified=24, nonfinite_samples=0, samples_at_or_over_full_scale=0, highest_peak_dbfs=max(m['peak_dbfs'] for m in measurements), max_peak_error_db=max(m['peak_error_db'] for m in measurements), max_rms_error_db=max(m['rms_error_db'] for m in measurements), measurements=measurements, qualification='CI-produced core captures at unnormalized factory settings. This artifact check does not certify all native/pluginval jobs, arbitrary DAWs, listening quality or acoustic/hardware equivalence. Earlier local99f5 auditions retain their separate provenance.')
    (WORK / 'artifact-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({key: proof[key] for key in ['status', 'artifact_id', 'wav_count', 'seconds', 'stereo_frames', 'highest_peak_dbfs', 'outer_sha256']}))


if __name__ == '__main__':
    main()
