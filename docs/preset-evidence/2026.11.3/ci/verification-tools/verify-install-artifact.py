#!/usr/bin/env python3
"""Inspect actual Actions installation ZIP bytes; never execute a product.

This is artifact acceptance only. The parent qualification must separately bind
producer/upload steps, native test results and all required validator exits.
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
import subprocess
import sys
import tarfile
import tempfile
import zipfile

REPO = Path.cwd()  # Run from the repository root or pass --repo explicitly.
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
REVISION = '2026.11.3'
JUCE_SHA = 'c352e2489000ffeb9e397051377d75eef2efe641'
ARTIFACTS = {
    ('linux', 'x86_64'): 'cs80-linux-install-bundle',
    ('windows', 'x86_64'): 'cs80-windows-install-bundle',
    ('macos', 'arm64'): 'cs80-macos-15-install-bundle',
    ('macos', 'x86_64'): 'cs80-macos-15-intel-install-bundle',
}
MAX_OUTER = 512 * 1024 * 1024
MAX_METADATA = 2 * 1024 * 1024


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha_file(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'Duplicate JSON key: {key}')
        result[key] = value
    return result


def load_json(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=unique_object)


def load_checker(repo):
    path = repo / 'scripts/verify-ci-bundle.py'
    spec = importlib.util.spec_from_file_location('cs80_install_inner_checker', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def source_text(source):
    return (f'Source commit: {source}\n'
            f'https://github.com/{REPOSITORY}/tree/{source}\n'
            f'Preset library revision: {REVISION}\n'
            f'Online preset guide: https://github.com/{REPOSITORY}/blob/{source}/docs/PRESET_GUIDE.md\n'
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


def describe_header(data, system, arch):
    # The production packager's complete bounded header checker runs first.
    if system == 'linux':
        return {'format': 'ELF64 little-endian',
                'machine': int.from_bytes(data[18:20], 'little'),
                'image_type': int.from_bytes(data[16:18], 'little')}
    if system == 'windows':
        offset = int.from_bytes(data[60:64], 'little')
        return {'format': 'PE32+',
                'machine': hex(int.from_bytes(data[offset + 4:offset + 6], 'little')),
                'characteristics': hex(int.from_bytes(data[offset + 22:offset + 24], 'little'))}
    universal = data[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf')
    return {'format': 'Mach-O universal' if universal else 'Mach-O 64-bit',
            'advertised_slice_verified': arch,
            'cpu_type': hex({'arm64': 0x0100000c, 'x86_64': 0x01000007}[arch])}


def read_inner_files(inner, wanted, root):
    result = {}
    if inner.name.endswith('.zip'):
        with zipfile.ZipFile(inner) as archive:
            for name in wanted:
                item = archive.getinfo(root + '/' + name)
                require(not item.is_dir(), f'Expected regular payload: {name}')
                result[name] = archive.read(item)
    else:
        with tarfile.open(inner, 'r:gz') as archive:
            for name in wanted:
                item = archive.getmember(root + '/' + name)
                require(item.isfile(), f'Expected regular native binary/metadata: {name}')
                stream = archive.extractfile(item)
                require(stream is not None, f'Unreadable payload: {name}')
                with stream:
                    result[name] = stream.read()
    return result


def verify_artifact(outer, api, source, run_id, system, arch, repo=REPO, pin_scripts=True):
    outer, repo = Path(outer), Path(repo)
    require(re.fullmatch('[0-9a-f]{40}', source), 'Expected full lowercase source SHA')
    require((system, arch) in ARTIFACTS, 'Unsupported target')
    require(type(run_id) is int and run_id > 0, 'Invalid run ID')
    require(type(api.get('id')) is int and api['id'] > 0, 'Invalid artifact ID')
    require(api.get('name') == ARTIFACTS[(system, arch)], 'Wrong artifact name for target')
    require(api.get('expired') is False, 'Artifact is expired or expiry state missing')
    workflow = api.get('workflow_run', {})
    require(workflow.get('id') == run_id and workflow.get('head_sha') == source,
            'Artifact API workflow source/run mismatch')
    require(type(api.get('size_in_bytes')) is int and 0 < api['size_in_bytes'] <= MAX_OUTER,
            'Missing or excessive API size')
    require(outer.is_file() and outer.stat().st_size == api['size_in_bytes'], 'Outer ZIP size differs from API')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', api.get('digest', '')), 'Missing SHA256 API digest')
    actual_outer_sha = sha_file(outer)
    require(actual_outer_sha == api['digest'].split(':', 1)[1], 'Outer ZIP SHA256 differs from API')
    checker_hashes = {}
    for relative in ('scripts/verify-ci-bundle.py', 'scripts/package-ci-artifacts.py'):
        raw = (repo / relative).read_bytes()
        if pin_scripts:
            committed = subprocess.check_output(['git', '-C', str(repo), 'show', f'{source}:{relative}'])
            require(raw == committed, f'Local checker differs from exact source commit: {relative}')
        checker_hashes[relative] = digest(raw)
    checker = load_checker(repo)
    root = f'cs80-reference-{system}-{arch}-{source[:12]}'
    inner_name = root + ('.zip' if system == 'windows' else '.tar.gz')
    sidecar_name = root + '.manifest.json'
    expected_members = {inner_name, sidecar_name, 'SHA256SUMS'}
    with tempfile.TemporaryDirectory(prefix='cs80-install-accept-') as directory:
        temp = Path(directory)
        with zipfile.ZipFile(outer) as archive:
            members = archive.infolist()
            require(len(members) == 3 and {m.filename for m in members} == expected_members,
                    'Outer ZIP must contain exactly inner archive, sidecar manifest and SHA256SUMS')
            for member in members:
                require(not member.is_dir() and not member.flag_bits & 1, 'Unexpected directory/encrypted outer entry')
                mode = member.external_attr >> 16
                require(stat.S_IFMT(mode) in (0, stat.S_IFREG), 'Outer ZIP contains nonregular member')
                limit = MAX_OUTER if member.filename == inner_name else MAX_METADATA
                require(0 < member.file_size <= limit, 'Outer member size invalid')
                with archive.open(member) as source_stream, (temp / member.filename).open('xb') as dest:
                    total = 0
                    for block in iter(lambda: source_stream.read(1024 * 1024), b''):
                        total += len(block)
                        require(total <= member.file_size, 'Outer member exceeds declared size')
                        dest.write(block)
                    require(total == member.file_size, 'Truncated outer member')
        inner = temp / inner_name
        integrity = checker.verify(inner, source)
        manifest = load_json(temp / sidecar_name)
        require(manifest['platform'] == system and manifest['architecture'] == arch, 'Manifest target mismatch')
        require(manifest.get('preset_library_revision') == REVISION, 'Wrong preset library revision')
        require(str(manifest.get('workflow_run_id')) == str(run_id), 'Manifest workflow run mismatch')
        require(manifest.get('juce_sha') == JUCE_SHA and manifest.get('juce_version') == '8.0.11',
                'Wrong JUCE source identity')
        paths = binary_paths(system, arch)
        payloads = read_inner_files(inner, [p for p, _ in paths] + ['SOURCE.txt', 'evidence/juce-provenance.txt'], root)
        require(payloads['SOURCE.txt'] == source_text(source), 'SOURCE.txt differs from exact revision/source contract')
        evidence = payloads['evidence/juce-provenance.txt'].decode('utf-8')
        fields = dict(line.split('=', 1) for line in evidence.splitlines() if '=' in line)
        require(fields.get('verification') == 'PASS' and fields.get('source_clean') == 'true'
                and fields.get('actual_sha') == JUCE_SHA and fields.get('actual_version') == '8.0.11'
                and fields.get('workflow_commit') == source, 'Embedded JUCE provenance mismatch')
        binaries = []
        for number, (relative, standalone) in enumerate(paths):
            raw = payloads[relative]
            test_binary = temp / f'header-{number}.bin'
            test_binary.write_bytes(raw)
            checker.packager.verify_binary(test_binary, system, arch, standalone)
            binaries.append({'path': relative, 'bytes': len(raw), 'sha256': digest(raw),
                             'platform': system, 'architecture': arch,
                             'role': 'standalone' if standalone else 'plugin',
                             'header_check': 'PASS', 'header': describe_header(raw, system, arch)})
        report = {
            'schema': 1, 'status': 'artifact_bytes_verified',
            'verified_at_utc': datetime.now(timezone.utc).isoformat(),
            'repository': REPOSITORY, 'source': source, 'preset_library_revision': REVISION,
            'run_id': run_id, 'target': system + '-' + arch,
            'artifact': {key: api.get(key) for key in ('id', 'name', 'size_in_bytes', 'digest', 'created_at', 'expires_at', 'workflow_run')},
            'outer_zip': {'file': outer.name, 'bytes': outer.stat().st_size, 'sha256': actual_outer_sha,
                          'members': sorted(expected_members)},
            'inner_archive': {'file': inner.name, 'bytes': inner.stat().st_size, 'sha256': sha_file(inner)},
            'manifest_sha256': sha_file(temp / sidecar_name),
            'checksum_manifest_sha256': sha_file(temp / 'SHA256SUMS'),
            'inventory_entries': len(manifest['files']), 'formats': manifest['formats'],
            'binary_headers': binaries, 'inner_checker': integrity,
            'checker_sha256': sha_file(Path(__file__)), 'source_checker_sha256': checker_hashes,
            'source_scripts_compared_to_commit': pin_scripts,
            'scope': 'Outer bytes, complete inner inventory/checksums, declared source/revision/run, JUCE provenance, and native headers. No product executed.',
            'remaining_qualification': 'Producer/upload/attempt binding, complete native tests, 13 validator invocations and payload identity with validator inputs must be established separately.',
            'signing_status': manifest.get('signature_status'),
        }
    return report


def fixture(directory, system, arch, source, run_id, wrong_header=False):
    checker = load_checker(REPO)
    packager = checker.packager
    base, output = directory / 'products', directory / 'packages'
    for relative, standalone in binary_paths(system, arch):
        path = base / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        raw = packager.binary_fixture(system, arch, standalone)
        path.write_bytes(raw)
        path.chmod(0o755)
        if system == 'macos':
            (path.parent.parent / 'Info.plist').write_text('<plist/>')
    metadata = {'preset_library_revision': REVISION, 'workflow_run_id': str(run_id),
                'juce_sha': JUCE_SHA, 'juce_version': '8.0.11'}
    extras = {'SOURCE.txt': source_text(source),
              'evidence/juce-provenance.txt': (f'verification=PASS\nsource_clean=true\nactual_sha={JUCE_SHA}\n'
                                               f'actual_version=8.0.11\nworkflow_commit={source}\n').encode()}
    inner, manifest = packager.package(base, output, system, arch, source, metadata, extras)
    if wrong_header:
        # Rebuild coherent checksums/manifests around a deliberately wrong CPU
        # field, so failure must come from header inspection, not a stale hash.
        root = inner.name.removesuffix('.zip').removesuffix('.tar.gz')
        first = binary_paths(system, arch)[0][0]
        if system == 'windows':
            with zipfile.ZipFile(inner) as archive:
                records = [(entry, archive.read(entry)) for entry in archive.infolist()]
        else:
            with tarfile.open(inner, 'r:gz') as archive:
                records = [(entry, archive.extractfile(entry).read() if entry.isfile() else None) for entry in archive]
        original = next(data for entry, data in records if getattr(entry, 'filename', getattr(entry, 'name', '')) == root + '/' + first)
        bad = bytearray(original)
        if system == 'linux':
            bad[18:20] = (183).to_bytes(2, 'little')
        elif system == 'windows':
            offset = int.from_bytes(bad[60:64], 'little')
            bad[offset + 4:offset + 6] = (0xaa64).to_bytes(2, 'little')
        else:
            bad[4:8] = (0x01000007 if arch == 'arm64' else 0x0100000c).to_bytes(4, 'little')
        for item in manifest['files']:
            if item['path'] == first:
                item['sha256'] = digest(bad)
        manifest_raw = (json.dumps(manifest, indent=2, sort_keys=True) + '\n').encode()
        sidecar = output / (root + '.manifest.json')
        sidecar.write_bytes(manifest_raw)
        replacements = {root + '/' + first: bytes(bad), root + '/manifest.json': manifest_raw}
        if system == 'windows':
            with zipfile.ZipFile(inner, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
                for entry, data in records:
                    archive.writestr(entry, replacements.get(entry.filename, data))
        else:
            with tarfile.open(inner, 'w:gz') as archive:
                for entry, data in records:
                    data = replacements.get(entry.name, data)
                    archive.addfile(entry, io.BytesIO(data) if data is not None else None)
        (output / 'SHA256SUMS').write_text(''.join(f'{sha_file(p)}  {p.name}\n' for p in (inner, sidecar)))
    outer = directory / 'artifact.zip'
    with zipfile.ZipFile(outer, 'w', compression=zipfile.ZIP_STORED) as archive:
        for path in output.iterdir():
            archive.write(path, path.name)
    api = {'id': 1, 'name': ARTIFACTS[(system, arch)], 'expired': False,
           'size_in_bytes': outer.stat().st_size, 'digest': 'sha256:' + sha_file(outer),
           'workflow_run': {'id': run_id, 'head_sha': source}}
    return outer, api


def self_test():
    passed = rejected = 0
    with tempfile.TemporaryDirectory(prefix='cs80-install-verifier-fixtures-') as directory:
        root = Path(directory)
        for number, (system, arch) in enumerate(ARTIFACTS):
            source, run_id = 'a' * 40, 123
            outer, api = fixture(root / f'valid-{number}', system, arch, source, run_id)
            result = verify_artifact(outer, api, source, run_id, system, arch, pin_scripts=False)
            require(len(result['binary_headers']) == (3 if system == 'macos' else 2), 'Fixture binary count')
            passed += 1
            bad_api = dict(api, digest='sha256:' + '0' * 64)
            try:
                verify_artifact(outer, bad_api, source, run_id, system, arch, pin_scripts=False)
            except ValueError as error:
                require('Outer ZIP SHA256' in str(error), 'Wrong failure for digest mutation')
                rejected += 1
            else:
                raise ValueError('Accepted bad outer digest fixture')
            bad_outer, bad_api = fixture(root / f'wrong-architecture-{number}', system, arch, source, run_id, True)
            try:
                verify_artifact(bad_outer, bad_api, source, run_id, system, arch, pin_scripts=False)
            except ValueError as error:
                require(any(word in str(error) for word in ('architecture', 'PE32+', 'Mach-O')), 'Wrong architecture failure: ' + str(error))
                rejected += 1
            else:
                raise ValueError('Accepted coherent wrong-architecture fixture')
    return {'positive_target_fixtures': passed, 'rejected_mutations': rejected,
            'scope': 'Four platform targets; each bad API digest and internally consistent wrong-CPU payload rejected. Inert header-only fixtures, not real products.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--artifact-api', type=Path)
    parser.add_argument('--source')
    parser.add_argument('--run-id', type=int)
    parser.add_argument('--platform', choices=['linux', 'windows', 'macos'])
    parser.add_argument('--architecture', choices=['x86_64', 'arm64'])
    parser.add_argument('--repo', type=Path, default=REPO)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    try:
        if args.self_test:
            result = self_test()
        else:
            require(all(v is not None for v in (args.archive, args.artifact_api, args.source,
                    args.run_id, args.platform, args.architecture, args.out)), 'Missing required acceptance argument')
            result = verify_artifact(args.archive, load_json(args.artifact_api), args.source,
                                     args.run_id, args.platform, args.architecture, args.repo)
        raw = json.dumps(result, indent=2) + '\n'
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile('w', dir=args.out.parent, delete=False, prefix='.accept-', encoding='utf-8') as stream:
                temporary = Path(stream.name)
                stream.write(raw)
            os.replace(temporary, args.out)
        print(raw, end='')
        return 0
    except (ValueError, OSError, KeyError, TypeError, RuntimeError, EOFError,
            subprocess.CalledProcessError, zipfile.BadZipFile, tarfile.TarError) as error:
        print(f'Install artifact verification failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
