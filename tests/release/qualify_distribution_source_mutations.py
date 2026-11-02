#!/usr/bin/env python3
"""Exact raw-notice source controls; no native package or numerical acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
STAGER = 'scripts/distribution_docs.py'
VERIFIER = 'scripts/verify_distribution_docs.py'
FILES = (STAGER, VERIFIER, 'scripts/package-ci-artifacts.py',
         'tests/release/test_distribution_docs.py',
         'tests/release/test_distribution_source_identity.py', 'installer/linux/make-deb.sh')
CASES = (
    ('staging-immutable-bytes', STAGER,
     '        require(allow_crlf and actual == expected.replace(b"\\n", b"\\r\\n"),\n'
     '                f"Required notice bytes differ from source revision: {relative}")',
     '        pass  # deliberately accept replacement source bytes',
     {'test_stager_rejects_changed_required_notice_behind_index_hint': 10,
      'test_ci_archive_rejects_changed_required_notice_behind_index_hint': 10,
      'test_ci_archive_status_document_is_bound_to_revision': 2,
      'test_windows_crlf_is_not_permission_to_change_text': 1,
      'test_unregistered_crlf_conversion_is_rejected': 1}),
    ('verifier-forged-origin', VERIFIER,
     "        raw_blob = subprocess.check_output(['git', '-C', str(root), 'cat-file', 'blob', blob])",
     '        raw_blob = actual_source  # deliberately trust mutable source instead of Git',
     {'test_independent_verifier_rejects_self_consistent_hidden_source_substitution': 10,
      'test_windows_crlf_is_not_permission_to_change_text': 1,
      'test_unregistered_crlf_conversion_is_rejected': 1}),
    ('staging-source-mode', STAGER,
     '        require(bool(actual_mode & 0o111) == (mode == "100755"),\n'
     '                f"Required notice source executable mode differs: {relative}")',
     '        pass  # deliberately omit physical source executable mode',
     {'test_notice_executable_mode_is_bound_without_trusting_core_filemode': 1}),
    ('verifier-source-mode', VERIFIER,
     "            require(bool(physical_mode & 0o111) == (mode == '100755'),\n"
     "                    'Source notice executable mode differs: ' + relative)",
     '            pass  # deliberately omit independent physical source mode',
     {'test_notice_executable_mode_is_bound_without_trusting_core_filemode': 1}),
    ('staging-index', STAGER,
     '    require(subprocess.run(["git", "-C", str(root), "diff", "--cached", "--quiet", "HEAD", "--"],\n'
     '                           check=False).returncode == 0,\n'
     '            "Staged source index differs from the packaging commit")',
     '    pass  # deliberately ignore staged index divergence',
     {'test_stager_rejects_staged_index_when_worktree_matches_head': 1}),
    ('verifier-index', VERIFIER,
     "    require(subprocess.run(['git', '-C', str(root), 'diff', '--cached', '--quiet', 'HEAD', '--'],\n"
     "                           check=False).returncode == 0, 'Source index differs from expected commit')",
     '    pass  # deliberately ignore independent staged-index check',
     {'test_verifier_rejects_staged_index_when_worktree_matches_head': 1}),
    ('staging-checkout-policy', STAGER,
     '                      and config.stdout.strip().lower() == b"true"\n', '',
     {'test_unregistered_crlf_conversion_is_rejected': 1}),
    ('verifier-checkout-policy', VERIFIER,
     "                    and setting.stdout.strip().lower() == b'true'\n", '',
     {'test_unregistered_crlf_conversion_is_rejected': 1}),
    ('ci-status-origin', 'scripts/package-ci-artifacts.py',
     '        "DISTRIBUTION_STATUS.md": revision_notice(ROOT, "docs/CI_DISTRIBUTION_STATUS.md", commit),',
     '        "DISTRIBUTION_STATUS.md": (ROOT / "docs/CI_DISTRIBUTION_STATUS.md").read_bytes(),',
     {'test_ci_archive_status_document_is_bound_to_revision': 2}),
)

CHILD = r'''
import collections,io,json,pathlib,re,sys,unittest
root,report=map(pathlib.Path,sys.argv[1:])
suite=unittest.defaultTestLoader.discover(str(root/'tests/release'),pattern='test_distribution_source_identity.py')
result=unittest.TextTestRunner(verbosity=2).run(suite)
families=collections.Counter(re.search(r'\.(test_\w+)(?:\s|$)',test.id())[1] for test,_ in result.failures)
report.write_text(json.dumps({'tests':result.testsRun,'failures':[test.id() for test,_ in result.failures],
 'failure_families':dict(families),'errors':[test.id() for test,_ in result.errors],
 'skipped':[str(test) for test,_ in result.skipped],'unexpected_successes':len(result.unexpectedSuccesses),
 'expected_failures':len(result.expectedFailures)},indent=2)+'\n')
sys.exit(0 if result.wasSuccessful() else 1)
'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--heavy-lock', type=Path, required=True)
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        raise ValueError('Complete qualification requires Linux; native Windows skips cannot qualify these mutants')
    args.out.mkdir(parents=True, exist_ok=False)
    frozen = {name: (ROOT / name).read_bytes() for name in FILES}
    record = {'status': 'incomplete', 'source_revision': subprocess.check_output(
        ['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
        'inputs': {name: sha(data) for name, data in frozen.items()},
        'runner_sha256': sha(Path(__file__).read_bytes()), 'cases': [],
        'scope': 'Synthetic Git/notice source-origin controls, not native package or binary qualification'}

    def save():
        (args.out / 'qualification.json').write_text(json.dumps(record, indent=2) + '\n')

    def run(name, change=None, expected=None, crash=False):
        case = args.out / name; stage = case / 'input'; stage.mkdir(parents=True)
        for path, data in frozen.items():
            target = stage / path; target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(data)
        delta = None
        if change:
            path, old, new = change; text = (stage / path).read_text()
            if text.count(old) != 1:
                raise RuntimeError('Mutation anchor absent or ambiguous: ' + name)
            changed = text.replace(old, new); (stage / path).write_text(changed)
            delta = {'path': path, 'old': old, 'new': new, 'sha256': sha(changed.encode())}
        child = case / 'run.py'; child.write_text(CHILD); report = case / 'result.json'
        command = ['flock', str(args.heavy_lock), 'timeout', '--kill-after=5s', '120s',
                   sys.executable, str(child), str(stage), str(report)]
        with (case / 'run.log').open('w') as log:
            log.write('command=' + json.dumps(command) + '\n'); log.flush()
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        outcome = json.loads(report.read_text()) if report.is_file() else None
        expected = expected or {}
        qualified = (outcome is not None and outcome['tests'] == 12
                     and not outcome['errors'] and not outcome['skipped']
                     and not outcome['unexpected_successes'] and not outcome['expected_failures']
                     and outcome['failure_families'] == expected
                     and result.returncode == (1 if expected else 0))
        accepted = qualified
        if crash:
            accepted = (outcome is not None and outcome['tests'] == 12 and outcome['errors']
                        and not outcome['failures'] and not outcome['skipped']
                        and result.returncode == 1 and not qualified)
        item = {'name': name, 'expected_failures': expected, 'mutation': delta,
                'runtime_error_refusal': crash, 'passed': bool(accepted), 'returncode': result.returncode,
                'log_sha256': sha((case / 'run.log').read_bytes()),
                'result_sha256': sha(report.read_bytes()) if report.is_file() else None}
        record['cases'].append(item); save(); print(json.dumps(item), flush=True)
        if not accepted:
            raise RuntimeError('Strict control did not qualify: ' + name)

    save(); run('unmodified')
    for name, path, old, new, expected in CASES:
        run(name, (path, old, new), expected)
    run('runtime-error-refused', (STAGER,
        '    require(bool(re.fullmatch(r"[0-9a-f]{40}", commit)), "Notice source requires a full revision")',
        '    raise RuntimeError("deliberate runtime crash is not a source-guard kill")'), crash=True)
    run('restored'); record['status'] = 'pass'; save()


if __name__ == '__main__':
    main()
