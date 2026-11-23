#!/usr/bin/env python3
"""A58 supplemental actual-artifact checks; never extracts or executes product code.

The frozen repository verifier remains unchanged and is invoked only after every
ZipInfo entry passes this supplement. Source and API identifiers are fixed to the
requested original native run. Use a new output directory for each invocation.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import subprocess
import sys
import types
import zipfile

SOURCE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
EXPORT_SOURCE = 'e32728b31d97fc356cac291cb98935fdcd38f803'
RUN = 45246863788
ATTEMPT = 1
JOB = 137945804596
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
NAME = 'cs80-curated-preset-audition-' + SOURCE
INVENTORY = 'docs/preset-evidence/2026.11.4/continuation/final-inventory/inventory.json'
INVENTORY_SHA = '0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611'
HELPER = 'docs/preset-evidence/2026.11.4/ci/verification-tools/verify-audition-zip.py'
HELPER_SHA = '75be3d29b566c9a88f9d301853ae8c60585aa604b3fc450ba23de1a6eaeebf62'
CAPTURE = 'scripts/capture-factory-auditions.py'
WORKFLOW = '.github/workflows/build.yml'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def unique_object(pairs):
    obj = {}
    for key, value in pairs:
        require(key not in obj, 'Duplicate JSON object key: ' + key)
        obj[key] = value
    return obj


def parse_json(data):
    return json.loads(data, object_pairs_hook=unique_object,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError('Nonfinite JSON ' + value)))


def read_json(path):
    return parse_json(path.read_bytes())


def timestamp(value):
    require(isinstance(value, str), 'Missing timestamp')
    value = datetime.fromisoformat(value.replace('Z', '+00:00'))
    require(value.tzinfo is not None, 'Timestamp lacks timezone')
    return value


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def source_contract(repo):
    git = lambda *args: subprocess.check_output(['git', '-C', str(repo), *args])
    require(git('diff', '--name-only', EXPORT_SOURCE, SOURCE, '--', 'src/core', 'src/plugin',
                'src/tools/preset_capture.cpp', CAPTURE) == b'', 'Retained export/product source paths differ')
    source_files = {}
    for name in (HELPER, CAPTURE, 'src/tools/preset_capture.cpp', WORKFLOW):
        pinned = git('show', SOURCE + ':' + name)
        require((repo / name).read_bytes() == pinned, 'Working copy differs from pinned source: ' + name)
        source_files[name] = {'sha256': sha(pinned), 'bytes': len(pinned)}
    require(source_files[HELPER]['sha256'] == HELPER_SHA, 'Frozen helper differs from A33/A34 review')
    producer = types.ModuleType('a58_source_bound_roster')
    producer.__file__ = str(repo / CAPTURE)
    exec(compile(git('show', SOURCE + ':' + CAPTURE), producer.__file__, 'exec'), producer.__dict__)
    groups = producer.selections()
    expected = producer.roster_summary(groups)
    require(expected == {'wav_count': 138, 'selected_program_count': 74, 'audio_seconds': 870},
            'Pinned source roster changed')
    require(len(groups) == 30, 'Pinned source folder count changed')
    retained_bytes = (repo / INVENTORY).read_bytes()
    require(sha(retained_bytes) == INVENTORY_SHA, 'Retained inventory changed')
    retained = parse_json(retained_bytes)
    require(set(retained) == {'schema', 'library_revision', 'parameter_ids', 'programs'}, 'Retained inventory shape')
    require(retained['schema'] == 1 and retained['library_revision'] == '2026.11.4', 'Retained inventory revision')
    require(len(retained['programs']) == 523 and len(retained['parameter_ids']) == 200, 'Retained inventory size')
    producer.validate_resolved_requests(groups, retained['programs'])
    by_name = {program['name']: program for program in retained['programs']}
    expected_files = {'README.txt', 'SHA256SUMS', 'inventory.json', 'provenance.json', 'source-commit.txt'}
    for group in groups:
        expected_files.add(group['folder'] + '/metrics.csv')
        for name in group['names']:
            p = by_name[name]
            expected_files.add(group['folder'] + '/' + str(p['host_index']) + '-' + re.sub('[^A-Za-z0-9]', '-', name) + '.wav')
    require(len(expected_files) == 173, 'Expected file roster changed')
    report = dict(schema=1, source_commit=SOURCE, source_tree=git('rev-parse', SOURCE + '^{tree}').decode().strip(),
                  repository=REPOSITORY, run_id=RUN, run_attempt=ATTEMPT, producer_job_id=JOB,
                  source_files=source_files, retained_inventory_sha256=INVENTORY_SHA,
                  retained_inventory_export_source=EXPORT_SOURCE,
                  export_to_release_product_source_paths_unchanged=True,
                  expected_only=dict(**expected, folder_count=30, stereo_frames=41760000,
                                     regular_files=173, checksums=172, inventory_programs=523,
                                     parameter_count=200, active_body_models=list(range(1, 20))),
                  expected_files=sorted(expected_files), source_selections=groups)
    return producer, retained, expected_files, report


def check_api(archive, api, run, job, log):
    require(type(api['id']) is int and api['id'] > 0, 'Invalid artifact ID')
    require(api['name'] == NAME and api['expired'] is False, 'Wrong/expired artifact')
    require(type(api['size_in_bytes']) is int and 0 < api['size_in_bytes'] < 512 * 1024 * 1024,
            'Artifact size invalid')
    require(archive.stat().st_size == api['size_in_bytes'], 'Actual ZIP size differs from API')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', api['digest']), 'API digest missing/invalid')
    digest = api['digest'][7:]
    require(file_sha(archive) == digest, 'Actual ZIP SHA256 differs from API')
    require(timestamp(api['expires_at']) > datetime.now(timezone.utc), 'Artifact already expired')
    require(api['url'] == f'https://api.github.com/repos/{REPOSITORY}/actions/artifacts/{api["id"]}', 'Artifact API URL')
    require(api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE, 'Artifact run/source mismatch')
    require(run['id'] == RUN and run['run_attempt'] == ATTEMPT and run['head_sha'] == SOURCE, 'Run source/attempt mismatch')
    require(run['path'] == WORKFLOW and run['repository']['full_name'] == REPOSITORY, 'Run repository/workflow mismatch')
    require(api['workflow_run']['repository_id'] == run['repository']['id']
            and api['workflow_run']['head_repository_id'] == run['head_repository']['id'], 'Artifact repository IDs differ')
    require(job['id'] == JOB and job['run_id'] == RUN and job['run_attempt'] == ATTEMPT
            and job['head_sha'] == SOURCE, 'Producer job identity mismatch')
    require(job['name'] == 'Linux / VST3 + Standalone' and 'ubuntu-24.04' in job['labels'], 'Wrong native producer')
    require(job['status'] == 'completed', 'Producer original job not completed; original log not final')
    required = ['Run actions/checkout@v5', 'Configure', 'Verify configured JUCE source', 'Build', 'Test',
                'Capture curated preset auditions', 'Upload curated preset auditions']
    steps = []
    for name in required:
        matches = [step for step in job['steps'] if step['name'] == name]
        require(len(matches) == 1, 'Missing/ambiguous step: ' + name)
        step = matches[0]
        require(step['status'] == 'completed' and step['conclusion'] == 'success', 'Step did not succeed: ' + name)
        require(timestamp(step['started_at']) <= timestamp(step['completed_at']), 'Invalid step interval')
        steps.append(step)
    require([step['number'] for step in steps] == [2, 4, 5, 6, 7, 8, 9], 'Producer step order changed')
    for before, after in zip(steps, steps[1:]):
        require(timestamp(before['completed_at']) <= timestamp(after['started_at']), 'Producer steps overlap/out of order')
    upload = steps[-1]
    require(timestamp(upload['started_at']) <= timestamp(api['created_at']) <= timestamp(upload['completed_at']),
            'Artifact created outside matching upload step')
    require(timestamp(api['created_at']) <= timestamp(api['updated_at']) <= timestamp(upload['completed_at']),
            'Artifact updated outside matching upload step')
    # Scope the immutable upload ID, digest, and byte count to this upload group.
    anchor = f'name: {NAME}'
    require(log.count(anchor) == 1, 'Original log lacks unique named audition upload')
    pos = log.index(anchor)
    start = log.rfind('##[group]Run actions/upload-artifact@v5', 0, pos)
    require(start >= 0, 'Named upload lacks upload action group')
    end = log.find('##[group]', pos)
    section = log[start:end if end >= 0 else len(log)]
    progress = [int(value) for value in re.findall(r'Uploaded bytes ([0-9]+)(?:\r?\n|$)', section)]
    require(progress and all(0 < value <= api['size_in_bytes'] for value in progress),
            'Upload progress missing or outside API size bound')
    require(all(before <= after for before, after in zip(progress, progress[1:]))
            and progress[-1] == api['size_in_bytes'],
            'Upload progress is not monotonic or final size differs from API')
    require(f'SHA256 digest of uploaded artifact zip is {digest}' in section, 'Upload log digest differs from API')
    require(f'Artifact {NAME}.zip successfully finalized. Artifact ID {api["id"]}' in section, 'Upload log ID differs from API')
    require(f'Artifact {NAME} has been successfully uploaded! Final size is {api["size_in_bytes"]} bytes. Artifact ID is {api["id"]}' in section,
            'Upload log name/size/ID differs from API')
    require(f'https://github.com/{REPOSITORY}/actions/runs/{RUN}/artifacts/{api["id"]}' in section, 'Upload log run differs')
    require('python3 scripts/capture-factory-auditions.py --capture build/cs80_preset_capture --out preset-audition' in log,
            'Original log lacks source workflow capture invocation')
    return dict(artifact_id=api['id'], artifact_name=NAME, zip_bytes=api['size_in_bytes'], zip_sha256=digest,
                expired=False, created_at=api['created_at'], updated_at=api['updated_at'], expires_at=api['expires_at'],
                source_commit=SOURCE, run_id=RUN, run_attempt=ATTEMPT, producer_job_id=JOB,
                producer_job_conclusion=job['conclusion'], required_successful_steps=steps,
                cumulative_upload_byte_progress=progress,
                upload_log_matches_name_id_size_digest_run=True)


def preflight(archive):
    infos = archive.infolist()
    require(0 < len(infos) < 500, 'Unexpected all-entry count')
    require(sum(info.file_size for info in infos) < 512 * 1024 * 1024, 'Expanded size exceeds bound')
    seen, folded, records = set(), set(), []
    for info in infos:
        name = info.filename
        require(name == info.orig_filename and name and len(name) <= 512, 'Truncated/empty/oversized ZIP name')
        require(all(32 <= ord(char) < 127 for char in name) and '\\' not in name and ':' not in name,
                'Unsafe ZIP name: ' + repr(name))
        directory = name.endswith('/')
        canonical = name[:-1] if directory else name
        path = PurePosixPath(canonical)
        require(canonical and not path.is_absolute() and str(path) == canonical
                and all(part not in ('', '.', '..') and not part.endswith((' ', '.')) for part in canonical.split('/')),
                'Noncanonical/escaping ZIP name: ' + repr(name))
        require(canonical not in seen and canonical.casefold() not in folded, 'Duplicate canonical ZIP path: ' + name)
        seen.add(canonical)
        folded.add(canonical.casefold())
        require(info.create_system in (0, 3), 'Unsupported ZIP creator platform')
        mode = (info.external_attr >> 16) & 0xffff
        kind = stat.S_IFMT(mode)
        require(kind in ((0, stat.S_IFDIR) if directory else (0, stat.S_IFREG)), 'Symlink/special/conflicting ZIP type: ' + name)
        require(not (info.external_attr & 0x10) or directory, 'DOS directory attribute on a file')
        require(not (info.flag_bits & 0x41), 'Encrypted ZIP entry unsupported')
        require(info.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED), 'Unsupported ZIP compression')
        require(0 <= info.file_size < 16 * 1024 * 1024, 'ZIP entry size outside bound')
        require(not directory or info.file_size == 0, 'Nonempty directory member')
        records.append(dict(path=name, canonical_path=canonical, kind='directory' if directory else 'regular_file',
                            create_system=info.create_system, unix_mode=oct(mode), external_attr=info.external_attr,
                            flags=info.flag_bits, compressed_bytes=info.compress_size, uncompressed_bytes=info.file_size,
                            compression=info.compress_type, crc32=f'{info.CRC:08x}'))
    regular = {record['canonical_path'] for record in records if record['kind'] == 'regular_file'}
    directories = {record['canonical_path'] for record in records if record['kind'] == 'directory'}
    for name in regular:
        require(not any(str(parent) in regular for parent in PurePosixPath(name).parents if str(parent) != '.'),
                'File ancestor conflicts with another ZIP file')
    # Read only after all entry metadata passes; reading never extracts.
    for info in infos:
        if info.is_dir():
            require(archive.read(info) == b'', 'Unexpected directory payload')
    return records, regular, directories


def compare_inventory(actual, retained):
    require(set(actual) == set(retained), 'Delivered inventory root keys differ from retained inventory')
    for key in ('schema', 'library_revision', 'parameter_ids'):
        require(actual[key] == retained[key], 'Delivered inventory field differs: ' + key)
    require(len(actual['programs']) == len(retained['programs']) == 523, 'Delivered program count differs')
    vector_hash = hashlib.sha256()
    identities = []
    for delivered, previous in zip(actual['programs'], retained['programs']):
        require(set(delivered) == set(previous), 'Delivered program field set differs')
        for key in set(previous) - {'values'}:
            require(type(delivered[key]) is type(previous[key]), f'Delivered program identity type differs: {key}')
            require(delivered[key] == previous[key], f'Delivered program {previous["index"]} identity/tag differs: {key}')
        require(len(delivered['values']) == len(previous['values']) == 200, 'Delivered vector size differs')
        require(all(type(value) in (int, float) and math.isfinite(value) for value in delivered['values']), 'Invalid delivered vector')
        new_bytes = struct.pack('<200f', *delivered['values'])
        old_bytes = struct.pack('<200f', *previous['values'])
        require(new_bytes == old_bytes, f'Delivered binary32 vector differs: {previous["index"]}')
        vector_hash.update(new_bytes)
        identities.append({key: value for key, value in delivered.items() if key != 'values'})
    return dict(programs_compared=523, parameters_per_program=200, values_compared=104600,
                identity_fields=['index', 'host_index', 'name', 'category', 'description', 'tags'],
                parameter_ids_identical=True, all_identities_and_tags_identical=True,
                all_vectors_identical_at_binary32=True, ordered_binary32_vectors_sha256=vector_hash.hexdigest(),
                ordered_identities_sha256=sha(json.dumps(identities, sort_keys=True, separators=(',', ':')).encode()))


def check_manifest_commands(provenance, producer, files):
    command = provenance['inventory_command']
    require(isinstance(command, list) and len(command) == 5 and command[1:4] == ['--library', 'all', '--inventory'], 'Inventory command shape')
    binary, inventory = PurePosixPath(command[0]), PurePosixPath(command[4])
    require(binary.is_absolute() and inventory.is_absolute() and str(binary) == command[0] and str(inventory) == command[4],
            'Noncanonical producer command path')
    require('..' not in binary.parts and '..' not in inventory.parts, 'Parent traversal in producer command paths')
    out = inventory.parent
    require(binary.parts[-2:] == ('build', 'cs80_preset_capture') and out.name == 'preset-audition'
            and inventory.name == 'inventory.json' and binary.parent.parent == out.parent, 'Unexpected producer binary/output paths')
    expected = []
    for group in producer.selections():
        cmd = [str(binary), '--library', 'all', '--out', str(out / group['folder']), '--wav',
               '--rate', '48000', '--velocity', str(group['velocity']), '--pressure', str(group['pressure']),
               '--hold', str(group['hold']), '--tail', str(group['tail'])]
        if 'note' in group:
            cmd += ['--note', str(group['note'])]
        for name in group['names']:
            cmd += ['--preset', name]
        expected.append(cmd)
    require(provenance['capture_commands'] == expected, 'Delivered capture commands differ from source producer')
    readme = producer.audition_readme(producer.selections(), producer.roster_summary(producer.selections()), SOURCE, '2026.11.4')
    require(files['README.txt'] == readme.encode(), 'Delivered README differs from source producer')
    return dict(inventory_command_matches_source=True, capture_commands_match_source=True,
                source_generated_readme_identical=True, capture_command_count=len(expected),
                normalization_scope='Source-pinned writer exports original float bits without export normalization or additional clipping; actual sample/CSV equality is checked by the frozen verifier.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--prepare', action='store_true')
    for flag in ('archive', 'artifact-api', 'run-api', 'job-api', 'job-log'):
        parser.add_argument('--' + flag, type=Path)
    args = parser.parse_args()
    require(not args.out.exists(), 'Output directory must be fresh')
    args.out.mkdir(parents=True)
    producer, retained, expected_files, contract = source_contract(args.repo)
    write_json(args.out / 'source-contract.json', contract)
    if args.prepare:
        write_json(args.out / 'status.json', dict(schema=1, status='prepared_pending_actual_archive', accepted=False,
                   expected_artifact_name=NAME, source_commit=SOURCE, run_id=RUN, run_attempt=ATTEMPT, producer_job_id=JOB))
        print('Prepared source contract only; actual archive acceptance is pending.')
        return
    require(all(getattr(args, key) for key in ('archive', 'artifact_api', 'run_api', 'job_api', 'job_log')), 'Actual archive/API/job inputs required')
    api, run, job = (read_json(path) for path in (args.artifact_api, args.run_api, args.job_api))
    binding = check_api(args.archive, api, run, job, args.job_log.read_text())
    write_json(args.out / 'producer-api-binding.json', binding)
    with zipfile.ZipFile(args.archive) as archive:
        entries, regular, directories = preflight(archive)
        manifests = [name for name in regular if PurePosixPath(name).name == 'provenance.json']
        require(len(manifests) == 1, 'Ambiguous provenance path')
        prefix = manifests[0][:-len('provenance.json')]
        require(regular == {prefix + name for name in expected_files}, 'All-entry regular file roster differs from source')
        allowed_dirs = set()
        for name in regular:
            allowed_dirs.update(str(parent) for parent in PurePosixPath(name).parents if str(parent) != '.')
        require(directories <= allowed_dirs, 'Unexpected directory entries')
        write_json(args.out / 'all-zip-entries.json', dict(accepted=True, entry_count=len(entries), regular_files=len(regular),
                   directory_entries=len(directories), archive_prefix=prefix, entries=entries))
        files = {name: archive.read(prefix + name) for name in ('inventory.json', 'provenance.json', 'README.txt')}
    provenance = parse_json(files['provenance.json'])
    inventory = parse_json(files['inventory.json'])
    write_json(args.out / 'observed-json-shapes.json', dict(provenance_schema=provenance.get('schema'),
               provenance_fields=sorted(provenance), inventory_schema=inventory.get('schema'),
               inventory_fields=sorted(inventory), first_program_fields=sorted(inventory['programs'][0])))
    comparison = compare_inventory(inventory, retained)
    comparison['delivered_inventory_sha256'] = sha(files['inventory.json'])
    comparison['retained_inventory_sha256'] = INVENTORY_SHA
    comparison['json_bytes_identical'] = files['inventory.json'] == (args.repo / INVENTORY).read_bytes()
    write_json(args.out / 'delivered-retained-inventory-comparison.json', comparison)
    commands = check_manifest_commands(provenance, producer, files)
    write_json(args.out / 'producer-commands-readme.json', commands)
    helper_report = args.out / 'frozen-helper-actual-audition.json'
    require(not helper_report.exists(), 'Frozen helper output must be fresh')
    invocation = [sys.executable, str(args.repo / HELPER), '--archive', str(args.archive), '--source', SOURCE,
                  '--run-id', str(RUN), '--attempt', str(ATTEMPT), '--expected-zip-sha256', binding['zip_sha256'],
                  '--repo', str(args.repo), '--out', str(helper_report)]
    result = subprocess.run(invocation, cwd=args.repo, capture_output=True, text=True)
    (args.out / 'frozen-helper.stdout.txt').write_text(result.stdout)
    (args.out / 'frozen-helper.stderr.txt').write_text(result.stderr)
    write_json(args.out / 'frozen-helper-invocation.json', dict(argv=invocation, exit_code=result.returncode,
                                                             helper_sha256=file_sha(args.repo / HELPER)))
    require(result.returncode == 0, 'Frozen verifier failed; inspect stdout/stderr')
    accepted = read_json(helper_report)
    require(accepted['accepted'] is True and accepted['source_commit'] == SOURCE and accepted['run_id'] == str(RUN)
            and accepted['run_attempt'] == str(ATTEMPT) and accepted['zip_sha256'] == binding['zip_sha256'],
            'Frozen helper report identity differs from invocation')
    report = dict(schema=1, status='accepted_actual_audition_scope', accepted=True, source_commit=SOURCE,
                  run_id=RUN, run_attempt=ATTEMPT, producer_job_id=JOB, artifact=binding,
                  zip_entry_count=len(entries), regular_files=len(regular), directory_entries=len(directories),
                  inventory_comparison=comparison, source_command_checks=commands,
                  frozen_helper_report={key: value for key, value in accepted.items() if key != 'wavs'},
                  input_hashes={str(path): file_sha(path) for path in (args.artifact_api, args.run_api, args.job_api, args.job_log)},
                  supplement_sha256=file_sha(Path(__file__)), verified_at_utc=datetime.now(timezone.utc).isoformat(),
                  limits=['Representative single-note numerical/archive evidence; no human listening claim.',
                          '74 selected programs auditioned; 523 programs inventoried.',
                          'No acoustic equivalence, complete host qualification, or overall release authorization claim.'])
    write_json(args.out / 'acceptance.json', report)
    print(json.dumps({key: value for key, value in report.items() if key not in ('artifact', 'inventory_comparison', 'input_hashes')}, indent=2))


if __name__ == '__main__':
    main()
