"""Independent, strictly bound verifier for the actual 2026.11.1 CI audition ZIP.
Importing this module performs no capture, download, verification, or file write.
"""
from pathlib import Path, PurePosixPath
from datetime import datetime, timezone
import array
import ast
import csv
import hashlib
import io
import json
import math
import re
import struct
import sys
import zipfile

BASE = Path('/workspace/scratch/ffe08111d1ce')
REPORT = Path(__file__).resolve().parent
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
SOURCE = '6d3cc220edef5a4a2c7cd968ea8a786c7c83198c'
TREE = '88ee322bf66b380c4227e224821783f2db04bbee'
RUN = 45044040694
ATTEMPT = '1'
LINUX_JOB = 137274597635
REVISION = '2026.11.1'
INVENTORY_SHA256 = 'fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa'
WORKFLOW_SHA256 = 'a609f613be8876ca0a1b49cf7242723c535603aa419442b4df1f6b128c235e3e'
CAPTURE_SCRIPT_SHA256 = '3116fe7a689053fc75d7e8b9c43393e62ab57fe3715ae60418e27296da2ca1fc'
NAME = 'cs80-curated-preset-audition-' + SOURCE
ZIP = BASE / ('ci-run-' + str(RUN)) / (NAME + '.zip')

class VerificationError(ValueError):
    pass

def require(condition, message):
    if not condition:
        raise VerificationError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def finite_number(value, description):
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise VerificationError('Invalid number: ' + description) from exc
    require(math.isfinite(number), 'Nonfinite number: ' + description)
    return number

def load_plan(script):
    literals = {}
    for node in ast.parse(script).body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            name = node.targets[0].id
            if name in ('original_presets', 'refined_presets', 'selections'):
                require(name not in literals, 'Duplicate source selection assignment')
                literals[name] = ast.literal_eval(node.value)
    require(set(literals) == {'original_presets', 'refined_presets', 'selections'}, 'Missing source selection literals')
    original, refined = literals['original_presets'], literals['refined_presets']
    require(len(original) == len(refined) == 12 and not original & refined, 'Expected disjoint original and refined sets of 12')
    groups = []
    for folder, pressure, hold, tail, names in literals['selections']:
        groups.append({'folder': folder, 'pressure': finite_number(pressure, folder + ' pressure'),
                       'hold_seconds': finite_number(hold, folder + ' hold'),
                       'tail_seconds': finite_number(tail, folder + ' tail'), 'presets': names})
    requested = [name for group in groups for name in group['presets']]
    require(len(requested) == len(set(requested)) == 24, 'Expected exactly 24 unique programs')
    require(set(requested) == original | refined, 'Source selection omits or adds a program')
    shape = [(g['folder'], g['pressure'], g['hold_seconds'], g['tail_seconds'], len(g['presets'])) for g in groups]
    require(shape == [('standard', 0, 3, 1, 10), ('pressure', 0.65, 3, 1, 1), ('refined', 0, 3, 1, 6), ('slow', 0, 5, 2, 7)], 'Source group contract changed')
    require(groups[1]['presets'] == ['Pressure Lead'], 'Pressure group must contain Pressure Lead')
    require('Slow Pad' in groups[3]['presets'], 'Slow Pad must use the longer group')
    total_seconds = sum((g['hold_seconds'] + g['tail_seconds']) * len(g['presets']) for g in groups)
    require(total_seconds == 117, 'Expected 117 total audio seconds')
    return groups

def expected_commands(groups):
    commands = []
    for group in groups:
        command = ['build/cs80_preset_capture', '--out', 'preset-audition/' + group['folder'], '--wav',
                   '--rate', '48000', '--velocity', '0.8', '--hold', format(group['hold_seconds'], 'g'),
                   '--tail', format(group['tail_seconds'], 'g'), '--pressure', format(group['pressure'], 'g')]
        for name in group['presets']:
            command.extend(['--preset', name])
        commands.append(command)
    return commands

