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
WINDOWS_COMPILER_SIDECARS = {'CS-80 Reference.exp', 'CS-80 Reference.lib'}


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


def coff_object(raw, require_exports=False):
    require(len(raw) >= 20 and int.from_bytes(raw[:2], 'little') == 0x8664,
            'Compiler sidecar is not an AMD64 COFF object')
    count = int.from_bytes(raw[2:4], 'little')
    symbol_at = int.from_bytes(raw[8:12], 'little')
    symbol_count = int.from_bytes(raw[12:16], 'little')
    require(1 <= count <= 96 and int.from_bytes(raw[16:18], 'little') == 0
            and 20 + 40 * count <= len(raw), 'Invalid COFF section table')
    names = []
    for number in range(count):
        section = raw[20 + 40 * number:60 + 40 * number]
        names.append(section[:8].rstrip(b'\0'))
        for size, offset, unit in ((int.from_bytes(section[16:20], 'little'), int.from_bytes(section[20:24], 'little'), 1),
                                  (int.from_bytes(section[32:34], 'little'), int.from_bytes(section[24:28], 'little'), 10),
                                  (int.from_bytes(section[34:36], 'little'), int.from_bytes(section[28:32], 'little'), 6)):
            if size:
                require(offset >= 20 + 40 * count and offset + size * unit <= len(raw),
                        'COFF section data exceeds the sidecar')
    if symbol_at or symbol_count:
        end = symbol_at + 18 * symbol_count
        require(symbol_at >= 20 + 40 * count and end + 4 <= len(raw), 'Invalid COFF symbol table')
        strings = int.from_bytes(raw[end:end + 4], 'little')
        require(strings >= 4 and end + strings <= len(raw), 'Invalid COFF string table')
    require(not require_exports or b'.edata' in names, 'Export sidecar has no .edata section')
    return {'format': 'AMD64 COFF object', 'sections': count, 'symbols': symbol_count}


def coff_import_library(raw):
    require(raw.startswith(b'!<arch>\n'), 'Compiler import sidecar is not a COFF archive')
    offset, members, objects, imports = 8, 0, 0, 0
    while offset < len(raw):
        require(offset + 60 <= len(raw), 'Truncated COFF archive member header')
        header = raw[offset:offset + 60]
        require(header[58:60] == b'`\n', 'Invalid COFF archive member header')
        size_text = header[48:58].decode('ascii').strip()
        require(size_text.isdigit(), 'Invalid COFF archive member size')
        size = int(size_text)
        start, end = offset + 60, offset + 60 + size
        require(end <= len(raw), 'Truncated COFF archive member')
        name = header[:16].decode('ascii').strip()
        data = raw[start:end]
        if name not in ('/', '//'):
            if data[:4] == b'\0\0\xff\xff':
                require(len(data) >= 20 and int.from_bytes(data[6:8], 'little') == 0x8664
                        and 20 + int.from_bytes(data[12:16], 'little') == len(data)
                        and data.endswith(b'\0'), 'Invalid AMD64 COFF short-import member')
                imports += 1
            else:
                coff_object(data)
                objects += 1
        members += 1
        require(members <= MAX_MEMBERS, 'Excessive COFF archive member count')
        if size % 2:
            require(raw[end:end + 1] == b'\n', 'Missing COFF archive alignment byte')
        offset = end + size % 2
    require(offset == len(raw) and imports > 0, 'COFF archive contains no AMD64 short-import member')
    return {'format': 'COFF import archive', 'archive_members': members,
            'coff_object_members': objects, 'short_import_members': imports}


def windows_sidecar(archive, member, path):
    require(not member.is_dir() and stat.S_IFMT(member.external_attr >> 16) in (0, stat.S_IFREG),
            'Compiler sidecar must be a regular top-level file')
    require(0 < member.file_size <= MAX_METADATA, 'Invalid compiler sidecar size')
    raw = archive.read(member)
    require(len(raw) == member.file_size, 'Truncated compiler sidecar')
    details = coff_object(raw, True) if path.endswith('.exp') else coff_import_library(raw)
    return {'path': path, 'type': 'file', 'bytes': len(raw), 'sha256': digest(raw),
            'format_check': details, 'included_in_vst3_payload_comparison': False}


