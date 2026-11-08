#!/usr/bin/env python3
"""Verify a downloaded CI bundle offline, without extracting or executing it.

Checksums establish consistency, not publisher authenticity, signing, safety of
running the product, or successful CI/DAW qualification. Obtain the expected
full commit and checksum evidence from a trusted repository workflow first.
"""
from __future__ import annotations
import argparse
import hashlib
import gzip
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import sys
import tarfile
import zipfile
import zlib

SPEC = importlib.util.spec_from_file_location('cs80_packager', Path(__file__).with_name('package-ci-artifacts.py'))
packager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packager)
MAX_ARCHIVE = 512 * 1024 * 1024
MAX_FILE = 256 * 1024 * 1024
MAX_TOTAL = 1024 * 1024 * 1024
MAX_METADATA = 2 * 1024 * 1024
MAX_MEMBERS = 4096


def require(condition, message):
    if not condition:
        raise ValueError(message)


def safe_name(name):
    require(isinstance(name, str) and name and all(32 <= ord(c) != 127 for c in name), 'Invalid member name')
    parts = name.split('/')
    require(not name.startswith('/') and '\\' not in name and all(':' not in part for part in parts)
            and all(p not in ('', '.', '..') for p in parts), f'Unsafe member path: {name}')


def read_limited(path, limit):
    require(path.is_file() and path.stat().st_size <= limit, f'Missing or oversized file: {path}')
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit, f'Oversized file: {path}')
    return data


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'Duplicate JSON key: {key}')
        result[key] = value
    return result


def validate_manifest(manifest, expected_commit):
    require(isinstance(manifest, dict) and type(manifest.get('schema_version')) is int
            and manifest['schema_version'] == 1, 'Unsupported manifest schema')
    require(manifest.get('commit') == expected_commit, 'Manifest source commit does not match expected commit')
    system, arch = manifest.get('platform'), manifest.get('architecture')
    require(system in ('linux', 'macos', 'windows') and arch in ('x86_64', 'arm64')
            and (system != 'windows' or arch == 'x86_64'), 'Unsupported platform/architecture')
    require(manifest.get('product') == packager.PRODUCT, 'Unexpected product')
    require(manifest.get('formats') == ['VST3', 'Standalone'] + (['AU'] if system == 'macos' else []),
            'Unexpected advertised formats')
    items = manifest.get('files')
    require(isinstance(items, list) and 0 < len(items) < MAX_MEMBERS, 'Invalid inventory count')
    names, windows_names, total = set(), {'manifest.json'}, 0
    for item in items:
        require(isinstance(item, dict), 'Invalid inventory entry')
        name = item.get('path')
        safe_name(name)
        first = name.split('/')[0].casefold() if system == 'windows' else name.split('/')[0]
        require(name not in names and first != 'manifest.json', f'Duplicate/reserved inventory path: {name}')
        names.add(name)
        if system == 'windows':
            folded = name.casefold()
            require(folded not in windows_names, 'Windows case-insensitive path collision')
            windows_names.add(folded)
            for component in name.split('/'):
                require(not component.endswith(('.', ' ')) and not any(c in component for c in '<>"|?*'),
                        'Windows ambiguous/invalid path component')
                require(not re.fullmatch(r'(CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])', component.split('.')[0].upper()),
                        'Windows reserved device path')
        require(item.get('type') in ('file', 'directory', 'symlink'), 'Unsupported inventory type')
        require(isinstance(item.get('mode'), str) and re.fullmatch(r'0o[0-7]{1,4}', item['mode']), 'Invalid mode')
        if item['type'] == 'file':
            size = item.get('size')
            require(type(size) is int and 0 <= size <= MAX_FILE, 'Invalid file size')
            total += size
            require(isinstance(item.get('sha256'), str) and re.fullmatch(r'[0-9a-f]{64}', item['sha256']), 'Invalid file digest')
        elif item['type'] == 'symlink':
            require(system != 'windows' and isinstance(item.get('target'), str), 'Invalid symlink inventory')
    require(total <= MAX_TOTAL, 'Inventory exceeds expanded-size limit')
    inventory = {item['path']: item for item in items}
    if system == 'macos':
        bundles = [f'VST3/{packager.PRODUCT}.vst3', f'AU/{packager.PRODUCT}.component',
                   f'Standalone/{packager.PRODUCT}.app']
        required = [base + '/Contents/MacOS/' + packager.PRODUCT for base in bundles]
        required += [base + '/Contents/Info.plist' for base in bundles]
    elif system == 'linux':
        required = [f'VST3/{packager.PRODUCT}.vst3/Contents/{arch}-linux/{packager.PRODUCT}.so',
                    f'Standalone/{packager.PRODUCT}']
    else:
        required = [f'VST3/{packager.PRODUCT}.vst3/Contents/x86_64-win/{packager.PRODUCT}.vst3',
                    f'Standalone/{packager.PRODUCT}.exe']
    require(all(inventory.get(name, {}).get('type') in ('file', 'symlink') for name in required),
            'Missing advertised product payload')
    validate_virtual_links(items, system)

    return f'cs80-reference-{system}-{arch}-{expected_commit[:12]}', system


