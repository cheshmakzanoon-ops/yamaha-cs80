#!/usr/bin/env python3
"""Preserve an observed failed gate without substituting successful diagnostics."""
from datetime import datetime, timezone
import importlib.util
import json
import re
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from common import ROOT, WORK, SOURCE, RUN_ID, read_json, validate_run, require, input_identity

spec = importlib.util.spec_from_file_location('gate_report', WORK / 'check-ci-gates.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)
run = read_json(WORK / 'run-final.json')
job = read_json(WORK / 'validator-job.json')
validate_run(run)
require(run['status'] == 'completed' and run['conclusion'] == 'failure', 'Expected actual failed build run')
require(job['id'] == 137324977642 and job['run_id'] == RUN_ID and job['head_sha'] == SOURCE
        and job['run_attempt'] == 1 and job['status'] == 'completed' and job['conclusion'] == 'failure',
        'Wrong final failed validator identity')
steps = {item['name']: item for item in job['steps']}
raw_job = gate.plain_log((WORK / 'validator-job.log').read_text())
report_dir = WORK / 'validator-reports'
lanes = []
for name, (prefix, step_name) in gate.LANES.items():
    log = (report_dir / (prefix + '.log')).read_text()
    core = (report_dir / (prefix + '-core.log')).read_text()
    records = gate.terminations(core)
    earlier = sorted(path.name for path in report_dir.glob(prefix + '-attempt*.log'))
    require(len(records) == 1 and not earlier, 'Unexpected internal attempts; retain and review them')
    seeds = re.findall(r'^Random seed: (0x[0-9a-fA-F]+)$', log, re.M)
    require(len(seeds) == 1, 'Ambiguous original lane seed')
    phases = re.findall(r'(Starting|Completed) tests in:', log)
    code = records[0]['raw_returncode']
    normalized = code if code >= 0 else 128 - code
    eligible = normalized == 139 and 'SUCCESS' not in log and 'FAILED' not in log and 'Starting' in phases and phases[-1] == 'Completed'
    try:
        gate.check_terminal_zero(records)
        terminal_check = {'passed': True}
    except ValueError as error:
        terminal_check = {'passed': False, 'reason': str(error)}
    lanes.append({
        'lane': name, 'step': steps[step_name], 'random_seed': seeds[0],
        'success_markers': log.splitlines().count('SUCCESS'), 'failed_substring_present': 'FAILED' in log,
        'original_terminations': records, 'normalized_wrapper_exit': normalized,
        'internal_attempt_count': len(records), 'earlier_attempt_logs': earlier,
        'retry_eligible_under_unchanged_wrapper': eligible,
        'terminal_zero_check': terminal_check,
        'original_core_available': 'No local ELF core was available for this attempt.' not in core,
        'core_log': core, 'final_test_log_tail': log.splitlines()[-12:],
        'log_identity': input_identity(report_dir / (prefix + '.log')),
        'core_identity': input_identity(report_dir / (prefix + '-core.log')),
    })
require(gate.terminations(raw_job) == [entry for lane in lanes for entry in lane['original_terminations']],
        'Raw job termination history differs from named report logs')
gui = next(lane for lane in lanes if lane['lane'] == 'gui')
require(gui['step']['conclusion'] == 'failure' and gui['success_markers'] == 1
        and gui['original_terminations'][0]['raw_returncode'] == -11,
        'Observed failure signature changed')
diagnostics = []
for kind, filename, step_name, pattern in (
    ('gdb', 'pluginval-gui-gdb.log', 'Diagnose validator failure with gdb', r'^Diagnostic process exit: (\d+); log capture exit: (\d+)$'),
    ('fault-thread', 'pluginval-gui-trace.log', 'Diagnose validator with fault-thread trace', r'^Trace diagnostic process exit: (\d+); log capture exit: (\d+)$'),
):
    content = (report_dir / filename).read_text()
    clean_job = re.sub(r'(?m)^\d{4}-\d\d-\d\dT\S+Z\s+', '', raw_job)
    exits = re.findall(pattern, clean_job, re.M)
    seeds = re.findall(r'^Random seed: (0x[0-9a-fA-F]+)$', content, re.M)
    require(len(exits) == 1 and seeds == [gui['random_seed']], 'Missing same-seed diagnostic evidence')
    diagnostics.append({'kind': kind, 'step': steps[step_name], 'random_seed': seeds[0],
        'success_markers': content.splitlines().count('SUCCESS'),
        'process_exit': int(exits[0][0]), 'log_capture_exit': int(exits[0][1]),
        'log_identity': input_identity(report_dir / filename),
        'interpretation': 'Diagnostic nonreproduction only; original randomized GUI gate remains failed.'})
check = subprocess.run([sys.executable, str(WORK / 'check-ci-gates.py'),
    '--reports-artifact-json', str(WORK / 'validator-reports-artifact.json'),
    '--test-evidence-artifact-json', str(WORK / 'test-evidence-artifact.json')],
    cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
require(check.returncode != 0, 'Strict success qualifier unexpectedly accepted failed job')
native = read_json(WORK / 'native-proof.json')
proof = {
    'assignment': 54, 'status': 'FAIL', 'release_qualification': 'BLOCKED_BY_REQUIRED_GUI_VALIDATOR_FAILURE',
    'verified_at_utc': datetime.now(timezone.utc).isoformat(),
    'source_commit': SOURCE, 'run_id': RUN_ID, 'run_attempt': 1,
    'builder_job_id': 137274597635, 'validator_job_id': job['id'],
    'artifact_byte_verification': 'PASS', 'native_ctest_gate': 'PASS', 'validator_gate': 'FAIL',
    'failure_signature': 'Randomized GUI validator printed SUCCESS, then actual child terminated with SIGSEGV (raw -11; normalized 139).',
    'lanes': lanes, 'internal_retry_count': 0, 'diagnostics': diagnostics,
    'fault_thread_log': (report_dir / 'pluginval-fault-trace.log').read_text(),
    'fault_thread_log_identity': input_identity(report_dir / 'pluginval-fault-trace.log'),
    'original_crash_stack_available': False,
    'cause_limit': 'No original local core was available with the kernel pipe collector. Same-seed GDB and fault-thread replays exited zero. The evidence does not establish crash ownership or root cause.',
    'native_counts': {key: len(native['ctest_execution'][key]) for key in ('executed_passed','failures','skipped','disabled','not_run','unknown_status')},
    'native_registered_tests': native['registered_ctest_count'],
    'native_total_seconds': native['total_ctest_seconds'],
    'strict_success_qualifier_result': {'exit_code': check.returncode, 'stdout': check.stdout, 'stderr': check.stderr},
    'validator_reports': read_json(WORK / 'validator-reports-manifest.json'),
    'inputs': {name: input_identity(WORK / name) for name in ('run-final.json','builder-job.json','validator-job.json','builder-job.log','validator-job.log','artifact-proof.json','native-proof.json')},
    'actions': 'Read-only GitHub/archive inspection. No CI retry/cancellation, source or wrapper changes, publication, or downloaded product execution.',
}
(WORK / 'ci-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
print(json.dumps({'status': proof['status'], 'release_qualification': proof['release_qualification'],
    'original_gui_seed': gui['random_seed'], 'raw_gui_returncode': gui['original_terminations'][0]['raw_returncode'],
    'internal_retry_count': proof['internal_retry_count'], 'original_crash_stack_available': False,
    'strict_success_qualifier_exit': check.returncode}, indent=2))
