#!/usr/bin/env python3
"""A57: independently inspect actual ee0dabca packages; never qualify a release.

Runs the unchanged source-pinned A41 verifier and the reviewed guide/notice
supplement, then independently streams all archive members and reads native
headers. No downloaded product is executed. Reports are exclusive creations.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path
import plistlib
import stat
import struct
import subprocess
import sys
import tarfile
import time
import zipfile

sys.dont_write_bytecode = True
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
REVISION, RUN, ATTEMPT = '2026.11.7', 45397189410, 1
A41 = 'docs/preset-evidence/2026.11.5/verification/a41-verify-install-artifact.py'
UNIX_DOCS = 'docs/preset-evidence/2026.11.7/verification/a48/inspect-packaged-docs.py'
WINDOWS_DOCS = 'docs/preset-evidence/2026.11.7/verification/a46/verify-windows-docs.py'
PINS = {
    A41: '803ff4378746937039400e777ff61bfce8ebaeb2ab1bba6699fe76076f520305',
    UNIX_DOCS: '8ac1fb82206e92131eed0523dd569ed7b97460b8158d9951e056c105e2f7723d',
    WINDOWS_DOCS: '5d3ac203c38356f736f07351507723069dc9528e6cee9e9b577a520bf71ae2ee',
}
TARGETS = {
    'macos-arm64': ('macos', 'arm64', 'cs80-macos-15-install-bundle'),
    'macos-x86_64': ('macos', 'x86_64', 'cs80-macos-15-intel-install-bundle'),
    'windows-x86_64': ('windows', 'x86_64', 'cs80-windows-install-bundle'),
    'linux-x86_64': ('linux', 'x86_64', 'cs80-linux-install-bundle'),
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def identity(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def file_identity(path):
    path = Path(path)
    with path.open('rb') as stream:
        return {'bytes': path.stat().st_size,
                'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()}


def write_json(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def stream_record(stream, expected_size):
    h, count, first, small = hashlib.sha256(), 0, bytearray(), bytearray()
    for chunk in iter(lambda: stream.read(1024 * 1024), b''):
        h.update(chunk)
        count += len(chunk)
        if len(first) < 4096:
            first.extend(chunk[:4096 - len(first)])
        if expected_size <= 2 * 1024 * 1024:
            small.extend(chunk)
    require(count == expected_size, 'Stream size changed')
    return {'bytes': count, 'sha256': h.hexdigest()}, bytes(first), bytes(small)


def native_header(prefix, platform, architecture, standalone):
    if platform == 'macos':
        require(prefix[:4] == b'\xcf\xfa\xed\xfe', 'Expected actual thin little-endian Mach-O64')
        cpu, subtype, image_type = struct.unpack_from('<III', prefix, 4)
        require(cpu == {'arm64': 0x0100000c, 'x86_64': 0x01000007}[architecture], 'Wrong Mach-O CPU')
        require(image_type in ({2} if standalone else {6, 8}), 'Wrong Mach-O native role')
        return dict(format='thin Mach-O64 little-endian', cpu_type=hex(cpu), cpu_subtype=hex(subtype),
                    image_type=image_type, universal=False)
    if platform == 'windows':
        require(prefix[:2] == b'MZ', 'Missing DOS header')
        offset = int.from_bytes(prefix[60:64], 'little')
        require(64 <= offset <= len(prefix) - 26 and prefix[offset:offset + 4] == b'PE\0\0', 'Missing PE header')
        machine = int.from_bytes(prefix[offset + 4:offset + 6], 'little')
        characteristics = int.from_bytes(prefix[offset + 22:offset + 24], 'little')
        optional_magic = int.from_bytes(prefix[offset + 24:offset + 26], 'little')
        require(machine == 0x8664 and optional_magic == 0x20b and characteristics & 2,
                'Expected x86-64 PE32+ executable')
        require(bool(characteristics & 0x2000) == (not standalone), 'Wrong PE DLL role')
        return dict(format='PE32+', machine=hex(machine), optional_header_magic=hex(optional_magic),
                    characteristics=hex(characteristics), dll=bool(characteristics & 0x2000))
    require(prefix[:7] == b'\x7fELF\x02\x01\x01', 'Expected little-endian ELF64')
    image_type, machine = struct.unpack_from('<HH', prefix, 16)
    require(machine == 62 and image_type in ({2, 3} if standalone else {3}), 'Wrong ELF CPU/native role')
    return dict(format='ELF64 little-endian', machine=machine, image_type=image_type)


def direct_archive(archive_path, receipt, platform, architecture):
    outer_raw = archive_path.read_bytes()
    require(identity(outer_raw) == {k: receipt['outer_zip'][k] for k in ('bytes', 'sha256')}, 'Outer bytes differ')
    inner_name = receipt['inner_archive']['file']
    root = inner_name.removesuffix('.tar.gz').removesuffix('.zip')
    with zipfile.ZipFile(io.BytesIO(outer_raw)) as outer:
        require(sorted(outer.namelist()) == receipt['outer_zip']['members'], 'Outer member set differs')
        manifest_raw = outer.read(root + '.manifest.json')
        inner_raw = outer.read(inner_name)
        sums = outer.read('SHA256SUMS').decode('utf-8')
    require(identity(inner_raw) == {k: receipt['inner_archive'][k] for k in ('bytes', 'sha256')}, 'Inner bytes differ')
    manifest = json.loads(manifest_raw)
    require(manifest['files'] == receipt['inventory'], 'Source-authoritative inventory differs')
    require(manifest['commit'] == SOURCE and manifest['preset_library_revision'] == REVISION
            and manifest['workflow_run_id'] == str(RUN), 'Manifest source/revision/run differs')
    expected_sums = {inner_name: identity(inner_raw)['sha256'], root + '.manifest.json': identity(manifest_raw)['sha256']}
    actual_sums = {}
    for line in sums.splitlines():
        digest, name = line.split('  ', 1)
        require(name not in actual_sums, 'Duplicate checksum member')
        actual_sums[name] = digest
    require(actual_sums == expected_sums, 'Outer checksum manifest differs')
    inventory = {item['path']: item for item in receipt['inventory']}
    require(len(inventory) == len(receipt['inventory']), 'Duplicate inventory path')
    observed, prefixes, small_files, actual_modes = {}, {}, {}, {}

    def record(name, mode, kind, size, stream=None, link=None):
        require(name.startswith(root + '/'), 'Unexpected inner root')
        relative = name[len(root) + 1:]
        require(relative not in observed, 'Duplicate inner path')
        actual_modes[relative] = oct(mode)
        if kind == 'file':
            data, prefix, small = stream_record(stream, size)
            observed[relative] = dict(type='file', **data)
            prefixes[relative], small_files[relative] = prefix, small
        elif kind == 'directory':
            observed[relative] = dict(type='directory')
        elif kind == 'symlink':
            observed[relative] = dict(type='symlink', target=link)
        else:
            raise ValueError('Unsupported actual member type')

    if platform == 'windows':
        with zipfile.ZipFile(io.BytesIO(inner_raw)) as inner:
            for member in inner.infolist():
                kind = 'directory' if member.is_dir() else 'file'
                require(stat.S_IFMT(member.external_attr >> 16) in (0, stat.S_IFREG, stat.S_IFDIR),
                        'Unexpected Windows special object')
                with inner.open(member) as stream:
                    record(member.filename.rstrip('/'), stat.S_IMODE(member.external_attr >> 16),
                           kind, member.file_size, stream)
    else:
        with tarfile.open(fileobj=io.BytesIO(inner_raw), mode='r:gz') as inner:
            for member in inner:
                kind = 'file' if member.isfile() else 'directory' if member.isdir() else 'symlink' if member.issym() else 'unsupported'
                if member.isfile():
                    with inner.extractfile(member) as stream:
                        record(member.name, member.mode, kind, member.size, stream)
                else:
                    record(member.name, member.mode, kind, member.size, link=member.linkname)
    require(set(observed) == set(inventory) | {'manifest.json'}, 'Actual versus declared inventory membership differs')
    require(small_files['manifest.json'] == manifest_raw, 'Embedded/sidecar manifest bytes differ')
    for name, item in inventory.items():
        expected = {k: v for k, v in item.items() if k not in ('path', 'mode', 'size')}
        if 'size' in item:
            expected['bytes'] = item['size']
        require(observed[name] == expected, 'Independent member digest/type differs: ' + name)
        require(platform == 'windows' or actual_modes[name] == item['mode'], 'Independent TAR mode differs: ' + name)
    headers = []
    for item in receipt['binary_headers']:
        name = item['path']
        parsed = native_header(prefixes[name], platform, architecture, item['role'] == 'standalone')
        headers.append(dict(path=name, **observed[name], inventory_mode=inventory[name]['mode'],
                            actual_archive_mode=actual_modes[name], role=item['role'], header=parsed))
    plists = []
    for name, raw in small_files.items():
        if name.endswith('/Info.plist'):
            value = plistlib.loads(raw)
            plists.append(dict(path=name, identifier=value.get('CFBundleIdentifier'),
                               executable=value.get('CFBundleExecutable'), package_type=value.get('CFBundlePackageType')))
    return dict(status='independent_actual_inventory_and_headers_verified',
                manifest_metadata={k: v for k, v in manifest.items() if k != 'files'},
                inventory_counts=dict(Counter(x['type'] for x in inventory.values())),
                inventory_entries=len(inventory), actual_members_including_embedded_manifest=len(observed),
                complete_member_types_sizes_hashes_verified=True,
                tar_modes_verified=platform != 'windows', windows_zip_mode_equality_required=False,
                embedded_sidecar_manifest_equal=True, exact_outer_checksums_verified=True,
                binary_headers=headers, mac_bundle_plists=plists,
                source_note=small_files['SOURCE.txt'].decode('utf-8'),
                juce_provenance=small_files['evidence/juce-provenance.txt'].decode('utf-8'),
                manifest_sha256=identity(manifest_raw)['sha256'],
                no_downloaded_product_executed=True, permanent_binary_extraction=False,
                platform_qualified=False, release_qualified=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--target', choices=TARGETS, required=True)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--input-dir', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    platform, architecture, artifact_name = TARGETS[args.target]
    require(not args.out_dir.exists(), 'Refusing to replace any existing target output')
    tree = subprocess.check_output(['git', '-C', str(args.repo), 'rev-parse', SOURCE + '^{tree}'], text=True).strip()
    require(tree == TREE, 'Frozen source tree differs')
    docs_helper = WINDOWS_DOCS if platform == 'windows' else UNIX_DOCS
    for helper in (A41, docs_helper):
        require(file_identity(args.repo / helper)['sha256'] == PINS[helper], 'Reviewed helper pin differs: ' + helper)
    candidates = sorted(args.input_dir.glob('attempt1-api-at-*-completion.json'))
    require(len(candidates) == 1, 'Expected one explicit original-attempt snapshot')
    run_api = candidates[0]
    acquisition_candidates = [p for p in (args.input_dir / 'retrieval-receipt.json',
                                         args.input_dir / 'producer-retrieval-receipt.json') if p.is_file()]
    require(len(acquisition_candidates) == 1, 'Expected one original acquisition receipt')
    acquisition_path = acquisition_candidates[0]
    acquisition = json.loads(acquisition_path.read_bytes())
    require(acquisition['source_commit'] == SOURCE and acquisition['run_id'] == RUN
            and acquisition['attempt'] == ATTEMPT, 'Acquisition context differs')
    input_records = {str(acquisition_path): file_identity(acquisition_path)}
    for name, expected in acquisition['files'].items():
        path = args.input_dir / name
        actual = file_identity(path)
        require(actual == expected, 'Authenticated acquisition digest differs: ' + name)
        input_records[str(path)] = actual
    archive_record = next(x for x in acquisition['archives'] if x['kind'] == 'install')
    require(file_identity(args.archive) == {k: archive_record[k] for k in ('bytes', 'sha256')}, 'Acquired archive differs')
    input_records[str(args.archive)] = file_identity(args.archive)
    args.out_dir.mkdir(parents=True)
    commands = []

    def run(name, argv):
        started, before = datetime.now(timezone.utc).isoformat(), time.monotonic()
        result = subprocess.run(argv, cwd=args.repo, env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'),
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
        log = args.out_dir / (name + '.log')
        with log.open('xb') as stream:
            stream.write(result.stdout)
        commands.append(dict(name=name, argv=[str(x) for x in argv], cwd=str(args.repo),
                             started_at_utc=started, elapsed_seconds=time.monotonic() - before,
                             returncode=result.returncode, log=str(log), log_identity=identity(result.stdout)))
        write_json(args.out_dir / (name + '-command.json'), commands[-1])
        require(result.returncode == 0, name + ' failed; retained complete log')

    acceptance = args.out_dir / 'install-acceptance.json'
    documents = args.out_dir / 'docs-and-notices.json'
    run('inspect-install', [sys.executable, '-B', A41, '--repo', str(args.repo), '--source', SOURCE,
                           '--revision', REVISION, '--run-id', str(RUN), '--attempt', str(ATTEMPT),
                           '--run-api', str(run_api), '--platform', platform, '--architecture', architecture,
                           '--archive', str(args.archive), '--artifact-api', str(args.input_dir / 'install-api.json'),
                           '--out', str(acceptance)])
    argv = [sys.executable, '-B', docs_helper, '--repo', str(args.repo), '--archive', str(args.archive),
            '--acceptance', str(acceptance), '--out', str(documents)]
    if platform != 'windows':
        argv += ['--source', SOURCE]
    run('inspect-docs-notices', argv)
    receipt = json.loads(acceptance.read_bytes())
    require(receipt['artifact']['name'] == artifact_name and receipt['source_tree'] == TREE, 'Receipt identity differs')
    direct = direct_archive(args.archive, receipt, platform, architecture)
    write_json(args.out_dir / 'direct-archive-crosscheck.json', direct)
    reference = args.input_dir / 'install-acceptance.json'
    reference_result = None
    if reference.is_file():
        other = json.loads(reference.read_bytes())
        compared = ['source', 'source_tree', 'preset_library_revision', 'run_id', 'run_attempt', 'target',
                    'artifact', 'outer_zip', 'inner_archive', 'manifest_sha256', 'checksum_manifest_sha256',
                    'inventory', 'binary_headers', 'source_contract_files', 'signing_status']
        require(all(receipt[key] == other[key] for key in compared), 'Independent result differs from accepted receipt')
        reference_result = dict(path=str(reference), **file_identity(reference), exact_equal_fields=compared)
    for path, expected in input_records.items():
        require(file_identity(path) == expected, 'Original input changed during independent inspection: ' + path)
    for helper in (A41, docs_helper):
        require(file_identity(args.repo / helper)['sha256'] == PINS[helper], 'Helper changed during inspection')
    write_json(args.out_dir / 'crosscheck-summary.json', dict(
        schema=1, status='actual_package_crosscheck_passed', source_commit=SOURCE, source_tree=TREE,
        factory_revision=REVISION, run_id=RUN, run_attempt=ATTEMPT, target=args.target,
        verified_at_utc=datetime.now(timezone.utc).isoformat(), artifact=receipt['artifact'],
        outer_zip=receipt['outer_zip'], inner_archive=receipt['inner_archive'],
        independent_install_receipt=dict(path=str(acceptance), **file_identity(acceptance)),
        independent_document_receipt=dict(path=str(documents), **file_identity(documents)),
        direct_archive_receipt=dict(path=str(args.out_dir / 'direct-archive-crosscheck.json'),
                                    **file_identity(args.out_dir / 'direct-archive-crosscheck.json')),
        accepted_reference_comparison=reference_result, input_files=input_records,
        inputs_unchanged=True, helper_pins={k: PINS[k] for k in (A41, docs_helper)},
        verifier_sha256=file_identity(__file__)['sha256'], commands=commands,
        platform_qualification_claimed=False, release_qualified=False,
        scope='Actual installation bytes, exact source contract and retained authenticated acquisition; native/validator/original-exit acceptance and public availability remain external.'))
    print(json.dumps(dict(status='actual_package_crosscheck_passed', target=args.target,
                          inventory_entries=direct['inventory_entries'], out=str(args.out_dir))))


if __name__ == '__main__':
    main()