def windows_container_contract(repo, source, api, consumer_log, pin_source):
    relative = '.github/workflows/build.yml'
    raw = (subprocess.check_output(['git', '-C', str(repo), 'show', source + ':' + relative])
           if pin_source else (repo / relative).read_bytes())
    workflow = raw.decode('utf-8')
    require('\n  windows:\n' in workflow and '\n  pluginval-windows:\n' in workflow,
            'Source workflow lacks the reviewed Windows jobs')
    native = workflow.split('\n  windows:\n', 1)[1].split('\n  pluginval-windows:\n', 1)[0]
    consumer = workflow.split('\n  pluginval-windows:\n', 1)[1]
    require('name: Upload Windows VST3' in native and 'name: cs80-windows-vst3' in native
            and 'path: build/CS80Reference_artefacts/Release/VST3' in native,
            'Source workflow does not upload the reviewed Windows container')
    require('name: Download Windows VST3' in consumer and 'name: cs80-windows-vst3' in consumer
            and 'path: vst3' in consumer, 'Source workflow does not download the reviewed artifact')
    selected = '"vst3\\CS-80 Reference.vst3" 2>&1 | Tee-Object -FilePath '
    commands = re.findall(re.escape(selected) + r'(pluginval-windows(?:-seed-regression)?\.log)', consumer)
    require(sorted(commands) == ['pluginval-windows-seed-regression.log', 'pluginval-windows.log'],
            'Source workflow does not explicitly select the bundle in both required Windows lanes')
    require(consumer_log is not None, 'Windows compiler sidecars require --consumer-log evidence')
    consumer_log = Path(consumer_log)
    require(consumer_log.is_file() and consumer_log.stat().st_size <= 16 * 1024 * 1024,
            'Missing/oversized Windows consumer log')
    log_raw = consumer_log.read_bytes()
    log = re.sub(r'\x1b\[[0-9;]*m', '', log_raw.decode('utf-8-sig'))
    expected = (f'- cs80-windows-vst3 (ID: {api["id"]}, Size: {api["size_in_bytes"]}, '
                f'Expected Digest: {api["digest"]})')
    require(expected in log and 'SHA256 digest of downloaded artifact is ' + api['digest'].split(':', 1)[1] in log,
            'Windows consumer log does not bind the exact downloaded artifact')
    paths = re.findall(r'Started validating: ([^\r\n]+)', log)
    require(len(paths) == 2 and all(p.endswith('\\vst3\\' + PRODUCT) for p in paths),
            'Windows consumer log does not select only the bundle in both lanes')
    return {'workflow_path': relative, 'workflow_commit': source, 'workflow_sha256': digest(raw),
            'workflow_source_pinned': pin_source,
            'uploaded_directory': 'build/CS80Reference_artefacts/Release/VST3',
            'download_directory': 'vst3', 'selected_plugin': 'vst3\\' + PRODUCT,
            'required_command_logs': sorted(commands),
            'consumer_log': {'file': consumer_log.name, 'bytes': len(log_raw), 'sha256': digest(log_raw),
                             'artifact_download_identity_matched': True, 'started_validation_paths': paths},
            'excluded_container_paths': sorted(WINDOWS_COMPILER_SIDECARS),
            'exclusion_rationale': 'The committed workflow uploads the whole VST3 output directory, including the MSVC export object and import library. Both actual consumer invocations explicitly select its CS-80 Reference.vst3 subdirectory. Only this exact, format-checked top-level compiler sidecar pair is recorded separately; every member inside the selected bundle remains subject to strict equality.'}


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


def zip_payload(archive, prefix, platform, reject_outside, sidecars=None):
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
            if reject_outside:
                require(platform == 'windows' and prefix == '' and path in WINDOWS_COMPILER_SIDECARS
                        and sidecars is not None, f'Validator artifact has unexpected member: {path}')
                sidecars.append(windows_sidecar(archive, member, path))
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


