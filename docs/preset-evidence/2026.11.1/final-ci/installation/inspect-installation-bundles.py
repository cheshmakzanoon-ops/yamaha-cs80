#!/usr/bin/env python3
"""Inspect existing final artifact-cache ZIPs without extraction or execution.

The public byte/API gate and native runtime gates remain separate qualifications.
This inspector rechecks outer API hashes and every inner inventoried byte while
reviewing the install layout, source-pinned guides, licenses and bundle metadata.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import plistlib
import re
import stat
import subprocess
import tarfile
import tempfile
from urllib.parse import unquote, urlsplit
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(__file__).resolve().parent
SOURCE = '6d3cc220edef5a4a2c7cd968ea8a786c7c83198c'
RUN = 45044040694
REVISION = '2026.11.1'
REPO = 'cheshmakzanoon-ops/yamaha-cs80'
JUCE = 'c352e2489000ffeb9e397051377d75eef2efe641'
PRODUCT = 'CS-80 Reference'
ARTIFACTS = {
    'cs80-windows-install-bundle': ('windows', 'x86_64'),
    'cs80-macos-15-install-bundle': ('macos', 'arm64'),
    'cs80-macos-15-intel-install-bundle': ('macos', 'x86_64'),
    'cs80-linux-install-bundle': ('linux', 'x86_64'),
}
GUIDES = {
    'docs/INSTALL_AND_QUICKSTART.md': 'INSTALL_AND_QUICKSTART.md',
    'docs/PRESET_GUIDE.md': 'PRESET_GUIDE.md',
    'docs/CI_DISTRIBUTION_STATUS.md': 'DISTRIBUTION_STATUS.md',
}
NOTICES = {
    'LICENSE': 'LICENSE',
    'licenses/BarlowCondensed-OFL.txt': 'assets/fonts/barlow-condensed/OFL.txt',
    'licenses/BarlowCondensed-SOURCE.txt': 'assets/fonts/barlow-condensed/SOURCE.txt',
    'licenses/MTS-ESP-LICENSE.txt': 'src/vendor/mts-esp/LICENSE',
    'licenses/libxml2-Copyright.txt': 'src/vendor/libxml2/Copyright',
    'licenses/libxml2-SOURCE.txt': 'src/vendor/libxml2/SOURCE.txt',
    'licenses/libxml2-ADDITIONAL-NOTICES.txt': 'src/vendor/libxml2/ADDITIONAL-NOTICES.txt',
}
MAX = 512 * 1024 * 1024
SMALL = 2 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def hash_stream(stream, keep=False):
    h, n, parts = hashlib.sha256(), 0, []
    while chunk := stream.read(1024 * 1024):
        n += len(chunk)
        require(n <= MAX and (not keep or n <= SMALL), 'Oversized member')
        h.update(chunk)
        if keep:
            parts.append(chunk)
    return h.hexdigest(), n, b''.join(parts) if keep else None


def git_bytes(path, source=SOURCE, root=ROOT):
    return subprocess.check_output(['git', '-C', str(root), 'show', source + ':' + path])


def import_trusted(name, path):
    require(path.read_bytes() == git_bytes(str(path.relative_to(ROOT))), 'Trusted helper differs from fixed source')
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def expected_documents(packager):
    # Only three small trusted source files are staged to reproduce the packager
    # transformation. No artifact members are extracted.
    with tempfile.TemporaryDirectory(prefix='source-guides-', dir=WORK) as temporary:
        source_root = Path(temporary)
        for path in GUIDES:
            output = source_root / path
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(git_bytes(path))
        previous = packager.ROOT
        try:
            packager.ROOT = source_root
            return {target: packager.packaged_markdown(path, SOURCE, GUIDES) for path, target in GUIDES.items()}
        finally:
            packager.ROOT = previous


def source_review(documents):
    install = re.sub(r'\s+', ' ', documents['INSTALL_AND_QUICKSTART.md'].decode())
    presets = re.sub(r'\s+', ' ', documents['PRESET_GUIDE.md'].decode())
    status = re.sub(r'\s+', ' ', documents['DISTRIBUTION_STATUS.md'].decode())
    required_install = [
        '2026.11.1', 'host process architecture', 'not universal binaries',
        '~/.vst3/CS-80 Reference.vst3', '~/Library/Audio/Plug-Ins/VST3/CS-80 Reference.vst3',
        '~/Library/Audio/Plug-Ins/Components/CS-80 Reference.component',
        '%LOCALAPPDATA%\\Programs\\Common\\VST3\\CS-80 Reference.vst3',
        '%COMMONPROGRAMFILES%\\VST3', 'Quit the DAW before copying',
        'Existing sessions and user presets restore', 'choose **Circuit**',
        'project version string alone is not a build identifier',
        '67 samples', '1.40 ms at 48 kHz', 'not a static/AppImage distribution',
    ]
    for phrase in required_install:
        require(phrase in install, 'Missing installation guidance: ' + phrase)
    for phrase in ('2026.11 and 2026.11.1 identify sound-data revisions',
                   'application\'s version', 'serialized sound',
                   'Pressure Lead is intentionally silent without pressure',
                   'Pure CS-80 disables these additions'):
        require(phrase in presets, 'Missing preset guidance: ' + phrase)
    for phrase in ('licenses/JUCE-LICENSE.md', 'SOURCE.txt',
                   'not complete source distributions', 'No Windows'):
        require(phrase in status, 'Missing distribution qualification: ' + phrase)
    project = git_bytes('CMakeLists.txt').decode()
    version = re.search(r'project\(CS80Reference VERSION ([0-9.]+) ', project).group(1)
    return {'application_version': version, 'preset_revision': REVISION,
            'required_installation_phrases': len(required_install),
            'guide_hashes': {name: sha(data) for name, data in documents.items()},
            'nonblocking_clarification': 'Frozen packaged guides predate the queue-only VST3 Program diagnosis. Current public README/report qualify that path; release notes should direct users to Library and link that current qualification.'}


def slug(text):
    text = re.sub(r'[`*_]', '', text).lower()
    return re.sub(r'[^\w -]', '', text).replace(' ', '-')


def check_links(documents, member_names):
    rows = []
    pattern = r'(?<!!)\[[^\]\n]+\]\(([^\s()]+)(?:\s+"[^\"]*")?\)'
    for name, data in documents.items():
        content = data.decode()
        for target in re.findall(pattern, content):
            url = urlsplit(target)
            if url.scheme or url.netloc:
                if url.netloc == 'github.com' and url.path.startswith('/' + REPO + '/'):
                    prefix = '/' + REPO + '/blob/' + SOURCE + '/'
                    require(url.path.startswith(prefix), 'Guide repository URL is not source-pinned: ' + target)
                    path = unquote(url.path[len(prefix):])
                    git_bytes(path)
                    rows.append({'guide': name, 'kind': 'source-pinned-repository', 'target': target})
                else:
                    rows.append({'guide': name, 'kind': 'external', 'target': target})
            else:
                path = unquote(url.path) if url.path else name
                require(path in member_names and path in documents, 'Missing local guide target: ' + target)
                if url.fragment:
                    headings = {slug(m.group(1)) for m in re.finditer(r'^#{1,6} (.+)$', documents[path].decode(), re.M)}
                    require(unquote(url.fragment) in headings, 'Missing local guide anchor: ' + target)
                rows.append({'guide': name, 'kind': 'local', 'target': target})
    return rows


def inspect_outer(meta, cache, verifier, expected_docs, app_version):
    name = meta['name']
    system, arch = ARTIFACTS[name]
    require(meta['workflow_run']['id'] == RUN and meta['workflow_run']['head_sha'] == SOURCE
            and meta['workflow_run']['head_branch'] == 'main' and not meta['expired'], 'Wrong or expired artifact')
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', meta['digest']) is not None, 'Missing API digest')
    outer_path = cache / (name + '-' + str(meta['id']) + '.zip')
    require(outer_path.is_file() and outer_path.stat().st_size == meta['size_in_bytes'] <= MAX, 'Missing or wrong-size cached ZIP')
    with outer_path.open('rb') as stream:
        digest, _, _ = hash_stream(stream)
    require('sha256:' + digest == meta['digest'], 'Outer artifact differs from API digest')
    stem = f'cs80-reference-{system}-{arch}-{SOURCE[:12]}'
    inner_name = stem + ('.zip' if system == 'windows' else '.tar.gz')
    manifest_name = stem + '.manifest.json'
    small_members, seen = {}, set()
    with zipfile.ZipFile(outer_path) as outer:
        names = outer.namelist()
        require(len(names) == len(set(names)) == 3 and set(names) == {inner_name, manifest_name, 'SHA256SUMS'}, 'Wrong outer ZIP layout')
        require(all(not m.flag_bits & 1 and m.file_size <= MAX for m in outer.infolist()), 'Invalid outer ZIP member')
        raw_manifest = outer.read(manifest_name)
        require(len(raw_manifest) <= SMALL, 'Oversized manifest')
        manifest = json.loads(raw_manifest, object_pairs_hook=verifier.unique_object)
        require(verifier.validate_manifest(manifest, SOURCE) == (stem, system), 'Manifest layout mismatch')
        require(manifest['platform'] == system and manifest['architecture'] == arch
                and manifest['preset_library_revision'] == REVISION and manifest['workflow_run_id'] == str(RUN)
                and manifest['juce_sha'] == JUCE and manifest['juce_version'] == '8.0.11', 'Wrong package identity')
        inventory = {item['path']: item for item in manifest['files']}
        sums = {}
        for line in outer.read('SHA256SUMS').decode().splitlines():
            match = re.fullmatch(r'([0-9a-f]{64})  ([^/\\]+)', line)
            require(match is not None and match[2] not in sums, 'Invalid inner checksum inventory')
            sums[match[2]] = match[1]
        require(set(sums) == {inner_name, manifest_name} and sums[manifest_name] == sha(raw_manifest), 'Sidecar checksum mismatch')
        with outer.open(inner_name) as stream:
            inner_hash, inner_size, _ = hash_stream(stream)
        require(sums[inner_name] == inner_hash, 'Inner archive checksum mismatch')
        metadata = set(expected_docs) | set(NOTICES) | {'licenses/JUCE-LICENSE.md', 'SOURCE.txt', 'evidence/juce-provenance.txt', 'manifest.json'}
        product_roots = {f'VST3/{PRODUCT}.vst3', f'Standalone/{PRODUCT}' + ('.app' if system == 'macos' else '.exe' if system == 'windows' else '')}
        if system == 'macos':
            product_roots.add(f'AU/{PRODUCT}.component')
            metadata |= {root + '/Contents/Info.plist' for root in product_roots}
        require(all(p in metadata or any(p == root or p.startswith(root + '/') for root in product_roots) for p in inventory), 'Unexpected package payload root')

        def member(fullname, kind, mode, size, stream=None, target=''):
            fullname = fullname.rstrip('/')
            verifier.safe_name(fullname)
            require(fullname.startswith(stem + '/'), 'Wrong inner root')
            relative = fullname[len(stem) + 1:]
            require(relative not in seen, 'Duplicate inner member')
            seen.add(relative)
            if relative == 'manifest.json':
                require(kind == 'file' and size <= SMALL, 'Invalid embedded manifest')
                data = stream.read(SMALL + 1)
                require(data == raw_manifest, 'Embedded/sidecar bytes differ')
                return
            require(relative in inventory, 'Uninventoried member')
            item = inventory[relative]
            require(item['type'] == kind, 'Member type differs')
            if system != 'windows':
                require(stat.S_IMODE(mode) == int(item['mode'], 8), 'Unix mode changed')
            if kind == 'file':
                h, n, data = hash_stream(stream, relative in metadata)
                require(n == size == item['size'] and h == item['sha256'], 'File bytes differ from inventory')
                if data is not None:
                    small_members[relative] = data
            elif kind == 'symlink':
                require(target == item['target'], 'Bundle symlink target differs')

        with outer.open(inner_name) as inner:
            if system == 'windows':
                with zipfile.ZipFile(inner) as archive:
                    require(len(archive.infolist()) <= 4096, 'Too many ZIP entries')
                    for entry in archive.infolist():
                        require(not entry.flag_bits & 1, 'Encrypted inner entry')
                        with archive.open(entry) as stream:
                            member(entry.filename, 'directory' if entry.is_dir() else 'file', entry.external_attr >> 16, entry.file_size, stream)
            else:
                with tarfile.open(fileobj=inner, mode='r|gz') as archive:
                    for entry in archive:
                        require(entry.isfile() or entry.isdir() or entry.issym(), 'Unsupported tar member type')
                        stream = archive.extractfile(entry) if entry.isfile() else None
                        try:
                            member(entry.name, 'file' if entry.isfile() else 'directory' if entry.isdir() else 'symlink', entry.mode, entry.size, stream, entry.linkname)
                        finally:
                            if stream:
                                stream.close()
        require(seen == set(inventory) | {'manifest.json'}, 'Missing inner member')

    for name, expected in expected_docs.items():
        require(small_members[name] == expected, 'Packaged guide differs from exact source transformation: ' + name)
    notice_results = []
    for name, source_path in NOTICES.items():
        expected = git_bytes(source_path)
        actual = small_members[name]
        require(actual.replace(b'\r\n', b'\n') == expected.replace(b'\r\n', b'\n'), 'Notice content differs: ' + name)
        notice_results.append({'path': name, 'sha256': sha(actual), 'source_sha256': sha(expected), 'match': 'byte-exact' if actual == expected else 'line-ending-only'})
    juce_license = git_bytes('LICENSE.md', JUCE, ROOT / 'deps/juce')
    actual = small_members['licenses/JUCE-LICENSE.md']
    require(actual.replace(b'\r\n', b'\n') == juce_license.replace(b'\r\n', b'\n'), 'Pinned JUCE license differs')
    notice_results.append({'path': 'licenses/JUCE-LICENSE.md', 'sha256': sha(actual), 'source_sha256': sha(juce_license), 'match': 'byte-exact' if actual == juce_license else 'line-ending-only'})
    source_text = small_members['SOURCE.txt'].decode()
    require(f'Source commit: {SOURCE}\n' in source_text and f'Preset library revision: {REVISION}\n' in source_text
            and f'https://github.com/{REPO}/tree/{SOURCE}' in source_text, 'SOURCE.txt mismatch')
    links = check_links({name: small_members[name] for name in expected_docs}, set(inventory))
    bundles = []
    executable_paths = []
    if system == 'macos':
        for root in sorted(product_roots):
            info = plistlib.loads(small_members[root + '/Contents/Info.plist'])
            executable = root + '/Contents/MacOS/' + info['CFBundleExecutable']
            require(info['CFBundleExecutable'] == PRODUCT and executable in inventory, 'Mac executable/plist mismatch')
            require(info['CFBundleShortVersionString'] == app_version, 'Mac application version differs from source')
            require(info.get('CFBundleIdentifier') and info.get('CFBundlePackageType') == ('APPL' if root.endswith('.app') else 'BNDL'), 'Wrong Mac bundle identity/type')
            executable_paths.append(executable)
            bundles.append({'path': root, 'identifier': info['CFBundleIdentifier'], 'type': info['CFBundlePackageType'], 'application_version': info['CFBundleShortVersionString']})
    elif system == 'linux':
        executable_paths = [f'VST3/{PRODUCT}.vst3/Contents/x86_64-linux/{PRODUCT}.so', f'Standalone/{PRODUCT}']
    for executable in executable_paths:
        require(int(inventory[executable]['mode'], 8) & 0o111, 'Installed native product lost executable bits')
    return {'artifact_id': meta['id'], 'artifact_name': meta['name'], 'platform': system, 'architecture': arch,
            'outer_sha256': digest, 'outer_bytes': outer_path.stat().st_size,
            'inner_name': inner_name, 'inner_sha256': inner_hash, 'inner_bytes': inner_size,
            'formats': manifest['formats'], 'inventory_entries': len(inventory), 'product_roots': sorted(product_roots),
            'unix_executable_paths': executable_paths, 'bundle_metadata': bundles,
            'license_notice_comparisons': notice_results, 'guide_links': links,
            'source_text': source_text, 'symlink_count': sum(i['type'] == 'symlink' for i in inventory.values()),
            'archive_products_extracted_or_executed': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-only', action='store_true')
    parser.add_argument('--artifacts-json', type=Path)
    parser.add_argument('--run-json', type=Path)
    parser.add_argument('--cache', type=Path, default=ROOT / 'build-coordination/artifact-cache')
    args = parser.parse_args()
    verifier = import_trusted('cs80_install_verifier', ROOT / 'scripts/verify-ci-bundle.py')
    packager = import_trusted('cs80_install_packager', ROOT / 'scripts/package-ci-artifacts.py')
    documents = expected_documents(packager)
    review = source_review(documents)
    review['source_guide_links'] = check_links(documents, set(documents))
    result = {'assignment': 57, 'source': SOURCE, 'run_id': RUN, 'verified_at_utc': datetime.now(timezone.utc).isoformat(),
              'source_review': review,
              'trusted_helpers': {name: sha((ROOT / 'scripts' / name).read_bytes()) for name in ('verify-ci-bundle.py', 'package-ci-artifacts.py')},
              'limits': 'Installation/layout, exact source documents and notice presence/content only. No downloaded product executed, native OS installation, signature verification, legal compliance conclusion, every-DAW compatibility or acoustic/hardware equivalence. CI and public release qualification remain separate.'}
    if args.source_only:
        result.update(status='source_contract_reviewed_actual_archives_pending', artifact_count=0)
        target = WORK / 'source-review.json'
    else:
        require(args.artifacts_json is not None and args.run_json is not None, 'Both raw API files are required')
        run = json.loads(args.run_json.read_text())
        require(run['id'] == RUN and run['head_sha'] == SOURCE and type(run['run_attempt']) is int and run['run_attempt'] == 1
                and run['event'] == 'push' and run['head_branch'] == 'main' and run['head_repository']['full_name'] == REPO, 'Wrong first-attempt run')
        records = json.loads(args.artifacts_json.read_text())['artifacts']
        selected = {name: [m for m in records if m['name'] == name] for name in ARTIFACTS}
        require(all(len(rows) == 1 for rows in selected.values()), 'All four unique final artifacts must exist')
        results = [inspect_outer(rows[0], args.cache, verifier, documents, review['application_version']) for rows in selected.values()]
        result.update(status='four_actual_installation_archives_verified', artifact_count=4, artifacts=results,
                      native_ci_snapshot={'status': run['status'], 'conclusion': run['conclusion'], 'run_attempt': run['run_attempt']})
        target = WORK / 'artifact-proof.json'
    target.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'status': result['status'], 'artifact_count': result['artifact_count'], 'proof': str(target)}, indent=2))


if __name__ == '__main__':
    main()
