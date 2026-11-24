#!/usr/bin/env python3
"""Independently audit frozen source and explicit partial release inputs.

This reads Git objects, files and retained receipts only. It does not execute a
product, test, archive verifier, network operation, or publication operation.
Run into a fresh output directory; existing checkpoint files are never replaced.
"""
from __future__ import annotations

import argparse
import collections
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess

SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
SOUND = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
BASE = 'docs/preset-evidence/2026.11.7/'
A40 = BASE + 'verification/a40/source-ee0dabca/'
ROOTS = {'.github', 'CMakeLists.txt', 'cmake', 'scripts', 'src', 'tests'}
ONLINE = {'README.md', 'docs/CIRCUIT_LIBRARY_2026_11_7.md',
          'docs/INSTALL_AND_QUICKSTART.md', 'docs/PRESET_GUIDE.md',
          'docs/CI_DISTRIBUTION_STATUS.md'}
GUIDES = ONLINE - {'README.md', 'docs/CIRCUIT_LIBRARY_2026_11_7.md'}
TECHNICAL = {'docs/IG00156_NODAL_MODEL.md', 'docs/IG00151_VCA_MODEL.md',
             'docs/VCA_REFERENCE.md'}


def identity(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def tree(repo, ref):
    rows = {}
    for row in git(repo, 'ls-tree', '-r', '-z', '--full-tree', ref).split(b'\0'):
        if row:
            meta, path = row.split(b'\t', 1)
            mode, kind, oid = meta.decode().split()
            rows[path.decode()] = {'mode': mode, 'type': kind, 'oid': oid}
    return rows


def blob(repo, ref, path):
    return git(repo, 'show', ref + ':' + path)


def current(path):
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode):
        raw, mode = os.readlink(path).encode(), '120000'
    else:
        assert stat.S_ISREG(info.st_mode), str(path)
        raw, mode = path.read_bytes(), '100755' if info.st_mode & stat.S_IXUSR else '100644'
    oid = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
    return {'mode': mode, 'type': 'blob', 'oid': oid}, identity(raw)


def change_class(path):
    if path in ONLINE:
        return 'authorized_later_online_document'
    if path == 'docs/TEAM_REVIEW_2026_11_7.md':
        return 'current_revision_team_review'
    if path.startswith(BASE):
        return 'current_revision_evidence_or_review'
    if path.startswith('docs/screenshots/'):
        return 'documentation_screenshot'
    return 'protected'


