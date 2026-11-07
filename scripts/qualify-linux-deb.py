#!/usr/bin/env python3
"""Qualify an existing successful same-repo Linux bundle; never execute products.

Only GitHub metadata and the named install-bundle artifact are downloaded. No
build, validator execution, generated-package installation, release publication or check-state writes.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import tarfile
import zipfile

REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
REPOSITORY_ID = 1436742091
PACKAGER_COMMIT = '635ad6e8f24978040b02a4a10e646d7e3c592ce4'
PACKAGER_BLOB = '4342b31ddc5762fcc2a5c3dca7842ac3abdf4df2'
PR_SOURCE_RUN = '42351562098'
PR_SOURCE_COMMIT = '635ad6e8f24978040b02a4a10e646d7e3c592ce4'
PR_ARTIFACT_ID = 13566771073
PR_ARTIFACT_DIGEST = 'sha256:01b00fc1e2c32c840507ed27052e9cb9cbf67cf3cc0149df920233be3bb3a29d'
PR_ARTIFACT_SIZE = 12413145
ARTIFACT_NAME = 'cs80-linux-install-bundle'
PRODUCT = 'CS-80 Reference'
JUCE_SHA = 'c352e2489000ffeb9e397051377d75eef2efe641'
MAX_ZIP = 128 * 1024 * 1024
MAX_TAR = 256 * 1024 * 1024
MAX_FILE = 64 * 1024 * 1024
MAX_MEMBERS = 2048
ROOT = Path(__file__).resolve().parents[1]
STANDALONE = f'Standalone/{PRODUCT}'
PLUGIN = f'VST3/{PRODUCT}.vst3/Contents/x86_64-linux/{PRODUCT}.so'
PACKAGING_PATHS = {
    '.github/workflows/qualify-linux-deb.yml',
    'installer/linux/make-deb.sh', 'installer/macos/make-pkg.sh',
    'installer/windows/cs80-reference.iss',
    'scripts/package-ci-artifacts.py', 'scripts/test-packaging-notices.py',
    'scripts/test-deb-runtime-dependencies.py',
    'scripts/qualify-linux-deb.py', 'scripts/test-qualify-linux-deb.py',
}
NOTICE_SOURCES = {
    'copyright': 'LICENSE',
    'INSTALL_AND_QUICKSTART.md': 'docs/INSTALL_AND_QUICKSTART.md',
}
EVIDENCE_FILES = {
    'standalone-elf.txt', 'vst3-elf.txt',
    'packaging.stdout.txt', 'packaging.stderr.txt',
    'source-manifest.json', 'juce-provenance.txt',
    'native-dpkg-packages.txt', 'verification.json',
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(*args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, **kwargs).stdout


def safe_name(name):
    require(isinstance(name, str) and name and '\\' not in name and all(32 <= ord(c) != 127 for c in name),
            'Invalid archive path')
    parts = name.split('/')
    require(not name.startswith('/') and all(p not in ('', '.', '..') for p in parts)
            and ':' not in parts[0] and str(PurePosixPath(name)) == name,
            f'Unsafe archive path: {name}')


def validate_inputs(repository, run_id, source_sha):
    require(repository == REPOSITORY, 'Only the configured same-repository source is permitted')
    require(bool(re.fullmatch(r'[1-9][0-9]{0,19}', run_id)), 'Expected a positive numeric run ID')
    require(bool(re.fullmatch(r'[0-9a-f]{40}', source_sha)), 'Expected a full lowercase source commit SHA')


def valid_sha(value):
    return isinstance(value, str) and bool(re.fullmatch(r'[0-9a-f]{40}', value))


def same_repository(value):
    return (isinstance(value, dict) and value.get('full_name') == REPOSITORY
            and value.get('id') == REPOSITORY_ID and value.get('fork') is False)


def validate_event(environment, event, candidate, parents, source_run_id, source_sha):
    """Bind the inspected checkout and inputs to the runner-provided event, not PR text."""
    require(isinstance(event, dict), 'Invalid qualification event payload')
    validate_inputs(environment.get('GITHUB_REPOSITORY', ''), source_run_id, source_sha)
    require(environment.get('GITHUB_REPOSITORY_ID') == str(REPOSITORY_ID)
            and same_repository(event.get('repository')), 'Qualification repository mismatch or fork')
    name = environment.get('GITHUB_EVENT_NAME')
    require(name in ('pull_request', 'workflow_dispatch'), 'Unsupported qualification event')
    require(valid_sha(candidate) and candidate == environment.get('GITHUB_SHA'),
            'Checkout and qualification event commit differ')
    for key in ('GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT'):
        require(bool(re.fullmatch(r'[1-9][0-9]{0,19}', environment.get(key, ''))), 'Invalid qualification run identity')
    context = {'event': name, 'checkout_commit': candidate, 'ref': environment.get('GITHUB_REF'),
               'run_id': environment['GITHUB_RUN_ID'], 'run_attempt': int(environment['GITHUB_RUN_ATTEMPT'])}
    if name == 'pull_request':
        require(event.get('action') in ('opened', 'synchronize', 'reopened'), 'Unsupported PR action')
        pr = event.get('pull_request', {})
        require(isinstance(pr, dict), 'Invalid PR context')
        number = event.get('number')
        require(type(number) is int and number > 0 and pr.get('number') == number
                and pr.get('state') == 'open', 'Invalid or closed PR context')
        base, head = pr.get('base', {}), pr.get('head', {})
        require(isinstance(base, dict) and isinstance(head, dict), 'Invalid PR base/head context')
        require(same_repository(base.get('repo')) and same_repository(head.get('repo')),
                'Only same-repository non-fork PRs may qualify')
        require(base.get('ref') == 'main' and context['ref'] == f'refs/pull/{number}/merge',
                'Expected a main-targeted PR merge ref')
        require(valid_sha(base.get('sha')) and valid_sha(head.get('sha'))
                and parents == [base['sha'], head['sha']], 'Checkout merge parents differ from PR base/head')
        require(event.get('inputs') in (None, {}) and source_run_id == PR_SOURCE_RUN and source_sha == PR_SOURCE_COMMIT,
                'PR qualification requires fixed approved source inputs')
        # GITHUB_SHA identifies the synthetic merge checkout; run head_sha is the
        # branch head. Check both without pretending they identify the same tree.
        context.update(pr_number=number, pr_head_commit=head['sha'], base_commit=base['sha'],
                       run_head_commit=head['sha'], checkout_kind='synthetic PR merge')
    else:
        ref = context['ref']
        require(isinstance(ref, str) and ref.startswith('refs/heads/')
                and event.get('ref') in (ref, ref.removeprefix('refs/heads/')),
                'Manual qualification requires a matching branch ref')
        require(event.get('inputs') == {'source_run_id': source_run_id, 'source_sha': source_sha},
                'Manual source inputs differ from the dispatch event')
        context.update(run_head_commit=candidate, checkout_kind='dispatched branch')
    return context


def validate_qualification_run(metadata, context):
    require(metadata.get('id') == int(context['run_id'])
            and metadata.get('run_attempt') == context['run_attempt']
            and metadata.get('head_sha') == context['run_head_commit']
            and metadata.get('event') == context['event']
            and metadata.get('path') == '.github/workflows/qualify-linux-deb.yml',
            'Qualification run metadata differs from the trusted event context')
    require(all(same_repository(metadata.get(key)) for key in ('repository', 'head_repository')),
            'Qualification run repository mismatch or fork')


def validate_pr_artifact(artifact, context):
    if context['event'] == 'pull_request':
        require(artifact.get('id') == PR_ARTIFACT_ID and artifact.get('digest') == PR_ARTIFACT_DIGEST
                and artifact.get('size_in_bytes') == PR_ARTIFACT_SIZE,
                'PR qualification requires the fixed approved source artifact')


def validate_run(metadata, run_id, source_sha):
    require(metadata.get('id') == int(run_id) and metadata.get('head_sha') == source_sha,
            'Run ID or source commit mismatch')
    for field in ('repository', 'head_repository'):
        repo = metadata.get(field, {})
        require(repo.get('full_name') == REPOSITORY and repo.get('id') == REPOSITORY_ID,
                'Source run must belong to this repository, not a fork')
    require(metadata.get('event') == 'push' and metadata.get('name') == 'build'
            and metadata.get('path') == '.github/workflows/build.yml',
            'Expected the ordinary same-repo push build, not a PR or unrelated workflow')
    require(metadata.get('status') == 'completed' and metadata.get('conclusion') == 'success',
            'Source build must have completed successfully')
    # Existing manifests have no run-attempt identity. Do not reuse a possibly
    # stale artifact from an earlier failed attempt under a later green run ID.
    require(metadata.get('run_attempt') == 1, 'Rerun artifacts cannot be bound to an attempt')


def select_artifact(response, run_id, source_sha):
    artifacts = response.get('artifacts', [])
    require(response.get('total_count') == len(artifacts), 'Artifact listing is incomplete')
    selected = [a for a in artifacts if a.get('name') == ARTIFACT_NAME]
    require(len(selected) == 1, 'Expected exactly one Linux install-bundle artifact')
    artifact = selected[0]
    require(type(artifact.get('id')) is int and artifact['id'] > 0, 'Invalid artifact ID')
    require(artifact.get('expired') is False, 'Artifact has expired')
    require(type(artifact.get('size_in_bytes')) is int
            and 0 < artifact['size_in_bytes'] <= MAX_ZIP, 'Artifact exceeds download size limit')
    require(bool(re.fullmatch(r'sha256:[0-9a-f]{64}', artifact.get('digest', ''))),
            'GitHub artifact digest is missing or invalid')
    origin = artifact.get('workflow_run', {})
    require(origin.get('id') == int(run_id) and origin.get('head_sha') == source_sha
            and origin.get('repository_id') == REPOSITORY_ID
            and origin.get('head_repository_id') == REPOSITORY_ID,
            'Artifact provenance does not match the successful same-repo run')
    return artifact


def source_identity(source, target):
    """Compare Git object metadata only. Never read or execute source artifact code."""
    differences = sorted(p for p in source.keys() | target.keys() if source.get(p) != target.get(p))
    allowed = lambda p: p == 'README.md' or p.startswith('docs/') or p in PACKAGING_PATHS
    require(all(allowed(p) for p in differences),
            'Non-packaging source/build-input differences: ' + ', '.join(p for p in differences if not allowed(p)))
    return differences


def github_tree(response):
    require(response.get('truncated') is False, 'Git tree listing is truncated')
    result = {}
    for item in response['tree']:
        if item['type'] == 'tree':
            continue
        require(item['type'] in ('blob', 'commit'), 'Unexpected Git object type')
        safe_name(item['path'])
        require(item['path'] not in result, 'Duplicate Git tree path')
        result[item['path']] = (item['mode'], item['type'], item['sha'])
    return result


def local_tree(root):
    result = {}
    for entry in run('git', '-C', str(root), 'ls-tree', '-rz', 'HEAD').split(b'\0'):
        if entry:
            meta, name = entry.decode().split('\t', 1)
            result[name] = tuple(meta.split())
    return result


def validate_manifest(manifest, source_sha, run_id):
    require(manifest.get('schema_version') == 1 and manifest.get('product') == PRODUCT
            and manifest.get('platform') == 'linux' and manifest.get('architecture') == 'x86_64'
            and manifest.get('formats') == ['VST3', 'Standalone'], 'Unsupported bundle manifest')
    require(manifest.get('commit') == source_sha and manifest.get('workflow_run_id') == run_id,
            'Manifest source/run mismatch')
    require(manifest.get('juce_sha') == JUCE_SHA and manifest.get('juce_version') == '8.0.11',
            'Unexpected JUCE source identity')
    entries = manifest.get('files', [])
    require(isinstance(entries, list) and 0 < len(entries) < MAX_MEMBERS, 'Invalid manifest inventory')
    inventory = {}
    for item in entries:
        name = item['path']
        safe_name(name)
        require(name not in inventory and name != 'manifest.json', 'Duplicate/reserved inventory path')
        require(item.get('type') in ('file', 'directory'), 'Linked/special bundle entries are not supported')
        require(item.get('mode') in ('0o644', '0o755'), 'Unsafe or unsupported bundle permissions')
        if item['type'] == 'file':
            require(type(item.get('size')) is int and 0 <= item['size'] <= MAX_FILE,
                    'Bundle member exceeds size limit')
            require(bool(re.fullmatch(r'[0-9a-f]{64}', item.get('sha256', ''))), 'Invalid file digest')
        inventory[name] = item
    for name in (STANDALONE, PLUGIN, 'LICENSE', 'SOURCE.txt',
                 'licenses/JUCE-LICENSE.md', 'evidence/juce-provenance.txt'):
        require(inventory.get(name, {}).get('type') == 'file', 'Required bundle file missing: ' + name)
    for name in (STANDALONE, PLUGIN):
        require(inventory[name]['mode'] == '0o755', 'Product execute bits are missing')
    return inventory


def read_bundle(path, artifact, source_sha, run_id):
    require(path.stat().st_size == artifact['size_in_bytes'] and path.stat().st_size <= MAX_ZIP,
            'Downloaded archive size mismatch')
    raw = path.read_bytes()
    require('sha256:' + digest(raw) == artifact['digest'], 'GitHub archive digest mismatch')
    stem = f'cs80-reference-linux-x86_64-{source_sha[:12]}'
    manifest_name, tar_name = stem + '.manifest.json', stem + '.tar.gz'
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        members = archive.infolist()
        expected = {'SHA256SUMS', manifest_name, tar_name}
        require(len(members) == 3 and {m.filename for m in members} == expected,
                'Unexpected or duplicate outer ZIP members')
        for m in members:
            safe_name(m.filename)
            require(not m.is_dir() and not stat.S_ISLNK(m.external_attr >> 16)
                    and stat.S_IFMT(m.external_attr >> 16) in (0, stat.S_IFREG)
                    and not (m.flag_bits & 1) and m.file_size <= MAX_ZIP,
                    'Unsupported outer ZIP member')
        manifest_bytes = archive.read(manifest_name)
        require(len(manifest_bytes) <= 1024 * 1024, 'Manifest is oversized')
        manifest = json.loads(manifest_bytes)
        inventory = validate_manifest(manifest, source_sha, run_id)
        compressed = archive.read(tar_name)
        sums = archive.read('SHA256SUMS').decode().splitlines()
        require(len(sums) == 2 and set(sums) == {
            digest(manifest_bytes) + '  ' + manifest_name,
            digest(compressed) + '  ' + tar_name}, 'Inner archive/manifest checksums do not match')
    with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
        tar_bytes = stream.read(MAX_TAR + 1)
    require(len(tar_bytes) <= MAX_TAR, 'Decompressed tar exceeds size limit')
    seen, payload = set(), {}
    with tarfile.open(fileobj=io.BytesIO(tar_bytes), mode='r:') as archive:
        for index, member in enumerate(archive):
            require(index < MAX_MEMBERS, 'Too many tar members')
            safe_name(member.name)
            require(member.name.startswith(stem + '/'), 'Wrong tar root')
            name = member.name[len(stem) + 1:]
            safe_name(name)
            require(name not in seen, 'Duplicate tar member')
            seen.add(name)
            require(member.isfile() or member.isdir(), 'Linked/special tar members are not supported')
            require(0 <= member.size <= MAX_FILE, 'Tar member exceeds size limit')
            if name == 'manifest.json':
                require(member.isfile() and member.size <= 1024 * 1024, 'Invalid embedded manifest')
                require(json.loads(archive.extractfile(member).read()) == manifest, 'Embedded manifest mismatch')
                continue
            require(name in inventory, 'Unmanifested tar member')
            item = inventory[name]
            require(item['type'] == ('file' if member.isfile() else 'directory')
                    and int(item['mode'], 8) == member.mode, 'Tar type/mode mismatch')
            if member.isfile():
                data = archive.extractfile(member).read()
                require(len(data) == item['size'] and digest(data) == item['sha256'], 'Tar payload hash/size mismatch')
                payload[name] = data
        require(seen == set(inventory) | {'manifest.json'}, 'Missing tar members')
    source_lines = payload['SOURCE.txt'].decode().splitlines()
    require(source_lines[:2] == [f'Source commit: {source_sha}',
            f'https://github.com/{REPOSITORY}/tree/{source_sha}'], 'SOURCE provenance mismatch')
    fields = {}
    for line in payload['evidence/juce-provenance.txt'].decode().splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            require(key not in fields, 'Duplicate JUCE provenance field')
            fields[key] = value
    for key, expected in {'verification': 'PASS', 'source_clean': 'true',
                          'actual_sha': JUCE_SHA, 'expected_sha': JUCE_SHA,
                          'actual_version': '8.0.11', 'expected_version': '8.0.11',
                          'workflow_commit': source_sha, 'workflow_run': run_id}.items():
        require(fields.get(key) == expected, 'JUCE provenance mismatch: ' + key)
    return manifest, payload


def materialize(build, manifest, payload):
    require(not build.exists(), 'Refusing to reuse a build directory')
    products = build / 'CS80Reference_artefacts/Release'
    for item in manifest['files']:
        name = item['path']
        if name == STANDALONE or name == f'VST3/{PRODUCT}.vst3' or name.startswith(f'VST3/{PRODUCT}.vst3/'):
            dest = products / name
            if item['type'] == 'directory':
                dest.mkdir(parents=True, exist_ok=True)
            else:
                dest.parent.mkdir(parents=True, exist_ok=True)
                with dest.open('xb') as stream:
                    stream.write(payload[name])
            dest.chmod(int(item['mode'], 8))
    license_file = build / '_deps/juce-src/LICENSE.md'
    license_file.parent.mkdir(parents=True)
    license_file.write_bytes(payload['licenses/JUCE-LICENSE.md'])
    return products


def api_json(endpoint):
    # GitHub CLI handles credentials and redirects. Endpoints are constructed
    # only from fixed repository identity plus validated numeric IDs / Git SHAs.
    return json.loads(run('gh', 'api', '--hostname', 'github.com', '--method', 'GET', endpoint))


def package_inventory(deb):
    control = run('dpkg-deb', '--ctrl-tarfile', str(deb))
    with tarfile.open(fileobj=io.BytesIO(control), mode='r:') as tar:
        files = [m for m in tar if not m.isdir()]
        require(len(files) == 1 and files[0].name in ('./control', 'control')
                and files[0].isfile(), 'Unexpected package control scripts or files')
    data = run('dpkg-deb', '--fsys-tarfile', str(deb))
    require(len(data) <= MAX_TAR, 'Debian package payload is oversized')
    payload, modes, seen = {}, {}, set()
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:') as tar:
        for member in tar:
            name = member.name.removeprefix('./').rstrip('/')
            if name in ('', '.'):
                require(member.isdir(), 'Invalid Debian package root member')
                continue
            safe_name(name)
            require(name not in seen, 'Duplicate Debian package member')
            seen.add(name)
            require(member.isfile() or member.isdir(), 'Linked/special Debian package member')
            require(member.uid == member.gid == 0 and not (member.mode & 0o7000), 'Unsafe package ownership/mode')
            if member.isfile():
                require(name not in payload and member.size <= MAX_FILE, 'Duplicate/oversized package file')
                payload[name] = tar.extractfile(member).read()
                modes[name] = member.mode
    return payload, modes


def verify_deb(deb, payload, root, extracted):
    require(run('dpkg-deb', '-f', str(deb), 'Package').strip() == b'cs80-reference', 'Unexpected package name')
    require(run('dpkg-deb', '-f', str(deb), 'Architecture').strip() == b'amd64', 'Unexpected package architecture')
    depends = run('dpkg-deb', '-f', str(deb), 'Depends').decode().strip()
    atoms = [re.sub(r'\s+', ' ', atom.strip()) for atom in depends.split(',')]
    for package in ('libc6', 'libstdc++6'):
        require(any(re.fullmatch(re.escape(package) + r' \(>= [0-9][^()\s]*\)', atom) for atom in atoms),
                'Missing versioned ABI floor: ' + package)
    for package in ('libasound2t64 | libasound2', 'libfreetype6', 'libfontconfig1',
                    'libx11-6', 'libxext6', 'libxrandr2', 'libxinerama1', 'libxcursor1'):
        require(package in atoms, 'Missing explicit dynamic-library dependency: ' + package)
    expected = {'usr/bin/cs80-reference': payload[STANDALONE]}
    for name, data in payload.items():
        if name.startswith(f'VST3/{PRODUCT}.vst3/'):
            expected['usr/lib/vst3/' + name.removeprefix('VST3/')] = data
    for name, source in NOTICE_SOURCES.items():
        expected['usr/share/doc/cs80-reference/' + name] = (root / source).read_bytes()
    expected['usr/share/doc/cs80-reference/JUCE-LICENSE.md'] = payload['licenses/JUCE-LICENSE.md']
    actual, modes = package_inventory(deb)
    require(actual.keys() == expected.keys(), 'Package file inventory/required notices differ: '
            + ', '.join(sorted(actual.keys() ^ expected.keys())))
    require(all(actual[name] == data for name, data in expected.items()), 'Packaged payload or notice bytes changed')
    require(modes['usr/bin/cs80-reference'] == modes['usr/lib/vst3/' + PLUGIN.removeprefix('VST3/')] == 0o755,
            'Packaged product execute bits changed')
    require(all(mode == 0o644 for name, mode in modes.items() if name.startswith('usr/share/doc/')),
            'Packaged notice permissions changed')
    # Extract only already verified regular-file bytes, never package scripts.
    extracted.mkdir()
    for name, data in actual.items():
        dest = extracted / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        dest.chmod(modes[name])
    return depends, {name: digest(data) for name, data in actual.items()}


def validate_packager(root):
    require(run('git', '-C', str(root), 'rev-parse', 'HEAD').decode().strip() == PACKAGER_COMMIT,
            'Packager checkout is not the reviewed corrected commit')
    require(run('git', '-C', str(root), 'rev-parse', 'HEAD:installer/linux/make-deb.sh').decode().strip() == PACKAGER_BLOB,
            'Corrected packager blob mismatch')
    require(not run('git', '-C', str(root), 'status', '--porcelain', '--untracked-files=no'), 'Packager checkout is dirty')


def check_packaging_result(result, evidence):
    (evidence / 'packaging.stdout.txt').write_text(result.stdout)
    (evidence / 'packaging.stderr.txt').write_text(result.stderr)
    warnings = 'dpkg-shlibdeps: warning:' in result.stderr
    if result.returncode != 0 or warnings:
        # Preserve native scanner diagnostics in the job log even though failed
        # packages/evidence are not uploaded. Escape command delimiters/newlines
        # so artifact-derived diagnostic text cannot issue workflow commands.
        for label, output in (('stdout', result.stdout), ('stderr', result.stderr)):
            safe = json.dumps(output).replace('::', r'\u003a\u003a')
            print(f'Packaging {label} (JSON): {safe}', file=sys.stderr)
    require(result.returncode == 0, 'Debian packaging failed; native diagnostics are above')
    require(not warnings, 'Dependency scanner warnings require review')


def stage_qualified_evidence(evidence, qualified):
    """Stage only scoped text/JSON evidence, never package/product/archive bytes."""
    require(not qualified.exists(), 'Refusing to reuse qualified output')
    require({p.name for p in evidence.iterdir()} == EVIDENCE_FILES,
            'Unexpected or missing evidence files; package/product uploads are forbidden')
    payload = {}
    for name in sorted(EVIDENCE_FILES):
        path = evidence / name
        require(path.is_file() and not path.is_symlink(), 'Evidence must be regular files')
        require(path.stat().st_size <= MAX_FILE, 'Evidence file exceeds size limit')
        data = path.read_bytes()
        text = data.decode('utf-8')
        require('\x00' not in text, 'Binary evidence is forbidden')
        if name.endswith('.json'):
            json.loads(text)
        payload[name] = data
    require(json.loads(payload['verification.json']).get('package_uploaded') is False,
            'Evidence report must explicitly declare package_uploaded=false')
    qualified.mkdir()
    for name, data in payload.items():
        path = qualified / name
        with path.open('xb') as stream:
            stream.write(data)
        path.chmod(0o644)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-run-id', required=True)
    parser.add_argument('--source-sha', required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--packager-root', type=Path, required=True)
    args = parser.parse_args()
    repository = os.environ.get('GITHUB_REPOSITORY', '')
    validate_inputs(repository, args.source_run_id, args.source_sha)
    candidate = run('git', '-C', str(ROOT), 'rev-parse', 'HEAD').decode().strip()
    # Raw commit headers retain parent IDs even in a depth-one checkout.
    headers = run('git', '-C', str(ROOT), 'cat-file', 'commit', 'HEAD').decode().split('\n\n', 1)[0]
    parents = [line.removeprefix('parent ') for line in headers.splitlines() if line.startswith('parent ')]
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
    context = validate_event(os.environ, event, candidate, parents, args.source_run_id, args.source_sha)
    endpoint = f'repos/{REPOSITORY}'
    validate_qualification_run(api_json(f'{endpoint}/actions/runs/{context["run_id"]}'), context)
    require(not any(os.environ.get(k) for k in ('DPKG_ADMINDIR', 'DPKG_ROOT', 'LD_LIBRARY_PATH', 'LD_PRELOAD')),
            'Non-native dependency lookup environment is forbidden')
    require(Path('/var/lib/dpkg/status').is_file(), 'Native installed-package metadata is required')
    require(not run('git', '-C', str(ROOT), 'status', '--porcelain', '--untracked-files=no'), 'Tracked checkout is dirty')
    packager = args.packager_root.resolve()
    validate_packager(packager)
    require(not args.work_dir.exists(), 'Refusing to reuse a qualification directory')
    args.work_dir.mkdir(parents=True)
    evidence = args.work_dir / 'evidence'; evidence.mkdir()
    source_run = api_json(f'{endpoint}/actions/runs/{args.source_run_id}')
    validate_run(source_run, args.source_run_id, args.source_sha)
    artifact = select_artifact(api_json(f'{endpoint}/actions/runs/{args.source_run_id}/artifacts?per_page=100'),
                               args.source_run_id, args.source_sha)
    validate_pr_artifact(artifact, context)
    source_commit = api_json(f'{endpoint}/git/commits/{args.source_sha}')
    require(source_commit.get('sha') == args.source_sha, 'Source commit API mismatch')
    tree_sha = source_commit['tree']['sha']
    require(re.fullmatch(r'[0-9a-f]{40}', tree_sha), 'Invalid source tree SHA')
    differences = source_identity(github_tree(api_json(f'{endpoint}/git/trees/{tree_sha}?recursive=1')), local_tree(packager))
    archive = args.work_dir / 'source-bundle.zip'
    with archive.open('xb') as stream:
        subprocess.run(['gh', 'api', '--hostname', 'github.com', '--method', 'GET',
                        f'{endpoint}/actions/artifacts/{artifact["id"]}/zip'], stdout=stream, check=True)
    manifest, payload = read_bundle(archive, artifact, args.source_sha, args.source_run_id)
    require(payload['LICENSE'] == (packager / 'LICENSE').read_bytes(), 'Source licence differs from checked-out source')
    products = materialize(args.work_dir / 'build', manifest, payload)
    env = dict(os.environ, LC_ALL='C')
    for label, rel in (('standalone', STANDALONE), ('vst3', PLUGIN)):
        header = run('readelf', '-h', str(products / rel), env=env).decode()
        require(re.search(r'Class:\s+ELF64\b', header)
                and re.search(r'Machine:\s+Advanced Micro Devices X86-64\s*\n', header), 'Wrong product ELF architecture')
        (evidence / (label + '-elf.txt')).write_bytes(run('readelf', '-h', '-d', '--version-info', str(products / rel), env=env))
    packages = args.work_dir / 'packages'
    result = subprocess.run(['bash', str(packager / 'installer/linux/make-deb.sh'), str(args.work_dir / 'build'), str(packages)],
                            capture_output=True, text=True, env=env, check=False)
    check_packaging_result(result, evidence)
    archives = list(packages.glob('*.deb'))
    require(len(archives) == 1, 'Expected exactly one Debian package')
    depends, file_hashes = verify_deb(archives[0], payload, packager, args.work_dir / 'extracted')
    (evidence / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (evidence / 'juce-provenance.txt').write_bytes(payload['evidence/juce-provenance.txt'])
    (evidence / 'native-dpkg-packages.txt').write_bytes(run('dpkg-query', '-W', '-f=${binary:Package}\t${Version}\n'))
    report = {'qualification': 'PASS', 'scope': 'source identity, archive integrity, native ABI packaging and #83 notice bytes; not complete licence/runtime/release qualification',
              'notice_coverage': {'verified': ['project LICENSE', 'JUCE licence', 'quickstart'], 'font_mts': 'absent; separate PR #70 not included'},
              'packager_commit': PACKAGER_COMMIT, 'packager_blob': PACKAGER_BLOB,
              'qualification_context': context, 'source_commit': args.source_sha, 'source_run_id': args.source_run_id,
              'source_run_attempt': source_run['run_attempt'], 'artifact_id': artifact['id'], 'artifact_digest': artifact['digest'],
              'packaging_only_source_differences': differences, 'depends': depends, 'files': file_hashes,
              'deb_sha256': digest(archives[0].read_bytes()), 'product_executed': False, 'package_installed': False,
              'package_uploaded': False, 'output_scope': 'scoped text/JSON evidence only; no package, product or source archive'}
    validate_run(api_json(f'{endpoint}/actions/runs/{args.source_run_id}'), args.source_run_id, args.source_sha)
    (evidence / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    stage_qualified_evidence(evidence, args.work_dir / 'qualified')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
