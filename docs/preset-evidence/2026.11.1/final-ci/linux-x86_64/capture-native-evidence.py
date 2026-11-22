#!/usr/bin/env python3
"""Capture actual native CTest evidence independently of validator outcome."""
from datetime import datetime, timezone
import importlib.util
import json
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from common import ROOT, WORK, SOURCE, RUN_ID, read_json, validate_run, validate_job, require_steps, download_artifact, input_identity, require


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


run = read_json(WORK / 'run.json')
native = read_json(WORK / 'builder-job.json')
validate_run(run)
validate_job(native, 'Linux / VST3 + Standalone')
require(native['id'] == 137274597635, 'Wrong fixed native job')
steps = require_steps(native, {
    'Configure', 'Verify configured JUCE source', 'Build', 'Test',
    'Capture curated preset auditions', 'Upload curated preset auditions',
    'Package Linux VST3 and Standalone', 'Upload Linux install bundle',
})
gate = module('linux_gate_report', WORK / 'check-ci-gates.py')
publisher = module('cs80_trusted_publisher', ROOT / 'scripts/publish-preset-refinement.py')
raw = gate.plain_log((WORK / 'builder-job.log').read_text())
passed = re.findall(r'100% tests passed, 0 tests failed out of (\d+)', raw)
require(len(passed) == 1, 'Missing complete Linux CTest success summary')
require(re.search(r'(?m)^(?:\d{4}-\d\d-\d\dT\S+Z\s+)?##\[error\]', raw) is None, 'Native job has an error annotation')
meta = read_json(WORK / 'test-evidence-artifact.json')
outer, download = download_artifact(meta, 'cs80-linux-test-evidence')
files = publisher.zip_files(outer)
names = [name for name in files if name == 'ctest-results.xml' or name.endswith('/ctest-results.xml')]
require(len(names) == 1, 'Missing or ambiguous native CTest JUnit evidence')
report = gate.ctest_execution_report(files[names[0]], raw)
require(report['registered_denominators_in_log'] == [579], 'Unexpected registered test count')
require(not report['failures'] and not report['declared_failures_or_errors'], 'Actual native test failure')
times = re.findall(r'Total Test time \(real\) =\s*([0-9.]+) sec', raw)
proof = {
    'assignment': 54, 'status': 'PASS', 'scope': 'Native Linux CTest and required build steps only; no validator verdict is inferred.',
    'verified_at_utc': datetime.now(timezone.utc).isoformat(),
    'source_commit': SOURCE, 'run_id': RUN_ID, 'run_attempt': 1, 'builder_job_id': native['id'],
    'registered_ctest_count': 579, 'ctest_reported_total': int(passed[0]), 'ctest_failed': 0,
    'total_ctest_seconds': float(times[0]) if len(times) == 1 else None,
    'ctest_execution': report, 'required_native_steps': steps,
    'test_evidence_artifact_api': meta, 'test_evidence_download': download,
    'junit_member': {'path': names[0], 'bytes': len(files[names[0]]), 'sha256': publisher.sha(files[names[0]])},
    'inputs': {name: input_identity(WORK / name) for name in ('run.json', 'builder-job.json', 'builder-job.log')},
}
(WORK / 'native-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
print(json.dumps({'status': proof['status'], 'total_ctest_seconds': proof['total_ctest_seconds'],
    'counts': {key: len(report[key]) for key in ('executed_passed', 'failures', 'skipped', 'disabled', 'not_run', 'unknown_status')},
    'skipped': report['skipped'], 'disabled': report['disabled'], 'not_run': report['not_run']}, indent=2))