def write(out, name, value):
    with (out / name).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write('\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    repo, out = args.repo.resolve(), args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    observed = datetime.datetime.now(datetime.timezone.utc).isoformat()
    head = git(repo, 'rev-parse', 'HEAD').decode().strip()
    main_ref = git(repo, 'rev-parse', 'refs/heads/main').decode().strip()
    assert head == main_ref
    assert git(repo, 'rev-parse', SOURCE + '^{tree}').decode().strip() == TREE
    assert git(repo, 'rev-parse', '--show-object-format').decode().strip() == 'sha1'
    frozen, tracked = tree(repo, SOURCE), tree(repo, head)
    readset = {}

    def read(path):
        raw = (repo / path).read_bytes()
        readset[path] = identity(raw)
        return raw

    def load(path):
        return json.loads(read(path))

    changes = [{'path': p, 'classification': change_class(p),
                'frozen': frozen.get(p), 'current_main': tracked.get(p)}
               for p in sorted(frozen.keys() | tracked.keys()) if frozen.get(p) != tracked.get(p)]
    forbidden = [r for r in changes if r['classification'] == 'protected']
    assert not forbidden, forbidden
    protected = {p for p in frozen if change_class(p) == 'protected'}
    assert protected == {p for p in tracked if change_class(p) == 'protected'}
    assert all(frozen[p] == tracked[p] for p in protected)

    # Hash actual working bytes and check Unix Git modes for every tracked path,
    # not merely the src directory or cached git status.
    work, records, missing = {}, {}, []
    for path in sorted(tracked):
        try:
            work[path], records[path] = current(repo / path)
        except FileNotFoundError:
            missing.append(path)
    work_changes = [{'path': p, 'classification': change_class(p),
                     'tracked_main': tracked[p], 'worktree': work.get(p)}
                    for p in sorted(tracked) if tracked[p] != work.get(p)]
    assert not [r for r in work_changes if r['classification'] == 'protected']
    index = {}
    for row in git(repo, 'ls-files', '--stage', '-z').split(b'\0'):
        if row:
            meta, path = row.split(b'\t', 1)
            mode, oid, stage = meta.decode().split()
            assert stage == '0', 'Unmerged index entry: ' + path.decode()
            index[path.decode()] = {'mode': mode, 'type': 'blob', 'oid': oid}
    index_changes = [p for p in sorted(index.keys() | tracked.keys()) if index.get(p) != tracked.get(p)]
    assert not [p for p in index_changes if change_class(p) == 'protected']
    untracked = [p.decode() for p in git(repo, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0') if p]
    assert not [p for p in untracked if change_class(p) == 'protected']

    # Includes tools, assets, installers, artifacts, root configuration and
    # license files outside A40's original six protected groups.
    nondocs = {p for p in frozen if not p.startswith('docs/') and p != 'README.md'}
    scanned_roots = sorted({p.split('/')[0] for p in nondocs if '/' in p})
    physical_extra = []
    for root in scanned_roots:
        for directory, dirs, files in os.walk(repo / root, followlinks=False):
            candidates = [Path(directory) / p for p in files]
            candidates += [Path(directory) / p for p in dirs if (Path(directory) / p).is_symlink()]
            for path in candidates:
                relative = path.relative_to(repo).as_posix()
                if relative not in tracked:
                    metadata, details = current(path)
                    ignored = subprocess.run(['git', '-C', str(repo), 'check-ignore', '-q', '--', relative]).returncode == 0
                    generated_cache = ignored and '/__pycache__/' in relative and relative.endswith('.pyc')
                    physical_extra.append({'path': relative, 'ignored_by_git': ignored,
                                           'python_bytecode_cache': generated_cache, **metadata, **details})
    assert all(row['python_bytecode_cache'] for row in physical_extra), physical_extra

    manifest_path = A40 + 'source-inputs-ee0dabca.json'
    manifest = load(manifest_path)
    assert manifest['source'] == SOURCE and manifest['tree'] == TREE
    source_paths = {p for p in frozen if p.split('/')[0] in ROOTS}
    assert source_paths == set(manifest['inputs']) and len(source_paths) == 945
    protected_rows = {}
    for path in sorted(source_paths):
        expected = manifest['inputs'][path]
        actual = identity(blob(repo, SOURCE, path))
        assert actual == expected == records[path]
        assert frozen[path] == tracked[path] == work[path] == index[path]
        protected_rows[path] = {**frozen[path], **actual}

    fixture_refs = []
    def collect(value, fixture):
        if isinstance(value, dict):
            for key, item in value.items():
                if key.startswith('docs/') and key in frozen and isinstance(item, str) and re.fullmatch('[0-9a-f]{64}', item):
                    fixture_refs.append({'fixture': fixture, 'path': key, 'declared_sha256': item})
                collect(item, fixture)
        elif isinstance(value, list):
            for item in value:
                collect(item, fixture)
    for path in frozen:
        if path.startswith('tests/') and 'source-freeze' in path and path.endswith('.json'):
            collect(json.loads(blob(repo, SOURCE, path)), path)
    critical_docs = TECHNICAL | {row['path'] for row in fixture_refs}
    assert not (critical_docs & ONLINE)
    for path in critical_docs:
        assert frozen[path] == tracked[path] == work[path] == index[path]
        protected_rows[path] = {**frozen[path], **records[path]}
    for row in fixture_refs:
        row['current_sha256'] = records[row['path']]['sha256']
        row['matches_fixture_pin'] = row['declared_sha256'] == row['current_sha256']
    assert all(row['matches_fixture_pin'] for row in fixture_refs)

    documents = []
    for path in sorted(ONLINE):
        documents.append({'path': path, 'packaged_guide': path in GUIDES,
                          'frozen': {**frozen[path], **identity(blob(repo, SOURCE, path))},
                          'current_main': {**tracked[path], **identity(blob(repo, head, path))},
                          'worktree': {**work[path], **records[path]},
                          'source_time_package_guides_must_remain_frozen': path in GUIDES})
    inventory = load(BASE + 'verification/a12/inventory.json')
    assert len(inventory['parameter_ids']) == 204 and len(inventory['programs']) == 523
    packed = b''.join(struct.pack('<204f', *p['values']) for p in inventory['programs'])
    assert hashlib.sha256(packed).hexdigest() == 'aedb533d123874448b4df31f3c25aa7cb109d4109c6dc22ee9548ebcdeb42273'

    write(out, 'protected-input-records.json', protected_rows)
    write(out, 'complete-tree-changes.json', changes)
    write(out, 'documentation-context.json', {'authorized_online_documents': documents,
          'technical_and_freeze_fixture_documents': sorted(critical_docs), 'fixture_references': fixture_refs,
          'whole_main_tree_equals_frozen_source': False, 'rebuilt_package_byte_identity_claimed': False,
          'scope': 'Online guides intentionally differ; accepted binaries and embedded guides retain the frozen source context.'})
    source_report = {'schema': 1, 'observed_at_utc': observed, 'binary_source': SOURCE, 'binary_tree': TREE,
        'sound_source': SOUND, 'documentation_head': head, 'local_main': main_ref,
        'tracked_frozen_paths': len(frozen), 'tracked_main_paths': len(tracked),
        'all_tracked_worktree_paths_hashed': len(work), 'all_tracked_worktree_bytes_hashed': sum(r['bytes'] for r in records.values()),
        'protected_path_count_beyond_publication_and_current_evidence': len(protected),
        'a40_manifest': {'path': manifest_path, **readset[manifest_path]}, 'a40_inputs_verified': 945,
        'non_documentation_protected_count': len(nondocs), 'non_documentation_root_counts': dict(collections.Counter(p.split('/')[0] for p in nondocs)),
        'source_critical_document_count': len(critical_docs), 'source_freeze_document_reference_count': len(fixture_refs),
        'protected_tree_paths_modes_and_blobs_equal': True, 'protected_worktree_bytes_and_modes_equal': True,
        'protected_index_paths_modes_and_blobs_equal': True, 'protected_additions_deletions': [],
        'all_tree_change_count': len(changes), 'all_tree_changes_by_class': dict(collections.Counter(r['classification'] for r in changes)),
        'worktree_changes_from_main': work_changes, 'missing_tracked_worktree_paths': missing,
        'staged_paths_at_checkpoint': index_changes,
        'untracked_nonignored_path_count': len(untracked), 'untracked_nonignored_protected_paths': [],
        'physical_extra_files_in_non_documentation_roots': physical_extra,
        'cache_scope': 'Observed ignored Python bytecode is inventoried, not deleted or relabeled as tracked source; clean CI checkout does not include these caches.',
        'protected_tree_canonical_sha256': identity(canonical({p:frozen[p] for p in sorted(protected)}))['sha256'],
        'all_tracked_worktree_canonical_sha256': identity(canonical(work))['sha256'],
        'inventory': {'path': BASE + 'verification/a12/inventory.json', **readset[BASE + 'verification/a12/inventory.json'],
                      'programs': 523, 'parameters': 204, 'values': 106692, 'binary32_values_sha256': hashlib.sha256(packed).hexdigest()},
        'source_continuity_verified': True, 'native_or_binary_execution_performed': False,
        'whole_main_tree_identity_claimed': False, 'release_qualified': False}
    write(out, 'source-continuity.json', source_report)

    # Reconcile actual, named partial receipts without pretending to repeat
    # their archive parsing, original native execution or public GETs.
    arm = load(BASE + 'ci/source-ee0dabca/macos-arm64/platform-qualification.json')
    windows = load(BASE + 'ci/source-ee0dabca/windows-x86_64/platform-qualification.json')
    assert arm['source_commit'] == windows['source'] == SOURCE
    assert arm['source_tree'] == windows['tree'] == TREE
    assert arm['run_id'] == windows['run_id'] == 45397189410
    assert arm['run_attempt'] == windows['attempt'] == 1
    assert arm['platform_qualified'] and windows['platform_qualified']
    assert not arm['release_qualified'] and not windows['release_qualified']
    mapping = load(BASE + 'ci/source-ee0dabca/public-downloads/download-mapping-phase2.json')
    assert mapping['verified_public_route_count'] == len(mapping['verified_downloads']) == 2
    assert not mapping['all_five_public_downloads_verified'] and not mapping['release_qualified']
    public_rows = []
    for download in mapping['verified_downloads']:
        for field in ('external_platform_qualification', 'public_get_receipt', 'identity_binding_receipt'):
            item = download[field]
            assert identity(read(item['path'])) == {k:item[k] for k in ('bytes','sha256')}
        receipt = load(download['public_get_receipt']['path'])
        assert receipt['status'] == 200 and receipt['method'] == 'GET' and not receipt['credentials_supplied']
        assert 'content-range' not in {k.lower() for k in receipt['headers']}
        assert receipt['actual_bytes'] == download['bytes'] and receipt['actual_sha256'] == download['sha256']
        assert download['producing_attempt'] == 1 and download['original_producer_upload_attempt_bound']
        public_rows.append({k:download[k] for k in ('target','artifact_id','bytes','sha256','producing_attempt','public_url')})
    intel = load(BASE + 'verification/a58/phase1/intel-original-failure.json')
    refusal = load(BASE + 'ci/source-ee0dabca/macos-x86_64/attempt-1-failed/rerun-request-while-active.json')
    assert not refusal['retry_started']
    ledger = load(BASE + 'work-log.json')
    assignments = ledger['assignments']
    assert {r['id'] for r in assignments} == {f'a{i:02}' for i in range(1,61)}
    assert len({r['agent'] for r in assignments}) == len(assignments) == 60
    assert len({r['role'] for r in assignments}) == 60
    completed = [r for r in assignments if r['status'] == 'completed']
    assert ledger['completed_count'] == len(completed)
    team = load(BASE + 'verification/a59/phase1-sixty/ledger-checkpoint.json')
    assert team['actual_recorded_assignment_count'] == team['unique_canonical_task_names'] == 60
    for name in ('a40','a42','a43','a49','a54','a55','a56','a58','a59'):
        read(BASE + f'reviews/{name}.md')
    for path in ('verification/a58/REFERENCE_MATRIX.md', 'verification/a58/phase1/snapshot.json',
                 'verification/a58/phase1-supplement.json', 'verification/a52/actual-supporting-findings.json'):
        read(BASE + path)
    scope = {'schema': 1, 'observed_at_utc': observed, 'status': 'phase_one_only_final_release_not_signed_off',
        'binary_source': SOURCE, 'binary_tree': TREE, 'documentation_head': head,
        'externally_qualified_platforms': ['macos-arm64','windows-x86_64'],
        'arm_native_counts': arm['native_counts'], 'windows_native_counts': windows['native']['counts'],
        'qualified_validator_lanes': 7, 'required_validator_lanes': 15,
        'qualified_packages': 2, 'required_packages': 4, 'public_downloads': public_rows,
        'verified_public_routes': 2, 'required_public_routes': 5,
        'actual_linux_and_audition_acceptance_supplied_to_a60': False,
        'intel_original_failure_retained': True, 'rerun_refusal': refusal,
        'intel_failure_receipt': {'path': BASE + 'verification/a58/phase1/intel-original-failure.json',
                                  **readset[BASE + 'verification/a58/phase1/intel-original-failure.json']},
        'team': {'assignments': 60, 'unique_tasks': 60, 'unique_roles':60, 'completed':len(completed),
                 'open': [{'id':r['id'],'status':r['status']} for r in assignments if r['status'] != 'completed'],
                 'all_sixty_completed': False, 'a59_checkpoint': team['recorded_at_utc'],
                 'disclosed_historical_citation_gap': team['missing_local_markdown_citations'],
                 'self_reference_rule': 'A60 phase-one report is an output; final audit must explicitly order prior stable readsets and final completion rather than require cyclic hashes.'},
        'original_attempt_1_all_platform_pass': False, 'all_ci_green_claimed': False,
        'human_listening_approval_claimed': False, 'exact_acoustic_equivalence_claimed': False,
        'new_product_tests_or_archive_reverification_run': False,
        'final_publication_review_complete': False, 'final_remote_push_and_protected_source_check_complete': False,
        'cross_platform_qualified': False, 'release_qualified': False, 'a60_assignment_complete': False}
    write(out, 'release-scope.json', scope)
    readset_changes = [p for p, details in readset.items() if identity((repo/p).read_bytes()) != details]
    assert not readset_changes, readset_changes
    assert git(repo, 'rev-parse', 'HEAD').decode().strip() == head, 'Main changed during audit; use a fresh checkpoint.'
    write(out, 'readset.json', {'inputs': readset, 'input_count': len(readset), 'changed_at_close': readset_changes,
          'own_report_and_generated_receipts_excluded': True,
          'reason': 'Outputs are created after their input checkpoint; no self-referential byte-hash claim.'})
    write(out, 'execution-receipt.json', {'observed_at_utc': observed, 'finished_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'script': str(Path(__file__).resolve().relative_to(repo)), **identity(Path(__file__).read_bytes()),
          'argv': ['python3','-B',str(Path(__file__).resolve().relative_to(repo)),'--repo','.', '--out-dir',str(out)],
          'result': 'read_only_phase_one_source_and_partial_receipt_reconciliation_completed',
          'source_continuity_verified': True, 'release_qualified': False})
    print(json.dumps({'source_continuity_verified': True, 'tracked_paths_hashed':len(work),
                      'a40_inputs_verified':945, 'source_critical_documents':len(critical_docs),
                      'external_platforms':2, 'public_routes':2, 'assignments':60,
                      'completed_assignments':len(completed), 'release_qualified':False}))


if __name__ == '__main__':
    main()
