#!/usr/bin/env python3
"""Independent partial cross-platform audit. Never grants all-platform acceptance.

Reads actual archives without executing product binaries. Retained API snapshots
and the original job log supply remote identity; a boolean qualification field
alone is never sufficient. Missing platform observations remain missing.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import io
import json
import re
import struct
import subprocess
import tarfile
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[5]
BASE = ROOT / 'docs/preset-evidence/2026.11.6/ci/source-fcc3c8e'
OUT = Path(__file__).resolve().parent
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
RUN = 45366603800
ATTEMPT = 1
REVISION = '2026.11.6'
ARM = BASE / 'macos-arm64'
MAC_LANES = ['vst3', 'au', 'au-gui', 'au-gui-seed', 'au-gui-seed-c76']


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def receipt(data):
    return {'bytes': len(data), 'sha256': sha(data)}


def verify_receipt(data, item):
    require(receipt(data) == {k: item[k] for k in ('bytes', 'sha256')}, 'bytes/hash receipt mismatch')


def source(path):
    return subprocess.check_output(['git', '-C', str(ROOT), 'show', SOURCE + ':' + path])


def identity(record):
    require(record['source_commit'] == SOURCE, 'source mismatch')
    require(record['run_id'] == RUN and record['run_attempt'] == ATTEMPT, 'run/attempt mismatch')
    require(record['factory_revision'] == REVISION, 'factory revision mismatch')


def check_lanes(outcomes):
    require([x['lane'] for x in outcomes] == MAC_LANES, 'five unique Mac lanes required')
    for item in outcomes:
        require(item['strictness'] == 10 and item['programs'] == 523, 'validator scope mismatch')
        require(item['original_normal_exit'] == 0, 'nonzero original validator exit')
    require(outcomes[3]['seed'] == '0x2b4eef5' and outcomes[4]['seed'] == '0x3c99748', 'fixed seed mismatch')


def main(archive_dir):
    q = read(ARM / 'platform-qualification.json')
    identity(q)
    for item in list(q['inputs'].values()) + [q['a45_gui_qualification'], q['original_job_log'], q['verifier']]:
        verify_receipt((ROOT / item['path']).read_bytes(), item)
    for path, item in q['source_files'].items():
        verify_receipt(source(path), item)
    job = read(ARM / 'job-api.json')
    require((job['id'], job['run_id'], job['run_attempt'], job['head_sha'], job['conclusion']) ==
            (138349816955, RUN, ATTEMPT, SOURCE, 'success'), 'original job identity/success mismatch')
    run = read(ARM / 'run-api.json')
    require((run['id'], run['run_attempt'], run['head_sha']) == (RUN, ATTEMPT, SOURCE), 'run API mismatch')
    steps = {x['name']: x for x in job['steps']}
    original_log = (ROOT / q['original_job_log']['path']).read_text()
    zips, actual_artifacts = {}, []
    for upload in q['uploads']:
        api = read(ARM / (upload['kind'] + '-api.json'))
        raw = (archive_dir / (str(api['id']) + '.zip')).read_bytes()
        verify_receipt(raw, upload)
        require(api['id'] == upload['artifact_id'] and api['name'] == upload['name'], 'artifact identity mismatch')
        require(api['size_in_bytes'] == len(raw) and api['digest'] == 'sha256:' + sha(raw), 'API digest/size mismatch')
        require(api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE, 'artifact source/run mismatch')
        require(not api['expired'], 'artifact expired at snapshot')
        require(steps[upload['upload_step']] == upload['original_api_step'], 'original upload step mismatch')
        require(steps[upload['upload_step']]['conclusion'] == 'success', 'upload step unsuccessful')
        marker = f"Artifact {api['name']} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {api['id']}"
        require(original_log.count(marker) == 1 and ('SHA256 digest of uploaded artifact zip is ' + sha(raw)) in original_log,
                'missing exact original upload identity/digest')
        zips[upload['kind']] = zipfile.ZipFile(io.BytesIO(raw))
        actual_artifacts.append({'role': upload['kind'], 'id': api['id'], 'name': api['name'], **receipt(raw),
                                 'expires_at': api['expires_at'], 'original_upload_step': upload['upload_step']})
    install = zips['install']
    require(len(install.namelist()) == len(set(install.namelist())) == 3, 'unexpected outer package roster')
    for line in install.read('SHA256SUMS').decode().splitlines():
        digest, name = line.split('  ', 1)
        require(sha(install.read(name)) == digest, 'inner archive/manifest checksum mismatch')
    manifest = json.loads(install.read(next(n for n in install.namelist() if n.endswith('.manifest.json'))))
    require((manifest['commit'], manifest['preset_library_revision'], manifest['workflow_run_id']) ==
            (SOURCE, REVISION, str(RUN)), 'install manifest identity mismatch')
    require(manifest['formats'] == ['VST3', 'Standalone', 'AU'], 'Mac format set mismatch')
    require((manifest['platform'], manifest['architecture']) == ('macos', 'arm64'), 'native target mismatch')
    require((manifest['juce_sha'], manifest['juce_version']) ==
            ('c352e2489000ffeb9e397051377d75eef2efe641', '8.0.11'), 'JUCE identity mismatch')
    tgz_name = next(n for n in install.namelist() if n.endswith('.tar.gz'))
    prefix = tgz_name.removesuffix('.tar.gz') + '/'
    with tarfile.open(fileobj=io.BytesIO(install.read(tgz_name)), mode='r:gz') as archive:
        regular = {m.name.removeprefix(prefix): archive.extractfile(m).read() for m in archive.getmembers() if m.isfile()}
    for item in manifest['files']:
        if item['type'] == 'file':
            require(len(regular[item['path']]) == item['size'] and sha(regular[item['path']]) == item['sha256'],
                    'inner file inventory mismatch')
    for item in q['binary_headers']:
        raw = regular[item['path']]
        verify_receipt(raw, item)
        require(raw[:4] == bytes.fromhex('cffaedfe') and struct.unpack('<I', raw[4:8])[0] == 0x100000c,
                'actual Mach-O ARM64 header mismatch')
    doc_audit = read(ROOT / 'docs/preset-evidence/2026.11.6/verification/a53/package-doc-audit.json')
    shared = {**doc_audit['document_receipts'], **doc_audit['notice_receipts']}
    for member, item in shared.items():
        verify_receipt(regular[member], item)
    require(f'Source commit: {SOURCE}\n' in regular['SOURCE.txt'].decode(), 'packaged SOURCE mismatch')
    require('204 core parameters and 310 host positions' in regular['PRESET_GUIDE.md'].decode(), 'factory parameter metadata mismatch')
    xml = zips['native'].read('ctest-results.xml')
    cases = ET.fromstring(xml).findall('testcase')
    require(len(cases) == len({x.attrib['name'] for x in cases}) == 273, 'actual native registration mismatch')
    skipped = [x.attrib['name'] for x in cases if x.find('skipped') is not None]
    require(sorted(skipped) == ['cs80_filter_batch_cap_tests', 'cs80_stereo_pra_exact_tests'], 'formal skip roster mismatch')
    require(not any(x.find('failure') is not None or x.find('error') is not None for x in cases), 'actual native failure')
    for member in ('ctest-results.xml', 'Testing/Temporary/LastTest.log', 'ci-juce-provenance.txt'):
        require(zips['native'].read(member) == (ARM / 'native-members' / member).read_bytes(), 'native member byte mismatch')
    last = zips['native'].read('Testing/Temporary/LastTest.log').decode()
    require('instrument body host identity and recall' in last, 'expanded body host phase absent')
    nested = sum(int(x) for x in re.findall(r'^OK \(skipped=(\d+)\)', last, re.M))
    require(nested == q['nested_python_skips'] == 5, 'nested Python skip count mismatch')
    gui = read(ROOT / q['a45_gui_qualification']['path'])
    check_lanes(gui['outcomes'])
    for lane in gui['outcomes']:
        require(steps[lane['step']]['conclusion'] == 'success', 'original validator step unsuccessful')
        for role in ('console', 'report'):
            item = lane[role]
            raw = zips['validator'].read(item['path'])
            verify_receipt(raw, item)
            require(raw.decode().strip().endswith('SUCCESS'), 'validator member lacks terminal success')
        require(lane['seed'] in zips['validator'].read(lane['console']['path']).decode(), 'actual validator seed absent')
    pending = {}
    contracts = {
        'windows-x86_64': ('windows/source-contract.json', 'source', 'native_run_id', 'attempt', 'expected_registered_derived', 251),
        'macos-x86_64': ('macos-x86_64/preparation-contract.json', 'source_commit', 'run_id', 'run_attempt', 'expected_registered_count', 273),
        'linux-x86_64': ('linux/preparation-contract.json', 'source', 'run_id', 'attempt', 'expected_registered_count_pending_actual', 609),
    }
    for target, (path, sk, rk, ak, ck, expected) in contracts.items():
        d = read(BASE / path)
        require((d[sk], d[rk], d[ak], d[ck]) == (SOURCE, RUN, ATTEMPT, expected), 'pending source contract disagreement')
        pending[target] = {'status': 'pending_actual_platform_qualification', 'expected_registrations_provisional': expected,
                           'observed_registrations': None, 'contract': str((BASE/path).relative_to(ROOT)),
                           'contract_receipt': receipt((BASE/path).read_bytes())}
    lane_map = {'macos-arm64': MAC_LANES, 'macos-x86_64': MAC_LANES,
                'windows-x86_64': ['headless-randomized-in-process', 'headless-seed-0x5037a0c-in-process'],
                'linux-x86_64': ['headless', 'gui', 'gui-seed-0x2da72db']}
    head = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    protected = subprocess.check_output(['git', '-C', str(ROOT), 'diff', '--name-only', SOURCE, head,
                                        '--', 'src', 'tests', 'scripts', '.github', 'CMakeLists.txt'], text=True).splitlines()
    require(not protected, 'later HEAD alters protected compiled/test/packaging source')
    result = {
        'schema': 1, 'status': 'partial_one_of_four_platforms_observed', 'release_qualified': False,
        'cross_platform_qualified': False, 'source_commit': SOURCE, 'factory_revision': REVISION,
        'run_id': RUN, 'run_attempt': ATTEMPT, 'documentation_head_observed': head,
        'compiled_source_and_documentation_head_distinct': head != SOURCE,
        'protected_source_changes_to_observed_head': protected,
        'observed_arm': {'actual_artifacts': actual_artifacts, 'native_counts': q['native_counts'],
                         'formal_skip_names': skipped, 'nested_python_skips': nested,
                         'formats': manifest['formats'], 'architecture': 'arm64',
                         'five_original_validator_steps_successful': True,
                         'shared_member_receipts': {k: receipt(regular[k]) for k in shared}},
        'pending': pending, 'required_validator_lanes': lane_map,
        'required_validator_count': sum(map(len, lane_map.values())), 'observed_validator_count': 5,
        'completion_requirements': [
            'Obtain and independently bind all three missing platforms to source/revision/run/attempt, actual ZIP API and original successful uploads.',
            'Reconcile each actual native roster and formal/nested skips; do not turn provisional counts into observations.',
            'Bind all fifteen original validator lanes and platform-specific validator input payloads; preserve retry/core evidence.',
            'Compare exact source-time guides/notices and factory metadata across all four; verify PE32+ x64, Mach-O x86_64/ARM64 and ELF64 x86_64 format sets.',
            'Require final native run, supporting jobs, actual audition/inventory, public downloads and publication gates outside this checker.'
        ],
        'limits': [
            'This bounded checker always leaves cross_platform_qualified and release_qualified false; root must complete missing platform observations.',
            'Shared guide/notice consistency is byte equality; compiled binaries must differ by platform and architecture.',
            'A53 establishes source-time guide/license content; this checker independently rehashes those members from the actual install ZIP.',
            'Frozen bundled distribution history is the .5 candidate/.4 publication snapshot; later online documentation HEAD is not the compiled source.',
            'No product binary executed locally, acoustic equivalence, new crash root cause, or cross-architecture binary equality is claimed.'
        ]}
    (OUT / 'partial-consistency.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'observed_platforms': 1, 'pending_platforms': 3, 'observed_validator_lanes': 5,
                      'required_validator_lanes': 15, 'cross_platform_qualified': False}, indent=2))


def negatives():
    q = read(ARM / 'platform-qualification.json')
    gui = read(ROOT / q['a45_gui_qualification']['path'])
    checks = []
    mutations = [('wrong_source', 'source_commit', '2a8d6ed580c6527226c81437393206ac8b0cd5a4'),
                 ('wrong_run', 'run_id', RUN + 1), ('wrong_attempt', 'run_attempt', 2),
                 ('wrong_revision', 'factory_revision', '2026.11.5')]
    for name, key, value in mutations:
        bad = copy.deepcopy(q); bad[key] = value
        try: identity(bad)
        except ValueError: checks.append(name)
        else: raise AssertionError(name + ' accepted')
    for name, operation in [('wrong_hash', lambda: verify_receipt(b'changed', receipt(b'original'))),
                            ('missing_fifth_lane', lambda: check_lanes(gui['outcomes'][:-1]))]:
        try: operation()
        except ValueError: checks.append(name)
        else: raise AssertionError(name + ' accepted')
    (OUT / 'negative-controls.json').write_text(json.dumps({'rejected': checks, 'count': len(checks)}, indent=2)+'\n')
    print('Rejected six deliberate identity/hash/lane mutations.')


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--archive-dir', type=Path, default=ROOT.parent / 'native-artifacts')
    p.add_argument('--negative-controls', action='store_true')
    args = p.parse_args()
    if args.negative_controls: negatives()
    else: main(args.archive_dir)
