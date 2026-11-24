#!/usr/bin/env python3
"""Check macOS installation/validator archive consistency; never qualify a release.

The expected commit, run, attempt and native architecture are caller inputs.
Artifact API receipts must come from an authenticated GitHub read. This offline
helper checks their consistency, not their authenticity. In particular neither
the artifact API nor the package manifest records a workflow run attempt: the
original run/job API records and timestamped job log remain mandatory.

Five validator console lanes and both recorded AU failure seeds are required.
Only input-path and fixed-seed consistency are checked here; original outcome
and command/report binding remain explicit external gates.

No historical source SHA, run, downloaded path or successful result is reused.
Only the exact expected Git source supplies the inventory and binary verifiers.
All downloaded ZIPs are read once; hashing and inspection use those same bytes.
No downloaded product is executed or installed. Standard library only.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import zipfile

MAX_ZIP = 512 * 1024 * 1024
MAX_FILE = 256 * 1024 * 1024
MAX_METADATA = 2 * 1024 * 1024
MAX_MEMBERS = 4096
PRODUCT = 'CS-80 Reference'
BUNDLES = {'VST3': f'VST3/{PRODUCT}.vst3', 'AU': f'AU/{PRODUCT}.component',
           'Standalone': f'Standalone/{PRODUCT}.app'}
LANES = {'vst3': 'pluginval-vst3.log', 'au': 'pluginval-au.log',
         'au-gui': 'pluginval-au-gui.log',
         'au-seed-regression': 'pluginval-au-seed-regression.log',
         'au-seed-c76-regression': 'pluginval-au-seed-c76-regression.log'}
FIXED_SEEDS = {'au-seed-regression': '0x2b4eef5',
               'au-seed-c76-regression': '0x3c99748'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'Duplicate JSON key: {key}')
        result[key] = value
    return result


def parse_json(data):
    return json.loads(data, object_pairs_hook=unique_object)


def snapshot(path, limit):
    # No stat-then-reopen or hash-then-reopen: inspect this exact byte snapshot.
    with Path(path).open('rb') as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit, f'Oversized input: {path}')
    return data


def identity(path, data):
    return {'path': str(Path(path).resolve()), 'bytes': len(data), 'sha256': digest(data)}


def safe_member(name):
    require(isinstance(name, str) and name and all(32 <= ord(c) != 127 for c in name),
            'Invalid ZIP member name')
    require(not name.startswith('/') and '\\' not in name
            and all(p not in ('', '.', '..') and ':' not in p for p in name.split('/')),
            f'Unsafe ZIP member name: {name}')


def zip_members(data):
    # Bound the central directory before ZipFile materializes its entries.
    tail = data[-65577:]
    offset = tail.rfind(b'PK\x05\x06')
    require(offset >= 0 and len(tail) - offset >= 22, 'Missing ZIP end record')
    require(offset < 20 or tail[offset - 20:offset - 16] != b'PK\x06\x07',
            'ZIP64 is unsupported')
    end = struct.unpack('<4s4H2IH', tail[offset:offset + 22])
    require(offset + 22 + end[7] == len(tail) and end[1] == end[2] == 0
            and end[3] == end[4] and 0 < end[4] <= MAX_MEMBERS
            and end[5] <= MAX_METADATA and end[6] != 0xffffffff,
            'Unsupported or oversized ZIP directory')
    members, names, total = {}, set(), 0
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        require(len(archive.infolist()) == end[4], 'ZIP entry count differs from directory')
        for entry in archive.infolist():
            name = entry.filename[:-1] if entry.is_dir() else entry.filename
            safe_member(name)
            require(name not in names, 'Duplicate ZIP member: ' + name)
            names.add(name)
            kind = stat.S_IFMT(entry.external_attr >> 16)
            require(kind in (0, stat.S_IFREG, stat.S_IFDIR)
                    and (kind != stat.S_IFDIR or entry.is_dir())
                    and (not entry.is_dir() or kind != stat.S_IFREG)
                    and not entry.flag_bits & 1, 'Unsupported ZIP entry kind/encryption')
            require(0 <= entry.file_size <= MAX_FILE, 'Oversized ZIP member')
            total += entry.file_size
            require(total <= MAX_ZIP, 'ZIP expanded-size limit exceeded')
            if entry.is_dir():
                require(entry.file_size == 0, 'ZIP directory contains data')
            else:
                with archive.open(entry) as stream:
                    content = stream.read(entry.file_size + 1)
                require(len(content) == entry.file_size, 'ZIP size mismatch')
                members[name] = content
    # Directory entries are not extracted, but reject file/parent ambiguity.
    for name in names:
        parts = name.split('/')
        require(all('/'.join(parts[:i]) not in members for i in range(1, len(parts))),
                'ZIP member has a regular-file ancestor')
    return members


def artifact_receipt(path, raw, data, expected_name, expected):
    api = parse_json(raw)
    require(isinstance(api, dict) and type(api.get('id')) is int and api['id'] > 0,
            'Missing artifact ID')
    require(api.get('name') == expected_name, 'Artifact name differs from expected lane')
    require(type(api.get('size_in_bytes')) is int and api['size_in_bytes'] == len(data)
            and api.get('digest') == 'sha256:' + digest(data), 'Artifact API digest/size mismatch')
    run = api.get('workflow_run', {})
    require(run.get('id') == expected['run'] and run.get('head_sha') == expected['source'],
            'Artifact API source/run mismatch')
    require(type(run.get('repository_id')) is int and run['repository_id'] > 0
            and run.get('head_repository_id') == run['repository_id'],
            'Artifact repository identity is missing or differs from head repository')
    if 'run_attempt' in run:
        require(run['run_attempt'] == expected['attempt'], 'Artifact API attempt mismatch')
    base = f"https://api.github.com/repos/{expected['repository']}/actions/artifacts/{api['id']}"
    require(api.get('url') == base and api.get('archive_download_url') == base + '/zip',
            'Artifact API repository/ID URL mismatch')
    return {'api_snapshot': identity(path, raw), 'id': api['id'], 'name': api['name'],
            'repository_id': run['repository_id'], 'digest': api['digest'],
            'size_in_bytes': api['size_in_bytes'], 'created_at': api.get('created_at'),
            'expires_at': api.get('expires_at'), 'expired_at_snapshot': api.get('expired'),
            'attempt_binding': 'Not established by an artifact receipt; original run/job API required.'}


def load_source_contract(repo, source, revision, directory):
    paths = ['scripts/package-ci-artifacts.py', 'scripts/verify-ci-bundle.py',
             'scripts/download-pluginval-macos.py', '.github/workflows/build.yml',
             'src/core/FactoryPresets.h']
    records, content = {}, {}
    for path in paths:
        raw = subprocess.check_output(['git', '-C', str(repo), 'show', f'{source}:{path}'])
        require(len(raw) <= MAX_METADATA, 'Oversized source contract file')
        records[path] = {'bytes': len(raw), 'sha256': digest(raw)}
        content[path] = raw
    declarations = re.findall(rb'^inline constexpr const char\* curatedLibraryRevision = "([^"\r\n]*)";$',
                              content['src/core/FactoryPresets.h'], re.MULTILINE)
    require(declarations == [revision.encode()], 'Expected source preset revision mismatch')
    # Execute only caller-selected repository source, never downloaded artifacts.
    scripts = directory / 'source-verifiers'
    scripts.mkdir()
    for name in ('package-ci-artifacts.py', 'verify-ci-bundle.py'):
        (scripts / name).write_bytes(content['scripts/' + name])
    spec = importlib.util.spec_from_file_location('a51_source_bundle_verifier', scripts / 'verify-ci-bundle.py')
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
    return verifier, records


def absolute_path(value):
    require(value.startswith('/') and not value.startswith('//') and '\\' not in value
            and all(32 <= ord(c) != 127 for c in value)
            and all(part not in ('', '.', '..') for part in value[1:].split('/')),
            'Noncanonical absolute evidence path')
    return value


def au_hashes(data):
    lines = data.decode('utf-8').splitlines()
    require(len(lines) == 2, 'AU hash evidence must contain exactly two records')
    result = []
    for line in lines:
        match = re.fullmatch(r'([0-9a-f]{64})  (/.+)', line)
        require(match is not None, 'Malformed AU hash record')
        result.append({'sha256': match[1], 'path': absolute_path(match[2])})
    require(result[0]['path'] != result[1]['path'], 'AU source and installed paths must differ')
    return result


def inspect_archives(expected, install_zip, install_api, pluginval_zip, pluginval_api,
                     verifier, source_records, scratch):
    """Separate byte checks from source-loader orchestration for bounded fixtures."""
    install_data = snapshot(install_zip, MAX_ZIP)
    pluginval_data = snapshot(pluginval_zip, MAX_ZIP)
    runner = {'arm64': 'macos-15', 'x86_64': 'macos-15-intel'}[expected['architecture']]
    receipts = []
    for path, data, name in ((install_api, install_data, f'cs80-{runner}-install-bundle'),
                             (pluginval_api, pluginval_data, f'cs80-pluginval-{runner}')):
        receipts.append(artifact_receipt(path, snapshot(path, MAX_METADATA), data, name, expected))
    require(receipts[0]['repository_id'] == receipts[1]['repository_id'],
            'Install and validator repository IDs differ')
    require(receipts[0]['id'] != receipts[1]['id'], 'Install and validator artifact IDs must differ')
    install = zip_members(install_data)
    evidence = zip_members(pluginval_data)
    stem = f"cs80-reference-macos-{expected['architecture']}-{expected['source'][:12]}"
    inner_name, manifest_name = stem + '.tar.gz', stem + '.manifest.json'
    require(set(install) == {inner_name, manifest_name, 'SHA256SUMS'},
            'Unexpected or missing installation ZIP payload')
    for name, data in install.items():
        # All names are the three fixed canonical basenames above.
        (scratch / name).write_bytes(data)
    bundle_result = verifier.verify(scratch / inner_name, expected['source'])
    manifest = parse_json(install[manifest_name])
    require(manifest.get('platform') == 'macos'
            and manifest.get('architecture') == expected['architecture'], 'Manifest architecture/platform mismatch')
    require(manifest.get('preset_library_revision') == expected['revision'], 'Manifest preset revision mismatch')
    require(manifest.get('workflow_run_id') == str(expected['run']), 'Manifest workflow run mismatch')
    require(manifest.get('juce_sha') == verifier.packager.JUCE_SHA
            and manifest.get('juce_version') == '8.0.11', 'Manifest JUCE source mismatch')
    inventory = {item['path']: item for item in manifest['files']}
    wanted = {base + '/Contents/MacOS/' + PRODUCT for base in BUNDLES.values()}
    wanted |= {'SOURCE.txt', 'evidence/juce-provenance.txt'}
    payloads = {}
    with tarfile.open(fileobj=io.BytesIO(install[inner_name]), mode='r|gz') as archive:
        for entry in archive:
            name = entry.name.removeprefix(stem + '/')
            if name in wanted:
                require(entry.isfile(), 'Required executable/source evidence must be a regular file: ' + name)
                payloads[name] = archive.extractfile(entry).read(MAX_FILE + 1)
    require(set(payloads) == wanted, 'Missing native executable or packaged source evidence')
    source_lines = payloads['SOURCE.txt'].decode('utf-8').splitlines()
    require(source_lines.count('Source commit: ' + expected['source']) == 1
            and source_lines.count('Preset library revision: ' + expected['revision']) == 1,
            'Packaged SOURCE.txt disagrees with expected source/revision')
    provenance = {}
    for line in payloads['evidence/juce-provenance.txt'].decode('utf-8').splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            require(key not in provenance, 'Duplicate packaged JUCE provenance key')
            provenance[key] = value
    require(all(provenance.get(k) == v for k, v in {
        'verification': 'PASS', 'source_clean': 'true', 'actual_sha': verifier.packager.JUCE_SHA,
        'actual_version': '8.0.11', 'workflow_commit': expected['source']}.items()),
        'Packaged JUCE evidence disagrees with expected source')
    bundles = {}
    for format_name, base in BUNDLES.items():
        executable = base + '/Contents/MacOS/' + PRODUCT
        item = inventory[executable]
        require(item['type'] == 'file' and int(item['mode'], 8) & 0o111,
                'Native executable lacks a regular executable inventory entry')
        binary = scratch / (format_name + '-header-input')
        binary.write_bytes(payloads[executable])
        verifier.packager.verify_binary(binary, 'macos', expected['architecture'], format_name == 'Standalone')
        entries = sorted((item for name, item in inventory.items()
                          if name == base or name.startswith(base + '/')), key=lambda x: x['path'])
        canonical = json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()
        bundles[format_name] = {'root': base, 'inventory_entries': len(entries),
                               'inventory_sha256': digest(canonical), 'executable': executable,
                               'executable_sha256': digest(payloads[executable]),
                               'advertised_architecture_present': expected['architecture'],
                               'binary_header_role': 'executable' if format_name == 'Standalone' else 'bundle/dylib',
                               'inventory': entries}
    before_name, after_name = 'au-payload-before-validation.sha256', 'au-payload-after-validation.sha256'
    require(before_name in evidence and after_name in evidence, 'Missing AU pre/post hash evidence')
    before, after = au_hashes(evidence[before_name]), au_hashes(evidence[after_name])
    require(before == after, 'AU pre/post executable hashes or paths differ')
    require(all(record['sha256'] == bundles['AU']['executable_sha256'] for record in before),
            'AU validation executable differs from distributed AU executable')
    executable_suffix = '/Contents/MacOS/' + PRODUCT
    source_suffix = '/build/CS80Reference_artefacts/Release/' + BUNDLES['AU'] + executable_suffix
    install_suffix = '/Library/Audio/Plug-Ins/Components/' + PRODUCT + '.component' + executable_suffix
    require(before[0]['path'].endswith(source_suffix) and before[1]['path'].endswith(install_suffix),
            'AU evidence source/install path roles differ from workflow')
    workspace, home = before[0]['path'][:-len(source_suffix)], before[1]['path'][:-len(install_suffix)]
    require(workspace and home, 'Missing AU workspace/home prefix')
    observed_paths = {}
    for lane, log_name in LANES.items():
        require(log_name in evidence, 'Missing validator console: ' + log_name)
        text = evidence[log_name].decode('utf-8')
        starts = re.findall(r'^Started validating: (.+)$', text, re.MULTILINE)
        expected_path = (workspace + '/build/CS80Reference_artefacts/Release/' + BUNDLES['VST3']
                         if lane == 'vst3' else before[1]['path'][:-len(executable_suffix)])
        require(starts == [expected_path], 'Validator input path or invocation count mismatch: ' + lane)
        fixed_seed = FIXED_SEEDS.get(lane)
        if fixed_seed is not None:
            seeds = re.findall(r'^Random seed: (.+)$', text, re.MULTILINE)
            require(seeds == [fixed_seed], 'Validator fixed seed or seed-record count mismatch: ' + lane)
        observed_paths[lane] = {'console': log_name, 'sha256': digest(evidence[log_name]),
                                'expected_fixed_seed': fixed_seed,
                                'path': expected_path,
                                'scope': 'Input-path and fixed-seed consistency only; lane success parsed separately.'}
    gates = [
        {'id': 'original-run-and-attempt', 'required': f"Authenticated run and job API records must identify source {expected['source']}, run {expected['run']}, attempt {expected['attempt']}, job macOS / {runner} / VST3 + AU + Standalone; require completed/success. Artifact receipts and manifests do not bind the attempt."},
        {'id': 'original-artifact-upload', 'required': 'Bind both artifact IDs, byte counts and SHA-256 digests above to the original successful Upload native macOS install bundle and Upload pluginval reports steps. Check creation/upload timestamps within those exact steps and authenticated repository identity.'},
        {'id': 'native-source-and-tests', 'required': 'Bind checkout, uname -m, clean JUCE source verification, configure, build, full native CTest/JUnit case inventory and test-evidence artifact to that exact job/attempt. A packaged PASS text does not establish its original execution.'},
        {'id': 'all-five-validator-lanes', 'required': 'Parse original reports and consoles for all five pluginval 1.0.4 lanes: strictness 10, 523 programs, zero failures, explicit SUCCESS, expected headless/GUI flags and fixed seeds 0x2b4eef5 and 0x3c99748 for the two separate AU GUI seed regressions; bind each original command, console/report identity and zero propagated exit to its successful API step. Verify the fetched pluginval asset receipt against the source-pinned downloader.'},
        {'id': 'whole-installed-au-bundle', 'required': 'The AU pre/post files bind only the executable. Bind successful, unsuppressed diff -qr "$AU_SOURCE" "$AU_DEST" before and after validation in the original Register built AU and Package native macOS VST3, AU and Standalone steps; require exact source/copy paths, signing before copying, original hash-output lines matching these files, and no intervening bundle mutation/rebuild. Only those original full-directory comparisons bind installed resources/signatures to the complete packaged AU inventory.'},
        {'id': 'validated-vst3-to-package-path', 'required': 'Bind the original VST3 command/Started validating path above to the source-pinned packager input build/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3 in the same job checkout. Prove ordered successful validation then packaging with no intervening mutation/rebuild. macOS does not upload an independent validated VST3 bundle; inventory consistency alone cannot establish validated-input equality.'},
        {'id': 'workflow-exit-and-order', 'required': 'Review the exact source-pinned macOS workflow and original timestamped command groups for set -euo pipefail, unsuppressed validation/grep/diff failures, no continue-on-error, no retries or substituted diagnostic runs, and the required register/validate/package/upload ordering. Source file hashes below identify review inputs, not proof that those commands ran.'},
        {'id': 'release-and-audition', 'required': 'Require every other native CI job and platform gate for this source separately. This helper does not establish signing/notarization, DAW runtime behavior, listening quality, instrument realism, or release qualification.'},
    ]
    return {'schema_version': 1, 'status': 'archive_consistency_and_au_executable_binding_verified',
            'release_qualified': False, 'validated_bundle_identity_established': False,
            'expected': expected, 'run_attempt_bound': False,
            'source_contract_files': source_records,
            'source_contract_scope': 'Exact committed files loaded/hashed; original workflow execution and shell semantics still require the gates below.',
            'downloads': {'install': identity(install_zip, install_data), 'pluginval': identity(pluginval_zip, pluginval_data)},
            'artifact_receipts': receipts, 'installation_verifier': bundle_result,
            'inner_archive': {'name': inner_name, 'sha256': digest(install[inner_name])},
            'manifest': {'name': manifest_name, 'sha256': digest(install[manifest_name]),
                         'source': manifest['commit'], 'revision': manifest['preset_library_revision'],
                         'workflow_run_id': manifest['workflow_run_id']},
            'packaged_bundles': bundles, 'au_before': before, 'au_after': after,
            'validator_input_paths': observed_paths, 'remaining_gates': gates,
            'limitations': ['API receipts are caller supplied; this tool does not authenticate them.',
                            'Mach-O headers prove advertised slice/role presence, not executable validity or exclusive single-architecture content.',
                            'Complete bundle inventories identify packaged bytes, types, paths, modes and link targets; they do not prove those bytes were validated.',
                            'No successful release, validator run, native runtime test or acoustic equivalence is inferred.']}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[5])
    parser.add_argument('--repository-full-name', default='cheshmakzanoon-ops/yamaha-cs80')
    parser.add_argument('--expected-source', required=True)
    parser.add_argument('--expected-run', required=True, type=int)
    parser.add_argument('--expected-attempt', required=True, type=int)
    parser.add_argument('--expected-architecture', required=True, choices=('arm64', 'x86_64'))
    parser.add_argument('--expected-revision', required=True)
    for name in ('install-zip', 'install-api', 'pluginval-zip', 'pluginval-api', 'out'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args(argv)
    require(re.fullmatch(r'[0-9a-f]{40}', args.expected_source), 'Expected full lowercase source SHA')
    require(args.expected_run > 0 and args.expected_attempt > 0, 'Expected positive run/attempt')
    require(re.fullmatch(r'[0-9]{4}\.(?:0[1-9]|1[0-2])(?:\.[1-9][0-9]*)?', args.expected_revision),
            'Expected canonical preset revision')
    require(re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', args.repository_full_name),
            'Invalid repository full name')
    require(not args.out.exists(), 'Refusing to overwrite an evidence report')
    expected = {'source': args.expected_source, 'run': args.expected_run,
                'attempt': args.expected_attempt, 'architecture': args.expected_architecture,
                'revision': args.expected_revision, 'repository': args.repository_full_name}
    with tempfile.TemporaryDirectory(prefix='a51-macos-binding-') as directory:
        scratch = Path(directory)
        verifier, records = load_source_contract(args.repo, expected['source'], expected['revision'], scratch)
        result = inspect_archives(expected, args.install_zip, args.install_api, args.pluginval_zip,
                                  args.pluginval_api, verifier, records, scratch)
    with args.out.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps({'status': result['status'], 'release_qualified': False,
                      'remaining_gates': [gate['id'] for gate in result['remaining_gates']],
                      'out': str(args.out)}))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, UnicodeError, zipfile.BadZipFile,
            tarfile.TarError, subprocess.CalledProcessError) as exc:
        print('macOS binding rejected: ' + str(exc), file=sys.stderr)
        raise SystemExit(1)