def verify(install, install_api, validator, validator_api, source, run_id, platform, repo=REPO, pin_scripts=True,
           consumer_log=None):
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
        sidecars = []
        with zipfile.ZipFile(validator) as archive:
            validated = zip_payload(archive, '', platform, True, sidecars)
        container_contract = None
        if sidecars:
            require({item['path'] for item in sidecars} == WINDOWS_COMPILER_SIDECARS,
                    'Incomplete known Windows compiler sidecar pair')
            container_contract = windows_container_contract(repo, source, validator_api, consumer_log, pin_scripts)
        members = compare_maps(distributed, validated)
        inner_identity = {'file': inner_name, 'bytes': inner.stat().st_size, 'sha256': sha_file(inner)}
    canonical = json.dumps(members, sort_keys=True, separators=(',', ':')).encode()
    return {'schema': 1, 'status': 'distributed_vst3_matches_validator_input',
            'verified_at_utc': datetime.now(timezone.utc).isoformat(),
            'source': source, 'run_id': run_id, 'platform': platform, 'architecture': 'x86_64',
            'installation_artifact': installed_identity, 'validator_input_artifact': validator_identity,
            'validator_container_sidecars': sorted(sidecars, key=lambda item: item['path']),
            'validator_container_contract': container_contract,
            'inner_archive': inner_identity, 'payload_tree_sha256': digest(canonical),
            'payload_entries': len(members),
            'files': sum(m['type'] == 'file' for m in members),
            'symlinks': sum(m['type'] == 'symlink' for m in members),
            'file_bytes': sum(m.get('bytes', 0) for m in members), 'members': members,
            'source_checker_sha256': checker_hashes, 'checker_sha256': sha_file(Path(__file__)),
            'scope': 'Complete selected VST3 bundle file bytes, resources, directory topology and symbolic-link targets. Only the exact Windows top-level .exp/.lib compiler pair can be recorded separately, with format, source-workflow and actual consumer-log checks. Parent directories are inferred when Actions ZIP omits directory records. Archive timestamps, ownership and permission normalization are not compared.',
            'remaining_qualification': 'Artifact producer/upload/attempt binding and successful original pluginval exits are separate requirements. No binary executed by this checker.'}


def fixture_api(path, name, source, run_id, identity):
    return {'id': identity, 'name': name, 'expired': False, 'size_in_bytes': path.stat().st_size,
            'digest': 'sha256:' + sha_file(path), 'workflow_run': {'id': run_id, 'head_sha': source}}


def compiler_sidecar_fixtures():
    # Inert export-section COFF object and import archive; no executable code.
    header = bytearray(20)
    header[:2] = (0x8664).to_bytes(2, 'little')
    header[2:4] = (1).to_bytes(2, 'little')
    section = bytearray(40)
    section[:8] = b'.edata\0\0'
    section[16:20] = (4).to_bytes(4, 'little')
    section[20:24] = (60).to_bytes(4, 'little')
    export = bytes(header + section) + b'\0' * 4
    names = b'FixtureEntry\0CS-80 Reference.vst3\0'
    import_header = bytearray(20)
    import_header[2:4] = b'\xff\xff'
    import_header[6:8] = (0x8664).to_bytes(2, 'little')
    import_header[12:16] = len(names).to_bytes(4, 'little')
    data = bytes(import_header) + names
    archive_header = ('imports.obj/'.ljust(16) + '0'.ljust(12) + '0'.ljust(6) + '0'.ljust(6)
                      + '0'.ljust(8) + str(len(data)).ljust(10)).encode() + b'`\n'
    library = b'!<arch>\n' + archive_header + data + (b'\n' if len(data) % 2 else b'')
    return {'CS-80 Reference.exp': export, 'CS-80 Reference.lib': library}


def write_validator(path, products, mutation=None, compiler_sidecars=False):
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
        if compiler_sidecars:
            for name, raw in compiler_sidecar_fixtures().items():
                if mutation == 'missing-sidecar' and name.endswith('.lib'):
                    continue
                if mutation == 'bad-exp-format' and name.endswith('.exp'):
                    raw = b'MZ-not-a-COFF-object'
                if mutation == 'bad-lib-format' and name.endswith('.lib'):
                    raw = b'not-a-COFF-archive'
                if mutation == 'inside-bundle-sidecar' and name.endswith('.exp'):
                    archive.writestr(PRODUCT + '/Contents/Resources/' + name, raw)
                archive.writestr(name, raw)
        if mutation == 'unknown-sidecar':
            archive.writestr('CS-80 Reference.pdb', b'unknown outside member')
        if mutation == 'other-export-basename':
            archive.writestr('Another Plugin.exp', compiler_sidecar_fixtures()['CS-80 Reference.exp'])


