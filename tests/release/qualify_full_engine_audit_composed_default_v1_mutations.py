#!/usr/bin/env python3
"""Delete actual composed-default V1 guard source in isolated copies; reject runtime-error 'kills'."""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / 'scripts/full_engine_audit_gate_composed_default_v1.py'
TESTS = ROOT / 'tests/release/test_full_engine_audit_composed_default_v1.py'
MUTATIONS = [
    ('ancestry', 'require(result.returncode == 0,', 'require(True,',
     ['test_nonancestor_registration', 'test_nonancestor_source', 'test_nonancestor_audio']),
    ('source_worktree', "require(not git('status',", "require(True or not git('status',",
     ['test_unstaged_source', 'test_staged_source', 'test_nonignored_untracked_source',
      'test_staged_index_divergence_is_rejected_even_with_original_worktree_bytes']),
    ('ignored_source', "require(not git('ls-files',", "require(True or not git('ls-files',",
     ['test_ignored_untracked_source']),
    ('source_commit_tree', "require(git('rev-parse', source + ':src') == expected_tree,", 'require(True,',
     ['test_wrong_registered_source_tree']),
    ('head_source_tree', "require(git('rev-parse', 'HEAD:src') == expected_tree,", 'require(True,',
     ['test_wrong_head_source_tree']),
    ('request_bytes', "require(raw == git_bytes('show', f'{REGISTRATION_SHA}:{REQUEST_PATH}'),", 'require(True,',
     ['test_modified_composed_default_v1_request']),
    ('original_file_bytes', 'require(current_bytes(relative, reason) == expected ==',
     'require(True or current_bytes(relative, reason) == expected ==',
     ['test_original_v1_files_are_not_redefined_by_composed_default_v1'] * 6 +
     ['test_changed_reporter_cannot_be_laundered_by_new_registration']),
    ('original_contract_values', 'require(request.get(key) == value,', 'require(True,',
     ['test_new_registration_cannot_weaken_original_inventory']),
    ('build_input', "require(metadata['anchor_commit'] == source and metadata['sha256'] == digest(expected)",
     "require(True or metadata['anchor_commit'] == source and metadata['sha256'] == digest(expected)",
     ['test_mutated_cmake_input']),
    ('completion_digest', "require(marker['provenance_sha256'] == digest(raw),", 'require(True,',
     ['test_completion_provenance_digest_mismatch']),
    ('completion_validation', 'provenance = validated_completion(context)',
     "provenance = json.loads((destination / 'audit-provenance.json').read_text())",
     ['test_completion_source_mismatch', 'test_completion_registration_mismatch',
      'test_completion_provenance_digest_mismatch', 'test_completion_raw_csv_digest_mismatch',
      'test_completion_binary_digest_mismatch']),
    ('physical_source_bytes', "require(digest(actual) == digest(expected), 'source-file-bytes:' + name)",
     "require(True, 'source-file-bytes:' + name)",
     ['test_assume_unchanged_cannot_hide_modified_source', 'test_skip_worktree_cannot_hide_modified_source']),
    ('physical_source_ancestor', 'require(current.is_dir() and not current.is_symlink(),',
     'require(current.is_dir(),', ['test_physical_source_rejects_symlinked_ancestor']),
    ('physical_source_mode', "require(bool(actual_mode & 0o111) == (mode == '100755'),", 'require(True,',
     ['test_physical_source_rejects_executable_mode_change']),
    ('physical_source_leaf',
     "require(path.is_file() and not path.is_symlink(), 'source-file-type:' + name)\n        actual_mode = path.lstat().st_mode",
     "require(path.is_file(), 'source-file-type:' + name)\n        actual_mode = path.stat().st_mode",
     ['test_physical_source_rejects_leaf_symlink']),
]


def classify(result, expected):
    failures = re.findall(r'^FAIL: (\w+) ', result.stdout, re.MULTILINE)
    errors = re.findall(r'^ERROR: (\w+) ', result.stdout, re.MULTILINE)
    complete = re.findall(r'^Ran (\d+) tests in ', result.stdout, re.MULTILINE) == ['45']
    summary = f'FAILED (failures={len(expected)})' if expected else 'OK'
    return {'qualified': result.returncode == (1 if expected else 0) and complete and not errors
            and 'skipped=' not in result.stdout and Counter(failures) == Counter(expected)
            and re.findall(r'^(?:OK|FAILED \(.*\))$', result.stdout, re.MULTILINE) == [summary],
            'ordinary_failures': failures, 'runtime_errors': errors, 'complete_test_inventory': complete}


def bounded_command(command, lock=None, *, runtime=300, grace=5):
    # GNU timeout owns the test's process group and terminates remaining children
    # before returning. flock retains the lock until that bounded command exits.
    # The waiting time before acquisition does not spend the unchanged 300s budget.
    bounded = ['timeout', f'--kill-after={grace}s', f'{runtime}s', *command]
    return ['flock', str(lock), *bounded] if lock else bounded


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--heavy-lock', type=Path, help='Hold shared lock for each complete test run; yield between cases')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    original = WRAPPER.read_text()
    test_bytes = TESTS.read_bytes()
    source_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    cases = [('unmodified', '', '', []), *MUTATIONS,
             ('runtime_error_control', 'def registered_context():',
              "def registered_context():\n    raise RuntimeError('deliberate unqualified guard crash')", []),
             ('restored', '', '', [])]
    reports = []
    with tempfile.TemporaryDirectory(prefix='cs80-composed-default-v1-source-mutants-') as temp:
        for name, before, after, expected in cases:
            if before and original.count(before) != 1:
                raise ValueError('Missing/nonunique mutation anchor: ' + name)
            path = Path(temp) / (name + '.py')
            path.write_text(original.replace(before, after) if before else original)
            env = dict(os.environ, CS80_AUDIT_COMPOSED_DEFAULT_V1_TEST_WRAPPER=str(path))
            command = bounded_command([sys.executable, str(TESTS)], args.heavy_lock)
            result = subprocess.run(command, cwd=ROOT, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                    check=False)
            (args.out / (name + '.log')).write_text(result.stdout)
            assessment = classify(result, expected)
            runtime_control = name == 'runtime_error_control'
            accepted = (not assessment['qualified'] and bool(assessment['runtime_errors'])
                        and assessment['complete_test_inventory']) if runtime_control else assessment['qualified']
            reports.append({'case': name, 'expected_failures': expected, 'observed_exit': result.returncode,
                            'assessment': assessment, 'runtime_control_not_semantic_kill': runtime_control,
                            'log_sha256': hashlib.sha256(result.stdout.encode()).hexdigest()})
            if not accepted:
                raise AssertionError(f'{name}: wrong guard failures or runtime errors: {assessment}')
    if WRAPPER.read_text() != original or TESTS.read_bytes() != test_bytes or source_commit != subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip():
        raise AssertionError('Original source/test/commit changed during isolated mutation qualification')
    report = {'scope': 'composed-default V1 source/completion guard strength with synthetic fixtures; not numerical acceptance',
              'tested_source_commit': source_commit,
              'wrapper_sha256': hashlib.sha256(original.encode()).hexdigest(),
              'test_sha256': hashlib.sha256(test_bytes).hexdigest(),
              'semantic_mutants': len(MUTATIONS), 'cases': reports}
    (args.out / 'qualification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'qualified_semantic_mutants': len(MUTATIONS), 'runtime_control_refused': True,
                      'positive_and_restored_passed': True, 'out': str(args.out)}))


if __name__ == '__main__':
    main()
