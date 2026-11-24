#!/usr/bin/env python3
"""A48 successor of A30: supplement A41 with exact-source guides and notices."""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import tempfile
from urllib.parse import urlsplit
import zipfile

sys.dont_write_bytecode = True
GUIDES = {
    'docs/INSTALL_AND_QUICKSTART.md': 'INSTALL_AND_QUICKSTART.md',
    'docs/PRESET_GUIDE.md': 'PRESET_GUIDE.md',
    'docs/CI_DISTRIBUTION_STATUS.md': 'DISTRIBUTION_STATUS.md',
}
NOTICES = {
    'LICENSE': 'LICENSE',
    'assets/fonts/barlow-condensed/OFL.txt': 'licenses/BarlowCondensed-OFL.txt',
    'assets/fonts/barlow-condensed/SOURCE.txt': 'licenses/BarlowCondensed-SOURCE.txt',
    'src/vendor/mts-esp/LICENSE': 'licenses/MTS-ESP-LICENSE.txt',
    'src/vendor/libxml2/Copyright': 'licenses/libxml2-Copyright.txt',
    'src/vendor/libxml2/SOURCE.txt': 'licenses/libxml2-SOURCE.txt',
    'src/vendor/libxml2/ADDITIONAL-NOTICES.txt': 'licenses/libxml2-ADDITIONAL-NOTICES.txt',
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def inline_targets(text):
    """Inspect actual inline targets outside fenced examples."""
    pattern = re.compile(r'!?\[[^\]\n]+\]\(([^\s()]+)(?:\s+"[^"]*")?\)')
    fence = ''
    targets = []
    for line in text.splitlines():
        marker = re.match(r'^ {0,3}(`{3,}|~{3,})', line)
        if marker:
            token = marker[1]
            if not fence:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = ''
        elif not fence:
            targets.extend(pattern.findall(line))
    return targets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--acceptance', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    receipt_raw, outer_raw = args.acceptance.read_bytes(), args.archive.read_bytes()
    receipt = json.loads(receipt_raw)
    require(receipt['status'] == 'artifact_bytes_verified' and receipt['source'] == args.source,
            'Expected a passing same-source A41 byte inspection')
    require(len(outer_raw) == receipt['outer_zip']['bytes']
            and sha(outer_raw) == receipt['outer_zip']['sha256'], 'Archive differs from A41 receipt')
    inventory = {item['path']: item for item in receipt['inventory']}
    required = list(GUIDES.values()) + list(NOTICES.values()) + [
        'licenses/JUCE-LICENSE.md', 'SOURCE.txt', 'evidence/juce-provenance.txt']
    require(all(inventory.get(name, {}).get('type') == 'file'
                and inventory[name]['mode'] == '0o644' and inventory[name]['size'] > 0
                for name in required), 'Missing/nonregular/empty metadata or changed metadata mode')
    inner_name = receipt['inner_archive']['file']
    with zipfile.ZipFile(io.BytesIO(outer_raw)) as outer:
        inner_raw = outer.read(inner_name)
    require(len(inner_raw) == receipt['inner_archive']['bytes']
            and sha(inner_raw) == receipt['inner_archive']['sha256'], 'Inner archive differs from A41 receipt')
    root = inner_name.removesuffix('.tar.gz').removesuffix('.zip')
    if inner_name.endswith('.zip'):
        with zipfile.ZipFile(io.BytesIO(inner_raw)) as inner:
            payloads = {name: inner.read(root + '/' + name) for name in required}
    else:
        with tarfile.open(fileobj=io.BytesIO(inner_raw), mode='r:gz') as inner:
            payloads = {}
            for name in required:
                member = inner.getmember(root + '/' + name)
                require(member.isfile(), 'Metadata payload is not a regular file')
                with inner.extractfile(member) as stream:
                    payloads[name] = stream.read()
    for name, raw in payloads.items():
        require(len(raw) == inventory[name]['size'] and sha(raw) == inventory[name]['sha256'],
                'Metadata differs from verified inventory: ' + name)
    source_records, comparisons = {}, []

    def source_blob(relative):
        raw = subprocess.check_output(['git', '-C', str(args.repo), 'show', args.source + ':' + relative])
        source_records[relative] = {'bytes': len(raw), 'sha256': sha(raw)}
        return raw

    with tempfile.TemporaryDirectory(prefix='a48-source-docs-') as directory:
        snapshot = Path(directory)
        script = snapshot / 'scripts/package-ci-artifacts.py'
        script.parent.mkdir()
        script.write_bytes(source_blob('scripts/package-ci-artifacts.py'))
        spec = importlib.util.spec_from_file_location('a48_pinned_packager', script)
        packager = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(packager)
        for source, destination in GUIDES.items():
            source_raw = source_blob(source)
            path = snapshot / source
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(source_raw)
            expected = packager.packaged_markdown(source, args.source, GUIDES)
            require(payloads[destination] == expected, 'Guide differs from source-specific rewrite: ' + destination)
            targets = inline_targets(expected.decode())
            local = [target for target in targets
                     if not urlsplit(target).scheme and not urlsplit(target).netloc and urlsplit(target).path]
            require(all(urlsplit(target).path in GUIDES.values() for target in local),
                    'Unbundled relative repository link remains: ' + destination)
            source_prefix = 'https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/' + args.source + '/'
            comparisons.append({'source': source, 'packaged': destination, 'result': 'PASS',
                                'comparison': 'exact bytes from packaged_markdown at requested source',
                                'bytes': len(expected), 'sha256': sha(expected),
                                'inline_link_targets': len(targets), 'local_bundled_guide_targets': local,
                                'source_pinned_link_targets': [target for target in targets if target.startswith(source_prefix)],
                                'other_external_or_fragment_targets': [target for target in targets
                                    if target not in local and not target.startswith(source_prefix)]})
        for source, destination in NOTICES.items():
            expected = source_blob(source)
            require(payloads[destination] == expected, 'Notice differs from source: ' + destination)
            comparisons.append({'source': source, 'packaged': destination, 'result': 'PASS',
                                'comparison': 'verbatim source bytes', 'bytes': len(expected), 'sha256': sha(expected)})
    result = {
        'status': 'source_specific_docs_and_notices_verified', 'release_qualified': False,
        'source': args.source, 'revision': receipt['preset_library_revision'], 'target': receipt['target'],
        'artifact_id': receipt['artifact']['id'], 'outer_bytes': len(outer_raw), 'outer_sha256': sha(outer_raw),
        'acceptance_receipt': {'file': args.acceptance.name, 'bytes': len(receipt_raw), 'sha256': sha(receipt_raw)},
        'inventory_counts': dict(Counter(item['type'] for item in receipt['inventory'])),
        'symlink_count': sum(item['type'] == 'symlink' for item in receipt['inventory']),
        'required_metadata': [inventory[name] for name in required],
        'source_comparisons': comparisons, 'source_files': source_records,
        'juce_license': {'path': 'licenses/JUCE-LICENSE.md', 'bytes': len(payloads['licenses/JUCE-LICENSE.md']),
                         'sha256': sha(payloads['licenses/JUCE-LICENSE.md']),
                         'independent_dependency_source_byte_comparison': False,
                         'provenance_boundary': 'Pinned packager reads configured clean JUCE source LICENSE.md. Original source verification and unchanged build/package sequence are reviewed by A48; this supplement proves presence and inventoried bytes only.'},
        'source_time_guides': 'Exact requested-source pending qualification guides, including historical .4 downloads, are retained. Existing external URLs are intentionally unchanged. This does not publish final online guides or qualify a download.',
        'generic_helper_boundary': 'A41 verifies complete declared inventory but does not require guides/notices or compare their content to source. This supplement adds those requirements for the checked actual bytes.',
        'current_external_validator_requirement': {'total': 15, 'linux': 3, 'windows': 2, 'macos_arm64': 5, 'macos_x86_64': 5,
            'note': 'A41 historical remaining_gates prose says 13; the current source workflow, not that prose, defines the .7 gates.'},
        'script_sha256': sha(Path(__file__).read_bytes()),
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'status': result['status'], 'target': result['target'],
                      'metadata_files': len(required), 'exact_source_comparisons': len(comparisons),
                      'out': str(args.out)}))


if __name__ == '__main__':
    main()