def validate_virtual_links(items, system, product='CS-80 Reference'):
    """Validate manifest symlinks using POSIX component-wise path resolution.

    Call after schema/path uniqueness validation. Links must resolve to an
    inventoried file/directory inside their own original product bundle. A
    descendant archived under a symlink is rejected regardless of link safety.
    Metadata files may have implicit directory parents, as the packager emits
    them, but symlink targets/parents must be explicitly inventoried.
    """
    def require(ok, message):
        if not ok:
            raise ValueError(message)

    inventory = {tuple(part.casefold() if system == 'windows' else part
                       for part in item['path'].split('/')): item for item in items}
    require(len(inventory) == len(items), 'Duplicate inventory path')
    roots = {('VST3', product + '.vst3')}
    if system == 'macos':
        roots |= {('AU', product + '.component'), ('Standalone', product + '.app')}
    for path in inventory:
        for end in range(1, len(path)):
            parent = inventory.get(path[:end])
            require(parent is None or parent['type'] == 'directory',
                    'Archived member beneath a non-directory: ' + '/'.join(path))

    def target_parts(target):
        require(isinstance(target, str) and target and not target.startswith('/')
                and '\\' not in target and all(32 <= ord(c) != 127 for c in target)
                and ':' not in target.split('/')[0], 'Unsafe symlink target')
        return target.split('/')

    def resolve(current, pending, root, visiting):
        for part in pending:
            # Even file/.. is invalid: POSIX must traverse file as a directory.
            parent = inventory.get(current)
            require(parent is not None and parent['type'] == 'directory',
                    'Symlink traverses missing/non-directory parent')
            if part in ('', '.'):
                continue
            if part == '..':
                require(len(current) > len(root), 'Symlink escapes original product bundle')
                current = current[:-1]
                continue
            child = current + (part,)
            node = inventory.get(child)
            require(node is not None, 'Dangling symlink target: ' + '/'.join(child))
            if node['type'] == 'symlink':
                require(child not in visiting, 'Cyclic symlink target')
                require(len(visiting) < 64, 'Symlink chain exceeds 64 links')
                current = resolve(current, target_parts(node['target']), root, visiting | {child})
            else:
                require(node['type'] in ('file', 'directory'), 'Unsupported symlink target type')
                current = child
        return current

    for path, item in inventory.items():
        if item['type'] != 'symlink':
            continue
        require(system != 'windows', 'Windows bundles cannot contain symlinks')
        root = path[:2]
        require(root in roots and len(path) > len(root), 'Symlink outside a product bundle')
        require(inventory.get(root, {}).get('type') == 'directory', 'Missing product root directory')
        resolve(path[:-1], target_parts(item['target']), root, {path})