def fixture_consumer_log(path, api, bad_identity=False, wrong_target=False):
    raw = (f'- cs80-windows-vst3 (ID: {api["id"] + int(bad_identity)}, Size: {api["size_in_bytes"]}, '
           f'Expected Digest: {api["digest"]})\n'
           'SHA256 digest of downloaded artifact is ' + api['digest'].split(':', 1)[1] + '\n')
    selected = 'CS-80 Reference.exp' if wrong_target else PRODUCT
    raw += ('Started validating: D:\\fixture\\vst3\\' + selected + '\n') * 2
    path.write_text(raw)


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
            # This boundary was discovered in the actual Windows upload:
            # the artifact includes MSVC link outputs beside the selected bundle.
            write_validator(validator, products, compiler_sidecars=True)
            api = fixture_api(validator, f'cs80-{platform}-vst3', source, run_id, 2)
            consumer_log = root / platform / 'consumer.log'
            fixture_consumer_log(consumer_log, api)
            if platform == 'linux':
                try:
                    verify(install, install_api, validator, api, source, run_id, platform, pin_scripts=False,
                           consumer_log=consumer_log)
                except ValueError as error:
                    require('Validator artifact has unexpected member' in str(error), 'Wrong Linux sidecar rejection')
                    rejected.append({'platform': platform, 'mutation': 'windows-sidecars-on-linux'})
                else:
                    raise ValueError('Accepted Windows sidecars on Linux')
                continue
            result = verify(install, install_api, validator, api, source, run_id, platform, pin_scripts=False,
                            consumer_log=consumer_log)
            require(len(result['validator_container_sidecars']) == 2 and result['files'] == 2,
                    'Windows sidecars changed the strict bundle inventory')
            positive += 1
            sidecar_mutations = {
                'changed-binary': 'VST3 payload mismatch',
                'missing-resource': 'VST3 payload mismatch',
                'extra-resource': 'VST3 payload mismatch',
                'inside-bundle-sidecar': 'VST3 payload mismatch',
                'unknown-sidecar': 'Validator artifact has unexpected member',
                'other-export-basename': 'Validator artifact has unexpected member',
                'bad-exp-format': 'AMD64 COFF object',
                'bad-lib-format': 'COFF archive',
                'missing-sidecar': 'Incomplete known Windows compiler sidecar pair',
                'wrong-consumer-artifact': 'does not bind the exact downloaded artifact',
                'wrong-consumer-target': 'does not select only the bundle',
                'missing-consumer-log': 'require --consumer-log evidence',
            }
            for mutation, expected in sidecar_mutations.items():
                write_validator(validator, products, mutation, compiler_sidecars=True)
                api = fixture_api(validator, f'cs80-{platform}-vst3', source, run_id, 2)
                fixture_consumer_log(consumer_log, api, mutation == 'wrong-consumer-artifact',
                                     mutation == 'wrong-consumer-target')
                try:
                    verify(install, install_api, validator, api, source, run_id, platform, pin_scripts=False,
                           consumer_log=None if mutation == 'missing-consumer-log' else consumer_log)
                except ValueError as error:
                    require(expected in str(error), f'Unexpected sidecar mutation failure: {error}')
                    rejected.append({'platform': platform, 'with_compiler_sidecars': True, 'mutation': mutation})
                else:
                    raise ValueError('Accepted sidecar-boundary mutation: ' + mutation)
    return {'status': 'synthetic_fixture_checks_passed', 'positive_platform_fixtures': positive,
            'rejected_mutations': rejected,
            'scope': 'Inert Windows/Linux header fixtures with resources and a Linux symbolic link, plus the Windows compiler-sidecar container boundary with source workflow and fixture consumer log. Mutated validator ZIPs receive correct updated API digests; strict bundle equality and sidecar exclusions are tested independently of download integrity.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install-archive', type=Path)
    parser.add_argument('--install-api', type=Path)
    parser.add_argument('--validator-archive', type=Path)
    parser.add_argument('--validator-api', type=Path)
    parser.add_argument('--consumer-log', type=Path,
                        help='Original Windows pluginval job log; required when the input ZIP contains the known compiler sidecars')
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
                    load_json(args.validator_api), args.source, args.run_id, args.platform, args.repo,
                    consumer_log=args.consumer_log)
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
