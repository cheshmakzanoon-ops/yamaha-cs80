#!/usr/bin/env python3
"""Preregistered isolated namespace/workflow omissions; no capture or API calls."""
import argparse
from collections import Counter
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
CONTRACT_PATH = 'tests/release/full-engine-audit-composed-default-v1-namespace.json'
CONTRACT = json.loads((ROOT / CONTRACT_PATH).read_text())
TEST = CONTRACT['registration_test']
WORKFLOW = CONTRACT['workflow_path']
WRAPPER = CONTRACT['wrapper_path']
MUTANTS = [
    ('wrong_branch', WORKFLOW, CONTRACT['branch'], 'work/unregistered-capture',
     ['test_workflow_has_distinct_branch_and_readonly_identity']),
    ('missing_fixture_trigger', WORKFLOW, "      - '" + TEST + "'\n", '',
     ['test_workflow_has_complete_source_and_runner_triggers']),
    ('missing_mutation_execution', WORKFLOW,
     'run: python tests/release/qualify_full_engine_audit_composed_default_v1_mutations.py --out artifacts/composed-default-v1-guard-mutations',
     'run: echo omitted-source-controls', ['test_workflow_executes_all_guards_before_build']),
    ('abbreviated_capture', WORKFLOW, '--suite all --rates', '--suite quick --rates',
     ['test_workflow_retains_original_build_and_complete_capture_command']),
    ('missing_completion_upload', WORKFLOW, '            artifacts/audit-composed-default-v1.complete\n', '',
     ['test_workflow_retains_both_provenance_layers_and_failures']),
    ('reused_host_marker', WRAPPER, "destination / 'audit-composed-default-v1.complete'",
     "destination / 'audit-v2.complete'", ['test_wrapper_keeps_exact_approved_guards_with_distinct_namespace']),
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def classify(text, code, expected):
    failures = re.findall(r'^FAIL: (\w+) ', text, re.M)
    errors = re.findall(r'^ERROR: (\w+) ', text, re.M)
    complete = re.findall(r'^Ran (\d+) tests in ', text, re.M) == ['7']
    summary = f'FAILED (failures={len(expected)})' if expected else 'OK'
    passed = code == (1 if expected else 0) and complete and not errors and 'skipped=' not in text
    passed = passed and Counter(failures) == Counter(expected)
    passed = passed and re.findall(r'^(?:OK|FAILED \(.*\))$', text, re.M) == [summary]
    return {'qualified': passed, 'ordinary_failures': failures, 'runtime_errors': errors,
            'complete_test_inventory': complete}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--heavy-lock', type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    paths = [TEST, WORKFLOW, WRAPPER, CONTRACT_PATH, CONTRACT['request_path'], *CONTRACT['protected_files']]
    inputs = {path: (ROOT / path).read_bytes() for path in paths}
    for path, data in inputs.items():
        if data != subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':' + path]):
            raise ValueError('Input differs from committed source: ' + path)
    cases = [('positive', None, '', '', []), *MUTANTS,
             ('runtime_error_control', TEST, '#!/usr/bin/env python3',
              "#!/usr/bin/env python3\nraise RuntimeError('deliberate namespace fixture crash')", []),
             ('restored', None, '', '', [])]
    records = []
    for name, path, before, after, expected in cases:
        lock = args.heavy_lock.open('a') if args.heavy_lock else None
        try:
            if lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
            stage = args.out / name
            stage.mkdir()
            for relative, data in inputs.items():
                target = stage / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            mutation = None
            if path:
                text = inputs[path].decode()
                if text.count(before) != 1:
                    raise ValueError('Missing/nonunique mutation anchor: ' + name)
                data = text.replace(before, after).encode()
                (stage / path).write_bytes(data)
                mutation = {'path': path, 'before': before, 'after': after,
                            'before_sha256': sha(inputs[path]), 'after_sha256': sha(data)}
            process = subprocess.Popen([sys.executable, TEST], cwd=stage, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            try:
                output, _ = process.communicate(timeout=60)
            except BaseException:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.communicate()
                raise
            (stage / 'run.log').write_bytes(output)
            assessment = classify(output.decode(), process.returncode, expected)
            runtime_control = name == 'runtime_error_control'
            accepted = (not assessment['qualified'] and not assessment['complete_test_inventory']
                        and b'RuntimeError: deliberate namespace fixture crash' in output) if runtime_control else assessment['qualified']
            record = {'case': name, 'source_commit': commit, 'mutation': mutation,
                      'expected_failures': expected, 'assessment': assessment, 'exit_code': process.returncode,
                      'runtime_control_not_semantic_kill': runtime_control, 'log_sha256': sha(output),
                      'input_sha256': {p: sha(b) for p, b in inputs.items()}}
            (stage / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
            records.append(record)
            if not accepted:
                raise AssertionError(name + ': wrong ordinary failures or unqualified runtime error')
            print(name, 'PASS', flush=True)
        finally:
            if lock:
                lock.close()
    for path, data in inputs.items():
        if (ROOT / path).read_bytes() != data:
            raise ValueError('Source changed during isolated qualification: ' + path)
    report = {'source_commit': commit, 'passed': True, 'semantic_mutants': len(MUTANTS), 'cases': records,
              'scope': 'composed-default V1 namespace and workflow registration, not renderer/numerical acceptance'}
    (args.out / 'qualification.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
