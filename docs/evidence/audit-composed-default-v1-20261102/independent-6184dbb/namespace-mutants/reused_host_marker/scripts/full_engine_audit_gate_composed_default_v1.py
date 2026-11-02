#!/usr/bin/env python3
"""Thin source-bound composed-default V1 wrapper; capture completion is not acceptance."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import stat
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]
REGISTRATION_SHA = '6eda5b0dee3b74e418d1c30517d4d7101fcb50f0'
REQUEST_PATH = '.github/full-engine-audit-request-composed-default-v1.json'
V1_REGISTRATION = '00da4498792523cf8d97fd40c17e12f215e190b8'
V1_GATE_COMMIT = '39ae6c2684456950302cb4ed10ecfae980c37ddd'
V1_REQUEST_PATH = '.github/full-engine-audit-request.json'
V1_GATE_PATH = 'scripts/full_engine_audit_gate.py'
V1_WORKFLOW_PATH = '.github/workflows/full-engine-audit.yml'


def require(condition, reason):
    if not condition:
        raise ValueError('composed-default V1 integrity guard: ' + reason)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git_bytes(*args):
    return subprocess.check_output(['git', '-C', str(REPO), *args], stderr=subprocess.PIPE)


def git(*args):
    return git_bytes(*args).decode().strip()


def require_commit(commit, label):
    require(isinstance(commit, str) and re.fullmatch('[0-9a-f]{40}', commit), 'missing-object:' + label)
    try:
        observed = git('rev-parse', '--verify', commit + '^{commit}')
    except subprocess.CalledProcessError as error:
        raise ValueError('composed-default V1 integrity guard: missing-object:' + label) from error
    require(observed == commit, 'missing-object:' + label)


def require_ancestor(commit, label):
    result = subprocess.run(['git', '-C', str(REPO), 'merge-base', '--is-ancestor', commit, 'HEAD'],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    require(result.returncode == 0, 'not-ancestor:' + label)


def source_clean():
    require(not git('status', '--porcelain=v1', '--untracked-files=all', '--', 'src'), 'dirty-source')
    require(not git('ls-files', '--others', '--ignored', '--exclude-standard', '--', 'src'), 'ignored-source')


def source_physical_bytes(source):
    """Compare actual files with registered Git blobs, not cached index/filter state."""
    entries = []
    for record in git_bytes('ls-tree', '-rz', source, '--', 'src').split(b'\0'):
        if not record:
            continue
        metadata, relative = record.split(b'\t', 1)
        mode, kind, blob = metadata.decode().split()
        name = relative.decode()
        require(kind == 'blob' and mode in ('100644', '100755'), 'source-registered-type:' + name)
        entries.append((mode, blob, name))
    require(bool(entries), 'source-empty-tree')
    # One batch obtains the original raw object bytes. No clean/smudge filters,
    # index flags or Git stat-cache promises participate in the comparison.
    objects = subprocess.check_output(['git', '-C', str(REPO), 'cat-file', '--batch'],
                                      input=('\n'.join(blob for _, blob, _ in entries) + '\n').encode(),
                                      stderr=subprocess.PIPE)
    offset, manifest = 0, []
    for mode, blob, name in entries:
        end = objects.find(b'\n', offset)
        require(end >= offset, 'source-object-framing:' + name)
        header = objects[offset:end].decode().split()
        require(len(header) == 3 and header[:2] == [blob, 'blob'], 'source-object-framing:' + name)
        size = int(header[2])
        expected = objects[end + 1:end + 1 + size]
        offset = end + size + 2
        require(len(expected) == size and objects[offset - 1:offset] == b'\n', 'source-object-framing:' + name)
        current = REPO
        for part in Path(name).parts[:-1]:
            current /= part
            require(current.is_dir() and not current.is_symlink(),
                    'source-ancestor:' + current.relative_to(REPO).as_posix())
        path = REPO / name
        require(path.is_file() and not path.is_symlink(), 'source-file-type:' + name)
        actual_mode = path.lstat().st_mode
        require(stat.S_ISREG(actual_mode), 'source-file-type:' + name)
        require(bool(actual_mode & 0o111) == (mode == '100755'), 'source-file-mode:' + name)
        actual = path.read_bytes()
        require(digest(actual) == digest(expected), 'source-file-bytes:' + name)
        manifest.append({'path': name, 'mode': mode, 'sha256': digest(actual)})
    require(offset == len(objects), 'source-object-trailer')
    return {'tracked_files': len(manifest), 'manifest_sha256': digest(json.dumps(manifest, sort_keys=True).encode()),
            'comparison': 'actual regular-file raw bytes, ancestor types and executable modes against registered blobs'}


def current_bytes(relative, reason):
    path = REPO / relative
    require(path.is_file() and not path.is_symlink(), reason)
    return path.read_bytes()


def registered_context():
    # No build or executable import occurs before all immutable identities pass.
    require_commit(REGISTRATION_SHA, 'R')
    require_ancestor(REGISTRATION_SHA, 'R')
    raw = current_bytes(REQUEST_PATH, 'request-bytes')
    require(raw == git_bytes('show', f'{REGISTRATION_SHA}:{REQUEST_PATH}'), 'request-bytes')
    request = json.loads(raw)
    require(request.get('schema_version') == 1 and request.get('request_version') == 2, 'request-version')
    source, audio = request['source_commit'], request['production_audio_commit']
    require_commit(source, 'F')
    require_ancestor(source, 'F')
    require_commit(audio, 'audio')
    require_ancestor(audio, 'audio')
    expected_tree = request['expected_src_tree']
    require(git('rev-parse', source + ':src') == expected_tree, 'source-tree:F')
    require(git('rev-parse', 'HEAD:src') == expected_tree, 'source-tree:HEAD')
    source_clean()
    physical_source = source_physical_bytes(source)

    original = json.loads(git_bytes('show', f'{V1_REGISTRATION}:{V1_REQUEST_PATH}'))
    for key, value in original.items():
        if key not in ('registered_utc', 'purpose', 'expected_src_tree'):
            require(request.get(key) == value, 'original-contract:' + key)
    anchors = {V1_REQUEST_PATH: V1_REGISTRATION, V1_GATE_PATH: V1_GATE_COMMIT,
               original['frozen_analysis']: V1_REGISTRATION,
               **{path: V1_REGISTRATION for path in original['existing_numerical_contracts']},
               V1_WORKFLOW_PATH: source}
    identity = request['original_v1_identity']
    require(identity['registration_commit'] == V1_REGISTRATION and
            identity['gate_commit'] == V1_GATE_COMMIT, 'original-anchors')
    require(set(identity['protected_files']) == set(anchors), 'protected-inventory')
    for relative, anchor in anchors.items():
        reason = 'protected-file:' + relative
        expected = git_bytes('show', f'{anchor}:{relative}')
        metadata = identity['protected_files'][relative]
        require(metadata['anchor_commit'] == anchor and metadata['sha256'] == digest(expected)
                and metadata['git_blob'] == git('rev-parse', f'{anchor}:{relative}'), reason)
        require(current_bytes(relative, reason) == expected ==
                git_bytes('show', f'{REGISTRATION_SHA}:{relative}'), reason)
    require(set(request['build_inputs']) == {'CMakeLists.txt'}, 'build-input-inventory')
    for relative, metadata in request['build_inputs'].items():
        expected = git_bytes('show', f'{source}:{relative}')
        require(metadata['anchor_commit'] == source and metadata['sha256'] == digest(expected)
                and current_bytes(relative, 'build-input:' + relative) == expected ==
                git_bytes('show', f'{REGISTRATION_SHA}:{relative}'), 'build-input:' + relative)

    spec = importlib.util.spec_from_file_location('unchanged_v1_audit_gate', REPO / V1_GATE_PATH)
    require(spec is not None and spec.loader is not None, 'gate-import')
    gate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gate)
    gate.REPO, gate.REQUEST_PATH, gate.REGISTRATION_SHA = REPO, Path(REQUEST_PATH), REGISTRATION_SHA
    registered_request, reporter = gate.registered_contract()
    require(registered_request == request, 'imported-request')
    return {'request': request, 'gate': gate, 'reporter': reporter, 'head': git('rev-parse', 'HEAD'),
            'physical_source': physical_source}


def validated_completion(context):
    request, head = context['request'], context['head']
    destination = REPO / 'artifacts'
    raw = current_bytes('artifacts/audit-provenance.json', 'completion-provenance')
    provenance = json.loads(raw)
    lines = current_bytes('artifacts/audit.complete', 'completion-marker').decode().splitlines()
    require(all('=' in line for line in lines), 'completion-marker')
    marker = dict(line.split('=', 1) for line in lines)
    require(len(marker) == len(lines) and set(marker) == {
        'completed_utc', 'source_sha', 'run_id', 'registration_commit', 'provenance_sha256'}, 'completion-marker')
    require(marker['source_sha'] == provenance['head_sha'] == head == git('rev-parse', 'HEAD'), 'completion-source')
    require(marker['registration_commit'] == provenance['registration_commit'] == REGISTRATION_SHA,
            'completion-registration')
    require(marker['provenance_sha256'] == digest(raw), 'completion-provenance')
    require(provenance['complete_capture'] is True and provenance['quick'] is False
            and provenance['src_tree_sha'] == request['expected_src_tree']
            and provenance['production_audio_commit'] == request['production_audio_commit']
            and provenance['registered_command'] == request['command']
            and provenance['registration_json_sha256'] == digest((REPO / REQUEST_PATH).read_bytes()),
            'completion-contract')
    require(marker['completed_utc'] == provenance['generated_utc'] and
            marker['run_id'] == str(provenance['github_run_id']), 'completion-run')
    require(set(provenance['raw_files']) == set(request['suite_files']), 'completion-inventory')
    for name, count in request['suite_files'].items():
        data = current_bytes('artifacts/audit/' + name, 'completion-raw:' + name)
        recorded = provenance['raw_files'][name]
        require(recorded['rows'] == count and recorded['bytes'] == len(data)
                and recorded['sha256'] == digest(data), 'completion-raw:' + name)
    context['gate'].validate_capture(destination / 'audit', request, context['reporter'])
    require(provenance['binary_sha256'] == digest(current_bytes('build-core/cs80_engine_audit', 'completion-binary')),
            'completion-binary')
    require(provenance['reporter_sha256'] == digest((REPO / request['frozen_analysis']).read_bytes()), 'completion-reporter')
    return provenance


def finish_composed_default_v1(context):
    destination = REPO / 'artifacts'
    sidecar, marker = destination / 'audit-composed-default-v1-provenance.json', destination / 'audit-v2.complete'
    require(not sidecar.exists() and not marker.exists(), 'stale-completion')
    source_clean()
    physical_source = source_physical_bytes(context['request']['source_commit'])
    provenance = validated_completion(context)
    v1_paths = ('audit.complete', 'audit-provenance.json', 'audit-summary.json', 'audit-summary.md')
    v1_hashes = {name: digest(current_bytes('artifacts/' + name, 'completion-output:' + name)) for name in v1_paths}
    evidence = {
        'schema_version': 2, 'scope': 'composed_default_v1_capture_not_numerical_acceptance',
        'registration_commit': REGISTRATION_SHA, 'head_sha': context['head'],
        'source_commit': context['request']['source_commit'], 'src_tree_sha': context['request']['expected_src_tree'],
        'production_audio_commit': context['request']['production_audio_commit'],
        'original_v1_registration': V1_REGISTRATION, 'original_v1_gate_commit': V1_GATE_COMMIT,
        'original_v1_gate_sha256': digest((REPO / V1_GATE_PATH).read_bytes()),
        'physical_source': physical_source,
        'v1_outputs_preserved_sha256': v1_hashes, 'raw_files': provenance['raw_files'],
        'binary_sha256': provenance['binary_sha256'], 'numerical_failures_waived': False,
        'cpu_qualification': False, 'hardware_measurement': False, 'release_qualification': False,
    }
    encoded = json.dumps(evidence, indent=2, sort_keys=True) + '\n'
    with sidecar.open('x') as stream:
        stream.write(encoded)
    # The distinct composed-default V1 marker is last. A failed sidecar can never leave composed-default V1 complete.
    with marker.open('x') as stream:
        stream.write(f'source_sha={context["head"]}\nregistration_commit={REGISTRATION_SHA}\n'
                     f'provenance_sha256={digest(encoded.encode())}\n'
                     'status=complete_capture_not_numerical_acceptance\n')
    return evidence


def finalize(context):
    destination = REPO / 'artifacts'
    require(not any((destination / name).exists() for name in
                    ('audit.complete', 'audit-composed-default-v1.complete', 'audit-composed-default-v1-provenance.json')), 'stale-completion')
    context['gate'].finalize(context['request'], context['reporter'])
    finish_composed_default_v1(context)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    for mode in ('preflight', 'self-test', 'finalize'):
        group.add_argument('--' + mode, action='store_true')
    args = parser.parse_args()
    context = registered_context()
    if args.preflight:
        context['gate'].preflight(context['request'])
        print(json.dumps({'composed_default_v1_registration': REGISTRATION_SHA, 'scope': 'composed-default capture; not numerical acceptance'}))
    elif args.self_test:
        context['gate'].self_test(context['request'], context['reporter'])
    else:
        finalize(context)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, AssertionError, subprocess.SubprocessError) as error:
        print('Full engine audit composed-default V1 refused: ' + str(error), file=sys.stderr)
        sys.exit(2)
