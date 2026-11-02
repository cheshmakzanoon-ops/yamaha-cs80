#!/usr/bin/env python3
"""Independently verify installer-extracted documents against a clean checkout.

This verifier intentionally does not import the staging/packaging implementation.
A self-consistent manifest is insufficient: notice bytes must match the actual
source inputs. This is not a binary/source, numerical, signing or DAW certificate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys

REPOSITORY = 'https://github.com/cheshmakzanoon-ops/yamaha-cs80'
INPUTS = {
    'LICENSE': 'LICENSE',
    'licenses/MTS-ESP-LICENSE.txt': 'deps/mts-esp/LICENSE',
    'licenses/BarlowCondensed-OFL.txt': 'assets/fonts/barlow-condensed/OFL.txt',
    'licenses/BarlowCondensed-SOURCE.txt': 'assets/fonts/barlow-condensed/SOURCE.txt',
    'INSTALL_AND_QUICKSTART.md': 'docs/INSTALL_AND_QUICKSTART.md',
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def regular_bytes(path: Path, root: Path) -> bytes:
    require(path.is_file(), f'Missing extracted document or source notice: {path}')
    require(path.is_relative_to(root), f'Document path escapes payload: {path}')
    cursor = root
    require(not cursor.is_symlink(), f'Linked document payload: {cursor}')
    for part in path.relative_to(root).parts:
        cursor /= part
        require(not cursor.is_symlink(), f'Linked document payload: {cursor}')
    content = path.read_bytes()
    require(bool(content), f'Empty document or source notice: {path}')
    return content


def verify_documents(root: Path, juce_license: Path, documents: Path, expected_source: str) -> dict:
    # Resolve only the checkout; resolving the extracted tree would hide symlinks.
    root = root.resolve()
    documents = documents.absolute()
    require(re.fullmatch('[0-9a-f]{40}', expected_source) is not None,
            'Expected source must be a full lowercase commit SHA')

    def git(*args):
        return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()

    require(Path(git('rev-parse', '--show-toplevel')).resolve() == root,
            'Expected source root is not the checkout root')
    require(git('rev-parse', 'HEAD') == expected_source, 'Expected source commit differs from checkout')
    require(subprocess.run(['git', '-C', str(root), 'diff', '--quiet', 'HEAD', '--'],
                           check=False).returncode == 0, 'Source checkout is dirty')
    require(subprocess.run(['git', '-C', str(root), 'diff', '--cached', '--quiet', 'HEAD', '--'],
                           check=False).returncode == 0, 'Source index differs from expected commit')
    tree = git('rev-parse', 'HEAD^{tree}')
    expected = {}
    for output, relative in INPUTS.items():
        git('ls-files', '--error-unmatch', '--', relative)
        actual_source = regular_bytes(root / relative, root)
        # Independent immutable-object comparison: do not import the stager or
        # let a matching extracted payload certify a hidden source replacement.
        tree_record = subprocess.check_output(
            ['git', '-C', str(root), 'ls-tree', '-z', expected_source, '--', relative])
        records = tree_record.split(b'\0')
        require(len(records) == 2 and records[-1] == b'' and b'\t' in records[0],
                'Required source notice is not registered in expected commit: ' + relative)
        metadata, name = records[0].split(b'\t', 1)
        mode, kind, blob = metadata.decode('ascii').split()
        require(name == relative.encode('utf-8') and kind == 'blob' and mode in ('100644', '100755'),
                'Unsupported source notice type: ' + relative)
        physical_mode = (root / relative).lstat().st_mode
        require(stat.S_ISREG(physical_mode), 'Source notice is not a regular file: ' + relative)
        if os.name != 'nt':
            require(bool(physical_mode & 0o111) == (mode == '100755'),
                    'Source notice executable mode differs: ' + relative)
        raw_blob = subprocess.check_output(['git', '-C', str(root), 'cat-file', 'blob', blob])
        expected_source_bytes = raw_blob
        if actual_source != raw_blob:
            setting = subprocess.run(['git', '-C', str(root), 'config', '--get', 'core.autocrlf'],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            require(sys.platform == 'win32' and setting.returncode == 0
                    and setting.stdout.strip().lower() == b'true'
                    and b'\r' not in raw_blob and b'\0' not in raw_blob,
                    'Unsupported source notice checkout conversion: ' + relative)
            expected_source_bytes = raw_blob.replace(b'\n', b'\r\n')
        require(actual_source == expected_source_bytes,
                'Source notice bytes differ from immutable Git blob: ' + relative)
        expected[output] = expected_source_bytes
    juce_license = juce_license.absolute()
    expected['licenses/JUCE-LICENSE.md'] = regular_bytes(juce_license, juce_license.parent)
    source_url = f'{REPOSITORY}/tree/{expected_source}'
    expected['SOURCE.txt'] = (
        f'Packaging source checkout: {expected_source}\n'
        f'{source_url}\n'
        'Use the complete source and build instructions at this exact revision.\n'
        'This package is not a source distribution.\n'
        'Checkout provenance does not independently establish binary/source equivalence,\n'
        'numerical acceptance, hardware qualification, signing or real-DAW acceptance.\n'
    ).encode('utf-8')

    manifest_path = documents / 'distribution-manifest.json'
    manifest_bytes = regular_bytes(manifest_path, documents)
    manifest = json.loads(manifest_bytes)
    require(type(manifest.get('schema_version')) is int and manifest['schema_version'] == 1,
            'Unsupported document manifest schema')
    require(manifest.get('product') == 'CS-80 Reference', 'Unexpected document product')
    require(manifest.get('source_commit') == expected_source, 'Document source commit mismatch')
    require(manifest.get('source_tree') == tree, 'Document source tree mismatch')
    require(manifest.get('source_url') == source_url, 'Document source URL mismatch')
    require(manifest.get('scope') == 'Clean packaging checkout identity and distribution-document bytes only'
            and manifest.get('qualification') == 'Not a binary/source-equivalence, numerical, hardware, signing or DAW certificate',
            'Document scope must not imply binary or release qualification')
    require(manifest.get('source_notice_policy') ==
            'Raw Git blobs; exact native-Windows core.autocrlf=true LF-to-CRLF conversion only',
            'Document source notice policy is absent or different')
    inventory = manifest.get('files')
    require(isinstance(inventory, list) and len(inventory) == len(expected),
            'Document inventory is incomplete or duplicated')
    names = [row.get('path') for row in inventory if isinstance(row, dict)]
    require(len(names) == len(expected) and len(set(names)) == len(names) and set(names) == set(expected),
            'Document inventory contains duplicate, missing or unsafe paths')
    observed = {path.relative_to(documents).as_posix()
                for path in documents.rglob('*') if path.is_file() or path.is_symlink()}
    allowed = set(expected) | {'distribution-manifest.json', 'copyright'}
    require(set(expected) | {'distribution-manifest.json'} <= observed and observed <= allowed,
            'Missing or unexpected extracted document payload inventory')
    for row in inventory:
        name = row['path']
        actual = regular_bytes(documents / name, documents)
        require(actual == expected[name], f'Extracted notice differs from exact source: {name}')
        require(type(row.get('size')) is int and row['size'] == len(actual), f'Document size mismatch: {name}')
        require(row.get('sha256') == hashlib.sha256(actual).hexdigest(), f'Document hash mismatch: {name}')
    copyright_path = documents / 'copyright'
    if copyright_path.exists() or copyright_path.is_symlink():
        require(regular_bytes(copyright_path, documents) == expected['LICENSE'],
                'Debian copyright notice differs from project licence')
    return {
        'schema_version': 1,
        'source_commit': expected_source,
        'source_tree': tree,
        'document_count': len(expected),
        'manifest_sha256': hashlib.sha256(manifest_bytes).hexdigest(),
        'files': [{'path': name, 'sha256': hashlib.sha256(content).hexdigest(), 'size': len(content)}
                  for name, content in sorted(expected.items())],
        'scope': 'Independently verified extracted installer documents against the exact checkout',
        'qualification': 'Not a binary/source-equivalence, numerical, signing, hardware or DAW certificate',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--juce-license', type=Path, required=True)
    parser.add_argument('--documents', type=Path, required=True)
    parser.add_argument('--source-sha', required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = verify_documents(args.root, args.juce_license, args.documents, args.source_sha)
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        with args.receipt.open('x', encoding='utf-8') as stream:
            json.dump(result, stream, indent=2)
            stream.write('\n')
        print(json.dumps({'document_verification': 'PASS', 'source_commit': result['source_commit'],
                          'document_count': result['document_count'], 'receipt': str(args.receipt)}))
        return 0
    except (ValueError, OSError, subprocess.SubprocessError, TypeError, KeyError) as error:
        print(f'Extracted distribution verification failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
