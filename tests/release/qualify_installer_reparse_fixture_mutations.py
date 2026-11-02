#!/usr/bin/env python3
"""Exact five-test fixture-identity controls; no native junction qualification."""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = 'tests/release/test_installer_binary_payload.py'
TEST = 'tests/release/test_installer_reparse_fixture_identity.py'
HELPER = 'scripts/verify_installer_binaries.py'
EXPECTED = ['test_noncanonical_artifact_retains_original_reparse_rejection',
            'test_noncanonical_nested_bundle_reparse_is_rejected',
            'test_selected_lexical_and_canonical_spellings_receive_attributes']


def classify(code, output, expected):
    failures = re.findall(r'^FAIL: (\w+) ', output, re.MULTILINE)
    errors = re.findall(r'^ERROR: (\w+) ', output, re.MULTILINE)
    inventory = re.findall(r'^Ran (\d+) tests in ', output, re.MULTILINE)
    summary = re.findall(r'^(?:OK(?: \(.*\))?|FAILED \(.*\))$', output, re.MULTILINE)
    wanted = f'FAILED (failures={len(expected)})' if expected else 'OK'
    return {'qualified': inventory == ['5'] and not errors and 'skipped=' not in output
            and sorted(failures) == sorted(expected) and summary == [wanted]
            and code == (1 if expected else 0),
            'ordinary_failures': failures, 'runtime_errors': errors, 'test_inventory': inventory,
            'expected_failures': expected, 'summary': summary}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--lock', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    originals = {p: (ROOT / p).read_text() for p in (FIXTURE, TEST, HELPER)}
    receipt = {'source_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'],
                                                       text=True).strip(),
               'inputs': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in originals},
               'semantic_controls': 1, 'cases': [], 'native_execution': False}
    cases = [('positive', None, '', '', []),
             ('lexical_only', FIXTURE, 'return ReparseStat(value) if path in targets else value',
              'return ReparseStat(value) if path == target else value', EXPECTED),
             ('runtime_error_control', TEST, '        self.case.setUp()\n',
              "        raise RuntimeError('deliberate fixture control crash')\n", []),
             ('restored', None, '', '', [])]
    with tempfile.TemporaryDirectory(prefix='cs80-canonical-reparse-controls-') as temporary:
        scratch = Path(temporary)
        for name, edited, before, after, expected in cases:
            for relative, original in originals.items():
                target = scratch / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                if relative == edited:
                    if original.count(before) != 1:
                        raise AssertionError('Nonunique control anchor: ' + name)
                    original = original.replace(before, after)
                target.write_text(original)
            for cache in scratch.rglob('__pycache__'):
                shutil.rmtree(cache)
            with args.lock.open('a') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
                result = subprocess.run([sys.executable, str(scratch / TEST)], stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True, timeout=60, check=False)
            assessment = classify(result.returncode, result.stdout, expected)
            runtime = name == 'runtime_error_control'
            accepted = (not assessment['qualified'] and assessment['test_inventory'] == ['5']
                        and len(assessment['runtime_errors']) == 5
                        and not assessment['ordinary_failures'] and result.returncode == 1
                        and assessment['summary'] == ['FAILED (errors=5)']) if runtime else assessment['qualified']
            case_dir = args.out / name
            case_dir.mkdir()
            log = case_dir / 'run.log'
            log.write_text(result.stdout)
            if edited:
                archived = case_dir / edited
                archived.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(scratch / edited, archived)
            receipt['cases'].append({'case': name, 'passed': accepted, 'exit': result.returncode,
                                     'assessment': assessment,
                                     'log_sha256': hashlib.sha256(log.read_bytes()).hexdigest()})
            (args.out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
            if not accepted:
                raise AssertionError('Unqualified fixture control: ' + name + ': ' + str(assessment))
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