def preflight_tar(archive):
    """Bound expanded headers before tarfile processes extended metadata."""
    total, count = 0, 0
    with gzip.open(archive, 'rb') as stream:
        while True:
            header = stream.read(512)
            total += len(header)
            require(len(header) == 512, 'Truncated tar header')
            if header == bytes(512):
                # Drain bounded zero padding so gzip CRC/truncation is checked.
                for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                    total += len(chunk)
                    require(total <= MAX_TOTAL and not any(chunk), 'Invalid tar trailing data')
                break
            count += 1
            require(count <= MAX_MEMBERS * 2, 'Too many tar headers')
            raw_size = header[124:136].strip(b' \0')
            require(raw_size and all(c in b'01234567' for c in raw_size), 'Unsupported tar size encoding')
            size = int(raw_size, 8)
            limit = MAX_METADATA if header[156:157] in (b'x', b'g', b'L', b'K') else MAX_FILE
            require(size <= limit, 'Oversized tar payload or extended metadata')
            padded = (size + 511) // 512 * 512
            total += padded
            require(total <= MAX_TOTAL, 'Tar expanded-size limit exceeded')
            while padded:
                chunk = stream.read(min(padded, 1024 * 1024))
                require(chunk, 'Truncated tar payload')
                padded -= len(chunk)


