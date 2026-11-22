#!/usr/bin/env python3
"""Require completed Linux CTest and strict validator results with retained raw exits."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import importlib.util
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET
from common import ROOT, WORK, SOURCE, RUN_ID, read_json, validate_run, validate_job, require_steps, download_artifact, input_identity, require

sys.dont_write_bytecode = True
LANES = {
    'headless': ('pluginval-headless', 'Validate VST3 headless'),
    'gui': ('pluginval-gui', 'Validate VST3 GUI under Xvfb'),
    'state-regression': ('pluginval-gui-seed-regression', 'Validate GUI state regression seed 0x2da72db'),
}


def plain_log(text):
    return re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', text)


def terminations(text):
    found = re.findall(r'Original validator termination: PID=(\d+); raw returncode=(-?\d+); kind=(exit|signal); ([^\r\n]+)', text)
    return [{'pid': int(pid), 'raw_returncode': int(code), 'kind': kind, 'details': details}
            for pid, code, kind, details in found]


def check_terminal_zero(records):
    require(bool(records), 'Missing original child termination record')
    final = records[-1]
    require(final['raw_returncode'] == 0 and final['kind'] == 'exit' and final['details'] == 'exit_code=0',
            'Validator final child did not exit zero; SUCCESS text cannot override a crash')


def ctest_execution_report(xml_bytes, raw_log):
    root = ET.fromstring(xml_bytes)
    cases = list(root.iter('testcase'))
    report = {'suite_attributes': root.attrib, 'testcase_records': len(cases), 'executed_passed': [],
              'failures': [], 'skipped': [], 'disabled': [], 'not_run': [], 'unknown_status': []}
    report['declared_failures_or_errors'] = [suite.attrib for suite in root.iter('testsuite')
        if int(suite.get('failures', '0')) or int(suite.get('errors', '0'))]
    for case in cases:
        item = {'name': case.get('name'), 'status': case.get('status'), 'time': case.get('time'),
                'skipped_messages': [node.get('message', '') for node in case.findall('skipped')]}
        status = (case.get('status') or '').lower()
        failed = case.find('failure') is not None or case.find('error') is not None
        skipped = bool(case.findall('skipped')) or status == 'skipped'
        disabled = status == 'disabled' or any('disabled' in text.lower() for text in item['skipped_messages'])
        not_run = status in ('notrun', 'not_run', 'not run')
        if failed:
            report['failures'].append(item)
        if skipped:
            report['skipped'].append(item)
        if disabled:
            report['disabled'].append(item)
        if not_run:
            report['not_run'].append(item)
        if status == 'run' and not any((failed, skipped, disabled, not_run)):
            report['executed_passed'].append(item)
        elif status not in ('run', 'skipped', 'disabled', 'notrun', 'not_run', 'not run'):
            report['unknown_status'].append(item)
    clean = re.sub(r'(?m)^\d{4}-\d\d-\d\dT\S+Z\s+', '', raw_log)
    numbered = [line for line in clean.splitlines() if re.match(r'\s*\d+/\d+\s+Test\s+#\d+:', line)]
    report['raw_nonexecuted_case_lines'] = [line for line in numbered if re.search(r'Skipped|Disabled|Not Run|Not Executed', line, re.I)]
    report['registered_denominators_in_log'] = sorted({int(re.match(r'\s*\d+/(\d+)', line)[1]) for line in numbered})
    report['classification_note'] = 'Lists retain raw JUnit status and may overlap (a disabled case may also be skipped). Only status=run without failure/error/skip/disabled/not-run is counted as executed_passed. Overall green is reported separately.'
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reports-artifact-json', type=Path, required=True)
    parser.add_argument('--test-evidence-artifact-json', type=Path, required=True)
    args = parser.parse_args()
    paths = {name: WORK / name for name in ('run.json', 'builder-job.json', 'validator-job.json', 'builder-job.log', 'validator-job.log')}
    run = read_json(paths['run.json'])
    native, validator = read_json(paths['builder-job.json']), read_json(paths['validator-job.json'])
    validate_run(run)
    validate_job(native, 'Linux / VST3 + Standalone')
    validate_job(validator, 'pluginval / Linux VST3 (strictness 10)')
    require(native['id'] == 137274597635, 'Wrong fixed native job')
    native_steps = require_steps(native, {
        'Configure', 'Verify configured JUCE source', 'Build', 'Test',
        'Capture curated preset auditions', 'Upload curated preset auditions',
        'Package Linux VST3 and Standalone', 'Upload Linux install bundle',
    })
    validator_steps = require_steps(validator, {value[1] for value in LANES.values()} | {
        'Download Linux VST3', 'Fetch pluginval 1.0.4', 'Verify diagnostic exit propagation',
        'Show first-attempt core diagnostics', 'Upload pluginval reports',
    })
    native_log = plain_log(paths['builder-job.log'].read_text())
    validator_log = plain_log(paths['validator-job.log'].read_text())
    passed = re.findall(r'100% tests passed, 0 tests failed out of (\d+)', native_log)
    require(len(passed) == 1, 'Missing complete Linux CTest success summary')
    for label, content in (('native', native_log), ('validator', validator_log)):
        require(re.search(r'(?m)^(?:\d{4}-\d\d-\d\dT\S+Z\s+)?##\[error\]', content) is None,
                label + ' job contains an error annotation')
    reports_meta = read_json(args.reports_artifact_json)
    outer, download = download_artifact(reports_meta, 'cs80-pluginval-reports')
    spec = importlib.util.spec_from_file_location('cs80_trusted_publisher', ROOT / 'scripts/publish-preset-refinement.py')
    publisher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(publisher)
    files = publisher.zip_files(outer)
    test_meta = read_json(args.test_evidence_artifact_json)
    test_outer, test_download = download_artifact(test_meta, 'cs80-linux-test-evidence')
    test_files = publisher.zip_files(test_outer)
    xml_names = [name for name in test_files if name == 'ctest-results.xml' or name.endswith('/ctest-results.xml')]
    require(len(xml_names) == 1, 'Missing or ambiguous native CTest JUnit evidence')
    execution_report = ctest_execution_report(test_files[xml_names[0]], native_log)
    require(execution_report['registered_denominators_in_log'] == [579], 'Unexpected registered native test count')
    require(not execution_report['failures'], 'JUnit records an actual failure; refusing green qualification')
    require(not execution_report['declared_failures_or_errors'], 'JUnit declares failures/errors; refusing green qualification')
    lanes = []
    for lane, (prefix, step_name) in LANES.items():
        final_name, core_name = prefix + '.log', prefix + '-core.log'
        require(final_name in files and core_name in files, 'Missing complete lane/core logs: ' + lane)
        final_text, core_text = files[final_name].decode(), files[core_name].decode()
        require(final_text.splitlines().count('SUCCESS') == 1 and 'FAILED' not in final_text, 'No clean final SUCCESS: ' + lane)
        records = terminations(core_text)
        require(1 <= len(records) <= 3, 'Missing or unexpected internal attempt count: ' + lane)
        check_terminal_zero(records)
        retained_attempts = []
        for index, record in enumerate(records[:-1], 1):
            name = prefix + '-attempt' + str(index) + '.log'
            require(name in files, 'Earlier attempt log was not preserved: ' + name)
            earlier = files[name].decode()
            require(record['raw_returncode'] in (-11, 139), 'Unexpected internal retry status')
            require('SUCCESS' not in earlier and 'FAILED' not in earlier, 'Ineligible retry after terminal marker')
            # Match the production wrapper's two exact awk patterns, including colon.
            phases = re.findall(r'(Starting|Completed) tests in:', earlier)
            require('Starting' in phases and phases[-1] == 'Completed', 'Ineligible retry before a completed phase')
            retained_attempts.append({'path': name, 'raw_returncode': record['raw_returncode'],
                                      'sha256': publisher.sha(files[name]), 'bytes': len(files[name])})
        actual_attempt_names = {name for name in files if re.fullmatch(re.escape(prefix) + r'-attempt\d+\.log', name)}
        require(actual_attempt_names == {item['path'] for item in retained_attempts}, 'Unaccounted earlier lane logs')
        lanes.append({'lane': lane, 'step': step_name, 'success_markers': 1,
                      'internal_attempt_count': len(records), 'original_terminations': records,
                      'earlier_attempts': retained_attempts,
                      'final_log': {'path': final_name, 'bytes': len(files[final_name]), 'sha256': publisher.sha(files[final_name])},
                      'core_log': {'path': core_name, 'bytes': len(files[core_name]), 'sha256': publisher.sha(files[core_name])}})
    raw_job_records = terminations(validator_log)
    require(raw_job_records == [record for lane in lanes for record in lane['original_terminations']],
            'Raw job termination history differs from named report logs')
    proof = {
        'assignment': 54, 'status': 'PASS', 'verified_at_utc': datetime.now(timezone.utc).isoformat(),
        'source_commit': SOURCE, 'run_id': RUN_ID, 'run_attempt': 1,
        'builder_job_id': native['id'], 'validator_job_id': validator['id'],
        'registered_ctest_count': 579, 'ctest_reported_total': int(passed[0]), 'ctest_failed': 0,
        'ctest_execution': execution_report, 'required_native_steps': native_steps,
        'required_validator_steps': validator_steps, 'lanes': lanes,
        'internal_retry_count': sum(lane['internal_attempt_count'] - 1 for lane in lanes),
        'reports_artifact_api': reports_meta, 'reports_download': download,
        'test_evidence_artifact_api': test_meta, 'test_evidence_download': test_download,
        'junit_member': {'path': xml_names[0], 'bytes': len(test_files[xml_names[0]]), 'sha256': publisher.sha(test_files[xml_names[0]])},
        'inputs': {name: input_identity(path) for name, path in paths.items()},
        'scope': 'Exact Linux builder and three strictness-10 lanes on first GitHub workflow/job attempt. Internal wrapper attempts, if any, are separately preserved and counted; GitHub run_attempt=1 alone does not establish zero internal retries. A post-SUCCESS nonzero raw child exit is rejected.'
    }
    (WORK / 'ci-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    brief = {key: proof[key] for key in ('status', 'run_id', 'run_attempt', 'registered_ctest_count', 'ctest_reported_total', 'internal_retry_count')}
    brief['execution_classifications'] = {key: len(execution_report[key]) for key in
        ('executed_passed', 'failures', 'skipped', 'disabled', 'not_run', 'unknown_status')}
    print(json.dumps(brief, indent=2))


if __name__ == '__main__':
    main()
