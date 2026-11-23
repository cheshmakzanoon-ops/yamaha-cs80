#!/usr/bin/env python3
"""Compare distributed VST3 payloads with the exact Actions inputs to pluginval.

Does not execute products or certify validator outcomes. The caller must bind
the artifact IDs to their successful producing/upload/validation steps.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import tarfile
import tempfile
import zipfile

REPO = Path.cwd()  # Run from the repository root or pass --repo explicitly.
PRODUCT = 'CS-80 Reference.vst3'
MAX_ARCHIVE = 512 * 1024 * 1024
MAX_ENTRY = 256 * 1024 * 1024
MAX_TOTAL = 1024 * 1024 * 1024
MAX_METADATA = 2 * 1024 * 1024
MAX_MEMBERS = 4096


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def sha_file(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'Duplicate JSON key: {key}')
        result[key] = value
    return result


def load_json(path):
    path = Path(path)
    require(path.is_file() and path.stat().st_size <= MAX_METADATA, 'Missing/oversized JSON metadata')
    return json.loads(path.read_text(), object_pairs_hook=unique_object)


def safe_path(name):
    require(isinstance(name, str) and name, 'Empty archive path')
    name = name[:-1] if name.endswith('/') else name
    require(name and not name.startswith('/') and '\\' not in name,
            f'Unsafe archive path: {name!r}')
    require(all(32 <= ord(c) != 127 for c in name), 'Control character in archive path')
    parts = name.split('/')
    require(all(p not in ('', '.', '..') and ':' not in p for p in parts),
            f'Unsafe archive path: {name!r}')
    return name


def check_api(path, api, expected_name, source, run_id):
    path = Path(path)
    require(type(api.get('id')) is int and api['id'] > 0, 'Invalid artifact ID')
    require(api.get('name') == expected_name, f'Wrong artifact name: {api.get("name")}')
    require(api.get('expired') is False, 'Artifact expiry state is not current')
    workflow = api.get('workflow_run', {})
    require(workflow.get('id') == run_id and workflow.get('head_sha') == source,
            'Artifact source/run differs from requested source/run')
    size = api.get('size_in_bytes')
    require(type(size) is int and 0 < size <= MAX_ARCHIVE, 'Invalid/oversized API archive size')
    require(path.is_file() and path.stat().st_size == size, 'Archive size differs from API')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', api.get('digest', '')), 'Missing artifact SHA256')
    actual = sha_file(path)
    require(actual == api['digest'].split(':', 1)[1], 'Archive SHA256 differs from API')
    return {'artifact_id': api['id'], 'name': expected_name, 'file': path.name,
            'bytes': size, 'sha256': actual}


def stream_digest(stream, size):
    require(0 <= size <= MAX_ENTRY, 'Oversized payload member')
    h = hashlib.sha256()
    total = 0
    for block in iter(lambda: stream.read(1024 * 1024), b''):
        total += len(block)
        require(total <= size, 'Member exceeds its declared size')
        h.update(block)
    require(total == size, 'Truncated member bytes')
    return {'type': 'file', 'bytes': size, 'sha256': h.hexdigest()}


def link_record(path, raw):
    require(len(raw) <= 4096, 'Oversized symbolic-link target')
    target = raw.decode('utf-8')
    require(target and not target.startswith('/') and '\\' not in target
            and all(32 <= ord(c) != 127 for c in target), 'Unsafe symlink target')
    parts = path.split('/')[:-1]
    for component in target.split('/'):
        require(component and ':' not in component, 'Unsafe symlink target component')
        if component == '..':
            require(len(parts) > 1, 'Symlink escapes the product bundle')
            parts.pop()
        elif component != '.':
            parts.append(component)
    require(parts and parts[0] == PRODUCT, 'Symlink escapes the product bundle')
    return {'type': 'symlink', 'target': target, 'resolved_target': '/'.join(parts)}


def finish_map(records, platform):
    require(records, 'Empty VST3 payload')
    for path in tuple(records):
        require(path == PRODUCT or path.startswith(PRODUCT + '/'), 'Member outside expected VST3 bundle')
        for parent in PurePosixPath(path).parents:
            name = parent.as_posix()
            if name == '.':
                break
            require(records.get(name, {'type': 'directory'})['type'] == 'directory',
                    f'Nondirectory parent: {name}')
            records.setdefault(name, {'type': 'directory'})
    require(records.get(PRODUCT) == {'type': 'directory'}, 'Missing VST3 root directory')
    folded = set()
    for path, item in records.items():
        if platform == 'windows':
            require(path.casefold() not in folded, 'Case-insensitive Windows path collision')
            folded.add(path.casefold())
        if item['type'] == 'symlink':
            require(platform != 'windows', 'Windows VST3 payload contains a symbolic link')
            seen = {path}
            target = item['resolved_target']
            while True:
                require(target in records and target not in seen, 'Dangling/cyclic symbolic link')
                seen.add(target)
                node = records[target]
                if node['type'] != 'symlink':
                    break
                target = node['resolved_target']
    binary = PRODUCT + ('/Contents/x86_64-win/CS-80 Reference.vst3' if platform == 'windows'
                        else '/Contents/x86_64-linux/CS-80 Reference.so')
    require(records.get(binary, {}).get('type') == 'file'
            and records[binary]['bytes'] > 0, 'Missing native VST3 binary')
    return {name: records[name] for name in sorted(records)}


def zip_payload(archive, prefix, platform, reject_outside):
    records, seen = {}, set()
    members = archive.infolist()
    require(0 < len(members) <= MAX_MEMBERS, 'Unexpected ZIP member count')
    require(sum(m.file_size for m in members) <= MAX_TOTAL, 'Expanded ZIP exceeds bound')
    for member in members:
        path = safe_path(member.filename)
        require(path not in seen, f'Duplicate archive member: {path}')
        seen.add(path)
        require(not member.flag_bits & 1 and member.compress_type in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED),
                'Encrypted or unsupported ZIP entry')
        require(0 <= member.file_size <= MAX_ENTRY, 'Oversized ZIP member')
        relative = path[len(prefix):] if path.startswith(prefix) else None
        selected = relative is not None and (relative == PRODUCT or relative.startswith(PRODUCT + '/'))
        if not selected:
            require(not reject_outside, f'Validator artifact has unexpected member: {path}')
            continue
        mode = stat.S_IFMT(member.external_attr >> 16)
        if member.is_dir():
            require(mode in (0, stat.S_IFDIR) and member.file_size == 0, 'Invalid directory ZIP entry')
            records[relative] = {'type': 'directory'}
        elif mode == stat.S_IFLNK:
            require(member.file_size <= 4096, 'Oversized symlink ZIP entry')
            records[relative] = link_record(relative, archive.read(member))
        else:
            require(mode in (0, stat.S_IFREG), 'Unsupported ZIP object type')
            with archive.open(member) as stream:
                records[relative] = stream_digest(stream, member.file_size)
    return finish_map(records, platform)


def tar_payload(path, prefix, platform):
    records, seen = {}, set()
    total = 0
    with tarfile.open(path, 'r:gz') as archive:
        for number, member in enumerate(archive, 1):
            require(number <= MAX_MEMBERS, 'Excessive tar member count')
            name = safe_path(member.name)
            require(name not in seen, f'Duplicate archive member: {name}')
            seen.add(name)
            require(member.isfile() or member.isdir() or member.issym(), 'Unsupported tar object type')
            require(0 <= member.size <= MAX_ENTRY, 'Oversized tar member')
            total += member.size
            require(total <= MAX_TOTAL, 'Expanded tar exceeds bound')
            relative = name[len(prefix):] if name.startswith(prefix) else None
            if relative is None or not (relative == PRODUCT or relative.startswith(PRODUCT + '/')):
                continue
            if member.isdir():
                records[relative] = {'type': 'directory'}
            elif member.issym():
                records[relative] = link_record(relative, member.linkname.encode('utf-8'))
            else:
                stream = archive.extractfile(member)
                require(stream is not None, 'Missing tar data stream')
                with stream:
                    records[relative] = stream_digest(stream, member.size)
    return finish_map(records, platform)


def compare_maps(distributed, validated):
    missing = sorted(set(distributed) - set(validated))
    extra = sorted(set(validated) - set(distributed))
    changed = sorted(p for p in set(distributed) & set(validated) if distributed[p] != validated[p])
    require(not (missing or extra or changed),
            f'VST3 payload mismatch: absent from validator={missing}, extra in validator={extra}, changed={changed}')
    return [{'path': name, **record} for name, record in distributed.items()]


def load_checker(repo, source, pin_scripts=True):
    hashes = {}
    for relative in ('scripts/verify-ci-bundle.py', 'scripts/package-ci-artifacts.py'):
        raw = (repo / relative).read_bytes()
        if pin_scripts:
            committed = subprocess.check_output(['git', '-C', str(repo), 'show', source + ':' + relative])
            require(raw == committed, f'Checker differs from source commit: {relative}')
        hashes[relative] = digest(raw)
    spec = importlib.util.spec_from_file_location('cs80_payload_inner_checker', repo / 'scripts/verify-ci-bundle.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module, hashes


def verify(install, install_api, validator, validator_api, source, run_id, platform, repo=REPO, pin_scripts=True):
    install, validator, repo = Path(install), Path(validator), Path(repo)
    require(re.fullmatch('[0-9a-f]{40}', source), 'Expected full lowercase source SHA')
    require(type(run_id) is int and run_id > 0 and platform in ('linux', 'windows'), 'Invalid target/run')
    installed_identity = check_api(install, install_api, f'cs80-{platform}-install-bundle', source, run_id)
    validator_identity = check_api(validator, validator_api, f'cs80-{platform}-vst3', source, run_id)
    checker, checker_hashes = load_checker(repo, source, pin_scripts)
    root = f'cs80-reference-{platform}-x86_64-{source[:12]}'
    inner_name = root + ('.zip' if platform == 'windows' else '.tar.gz')
    sidecar = root + '.manifest.json'
    with tempfile.TemporaryDirectory(prefix='cs80-vst3-identity-') as directory:
        temp = Path(directory)
        with zipfile.ZipFile(install) as outer:
            items = outer.infolist()
            require(len(items) == 3 and {m.filename for m in items} == {inner_name, sidecar, 'SHA256SUMS'},
                    'Unexpected installation Actions ZIP members')
            for member in items:
                require(not member.is_dir() and not member.flag_bits & 1
                        and stat.S_IFMT(member.external_attr >> 16) in (0, stat.S_IFREG), 'Invalid outer ZIP object')
                limit = MAX_ARCHIVE if member.filename == inner_name else MAX_METADATA
                require(0 < member.file_size <= limit, 'Oversized outer ZIP member')
                total = 0
                with outer.open(member) as source_stream, (temp / member.filename).open('xb') as output:
                    for block in iter(lambda: source_stream.read(1024 * 1024), b''):
                        total += len(block)
                        require(total <= member.file_size, 'Outer ZIP member exceeds declared size')
                        output.write(block)
                require(total == member.file_size, 'Truncated outer ZIP member')
        inner = temp / inner_name
        checker.verify(inner, source)
        manifest = load_json(temp / sidecar)
        require(manifest.get('platform') == platform and manifest.get('architecture') == 'x86_64'
                and manifest.get('preset_library_revision') == '2026.11.3'
                and str(manifest.get('workflow_run_id')) == str(run_id), 'Installation manifest target/revision/run mismatch')
        prefix = root + '/VST3/'
        if platform == 'windows':
            with zipfile.ZipFile(inner) as archive:
                distributed = zip_payload(archive, prefix, platform, False)
        else:
            distributed = tar_payload(inner, prefix, platform)
        with zipfile.ZipFile(validator) as archive:
            validated = zip_payload(archive, '', platform, True)
        members = compare_maps(distributed, validated)
        inner_identity = {'file': inner_name, 'bytes': inner.stat().st_size, 'sha256': sha_file(inner)}
    canonical = json.dumps(members, sort_keys=True, separators=(',', ':')).encode()
    return {'schema': 1, 'status': 'distributed_vst3_matches_validator_input',
            'verified_at_utc': datetime.now(timezone.utc).isoformat(),
            'source': source, 'run_id': run_id, 'platform': platform, 'architecture': 'x86_64',
            'installation_artifact': installed_identity, 'validator_input_artifact': validator_identity,
            'inner_archive': inner_identity, 'payload_tree_sha256': digest(canonical),
            'payload_entries': len(members),
            'files': sum(m['type'] == 'file' for m in members),
            'symlinks': sum(m['type'] == 'symlink' for m in members),
            'file_bytes': sum(m.get('bytes', 0) for m in members), 'members': members,
            'source_checker_sha256': checker_hashes, 'checker_sha256': sha_file(Path(__file__)),
            'scope': 'Complete VST3 file bytes, resources, directory topology and symbolic-link targets. Parent directories are inferred when Actions ZIP omits directory records. Archive timestamps, ownership and permission normalization are not compared.',
            'remaining_qualification': 'Artifact producer/upload/attempt binding and successful original pluginval exits are separate requirements. No binary executed by this checker.'}


def fixture_api(path, name, source, run_id, identity):
    return {'id': identity, 'name': name, 'expired': False, 'size_in_bytes': path.stat().st_size,
            'digest': 'sha256:' + sha_file(path), 'workflow_run': {'id': run_id, 'head_sha': source}}


def write_validator(path, products, mutation=None):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for item in sorted((products / 'VST3').rglob('*')):
            if item.is_dir():
                continue  # Actions can omit directory records; topology is inferred.
            relative = item.relative_to(products / 'VST3').as_posix()
            if mutation == 'missing-resource' and relative.endswith('moduleinfo.json'):
                continue
            if item.is_symlink():
                info = zipfile.ZipInfo(relative)
                info.create_system = 3
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
                target = os.readlink(item)
                if mutation == 'changed-link':
                    target = '../Resources/moduleinfo.json'
                archive.writestr(info, target)
            else:
                raw = item.read_bytes()
                if mutation == 'changed-binary' and relative.endswith(('.so', '.vst3')):
                    raw = raw[:-1] + bytes([raw[-1] ^ 1])
                archive.writestr(relative, raw)
        if mutation == 'extra-resource':
            archive.writestr(PRODUCT + '/Contents/Resources/extra.txt', b'extra')
        if mutation == 'unsafe-path':
            archive.writestr(PRODUCT + '/../outside', b'unsafe')


def self_test():
    positive, rejected = 0, []
    checker, _ = load_checker(REPO, 'a' * 40, False)
    source, run_id = 'a' * 40, 123
    with tempfile.TemporaryDirectory(prefix='cs80-payload-fixture-') as directory:
        root = Path(directory)
        for platform in ('linux', 'windows'):
            products = root / platform / 'products'
            suffix = 'Contents/x86_64-win/CS-80 Reference.vst3' if platform == 'windows' else 'Contents/x86_64-linux/CS-80 Reference.so'
            plugin = products / 'VST3' / PRODUCT / suffix
            standalone = products / 'Standalone' / ('CS-80 Reference.exe' if platform == 'windows' else 'CS-80 Reference')
            resource = products / 'VST3' / PRODUCT / 'Contents/Resources/moduleinfo.json'
            for path, executable in ((plugin, False), (standalone, True)):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(checker.packager.binary_fixture(platform, 'x86_64', executable))
                path.chmod(0o755)
            resource.parent.mkdir(parents=True, exist_ok=True)
            resource.write_text('{"fixture":"module resource"}\n')
            if platform == 'linux':
                (plugin.parent / 'binary-link.so').symlink_to(plugin.name)
            output = root / platform / 'packages'
            inner, _ = checker.packager.package(products, output, platform, 'x86_64', source,
                    {'preset_library_revision': '2026.11.3', 'workflow_run_id': str(run_id)}, {})
            install = root / platform / 'install.zip'
            with zipfile.ZipFile(install, 'w', compression=zipfile.ZIP_STORED) as archive:
                for item in output.iterdir():
                    archive.write(item, item.name)
            install_api = fixture_api(install, f'cs80-{platform}-install-bundle', source, run_id, 1)
            validator = root / platform / 'validator.zip'
            write_validator(validator, products)
            api = fixture_api(validator, f'cs80-{platform}-vst3', source, run_id, 2)
            result = verify(install, install_api, validator, api, source, run_id, platform, pin_scripts=False)
            require(result['files'] == 2 and result['symlinks'] == (platform == 'linux'), 'Unexpected fixture content')
            positive += 1
            mutations = ['changed-binary', 'missing-resource', 'extra-resource', 'unsafe-path']
            if platform == 'linux':
                mutations.append('changed-link')
            for mutation in mutations:
                write_validator(validator, products, mutation)
                api = fixture_api(validator, f'cs80-{platform}-vst3', source, run_id, 2)
                try:
                    verify(install, install_api, validator, api, source, run_id, platform, pin_scripts=False)
                except ValueError as error:
                    require(('Unsafe archive path' if mutation == 'unsafe-path' else 'VST3 payload mismatch') in str(error),
                            f'Unexpected mutation failure: {error}')
                    rejected.append({'platform': platform, 'mutation': mutation})
                else:
                    raise ValueError('Accepted mutation: ' + mutation)
    return {'status': 'synthetic_fixture_checks_passed', 'positive_platform_fixtures': positive,
            'rejected_mutations': rejected,
            'scope': 'Inert Windows/Linux header fixtures with resources; Linux includes a symbolic link. Mutated validator ZIPs have correct updated API digests, so payload differences are tested independently of download integrity.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install-archive', type=Path)
    parser.add_argument('--install-api', type=Path)
    parser.add_argument('--validator-archive', type=Path)
    parser.add_argument('--validator-api', type=Path)
    parser.add_argument('--source')
    parser.add_argument('--run-id', type=int)
    parser.add_argument('--platform', choices=['linux', 'windows'])
    parser.add_argument('--repo', type=Path, default=REPO)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    try:
        if args.self_test:
            result = self_test()
        else:
            require(all(x is not None for x in (args.install_archive, args.install_api, args.validator_archive,
                    args.validator_api, args.source, args.run_id, args.platform, args.out)), 'Missing acceptance argument')
            result = verify(args.install_archive, load_json(args.install_api), args.validator_archive,
                    load_json(args.validator_api), args.source, args.run_id, args.platform, args.repo)
        raw = json.dumps(result, indent=2) + '\n'
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile('w', dir=args.out.parent, prefix='.payload-', delete=False, encoding='utf-8') as stream:
                temporary = Path(stream.name)
                stream.write(raw)
            os.replace(temporary, args.out)
        print(raw, end='')
        return 0
    except (ValueError, OSError, KeyError, TypeError, RuntimeError, EOFError,
            subprocess.CalledProcessError, zipfile.BadZipFile, tarfile.TarError) as error:
        print(f'Validator payload identity failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
