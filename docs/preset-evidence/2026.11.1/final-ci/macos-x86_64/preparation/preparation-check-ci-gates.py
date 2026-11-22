#!/usr/bin/env python3
"""Check the exact first-attempt Intel Mac job against retained native CI logs."""
import hashlib
import json
from pathlib import Path
import re

WORK = Path(__file__).resolve().parent
SOURCE = '6d3cc220edef5a4a2c7cd968ea8a786c7c83198c'
RUN = 45044040694
JOB = 137274597912
run = json.loads((WORK / 'run.json').read_text())
job = json.loads((WORK / 'job.json').read_text())
log = (WORK / 'job.log').read_text()
assert run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == 1
assert run['head_branch'] == 'main' and run['event'] == 'push'
assert run['name'] == 'build' and run['path'] == '.github/workflows/build.yml'
assert run['head_repository']['full_name'] == 'cheshmakzanoon-ops/yamaha-cs80'
assert job['id'] == JOB and job['run_id'] == RUN and job['head_sha'] == SOURCE and job['run_attempt'] == 1
assert job['name'] == 'macOS / macos-15-intel / VST3 + AU + Standalone'
assert job['status'] == 'completed' and job['conclusion'] == 'success'
required = {
    'Record native platform', 'Configure', 'Verify configured JUCE source', 'Build', 'Test',
    'Fetch pluginval 1.0.4', 'Register built AU', 'Validate VST3 with pluginval',
    'Validate AU with pluginval', 'Validate AU with pluginval GUI',
    'Package native macOS VST3, AU and Standalone', 'Upload native macOS install bundle',
}
steps = {s['name']: s for s in job['steps']}
assert len(steps) == len(job['steps']), 'Duplicate step names'
assert all(steps.get(name, {}).get('conclusion') == 'success' for name in required)
plain = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', log)
passed = re.findall(r'100% tests passed, 0 tests failed out of (\d+)', plain)
assert len(passed) == 1, 'Missing or ambiguous full CTest success'
markers = re.findall(r'(?m)^(?:\d{4}-\d\d-\d\dT\S+Z\s+)?SUCCESS\s*$', plain)
assert len(markers) == 3, 'Expected three exact pluginval completion markers'
assert plain.count('--strictness-level 10') >= 3
assert plain.count('set -euo pipefail') >= 3
assert not re.search(r'(?m)^.*##\[error\]', plain), 'Job contains an error annotation'
proof = {
    'assignment': 53, 'status': 'PASS', 'source_commit': SOURCE,
    'run_id': RUN, 'run_attempt': 1, 'job_id': JOB, 'job_name': job['name'],
    'job_conclusion': job['conclusion'], 'started_at': job['started_at'], 'completed_at': job['completed_at'],
    'required_steps': [steps[name] for name in sorted(required)],
    'ctest_count': int(passed[0]), 'ctest_failed': 0, 'pluginval_exact_success_markers': len(markers),
    'validator_exit_evidence': 'All three successful required steps use set -euo pipefail, run pluginval through tee, then require an exact SUCCESS marker. Successful step/pipeline status establishes validator exit zero; numeric raw child exits are not separately printed in the log.',
    'inputs': {name: {'bytes': (WORK / name).stat().st_size, 'sha256': hashlib.sha256((WORK / name).read_bytes()).hexdigest()} for name in ('run.json', 'job.json', 'job.log')},
    'scope': 'The exact Intel Mac builder, CTest, VST3, AU and AU-GUI validation lanes. Other platforms or publication may remain pending.',
}
(WORK / 'ci-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
print(json.dumps({key: proof[key] for key in ('status', 'run_attempt', 'job_id', 'ctest_count', 'pluginval_exact_success_markers')}, indent=2))
