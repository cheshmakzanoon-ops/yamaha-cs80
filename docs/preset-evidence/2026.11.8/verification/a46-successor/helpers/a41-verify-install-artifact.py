#!/usr/bin/env python3
"""Inspect an explicitly identified native installation artifact; never qualify a release.

All external inputs are snapshotted once. The complete bundle checker and binary
header checker are loaded from the requested Git commit, never from downloaded
products or mutable working-tree scripts. Supplied API receipts are checked for
consistency, not authenticated by this offline program. Original successful
producer/upload/attempt, native-test and validator bindings remain separate.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import zipfile

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[4]
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
JUCE_SHA = 'c352e2489000ffeb9e397051377d75eef2efe641'
JUCE_VERSION = '8.0.11'
ARTIFACTS = {
    ('linux', 'x86_64'): 'cs80-linux-install-bundle',
    ('windows', 'x86_64'): 'cs80-windows-install-bundle',
    ('macos', 'arm64'): 'cs80-macos-15-install-bundle',
    ('macos', 'x86_64'): 'cs80-macos-15-intel-install-bundle',
}
MAX_ARCHIVE = 512 * 1024 * 1024
MAX_FILE = 256 * 1024 * 1024
MAX_METADATA = 2 * 1024 * 1024


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result


def parse_json(raw):
    value = json.loads(raw, object_pairs_hook=unique_object)
    require(isinstance(value, dict), 'JSON metadata must be an object')
    return value


def snapshot(path, limit):
    path = Path(path)
    descriptor = os.open(path, os.O_RDONLY | getattr(os, 'O_NONBLOCK', 0) | getattr(os, 'O_NOFOLLOW', 0))
    with os.fdopen(descriptor, 'rb') as stream:
        info = os.fstat(stream.fileno())
        require(stat.S_ISREG(info.st_mode) and 0 < info.st_size <= limit,
                'Missing, nonregular, empty or oversized input: ' + path.name)
        raw = stream.read(limit + 1)
    require(0 < len(raw) <= limit, 'Empty or oversized snapshot: ' + path.name)
    return raw


def identity(path, raw):
    return {'file': Path(path).name, 'bytes': len(raw), 'sha256': digest(raw)}


def source_text(source, revision, repository=REPOSITORY):
    return (f'Source commit: {source}\n'
            f'https://github.com/{repository}/tree/{source}\n'
            f'Preset library revision: {revision}\n'
            f'Online preset guide: https://github.com/{repository}/blob/{source}/docs/PRESET_GUIDE.md\n'
            'Use the full source at this commit; this archive is not a source distribution.\n').encode()


def binary_paths(system, arch):
    product = 'CS-80 Reference'
    if system == 'macos':
        return [(f'VST3/{product}.vst3/Contents/MacOS/{product}', False),
                (f'AU/{product}.component/Contents/MacOS/{product}', False),
                (f'Standalone/{product}.app/Contents/MacOS/{product}', True)]
    if system == 'linux':
        return [(f'VST3/{product}.vst3/Contents/{arch}-linux/{product}.so', False),
                (f'Standalone/{product}', True)]
    return [(f'VST3/{product}.vst3/Contents/x86_64-win/{product}.vst3', False),
            (f'Standalone/{product}.exe', True)]


def describe_header(raw, system, arch):
    if system == 'linux':
        return {'format': 'ELF64 little-endian', 'machine': int.from_bytes(raw[18:20], 'little'),
                'image_type': int.from_bytes(raw[16:18], 'little')}
    if system == 'windows':
        offset = int.from_bytes(raw[60:64], 'little')
        return {'format': 'PE32+', 'machine': hex(int.from_bytes(raw[offset + 4:offset + 6], 'little')),
                'characteristics': hex(int.from_bytes(raw[offset + 22:offset + 24], 'little'))}
    universal = raw[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf')
    return {'format': 'Mach-O universal' if universal else 'Mach-O 64-bit',
            'advertised_slice_verified': arch,
            'cpu_type': hex({'arm64': 0x0100000c, 'x86_64': 0x01000007}[arch])}


def load_source_contract(repo, source, revision, directory):
    paths = ['scripts/verify-ci-bundle.py', 'scripts/package-ci-artifacts.py',
             'scripts/verify-ci-juce-source.cmake', '.github/workflows/build.yml',
             'src/core/FactoryPresets.h']
    content, records = {}, {}
    for relative in paths:
        raw = subprocess.check_output(['git', '-C', str(repo), 'show', source + ':' + relative])
        require(0 < len(raw) <= MAX_METADATA, 'Oversized source contract file')
        content[relative] = raw
        records[relative] = {'bytes': len(raw), 'sha256': digest(raw)}
    declarations = re.findall(rb'^inline constexpr const char\* curatedLibraryRevision = "([^"\r\n]*)";$',
                              content['src/core/FactoryPresets.h'], re.MULTILINE)
    require(declarations == [revision.encode()], 'Expected revision differs from exact source commit')
    scripts = Path(directory) / 'source-verifiers'
    scripts.mkdir()
    for name in ('package-ci-artifacts.py', 'verify-ci-bundle.py'):
        (scripts / name).write_bytes(content['scripts/' + name])
    spec = importlib.util.spec_from_file_location('a41_source_bundle_checker', scripts / 'verify-ci-bundle.py')
    checker = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checker)
    require(checker.packager.JUCE_SHA == JUCE_SHA, 'Source packager JUCE pin differs from expected pin')
    tree = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', source + '^{tree}'], text=True).strip()
    require(re.fullmatch(r'[0-9a-f]{40}', tree), 'Invalid source tree identity')
    return checker, records, tree


def check_context(expected, run_api, artifact_api, archive_raw):
    require(type(run_api.get('id')) is int and run_api['id'] == expected['run_id']
            and run_api.get('head_sha') == expected['source']
            and type(run_api.get('run_attempt')) is int and run_api['run_attempt'] == expected['attempt'],
            'Run API source/run/attempt mismatch')
    repository = run_api.get('repository', {})
    head_repository = run_api.get('head_repository', {})
    require(isinstance(repository, dict) and isinstance(head_repository, dict)
            and repository.get('full_name') == expected['repository']
            and head_repository.get('full_name') == expected['repository']
            and type(repository.get('id')) is int and repository['id'] > 0
            and head_repository.get('id') == repository['id'], 'Run API repository identity mismatch')
    require(type(artifact_api.get('id')) is int and artifact_api['id'] > 0, 'Invalid artifact ID')
    require(artifact_api.get('name') == ARTIFACTS[(expected['platform'], expected['architecture'])],
            'Artifact name differs from expected target')
    require(artifact_api.get('expired') is False, 'Artifact expired or expiry state missing')
    workflow = artifact_api.get('workflow_run', {})
    require(isinstance(workflow, dict) and type(workflow.get('id')) is int
            and workflow['id'] == expected['run_id'] and workflow.get('head_sha') == expected['source']
            and workflow.get('repository_id') == repository['id']
            and workflow.get('head_repository_id') == repository['id'], 'Artifact source/run/repository mismatch')
    if 'run_attempt' in workflow:
        require(type(workflow['run_attempt']) is int and workflow['run_attempt'] == expected['attempt'],
                'Artifact declared attempt mismatch')
    base = f"https://api.github.com/repos/{expected['repository']}/actions/artifacts/{artifact_api['id']}"
    require(artifact_api.get('url') == base and artifact_api.get('archive_download_url') == base + '/zip',
            'Artifact API repository/ID URL mismatch')
    size = artifact_api.get('size_in_bytes')
    require(type(size) is int and 0 < size <= MAX_ARCHIVE and size == len(archive_raw), 'Archive size differs from API')
    require(artifact_api.get('digest') == 'sha256:' + digest(archive_raw), 'Archive SHA256 differs from API')
    return {'id': expected['run_id'], 'run_attempt': expected['attempt'], 'head_sha': expected['source'],
            'repository_id': repository['id'], 'status_observed': run_api.get('status'),
            'conclusion_observed': run_api.get('conclusion'), 'artifact_attempt_binding_verified': False,
            'scope': 'Supplied API context only. Original successful upload/job records must bind the artifact to the attempt.'}


def unpack_outer(raw, expected_names, inner_name, directory):
    # Bound ZipFile allocation before it materializes the central directory.
    tail = raw[-65577:]
    index = tail.rfind(b'PK\x05\x06')
    require(index >= 0 and len(tail) - index >= 22, 'Missing ZIP end record')
    require(index < 20 or tail[index - 20:index - 16] != b'PK\x06\x07', 'ZIP64 is unsupported')
    end = struct.unpack('<4s4H2IH', tail[index:index + 22])
    require(index + 22 + end[7] == len(tail) and end[1] == end[2] == 0
            and end[3] == end[4] == 3 and end[5] <= MAX_METADATA
            and end[6] != 0xffffffff and end[5] + end[6] == len(raw) - len(tail) + index,
            'Invalid or oversized outer ZIP directory')
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        members = archive.infolist()
        require(len(members) == 3 and {m.filename for m in members} == expected_names,
                'Outer ZIP must contain exactly the expected inner archive, manifest and SHA256SUMS')
        for member in members:
            require(not member.is_dir() and not member.flag_bits & 1
                    and member.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED)
                    and stat.S_IFMT(member.external_attr >> 16) in (0, stat.S_IFREG), 'Invalid outer ZIP object')
            limit = MAX_ARCHIVE if member.filename == inner_name else MAX_METADATA
            require(0 < member.file_size <= limit, 'Oversized/empty outer ZIP member')
            total = 0
            with archive.open(member) as source, (directory / member.filename).open('xb') as destination:
                for block in iter(lambda: source.read(1024 * 1024), b''):
                    total += len(block)
                    require(total <= member.file_size, 'Outer ZIP member exceeds declared size')
                    destination.write(block)
            require(total == member.file_size, 'Truncated outer ZIP member')


def read_payloads(inner, root, wanted):
    result = {}
    if inner.name.endswith('.zip'):
        with zipfile.ZipFile(inner) as archive:
            for name in wanted:
                entry = archive.getinfo(root + '/' + name)
                require(not entry.is_dir() and stat.S_IFMT(entry.external_attr >> 16) in (0, stat.S_IFREG),
                        'Required payload is not a regular file: ' + name)
                require(0 < entry.file_size <= MAX_FILE, 'Invalid required payload size')
                result[name] = archive.read(entry)
    else:
        with tarfile.open(inner, 'r:gz') as archive:
            for name in wanted:
                entry = archive.getmember(root + '/' + name)
                require(entry.isfile() and 0 < entry.size <= MAX_FILE, 'Required payload is not a bounded regular file: ' + name)
                with archive.extractfile(entry) as stream:
                    raw = stream.read(entry.size + 1)
                require(len(raw) == entry.size, 'Truncated required payload')
                result[name] = raw
    return result


def inspect_snapshot(expected, archive_path, archive_raw, artifact_raw, run_raw, checker, source_records, tree, directory):
    """Inspect already frozen bytes; fixtures may supply an inert source checker."""
    api, run_api = parse_json(artifact_raw), parse_json(run_raw)
    context = check_context(expected, run_api, api, archive_raw)
    root = f"cs80-reference-{expected['platform']}-{expected['architecture']}-{expected['source'][:12]}"
    inner_name = root + ('.zip' if expected['platform'] == 'windows' else '.tar.gz')
    sidecar_name = root + '.manifest.json'
    names = {inner_name, sidecar_name, 'SHA256SUMS'}
    unpack_outer(archive_raw, names, inner_name, directory)
    inner = directory / inner_name
    integrity = checker.verify(inner, expected['source'])
    manifest_raw = snapshot(directory / sidecar_name, MAX_METADATA)
    manifest = parse_json(manifest_raw)
    require(manifest['platform'] == expected['platform'] and manifest['architecture'] == expected['architecture'],
            'Manifest target mismatch')
    require(manifest.get('preset_library_revision') == expected['revision'], 'Wrong preset revision')
    require(str(manifest.get('workflow_run_id')) == str(expected['run_id']), 'Manifest workflow run mismatch')
    declared_attempt = manifest.get('workflow_run_attempt')
    require(declared_attempt is None or str(declared_attempt) == str(expected['attempt']), 'Manifest declared attempt mismatch')
    require(manifest.get('juce_sha') == JUCE_SHA and manifest.get('juce_version') == JUCE_VERSION, 'Manifest JUCE pin mismatch')
    paths = binary_paths(expected['platform'], expected['architecture'])
    payloads = read_payloads(inner, root, [name for name, _ in paths] + ['SOURCE.txt', 'evidence/juce-provenance.txt'])
    require(payloads['SOURCE.txt'] == source_text(expected['source'], expected['revision'], expected['repository']),
            'SOURCE.txt differs from exact revision/source contract')
    fields = {}
    for line in payloads['evidence/juce-provenance.txt'].decode('utf-8').splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            require(key not in fields, 'Duplicate JUCE provenance key')
            fields[key] = value
    require(all(fields.get(key) == value for key, value in {
        'verification': 'PASS', 'source_clean': 'true', 'actual_sha': JUCE_SHA,
        'actual_version': JUCE_VERSION, 'workflow_commit': expected['source']}.items()), 'Embedded JUCE provenance mismatch')
    inventory = {item['path']: item for item in manifest['files']}
    binaries = []
    for number, (name, standalone) in enumerate(paths):
        item, raw = inventory[name], payloads[name]
        require(item['type'] == 'file', 'Native executable must be an inventoried regular file')
        require(expected['platform'] == 'windows' or int(item['mode'], 8) & 0o111,
                'Native executable lacks executable inventory mode')
        test_binary = directory / f'header-{number}.bin'
        test_binary.write_bytes(raw)
        checker.packager.verify_binary(test_binary, expected['platform'], expected['architecture'], standalone)
        binaries.append({'path': name, 'bytes': len(raw), 'sha256': digest(raw), 'mode': item['mode'],
                         'platform': expected['platform'], 'architecture': expected['architecture'],
                         'role': 'standalone' if standalone else 'plugin', 'header_check': 'PASS',
                         'header': describe_header(raw, expected['platform'], expected['architecture'])})
    return {'schema': 2, 'status': 'artifact_bytes_verified', 'release_qualified': False,
            'native_validation_passed': None, 'artifact_attempt_binding_verified': False,
            'verified_at_utc': datetime.now(timezone.utc).isoformat(), 'repository': expected['repository'],
            'source': expected['source'], 'source_tree': tree, 'preset_library_revision': expected['revision'],
            'run_id': expected['run_id'], 'run_attempt': expected['attempt'], 'run_context': context,
            'manifest_attempt_declared': declared_attempt,
            'target': expected['platform'] + '-' + expected['architecture'],
            'artifact': {key: api.get(key) for key in ('id', 'name', 'size_in_bytes', 'digest', 'created_at', 'expires_at', 'workflow_run')},
            'outer_zip': dict(identity(archive_path, archive_raw), members=sorted(names)),
            'inner_archive': identity(inner, snapshot(inner, MAX_ARCHIVE)),
            'manifest_sha256': digest(manifest_raw),
            'checksum_manifest_sha256': digest(snapshot(directory / 'SHA256SUMS', MAX_METADATA)),
            'inventory_entries': len(manifest['files']), 'inventory': manifest['files'], 'formats': manifest['formats'],
            'binary_headers': binaries, 'inner_checker': integrity, 'source_contract_files': source_records,
            'source_scripts_loaded_from_commit': True,
            'signing_status': manifest.get('signature_status'),
            'scope': 'Exact outer bytes, complete inner inventory/checksums, source/revision/run declarations, JUCE provenance and native header architecture/roles. No product executed.',
            'remaining_gates': [
                'Authenticate API receipts and downloaded bytes through the original GitHub acquisition.',
                'Bind the exact artifact ID/digest/size/timestamps to the original successful upload step and reviewed run attempt; artifact and current manifest do not identify the attempt.',
                'Require all original native configure/JUCE-source/build/Test/package jobs and steps, full expected CTest rosters and reviewed skips for the same source/run/attempt.',
                'Require all 13 original pluginval lanes, version/strictness/program count/seeds, unsuppressed original zero exits and any eligible preserved Linux retries.',
                'Bind every distributed VST3/AU bundle to the actual validated payload using the separate Windows/Linux and macOS binding evidence.',
                'Verify the actual audition archive, remaining platforms and current download availability before publication.'
            ],
            'limits': ['Receipts are caller supplied and not authenticated by this offline checker.',
                       'A matching run attempt in supplied context is not artifact-upload attempt binding.',
                       'Native headers establish advertised architecture and image role, not runtime validity or exclusive universal-binary slices.',
                       'No DAW compatibility, installation workflow, signing/notarization, listening quality or acoustic equivalence is inferred.']}


def verify(archive, artifact_api, run_api, expected, repo=REPO):
    require(re.fullmatch(r'[0-9a-f]{40}', expected['source']), 'Expected full lowercase source SHA')
    require(re.fullmatch(r'[0-9]{4}\.(?:0[1-9]|1[0-2])(?:\.[1-9][0-9]*)?', expected['revision']), 'Expected canonical revision')
    require(type(expected['run_id']) is int and expected['run_id'] > 0
            and type(expected['attempt']) is int and expected['attempt'] > 0, 'Expected positive run and attempt')
    require((expected['platform'], expected['architecture']) in ARTIFACTS, 'Unsupported target')
    require(re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', expected['repository']), 'Invalid repository identity')
    raw = snapshot(archive, MAX_ARCHIVE)
    artifact_raw, run_raw = snapshot(artifact_api, MAX_METADATA), snapshot(run_api, MAX_METADATA)
    with tempfile.TemporaryDirectory(prefix='a41-install-accept-') as directory:
        scratch = Path(directory)
        checker, records, tree = load_source_contract(repo, expected['source'], expected['revision'], scratch)
        result = inspect_snapshot(expected, archive, raw, artifact_raw, run_raw, checker, records, tree, scratch)
    result['input_metadata'] = {'artifact_api': identity(artifact_api, artifact_raw), 'run_api': identity(run_api, run_raw)}
    result['checker_sha256'] = digest(Path(__file__).read_bytes())
    return result


def publish_report(path, raw):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile('w', dir=path.parent, prefix='.a41-', delete=False, encoding='utf-8') as stream:
            temporary = Path(stream.name)
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temporary, path)  # Atomic publication; existing file/symlink is never replaced.
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=REPO)
    parser.add_argument('--repository', default=REPOSITORY)
    for name in ('archive', 'artifact-api', 'run-api', 'out'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--source', required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--run-id', required=True, type=int)
    parser.add_argument('--attempt', required=True, type=int)
    parser.add_argument('--platform', required=True, choices=('linux', 'windows', 'macos'))
    parser.add_argument('--architecture', required=True, choices=('x86_64', 'arm64'))
    args = parser.parse_args(argv)
    require(not args.out.exists() and not args.out.is_symlink(), 'Refusing to overwrite an evidence report')
    expected = {'source': args.source, 'revision': args.revision, 'run_id': args.run_id,
                'attempt': args.attempt, 'platform': args.platform,
                'architecture': args.architecture, 'repository': args.repository}
    result = verify(args.archive, args.artifact_api, args.run_api, expected, args.repo)
    publish_report(args.out, json.dumps(result, indent=2) + '\n')
    print(json.dumps({'status': result['status'], 'target': result['target'],
                      'release_qualified': False, 'out': str(args.out)}))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError, RuntimeError, EOFError, UnicodeError,
            subprocess.CalledProcessError, zipfile.BadZipFile, tarfile.TarError) as error:
        print('Install artifact verification failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