def verify_provenance(provenance, groups):
    require(provenance.get('schema') == 1, 'Unexpected provenance schema')
    require(provenance.get('source_commit') == SOURCE, 'Wrong source commit in provenance')
    require(provenance.get('repository') == REPOSITORY, 'Wrong repository in provenance')
    require(type(provenance.get('run_id')) is str and provenance['run_id'] == str(RUN), 'Wrong or non-string run ID in provenance')
    require(type(provenance.get('run_attempt')) is str and provenance['run_attempt'] == ATTEMPT, 'Provenance attempt must be the exact string "1"')
    require(provenance.get('capture_status') == 'complete', 'Capture did not complete')
    require(provenance.get('library_revision') == REVISION, 'Wrong library revision')
    require(provenance.get('expected_wav_count') == provenance.get('wav_count') == 24, 'Wrong expected or actual WAV count')
    require(provenance.get('selections') == groups, 'Provenance selections differ from published source')
    require(provenance.get('capture_commands') == expected_commands(groups), 'Capture commands differ from published source')
    require(provenance.get('inventory_command') == ['build/cs80_preset_capture', '--inventory', 'preset-audition/inventory.json'], 'Wrong inventory command')
    require(re.fullmatch(r'[0-9a-f]{64}', provenance.get('capture_binary_sha256', '')) is not None, 'Invalid capture binary digest')
    require(isinstance(provenance.get('platform'), str) and bool(provenance['platform']), 'Missing capture platform')
    require(not provenance.get('error'), 'Capture provenance contains an error')

def verify_checksums(files):
    require('SHA256SUMS' in files, 'Missing checksum list')
    checked = {}
    for line in files['SHA256SUMS'].decode('utf-8').splitlines():
        parts = line.split('  ', 1)
        require(len(parts) == 2, 'Malformed checksum entry')
        digest, path = parts
        require(re.fullmatch(r'[0-9a-f]{64}', digest) is not None, 'Malformed checksum digest')
        require(path not in checked and path in files and path != 'SHA256SUMS', 'Missing or duplicate checksum target')
        require(sha(files[path]) == digest, 'Checksum mismatch: ' + path)
        checked[path] = digest
    require(set(checked) == set(files) - {'SHA256SUMS'}, 'Incomplete checksum coverage')
    return checked

def parameter_digest(values):
    value_hash = 14695981039346656037
    for value in values:
        for byte in struct.pack('<f', finite_number(value, 'inventory parameter')):
            value_hash = ((value_hash ^ byte) * 1099511628211) & 0xffffffffffffffff
    return format(value_hash, 'x')

def inspect_wav(data, expected_frames):
    require(len(data) >= 58 and data[:4] == b'RIFF' and data[8:12] == b'WAVE', 'Invalid WAV container')
    require(struct.unpack_from('<I', data, 4)[0] + 8 == len(data), 'RIFF size does not match file')
    chunks, offset = {}, 12
    while offset + 8 <= len(data):
        kind, size = struct.unpack_from('<4sI', data, offset)
        start = offset + 8
        require(start + size <= len(data) and kind not in chunks, 'Malformed or repeated RIFF chunk')
        chunks[kind] = data[start:start + size]
        offset = start + size + (size & 1)
    require(offset == len(data), 'Trailing partial RIFF chunk')
    require({b'fmt ', b'fact', b'data'} <= set(chunks), 'Missing float WAV chunks')
    require(len(chunks[b'fmt ']) == 18 and len(chunks[b'fact']) == 4, 'Unexpected fmt or fact chunk size')
    fmt = struct.unpack('<HHIIHHH', chunks[b'fmt '])
    require(fmt == (3, 2, 48000, 384000, 8, 32, 0), 'Expected float32 stereo 48 kHz WAV')
    require(len(chunks[b'data']) % 8 == 0, 'Partial interleaved frame')
    frames = len(chunks[b'data']) // 8
    require(frames == expected_frames, 'WAV frame count differs from its source group')
    require(struct.unpack('<I', chunks[b'fact'])[0] == frames, 'WAV fact frame count disagrees')
    samples = array.array('f')
    samples.frombytes(chunks[b'data'])
    if sys.byteorder != 'little':
        samples.byteswap()
    nonfinite = sum(not math.isfinite(sample) for sample in samples)
    over = sum(abs(sample) > 1.0 for sample in samples)
    require(nonfinite == 0, 'WAV contains nonfinite samples')
    require(over == 0, 'WAV contains samples over full scale')
    peak = max(map(abs, samples), default=0)
    rms = math.sqrt(math.fsum(float(sample) * float(sample) for sample in samples) / len(samples)) if samples else 0
    require(peak > 0 and rms > 0, 'WAV is silent')
    return {'format': 'IEEE float32', 'format_code': 3, 'channels': 2, 'sample_rate_hz': 48000,
            'frames': frames, 'fact_frames': frames, 'interleaved_samples': len(samples),
            'duration_seconds': frames / 48000, 'raw_peak_dbfs': 20 * math.log10(max(peak, 1e-12)),
            'raw_rms_dbfs': 20 * math.log10(max(rms, 1e-12)),
            'raw_nonfinite_samples': nonfinite, 'raw_samples_over_full_scale': over}

