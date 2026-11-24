#!/usr/bin/env python3
"""Join the subsequently completed Windows receipt and original rerun refusal.

Does not rerun native products or rewrite the earlier partial snapshot.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sys

BASE = Path('docs/preset-evidence/2026.11.7')
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
RUN = 45397189410


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    repo = args.repo.resolve()
    if args.out.exists():
        raise ValueError('Refusing an existing output')
    records = []

    def require(ok, message):
        if not ok:
            raise ValueError(message)

    def record(path):
        path = path if path.is_absolute() else repo / path
        raw = path.read_bytes()
        try:
            name = str(path.relative_to(repo))
        except ValueError:
            name = str(path)
        result = dict(path=name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())
        records.append(result)
        return raw, result

    def read(path):
        return json.loads(record(path)[0])

    a58 = BASE / 'verification/a58'
    native = read(a58 / 'phase1/native-observations.json')['windows-x86_64']
    artifacts = read(a58 / 'phase1/artifact-attempt-matrix.json')
    lanes = read(a58 / 'phase1/validator-lanes.json')
    win_path = BASE / 'ci/source-ee0dabca/windows-x86_64/platform-qualification.json'
    win = read(win_path)
    require((win['source'], win['tree'], win['run_id'], win['attempt'], win['target']) ==
            (SOURCE, TREE, RUN, 1, 'windows-x86_64'), 'Windows final receipt context differs')
    require((win['producer_job_id'], win['consumer_job_id']) == (138455976256, 138483772246), 'Windows original jobs differ')
    require(all(win[k] is True for k in ('native_results_accepted', 'original_attempt_binding_verified',
            'original_ctest_exit_zero_verified', 'original_validator_exits_zero_verified', 'platform_qualified')),
            'Windows scoped platform acceptance incomplete')
    require(win['release_qualified'] is False and win['global_qualified'] is False, 'Windows receipt overclaims scope')
    require(win['native']['counts'] == native['counts'], 'Windows final native counts differ from independently parsed archive')
    require(win['native']['formal_skips'] == native['formal_skips'] and
            win['native']['nested_python_skips'] == native['nested_python_skips'], 'Windows final skips differ')
    for artifact in [a for a in artifacts if a['target'] == 'windows-x86_64']:
        bound = win['artifact_bindings'][artifact['kind']]
        for key in ('artifact_id', 'bytes', 'sha256', 'producing_job_id', 'producing_attempt'):
            require(bound[key] == artifact[key], 'Windows final artifact/attempt differs: ' + key)
    for lane in [x for x in lanes if x['target'] == 'windows-x86_64']:
        match = [x for x in win['validators']['lanes'] if x['console']['sha256'] == lane['console']['sha256']]
        require(len(match) == 1 and match[0]['original_normal_exit'] == 0 and
                match[0]['report']['sha256'] == lane['report']['sha256'], 'Windows final original validator differs')
    for expected in win['input_report_identities']:
        actual = record(Path(expected['path']))[1]
        require(all(actual[k] == expected[k] for k in ('bytes', 'sha256')), 'Windows final component report changed')
    refusal_path = BASE / 'ci/source-ee0dabca/macos-x86_64/attempt-1-failed/rerun-request-while-active.json'
    refusal = read(refusal_path)
    require((refusal['source_commit'], refusal['run_id'], refusal['requested_job_id']) ==
            (SOURCE, RUN, 138455976307), 'Rerun refusal context differs')
    require(refusal['retry_started'] is False and refusal['original_failure_preserved'] is True and
            refusal['tool_result']['error_data']['status'] == '403', 'Rerun refusal not retained')
    require(refusal['tool_result']['error_data']['message'] == 'The workflow run containing this job is already running',
            'Rerun refusal reason differs')
    record(Path(__file__).resolve())
    result = dict(schema=1, status='partial_windows_completion_and_rerun_refusal_reconciled',
                  recorded_at_utc=datetime.now(timezone.utc).isoformat(), source_commit=SOURCE, source_tree=TREE,
                  run_id=RUN, external_platforms_qualified=['macos-arm64', 'windows-x86_64'],
                  windows_original_attempt=1, windows_receipt=next(x for x in records if x['path'] == str(win_path)),
                  windows_native_counts=native['counts'], windows_actual_component_report_identities_verified=len(win['input_report_identities']),
                  intel_original_failure_preserved=True, intel_retry_started=False,
                  intel_rerun_request_http_status=403,
                  intel_rerun_refusal_reason=refusal['tool_result']['error_data']['message'],
                  replaces_prior_snapshot=False, all_four_platforms_qualified=False, release_qualified=False,
                  remaining_gates_unchanged=['Intel accepted complete replacement attempt', 'Linux actual platform qualification',
                    'Actual audition content and CI binding', 'Four remaining full public GETs',
                    'Final provenance, skips, package, 60-review ledger and independent signoff'],
                  inputs=records, argv=sys.argv)
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ('status', 'external_platforms_qualified', 'intel_retry_started', 'release_qualified')}))


if __name__ == '__main__':
    main()