def verify(archive, expected_commit, manifest_path=None, sums_path=None):
    require(bool(re.fullmatch(r'[0-9a-f]{40}', expected_commit)), 'Expected full lowercase source commit SHA')
    archive = Path(archive)
    require(archive.is_file() and archive.stat().st_size <= MAX_ARCHIVE, 'Missing or oversized archive')
    extension = '.zip' if archive.name.endswith('.zip') else '.tar.gz'
    require(archive.name.endswith(extension), 'Expected inner .zip or .tar.gz bundle, not the outer Actions ZIP')
    stem = archive.name[:-len(extension)]
    manifest_path = Path(manifest_path) if manifest_path else archive.with_name(stem + '.manifest.json')
    sums_path = Path(sums_path) if sums_path else archive.with_name('SHA256SUMS')
    checksums = {}
    for line in read_limited(sums_path, MAX_METADATA).decode('utf-8').splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  ([^/\\]+)', line)
        require(match is not None, 'Invalid SHA256SUMS line')
        digest, name = match.groups()
        safe_name(name)
        require(name not in checksums, 'Duplicate checksum filename')
        checksums[name] = digest
    raw_manifest = read_limited(manifest_path, MAX_METADATA)
    for path in (archive, manifest_path):
        require(path.name in checksums and packager.sha_file(path) == checksums[path.name], f'Checksum mismatch: {path.name}')
    manifest = json.loads(raw_manifest, object_pairs_hook=unique_object)
    root, system = validate_manifest(manifest, expected_commit)
    require(stem == root and extension == ('.zip' if system == 'windows' else '.tar.gz'), 'Archive filename/platform mismatch')
    require(manifest_path.name == root + '.manifest.json', 'Sidecar filename mismatch')
    require(set(checksums) == {archive.name, manifest_path.name}, 'Unexpected checksum entries')
    expected = {root + '/' + item['path']: item for item in manifest['files']}
    seen = set()
    count, total = 0, 0
    embedded = None

    def member(name, kind, mode, size, stream=None, target=''):
        nonlocal count, total, embedded
        if name.endswith('/') and kind == 'directory':
            name = name[:-1]
        safe_name(name)
        require(name.startswith(root + '/') and name not in seen, 'Escaping or duplicate archive member')
        seen.add(name)
        count += 1
        total += size
        require(count <= MAX_MEMBERS and 0 <= size <= MAX_FILE and total <= MAX_TOTAL, 'Archive resource limit exceeded')
        if name == root + '/manifest.json':
            require(kind == 'file' and size <= MAX_METADATA, 'Invalid embedded manifest')
            embedded = stream.read(MAX_METADATA + 1)
            require(embedded == raw_manifest, 'Embedded manifest differs from sidecar')
            return
        require(name in expected, f'Unexpected archive member: {name}')
        item = expected[name]
        require(kind == item['type'], f'Archive member type mismatch: {name}')
        if extension != '.zip':
            require(stat.S_IMODE(mode) == int(item['mode'], 8), f'Archive mode mismatch: {name}')
        if kind == 'file':
            require(size == item['size'], f'Archive size mismatch: {name}')
            digest, actual = hashlib.sha256(), 0
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                actual += len(chunk)
                require(actual <= size, 'Member exceeds declared size')
                digest.update(chunk)
            require(actual == size and digest.hexdigest() == item['sha256'], f'Archive content mismatch: {name}')
        else:
            require(size == 0, 'Non-file member contains payload')
            if kind == 'symlink':
                require(target == item['target'], 'Archive symlink target mismatch')

    if extension == '.zip':
        # Reject oversized central directories before ZipFile materializes them.
        # Native CI bundles fit classic ZIP; ZIP64/multi-disk are not supported.
        with archive.open('rb') as stream:
            stream.seek(max(0, archive.stat().st_size - 65577))
            tail = stream.read(65577)
        index = tail.rfind(b'PK\x05\x06')
        require(index >= 0 and len(tail) - index >= 22, 'Missing ZIP end record')
        require(index < 20 or tail[index - 20:index - 16] != b'PK\x06\x07', 'ZIP64 is unsupported')
        end = struct.unpack('<4s4H2IH', tail[index:index + 22])
        require(index + 22 + end[7] == len(tail) and end[1] == end[2] == 0
                and end[3] == end[4] and end[4] <= MAX_MEMBERS
                and end[5] <= MAX_METADATA and end[6] != 0xffffffff,
                'Unsupported or oversized ZIP directory')
        with zipfile.ZipFile(archive) as bundle:
            require(len(bundle.infolist()) <= MAX_MEMBERS, 'Too many ZIP members')
            for entry in bundle.infolist():
                mode = entry.external_attr >> 16
                require(stat.S_IFMT(mode) in (0, stat.S_IFREG, stat.S_IFDIR), 'Unsupported ZIP member type')
                require(stat.S_IFMT(mode) != stat.S_IFDIR or entry.is_dir(), 'ZIP directory attributes disagree')
                require(not entry.is_dir() or stat.S_IFMT(mode) != stat.S_IFREG, 'ZIP file attributes disagree')
                require(not entry.flag_bits & 1, 'Encrypted ZIP members are unsupported')
                if entry.is_dir():
                    member(entry.filename, 'directory', mode, entry.file_size)
                else:
                    with bundle.open(entry) as stream:
                        member(entry.filename, 'file', mode, entry.file_size, stream)
    else:
        preflight_tar(archive)
        with tarfile.open(archive, 'r|gz') as bundle:
            for entry in bundle:
                require(entry.isfile() or entry.isdir() or entry.issym(), 'Unsupported tar member type')
                kind = 'file' if entry.isfile() else 'directory' if entry.isdir() else 'symlink'
                stream = bundle.extractfile(entry) if entry.isfile() else None
                try:
                    member(entry.name, kind, entry.mode, entry.size, stream, entry.linkname)
                finally:
                    if stream is not None:
                        stream.close()
    require(embedded is not None and seen == set(expected) | {root + '/manifest.json'}, 'Missing archive members')
    return {'result': 'integrity and declared source consistency verified', 'commit': expected_commit, 'platform': system,
            'architecture': manifest['architecture'], 'files': len(manifest['files']),
            'qualification': 'Not authenticity, proof of binary source, signing, CI pass, or DAW qualification.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--checksums', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.archive, args.expected_commit, args.manifest, args.checksums), indent=2))
        return 0
    except (ValueError, OSError, KeyError, TypeError, tarfile.TarError, zipfile.BadZipFile, RuntimeError, EOFError, zlib.error) as error:
        print(f'Bundle verification failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
