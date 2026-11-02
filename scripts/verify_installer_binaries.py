#!/usr/bin/env python3
"""Compare installer payload with frozen, independently re-read build inputs.

Inputs must already include any intentional signing/stripping. This proves only
package transport identity, not the compiler/source relationship or valid signing.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import stat
import xml.etree.ElementTree as ET

PRODUCT = 'CS-80 Reference'
LAYOUT = {
    'linux': {
        'vst3': ('VST3/' + PRODUCT + '.vst3', 'directory', 'usr/lib/vst3/' + PRODUCT + '.vst3', None),
        'standalone': ('Standalone/' + PRODUCT, 'file', 'usr/bin/cs80-reference', None),
    },
    'macos': {
        'vst3': ('VST3/' + PRODUCT + '.vst3', 'directory', '/Library/Audio/Plug-Ins/VST3', 'vst3'),
        'au': ('AU/' + PRODUCT + '.component', 'directory', '/Library/Audio/Plug-Ins/Components', 'au'),
        'standalone': ('Standalone/' + PRODUCT + '.app', 'directory', '/Applications', 'app'),
    },
    'windows': {
        'standalone': ('Standalone/' + PRODUCT + '.exe', 'file', PRODUCT + '.exe', None),
    },
}
QUALIFICATION = ('Installer payload identity only; not compilation/source equivalence, '
                 'signing validity, numerical acceptance or DAW qualification')
PRIMARY_BINARY = {
    'linux': {'vst3': 'Contents/x86_64-linux/' + PRODUCT + '.so', 'standalone': '.'},
    'macos': {key: 'Contents/MacOS/' + PRODUCT for key in ('vst3', 'au', 'standalone')},
    'windows': {'standalone': '.'},
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest_json(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def independent_root(root):
    root = Path(root)
    require(root.is_dir() and not root.is_symlink()
            and not getattr(root, 'is_junction', lambda: False)(), 'Missing or linked payload root')
    return root.resolve()


def component_path(root, relative):
    require(not Path(relative).is_absolute() and '..' not in Path(relative).parts, 'Unsafe component path')
    current = root
    for part in Path(relative).parts:
        current /= part
        require(not current.is_symlink() and not getattr(current, 'is_junction', lambda: False)(),
                'Linked component path is not an independently verified payload')
    return current


def file_digest(path):
    before = path.stat()
    hashed, count = hashlib.sha256(), 0
    with path.open('rb') as stream:
        while chunk := stream.read(1024 * 1024):
            hashed.update(chunk)
            count += len(chunk)
    after = path.stat()
    require((before.st_size, before.st_mode, before.st_mtime_ns, before.st_ctime_ns) ==
            (after.st_size, after.st_mode, after.st_mtime_ns, after.st_ctime_ns)
            and count == before.st_size, 'Payload file changed while hashing: ' + str(path))
    return count, hashed.hexdigest()


def inventory(root, platform, expected_kind):
    require(root.exists() and not root.is_symlink(), 'missing or linked component: ' + str(root))
    require((root.is_dir() if expected_kind == 'directory' else root.is_file()),
            'Component type differs from declared build input')
    records = []
    boundary = root.resolve() if root.is_dir() else root.parent.resolve()

    def visit(path, relative):
        value = path.lstat()
        require(not getattr(path, 'is_junction', lambda: False)(), 'Unsupported junction in binary payload')
        if stat.S_ISLNK(value.st_mode):
            target = os.readlink(path)
            require(not Path(target).is_absolute() and path.exists()
                    and path.resolve().is_relative_to(boundary), 'Unsafe external or dangling payload link')
            records.append({'path': relative, 'type': 'symlink', 'target': target})
        elif stat.S_ISDIR(value.st_mode):
            require(not value.st_mode & (stat.S_ISUID | stat.S_ISGID), 'Privileged payload directory mode')
            records.append({'path': relative, 'type': 'directory',
                            'mode': None if platform == 'windows' else f'{stat.S_IMODE(value.st_mode):04o}'})
            for child in sorted(path.iterdir()):
                visit(child, child.relative_to(root).as_posix())
        else:
            require(stat.S_ISREG(value.st_mode), 'Special-file payload type is forbidden')
            require(not value.st_mode & (stat.S_ISUID | stat.S_ISGID), 'Privileged binary payload mode')
            count, sha = file_digest(path)
            records.append({'path': relative, 'type': 'file', 'bytes': count, 'sha256': sha,
                            'mode': None if platform == 'windows' else f'{stat.S_IMODE(value.st_mode):04o}'})

    visit(root, '.')
    require(any(record['type'] == 'file' for record in records), 'Empty binary component inventory')
    return sorted(records, key=lambda row: row['path'])


def capture_build_payload(platform, build):
    require(platform in LAYOUT, 'Unknown installer platform')
    artifacts = component_path(independent_root(build), 'CS80Reference_artefacts/Release')
    components, transformations = {}, []
    for key, (relative, kind, destination, identifier) in LAYOUT[platform].items():
        path = component_path(artifacts, relative)
        primary = component_path(path, PRIMARY_BINARY[platform][key])
        require(primary.is_file() and not primary.is_symlink() and primary.stat().st_size > 0,
                'Missing or empty primary build binary: ' + key)
        entries = inventory(path, platform, kind)
        if platform == 'linux' and key == 'standalone':
            source_mode = entries[0]['mode']
            entries[0]['mode'] = '0755'
            transformations.append({'component': key, 'kind': 'POSIX mode normalization',
                                    'input_mode': source_mode, 'installed_mode': '0755',
                                    'reason': 'Registered make-deb.sh install -m0755; bytes are unchanged'})
        components[key] = {'input_relative': relative, 'kind': kind, 'install_destination': destination,
                           'package_identifier': identifier, 'entries': entries}
    return {'schema_version': 1, 'platform': platform, 'components': components,
            'capture_stage': 'Existing post-signing/stripping build inputs, frozen before installer creation',
            'transformations': transformations, 'qualification': QUALIFICATION}


def mac_components(expanded):
    found = {}
    allowed = {'com.referenceinstruments.cs80.' + suffix for suffix in ('vst3', 'au', 'app', 'docs')}
    for metadata in expanded.rglob('PackageInfo'):
        component_path(expanded, metadata.relative_to(expanded))
        require(metadata.is_file(), 'Missing native component PackageInfo')
        try:
            info = ET.fromstring(metadata.read_bytes())
        except ET.ParseError as error:
            raise ValueError('Malformed native PackageInfo: ' + str(metadata)) from error
        identifier = info.get('identifier')
        require(identifier in allowed and identifier not in found, 'Unexpected or duplicate native component inventory')
        found[identifier] = (metadata.parent, info.get('install-location'))
    return found


def verify_installed_payload(platform, captured, build, expanded):
    require(platform in LAYOUT and captured.get('platform') == platform, 'Installer platform identity mismatch')
    # Do not trust even a self-consistent supplied manifest: independently reread
    # the actual inputs and reject signing/stripping or other changes after capture.
    current = capture_build_payload(platform, build)
    require(captured == current, 'Actual build input bytes/inventory or capture contract changed after snapshot')
    expanded = independent_root(expanded)
    native_components = mac_components(expanded) if platform == 'macos' else {}
    verified = {}
    for key, expected in captured['components'].items():
        if platform == 'macos':
            identifier = 'com.referenceinstruments.cs80.' + expected['package_identifier']
            require(identifier in native_components, 'Missing native binary component: ' + key)
            directory, destination = native_components[identifier]
            require(destination == expected['install_destination'], 'Wrong native component install-location: ' + key)
            name = Path(expected['input_relative']).name
            matches = list(directory.rglob(name))
            require(len(matches) == 1, 'Missing or duplicate expanded component bundle: ' + key)
            actual = component_path(expanded, matches[0].relative_to(expanded))
        else:
            actual = component_path(expanded, expected['install_destination'])
        observed = inventory(actual, platform, expected['kind'])
        wanted = {entry['path']: entry for entry in expected['entries']}
        present = {entry['path']: entry for entry in observed}
        require(set(wanted) == set(present), 'Binary payload inventory mismatch: ' + key)
        for path, row in wanted.items():
            value = present[path]
            require(value['type'] == row['type'], 'Binary payload type mismatch: ' + key + '/' + path)
            if row['type'] == 'file':
                require(value['bytes'] == row['bytes'] and value['sha256'] == row['sha256'],
                        'Binary payload bytes/hash mismatch: ' + key + '/' + path)
            elif row['type'] == 'symlink':
                require(value['target'] == row['target'], 'Binary payload link mismatch: ' + key + '/' + path)
            if 'mode' in row:
                require(value['mode'] == row['mode'], 'Binary payload mode mismatch: ' + key + '/' + path)
        verified[key] = {'entries': len(observed), 'payload_inventory_sha256': digest_json(observed),
                         'install_destination': expected['install_destination']}
    return {'schema_version': 1, 'platform': platform, 'verified_components': sorted(verified),
            'components': verified, 'input_manifest_sha256': digest_json(captured),
            'platform_limit': ('Windows Standalone only; VST3 not installed; NTFS ACLs not qualified'
                               if platform == 'windows' else
                               'POSIX permission bits and bytes checked; ownership, extended attributes and signing validity not qualified'),
            'qualification': QUALIFICATION}
