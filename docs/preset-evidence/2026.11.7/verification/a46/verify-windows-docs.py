#!/usr/bin/env python3
"""Exact ee0dabca Windows metadata supplement; never qualify a platform/release.

The unchanged A30 guide mapping and inline-link inspection are reused by hash.
Only the seven copied repository notices may use one common, demonstrated full
LF-to-CRLF checkout transform. Actual original checkout/package provenance is a
separate mandatory gate. No downloaded product is imported or executed.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
import zipfile

sys.dont_write_bytecode = True
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
RUN = 45397189410
CONTEXT = dict(source=SOURCE, source_tree=TREE, revision='2026.11.7',
               run_id=RUN, attempt=1, target='windows-x86_64')
A30 = 'docs/preset-evidence/2026.11.7/verification/a30/inspect-packaged-docs.py'
A41 = 'docs/preset-evidence/2026.11.5/verification/a41-verify-install-artifact.py'
PINS = {A30: '2f4c7584a2de26f7b1c2e205ed28c05cd75515ffca3094e760ac851bfb239132',
        A41: '803ff4378746937039400e777ff61bfce8ebaeb2ab1bba6699fe76076f520305'}
WORKFLOW_SHA = '9cb87b71aefd18f0e753f203d8f2fa2b537935fedde33fdc87f49eb64b04fccb'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def identity(raw):
    return dict(bytes=len(raw), sha256=sha(raw))


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def dependencies(repo):
    for relative, expected in PINS.items():
        require(sha((repo / relative).read_bytes()) == expected,
                'Historical dependency differs: ' + relative)
    return module(repo / A30, 'a46_original_a30'), module(repo / A41, 'a46_original_a41')


def compare_notices(source_payloads, checkout_payloads, packaged_payloads):
    """No newline normalization: compare every actual raw byte in one mode."""
    require(len(source_payloads) == 7 and set(source_payloads) == set(checkout_payloads)
            == set(packaged_payloads), 'Expected exactly the same seven notice paths')
    for name, source in source_payloads.items():
        require(source and b'\n' in source and b'\r' not in source and b'\0' not in source
                and not source.startswith(b'\xef\xbb\xbf'), 'Source is not the reviewed LF-only notice: ' + name)
        require(checkout_payloads[name] == source.replace(b'\n', b'\r\n'),
                'Checkout differs from the one permitted complete transform: ' + name)
    raw_mode = all(packaged_payloads[name] == source for name, source in source_payloads.items())
    crlf_mode = all(packaged_payloads[name] == checkout_payloads[name] for name in source_payloads)
    require(raw_mode or crlf_mode, 'Notices are neither all raw source nor all complete source-checkout CRLF bytes')
    require(not (raw_mode and crlf_mode), 'Ambiguous notice modes')
    return 'all_raw_source_bytes' if raw_mode else 'all_complete_lf_to_crlf_checkout_bytes'


def source_contract(repo, scratch, reference):
    """Use an isolated repository/index; shared checkout/index/refs are untouched."""
    require(git(repo, 'rev-parse', SOURCE + '^{tree}').decode().strip() == TREE,
            'Frozen source tree differs')
    source_records = {}

    def blob(path):
        raw = git(repo, 'show', SOURCE + ':' + path)
        source_records[path] = identity(raw)
        return raw

    workflow = blob('.github/workflows/build.yml')
    require(sha(workflow) == WORKFLOW_SHA, 'Frozen native workflow differs')
    source = {path: blob(path) for path in reference.NOTICES}
    attrs = [name for name in git(repo, 'ls-tree', '-r', '--name-only', SOURCE).decode().splitlines()
             if name.rsplit('/', 1)[-1] == '.gitattributes']
    require(attrs == ['.gitattributes'], 'Source attribute-file roster changed')
    attribute_bytes = blob('.gitattributes')
    clean = scratch / 'controlled-checkout'
    env = os.environ.copy()
    for key in ('GIT_DIR', 'GIT_WORK_TREE', 'GIT_INDEX_FILE', 'GIT_COMMON_DIR', 'GIT_CONFIG_COUNT',
                'GIT_OBJECT_DIRECTORY', 'GIT_ALTERNATE_OBJECT_DIRECTORIES'):
        env.pop(key, None)
    env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_SYSTEM=os.devnull, GIT_ATTR_NOSYSTEM='1')
    subprocess.run(['git', 'init', '-q', str(clean)], check=True, env=env)
    objects = Path(git(repo, 'rev-parse', '--git-path', 'objects').decode().strip())
    if not objects.is_absolute():
        objects = repo / objects
    env['GIT_OBJECT_DIRECTORY'] = str(objects.resolve())
    base = ['git', '-C', str(clean), '-c', 'core.autocrlf=true', '-c', 'core.eol=crlf',
            '-c', 'core.attributesFile=' + os.devnull]
    commands = [base + ['read-tree', SOURCE],
                base + ['checkout-index', '--', '.gitattributes', *source]]
    for command in commands:
        subprocess.run(command, check=True, env=env, capture_output=True)
    require((clean / '.gitattributes').read_bytes() == attribute_bytes.replace(b'\n', b'\r\n'),
            'Controlled source attributes changed beyond checkout EOL')
    attr_command = base + ['check-attr', '--cached', 'text', 'eol', 'filter', 'working-tree-encoding', '--', *source]
    attr_text = subprocess.check_output(attr_command, env=env).decode()
    require(len(attr_text.splitlines()) == 28 and all(line.endswith(': unspecified')
            for line in attr_text.splitlines()), 'Unexpected notice attribute or filter')
    checkout = {path: (clean / path).read_bytes() for path in source}
    require(compare_notices(source, checkout, source) == 'all_raw_source_bytes', 'Source-mode control')
    require(compare_notices(source, checkout, checkout) == 'all_complete_lf_to_crlf_checkout_bytes',
            'Checkout-mode control')
    diff_command = base + ['diff', '--quiet', SOURCE, '--', '.gitattributes', *source]
    subprocess.run(diff_command, check=True, env=env)

    snapshot = scratch / 'source-docs'
    packager_path = snapshot / 'scripts/package-ci-artifacts.py'
    packager_path.parent.mkdir(parents=True)
    packager_path.write_bytes(blob('scripts/package-ci-artifacts.py'))
    packager = module(packager_path, 'a46_exact_source_packager')
    guides = {}
    for path, destination in reference.GUIDES.items():
        local = snapshot / path
        local.parent.mkdir(parents=True, exist_ok=True)
        local.write_bytes(blob(path))
        guides[destination] = packager.packaged_markdown(path, SOURCE, reference.GUIDES)

    proof = dict(schema=1, status='source_checkout_transform_prepared_no_actual_archive', **CONTEXT,
                 recorded_at_utc=datetime.now(timezone.utc).isoformat(),
                 helper_sha256=PINS, source_files=source_records,
                 isolated_source_checkout=True, shared_index_or_refs_modified=False,
                 controlled_configuration={'core.autocrlf': 'true', 'core.eol': 'crlf',
                                           'global_and_system_configuration': 'disabled',
                                           'global_and_system_attributes': 'disabled'},
                 checkout_commands=commands, attributes_command=attr_command,
                 attributes=attr_text.splitlines(), scoped_clean_diff_command=diff_command,
                 scoped_clean_diff_exit=0,
                 notices=[dict(source_path=path, packaged_path=reference.NOTICES[path],
                               source=identity(raw), source_lf_count=raw.count(b'\n'),
                               source_cr_count=raw.count(b'\r'), source_nul_count=raw.count(b'\0'),
                               controlled_checkout=identity(checkout[path]),
                               exact_full_lf_to_crlf=True) for path, raw in source.items()],
                 guides=[dict(source_path=path, packaged_path=dest, expected=identity(guides[dest]))
                         for path, dest in reference.GUIDES.items()],
                 actual_ci_git_configuration_observed=False, actual_artifacts_checked=False,
                 native_results_accepted=False, platform_qualified=False, release_qualified=False)
    return source, checkout, guides, proof


def check_receipt(receipt):
    expected = {'status': 'artifact_bytes_verified', 'source': SOURCE, 'source_tree': TREE,
                'preset_library_revision': '2026.11.7', 'run_id': RUN, 'run_attempt': 1,
                'target': 'windows-x86_64', 'checker_sha256': PINS[A41],
                'source_scripts_loaded_from_commit': True, 'release_qualified': False,
                'native_validation_passed': None, 'artifact_attempt_binding_verified': False}
    require(all(receipt.get(key) == value and type(receipt.get(key)) is type(value)
                for key, value in expected.items()), 'Expected unchanged passing A41 receipt for exact ee0 Windows context')
    require(receipt['artifact']['name'] == 'cs80-windows-install-bundle'
            and type(receipt['artifact']['id']) is int and receipt['artifact']['id'] > 0,
            'Wrong installation artifact receipt')


def compare_metadata(reference, receipt, payloads, source, checkout, guides):
    required = [*reference.GUIDES.values(), *reference.NOTICES.values(),
                'licenses/JUCE-LICENSE.md', 'SOURCE.txt', 'evidence/juce-provenance.txt']
    require(len(required) == len(set(required)) == 13 and set(payloads) == set(required),
            'Required metadata roster differs')
    items = receipt['inventory']
    inventory = {item['path']: item for item in items}
    require(len(inventory) == len(items), 'Duplicate verified inventory path')
    for name in required:
        item = inventory.get(name, {})
        raw = payloads[name]
        require(item.get('type') == 'file' and item.get('mode') == '0o644'
                and type(item.get('size')) is int and item['size'] > 0,
                'Missing/nonregular/empty metadata or changed metadata mode: ' + name)
        require(item['size'] == len(raw) and item['sha256'] == sha(raw),
                'Actual metadata differs from A41 verified inventory: ' + name)
    guide_rows = []
    for path, destination in reference.GUIDES.items():
        expected = guides[destination]
        require(payloads[destination] == expected,
                'Guide differs from exact source packaged_markdown bytes: ' + destination)
        targets = reference.inline_targets(expected.decode('utf-8'))
        local = [target for target in targets if not urlsplit(target).scheme
                 and not urlsplit(target).netloc and urlsplit(target).path]
        require(all(urlsplit(target).path in reference.GUIDES.values() for target in local),
                'Unbundled relative inline guide target: ' + destination)
        prefix = 'https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/' + SOURCE + '/'
        guide_rows.append(dict(source=path, packaged=destination, result='PASS', **identity(expected),
                               inline_link_targets=len(targets), local_bundled_guide_targets=local,
                               source_pinned_link_targets=[target for target in targets if target.startswith(prefix)],
                               other_external_or_fragment_targets=[target for target in targets
                                   if target not in local and not target.startswith(prefix)]))
    packaged = {path: payloads[destination] for path, destination in reference.NOTICES.items()}
    mode = compare_notices(source, checkout, packaged)
    notice_rows = [dict(source=path, packaged=reference.NOTICES[path], result='PASS',
                        source_raw=identity(source[path]), actual_raw=identity(packaged[path]),
                        controlled_checkout=identity(checkout[path]), comparison_mode=mode)
                   for path in source]
    return required, guide_rows, notice_rows, mode


def inspect(repo, archive_path, acceptance_path):
    reference, a41 = dependencies(repo)
    receipt_raw = a41.snapshot(acceptance_path, 32 * 1024**2)
    receipt = a41.parse_json(receipt_raw)
    check_receipt(receipt)
    outer_raw = a41.snapshot(archive_path, a41.MAX_ARCHIVE)
    require(identity(outer_raw) == {key: receipt['outer_zip'][key] for key in ('bytes', 'sha256')},
            'Archive differs from accepted A41 bytes')
    inner_name = 'cs80-reference-windows-x86_64-' + SOURCE[:12] + '.zip'
    require(receipt['inner_archive']['file'] == inner_name, 'Unexpected Windows inner archive')
    with zipfile.ZipFile(io.BytesIO(outer_raw)) as outer:
        inner_raw = outer.read(inner_name)
    require(identity(inner_raw) == {key: receipt['inner_archive'][key] for key in ('bytes', 'sha256')},
            'Inner archive differs from accepted A41 bytes')
    required = [*reference.GUIDES.values(), *reference.NOTICES.values(),
                'licenses/JUCE-LICENSE.md', 'SOURCE.txt', 'evidence/juce-provenance.txt']
    payloads = {}
    with zipfile.ZipFile(io.BytesIO(inner_raw)) as inner:
        for name in required:
            member = inner.getinfo(inner_name.removesuffix('.zip') + '/' + name)
            require(not member.is_dir() and stat.S_IFMT(member.external_attr >> 16) in (0, stat.S_IFREG)
                    and 0 < member.file_size <= a41.MAX_METADATA, 'Invalid metadata ZIP member: ' + name)
            payloads[name] = inner.read(member)
    with tempfile.TemporaryDirectory(prefix='a46-windows-docs-') as directory:
        source, checkout, guides, proof = source_contract(repo, Path(directory), reference)
        required, guide_rows, notice_rows, mode = compare_metadata(reference, receipt, payloads, source, checkout, guides)
    return dict(schema=1, status='windows_source_docs_and_notices_verified_external_provenance_pending',
                **CONTEXT, verified_at_utc=datetime.now(timezone.utc).isoformat(),
                artifact_id=receipt['artifact']['id'], outer_zip=identity(outer_raw), inner_archive=identity(inner_raw),
                acceptance_receipt=dict(path=str(acceptance_path), **identity(receipt_raw)),
                inventory_counts=dict(Counter(item['type'] for item in receipt['inventory'])),
                required_metadata=[item for item in receipt['inventory'] if item['path'] in required],
                guide_comparisons=guide_rows, notice_comparisons=notice_rows, common_notice_mode=mode,
                source_checkout_proof=proof,
                juce_license=dict(path='licenses/JUCE-LICENSE.md', **identity(payloads['licenses/JUCE-LICENSE.md']),
                    independent_dependency_source_byte_comparison=False,
                    provenance_boundary='Presence/inventory bytes only here. Require original clean JUCE 8.0.11 c352e248 verification followed by unchanged source package command reading its LICENSE.md.'),
                source_time_guides='Exact frozen ee0dabca source-time guides; their historical/pending text is retained. No final publication is asserted.',
                original_clean_checkout_and_package_sequence_verified=False,
                actual_ci_git_configuration_observed=False,
                current_external_validator_requirement=dict(total=15, windows=2),
                normalization_scope='Only seven repository notice byte payloads, all in one mode. No guide, SOURCE.txt, provenance, executable, resource, JUCE-license or other-file normalization.',
                historical_a30_modified=False, helper_sha256=PINS,
                checker_sha256=sha(Path(__file__).read_bytes()),
                native_results_accepted=False, platform_qualified=False, release_qualified=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--prepare', action='store_true')
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--acceptance', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    require(not args.out.exists() and not args.out.is_symlink(), 'Refusing to replace evidence')
    reference, a41 = dependencies(args.repo)
    if args.prepare:
        require(args.archive is None and args.acceptance is None, 'Preparation accepts no actual archive/receipt')
        with tempfile.TemporaryDirectory(prefix='a46-windows-prepare-') as directory:
            _, _, _, result = source_contract(args.repo, Path(directory), reference)
        result['checker_sha256'] = sha(Path(__file__).read_bytes())
    else:
        require(args.archive is not None and args.acceptance is not None, 'Actual archive and passing A41 receipt required')
        result = inspect(args.repo, args.archive, args.acceptance)
    a41.publish_report(args.out, json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(status=result['status'], target=result['target'],
                          common_notice_mode=result.get('common_notice_mode'), platform_qualified=False,
                          release_qualified=False, out=str(args.out))))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, TypeError, UnicodeError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        print('Windows metadata verification failed: ' + str(error), file=sys.stderr)
        raise SystemExit(2)