def check_metric_agreement(wav, row):
    for metric in ('peak_dbfs', 'rms_dbfs'):
        value = finite_number(row[metric], metric)
        require(abs(wav['raw_' + metric] - value) <= 1e-6, 'CSV/raw WAV mismatch: ' + metric)

def verify_source(report):
    source = json.loads((report / 'source-evidence.json').read_text())
    require(source['source_commit'] == SOURCE and source['source_tree'] == TREE, 'Source evidence binding mismatch')
    require(source['run_id'] == RUN and source['run_attempt'] == ATTEMPT and source['linux_job_id'] == LINUX_JOB, 'Source evidence run binding mismatch')
    workflow = (report / 'published-build.yml').read_bytes()
    script = (report / 'published-audition.py').read_bytes()
    require(sha(workflow) == source['workflow_sha256'] == WORKFLOW_SHA256, 'Published workflow digest changed')
    require(sha(script) == source['audition_python_sha256'] == CAPTURE_SCRIPT_SHA256, 'Published capture script digest changed')
    match = re.search(r"      - name: Capture curated preset auditions\n.*?          python3 - <<'PY'\n(.*?)          PY\n", workflow.decode(), re.DOTALL)
    require(match is not None, 'Source capture heredoc missing')
    extracted = ''.join(line[10:] if line.startswith('          ') else line for line in match.group(1).splitlines(keepends=True))
    require(extracted.encode() == script, 'Capture script does not match workflow heredoc')
    for record in source['published_files']:
        require(sha((report / record['local_file']).read_bytes()) == record['sha256'], 'Frozen source/evidence file changed: ' + record['local_file'])
    final = json.loads((report / 'final-core-evidence.json').read_text())
    retained = json.loads((report / 'retained-inventory-evidence.json').read_text())
    require(final['library_revision'] == REVISION, 'Wrong final evidence revision')
    require(final['input_sha256']['build-audit/inventory-final.json'] == INVENTORY_SHA256, 'Final-bank inventory reference differs')
    require(retained['source_artifacts']['build-w04-distinctness/inventory.json']['sha256'] == INVENTORY_SHA256, 'Retained inventory reference differs')
    return source, load_plan(script.decode())

