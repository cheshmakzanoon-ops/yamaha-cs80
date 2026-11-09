#!/usr/bin/env python3
"""Independent post-run integrity checks; writes only the audit output."""
from pathlib import Path
import hashlib
import json
import re
import sys

root = Path(sys.argv[1]).resolve()
summary = json.loads((root / 'summary.json').read_text())
commands = json.loads((root / 'commands.json').read_text())
hash_file = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
checks = []


def verify(ok, label):
    checks.append({'check': label, 'passed': bool(ok)})
    if not ok:
        raise AssertionError(label)


verify(len(summary['mutations']) == 11, '11 scored behavioral mutants')
verify(summary['gate_passed'] and summary['inputs_unchanged'], 'gate and production invariants passed')
verify(summary['control']['status'] == 'passed', 'unmutated control passed')
verify(summary['redundancy_control']['status'] == 'undetected-redundant', 'literal redundant guard deletion disclosed as survivor')
cases = {'unmutated-control': summary['control'], **summary['mutations'],
         'omit-router-ui-guard-only': summary['redundancy_control']}
for name, case in cases.items():
    run = next(x for x in commands if x['label'] == name + '-run')
    text = (root / run['stdout']).read_text()
    lines = text.splitlines()
    matches = re.findall(r'^Immediate sixteen-voice release: ([0-9]+) checks, ([0-9]+) failures$', text, re.M)
    verify(len(matches) == 1 and lines[-1] == f'Immediate sixteen-voice release: 1418 checks, {case["failures"]} failures', name + ': unique final full summary')
    verify(int(matches[0][0]) == 1418 and int(matches[0][1]) == case['failures'], name + ': numerical summary agrees')
    verify(sum(x.startswith('FAIL: ') for x in lines) == case['failures'], name + ': exact failure diagnostics')
    verify(run['exit_code'] == case['expected_exit'] == case['exit_code'], name + ': normal expected exit')
    verify((case['failures'] > 0) == (case['expected_exit'] == 1), name + ': expected numerical outcome')
    verify(not (root / run['stderr']).read_text(), name + ': no runtime stderr')
    for artifact, key in [('Engine.o', 'engine_object_sha256'), ('tests.o', 'test_object_sha256'),
                          ('probe', 'binary_sha256'), ('link.map', 'link_map_sha256'), ('mutation.diff', 'diff_sha256')]:
        verify(hash_file(root / name / artifact) == case[key], name + ': ' + artifact + ' hash')
    verify('libcs80_core.a(Engine.cpp.o)' not in (root / name / 'link.map').read_text(), name + ': no stale archive Engine selected')
    source_hashes = json.loads((root / name / 'source-hashes.json').read_text())
    verify(all(hash_file(root / name / 'src' / p) == digest for p, digest in source_hashes.items()), name + ': private source hashes')
    link = next(x for x in commands if x['label'] == name + '-link')['argv']
    obj = str(root / name / 'Engine.o')
    lib = next(x for x in link if x.endswith('/libcs80_core.a'))
    verify(link.index(obj) < link.index(lib), name + ': private Engine before unchanged archive')
for cmd in commands:
    verify(hash_file(root / cmd['stdout']) == cmd['stdout_sha256'], cmd['label'] + ': stdout hash')
    verify(hash_file(root / cmd['stderr']) == cmd['stderr_sha256'], cmd['label'] + ': stderr hash')
    if not cmd['label'].endswith('-run'):
        verify(cmd['exit_code'] == 0, cmd['label'] + ': build/inspection succeeded')
inputs = json.loads((root / 'input-hashes.json').read_text())
verify(hash_file(root / 'frozen/SixteenVoiceReleaseTests.cpp') == inputs['source_test_sha256']['tests/SixteenVoiceReleaseTests.cpp'], 'fixture unchanged')
verify(hash_file(Path(inputs['archive_path'])) == inputs['archive_sha256'], 'matching archive still unchanged')
parser = json.loads((root / 'parser-self-test.json').read_text())
verify(parser['valid_cases'] == 2 and len(parser['rejected_invalid_cases']) == 12, 'parser positive and negative controls recorded')
result = {'passed': True, 'check_count': len(checks), 'checks': checks,
          'executed_reproducer_sha256': hash_file(root / 'reproducer.py'),
          'summary_sha256': hash_file(root / 'summary.json')}
(root / 'audit.json').write_text(json.dumps(result, indent=2) + '\n')
print(f'Evidence audit: {len(checks)} checks passed')