def verify_artifact(report=REPORT, archive_path=ZIP):
    source, groups = verify_source(report)
    artifact = json.loads((report / 'artifact-metadata.json').read_text())['artifact']
    job = json.loads((report / 'linux-job-at-download.json').read_text())['job']
    now = datetime.now(timezone.utc)
    require(artifact['name'] == NAME and type(artifact['id']) is int and artifact['id'] > 0, 'Wrong artifact identity')
    require(artifact['workflow_run']['id'] == RUN and artifact['workflow_run']['head_sha'] == SOURCE, 'Artifact run or source mismatch')
    require(artifact['expired'] is False, 'Artifact API reports expiry')
    require(datetime.fromisoformat(artifact['expires_at'].replace('Z', '+00:00')) > now, 'Artifact has expired')
    require(job['id'] == LINUX_JOB and job['run_id'] == RUN, 'Wrong Linux job or attempt')
    if 'run_attempt' in job:
        require(type(job['run_attempt']) is int and job['run_attempt'] == int(ATTEMPT), 'API job attempt mismatch')
    if 'head_sha' in job:
        require(job['head_sha'] == SOURCE, 'API job source mismatch')
    steps = {}
    for name in ['Test', 'Capture curated preset auditions', 'Upload curated preset auditions']:
        found = [step for step in job['steps'] if step['name'] == name]
        require(len(found) == 1 and found[0]['status'] == 'completed' and found[0]['conclusion'] == 'success', 'Required step did not succeed: ' + name)
        steps[name] = found[0]
    archive_bytes = archive_path.read_bytes()
    archive_sha = sha(archive_bytes)
    require(len(archive_bytes) == artifact['size_in_bytes'], 'ZIP size differs from API')
    require(artifact['digest'] == 'sha256:' + archive_sha, 'ZIP digest differs from API')
    with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
        require(archive.testzip() is None, 'ZIP CRC failure')
        names = [info.filename for info in archive.infolist() if not info.is_dir()]
        require(len(names) == len(set(names)), 'Duplicate ZIP paths')
        for name in names:
            path = PurePosixPath(name)
            require(not path.is_absolute() and '..' not in path.parts and '\\' not in name, 'Unsafe archive path')
        files = {name: archive.read(name) for name in names}
    require({'source-commit.txt', 'provenance.json', 'inventory.json', 'README.txt', 'SHA256SUMS'} <= set(files), 'Missing required archive files')
    checksums = verify_checksums(files)
    require(files['source-commit.txt'].decode() == SOURCE + '\n', 'Source marker mismatch')
    provenance = json.loads(files['provenance.json'])
    verify_provenance(provenance, groups)
    inventory_sha = sha(files['inventory.json'])
    require(inventory_sha == INVENTORY_SHA256, 'Actual inventory bytes differ from final-bank evidence hash')
    inventory = json.loads(files['inventory.json'])
    require(inventory['library_revision'] == REVISION and len(inventory['programs']) == 301, 'Wrong inventory revision or count')
    programs = {program['index']: program for program in inventory['programs']}
    require(set(programs) == set(range(301)), 'Wrong inventory program indices')
    require(len({program['name'] for program in programs.values()}) == 301, 'Duplicate inventory names')
    expected_csvs = {group['folder'] + '/metrics.csv' for group in groups}
    require({name for name in files if name.endswith('/metrics.csv')} == expected_csvs, 'Wrong metrics CSV set')
    results, expected_wavs = [], set()
    for group in groups:
        rows = list(csv.DictReader(io.StringIO(files[group['folder'] + '/metrics.csv'].decode())))
        require(len(rows) == len(group['presets']) and {row['name'] for row in rows} == set(group['presets']), 'CSV selection mismatch: ' + group['folder'])
        expected_frames = round((group['hold_seconds'] + group['tail_seconds']) * 48000)
        for row in rows:
            index = int(row['index'])
            require(index in programs, 'CSV index missing from inventory')
            program = programs[index]
            require(program['name'] == row['name'] and program['category'] == row['category'], 'CSV identity differs from inventory')
            expected_note = {'Bass': 48, 'Winds': 67, 'Leads': 67, 'Bells': 72}.get(program['category'], 60)
            settings = {'note': expected_note, 'sample_rate': 48000, 'velocity': 0.8, 'pressure': group['pressure'],
                        'hold_seconds': group['hold_seconds'], 'tail_seconds': group['tail_seconds'],
                        'rank': 0, 'nonfinite': 0, 'samples_over_full_scale': 0}
            for key, expected in settings.items():
                require(abs(finite_number(row[key], row['name'] + ' ' + key) - expected) <= 1e-6, 'CSV capture setting differs: ' + row['name'] + ' ' + key)
            require(row['parameter_digest'] == parameter_digest(program['values']), 'CSV parameter digest differs from inventory: ' + row['name'])
            filename = str(index) + '-' + re.sub(r'[^A-Za-z0-9]', '-', row['name']) + '.wav'
            path = group['folder'] + '/' + filename
            require(path in files and path not in expected_wavs, 'Missing or duplicate WAV: ' + path)
            expected_wavs.add(path)
            wav = inspect_wav(files[path], expected_frames)
            check_metric_agreement(wav, row)
            results.append({'name': row['name'], 'index': index, 'category': row['category'],
                            'group': group['folder'], 'path': path, 'sha256': sha(files[path]), 'bytes': len(files[path]),
                            'midi_note': int(row['note']), 'velocity': float(row['velocity']), 'pressure': float(row['pressure']),
                            'hold_seconds': float(row['hold_seconds']), 'tail_seconds': float(row['tail_seconds']),
                            'rank': int(row['rank']), 'parameter_digest': row['parameter_digest'],
                            'parameter_digest_matches_inventory': True,
                            'csv_peak_dbfs': float(row['peak_dbfs']), 'csv_rms_dbfs': float(row['rms_dbfs']),
                            'csv_nonfinite_samples': int(row['nonfinite']), 'csv_samples_over_full_scale': int(row['samples_over_full_scale']), **wav})
    require({name for name in files if name.lower().endswith('.wav')} == expected_wavs, 'Extra or missing WAV in archive')
    require(len(results) == 24 and sum(row['duration_seconds'] for row in results) == 117, 'Wrong aggregate clip count or duration')
    require(sum(row['frames'] for row in results) == 5616000, 'Wrong aggregate frames')
    lead = next(row for row in results if row['name'] == 'Pressure Lead')
    require(abs(lead['pressure'] - 0.65) <= 1e-6 and lead['raw_rms_dbfs'] > -80, 'Pressure Lead is not demonstrably nonsilent at pressure .65')
    require(all(row['pressure'] == 0 for row in results if row['name'] != 'Pressure Lead'), 'Unexpected nonzero pressure')
    return {
        'result': 'PASS', 'scope': 'Actual 2026.11.1 CI audition archive and capture/upload steps only; other native and validator gates are separate',
        'verified_at_utc': now.isoformat(), 'repository': REPOSITORY, 'source_commit': SOURCE, 'source_tree': TREE,
        'run_id': RUN, 'run_attempt': ATTEMPT, 'linux_job_id': LINUX_JOB,
        'artifact_id': artifact['id'], 'artifact_name': artifact['name'], 'artifact_zip': str(archive_path),
        'artifact_zip_sha256': archive_sha, 'artifact_zip_bytes': len(archive_bytes),
        'artifact_created_at': artifact['created_at'], 'artifact_expires_at': artifact['expires_at'],
        'api_digest_size_run_source_expiry_verified': True, 'zip_crc_all_valid': True,
        'archive_files': len(files), 'checksum_entries_verified': len(checksums), 'checksum_coverage_complete': True,
        'source_workflow_sha256': WORKFLOW_SHA256, 'source_capture_python_sha256': CAPTURE_SCRIPT_SHA256,
        'capture_status': provenance['capture_status'], 'capture_platform': provenance['platform'],
        'capture_binary_sha256_recorded_in_provenance': provenance['capture_binary_sha256'],
        'capture_commands_and_selections_match_published_source': True,
        'required_steps': steps, 'linux_job_status_at_observation': job['status'], 'linux_job_conclusion_at_observation': job['conclusion'],
        'library_revision': REVISION, 'inventory_program_count': 301, 'inventory_sha256': inventory_sha,
        'inventory_matches_retained_final_bank_hash': True,
        'wav_count': len(results), 'csv_row_count': len(results), 'all_24_names_match_source': True,
        'total_audio_seconds': 117, 'total_frames': 5616000, 'total_interleaved_samples': 11232000,
        'groups': [{**group, 'wav_count': len(group['presets'])} for group in groups],
        'all_raw_and_csv_nonfinite_and_over_full_scale_zero': True, 'all_wavs_nonsilent': True,
        'raw_csv_peak_rms_agreement_tolerance_db': 1e-6, 'all_parameter_digests_match_inventory_float32_values': True,
        'pressure_lead_nonsilent_at_requested_pressure_0_65': True, 'other_programs_pressure_zero': True,
        'no_audio_regenerated': True, 'no_repository_edits': True,
        'wavs': results, 'verified_SHA256SUMS': checksums,
    }

def write_report(result, report):
    (report / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
    lines = ['# Actual 2026.11.1 CI audition artifact — PASS', '',
             f"Verified artifact **{result['artifact_id']}**, run **{RUN}**, attempt **{ATTEMPT}**, source `{SOURCE}`, tree `{TREE}`.", '',
             f"ZIP: **{result['artifact_zip_bytes']:,} bytes**, SHA-256 `{result['artifact_zip_sha256']}`. Both match the API; source/run/expiry match. Test, capture, and upload steps succeeded. Other native and validator gates remain separate.", '',
             f"All ZIP CRCs and **{result['checksum_entries_verified']}** checksum entries verify, with complete archive-file coverage. Provenance says `capture_status=complete`; commands and group selections match the exact published workflow.", '',
             f"Inventory: **301 programs**, revision **{REVISION}**, exact retained-final-bank SHA-256 `{INVENTORY_SHA256}`. Every CSV parameter digest also matches the corresponding inventory float32 values.", '',
             '**24 actual float32 stereo 48 kHz WAVs, 117 seconds total**: 17 clips contain 192,000 frames (3-second hold + 1-second release); seven slow clips contain 336,000 frames (5+2 seconds). Total: **5,616,000 frames / 11,232,000 interleaved samples**.', '',
             'All decoded samples and CSV counters are finite and within full scale. Every clip is nonsilent, and decoded peak/RMS agrees with CSV within 0.000001 dB. Velocity is 0.8 and rank is 0 (full patch); only Pressure Lead uses pressure 0.65. No audio was regenerated and no repository files were edited.', '',
             '| Program | Group | Seconds | Note | Pressure | Peak dBFS | RMS dBFS |',
             '| --- | --- | ---: | ---: | ---: | ---: | ---: |']
    for row in result['wavs']:
        lines.append(f"| {row['name']} | {row['group']} | {row['duration_seconds']:g} | {row['midi_note']} | {row['pressure']:.2f} | {row['raw_peak_dbfs']:.6f} | {row['raw_rms_dbfs']:.6f} |")
    lines.extend(['', '`verification.json` contains full per-WAV hashes, sample counts, controller conditions, decoded/CSV metrics, parameter digests, and every verified internal checksum.', ''])
    (report / 'RESULTS.md').write_text('\n'.join(lines))

def main():
    required = [REPORT / 'artifact-metadata.json', REPORT / 'linux-job-at-download.json', ZIP]
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        print(json.dumps({'status': 'NOT_VERIFIED_MISSING_INPUTS', 'actual_artifact_verification_performed': False, 'missing': missing}, indent=2))
        return 2
    try:
        result = verify_artifact()
    except Exception as error:
        failure = {'result': 'FAIL', 'scope': 'Actual audition verification failed; no PASS claimed', 'source_commit': SOURCE, 'run_id': RUN, 'run_attempt': ATTEMPT, 'error_type': type(error).__name__, 'error': str(error)}
        (REPORT / 'verification.json').write_text(json.dumps(failure, indent=2) + '\n')
        print(json.dumps(failure, indent=2))
        return 1
    write_report(result, REPORT)
    print(json.dumps({key: value for key, value in result.items() if key not in ('wavs', 'verified_SHA256SUMS', 'required_steps', 'groups')}, indent=2))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
