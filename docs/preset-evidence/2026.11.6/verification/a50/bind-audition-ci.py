#!/usr/bin/env python3
"""Bind an accepted .6 content report to retained authenticated GitHub evidence.

Inputs are caller-retained raw API JSON and original job log, not generated
summaries. This program verifies their agreement; transport authentication is
established by the caller's GitHub retrieval receipt. It never qualifies a release.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('audition_content', HERE / 'verify-audition-archive.py')
content = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(content)
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
TREE = '74e3cbd3e5f9a2cd75135d4de67bf24ff0066b18'
RUN = 45366603800
ATTEMPT = 1
JOB = 138349816508
REPO = content.REPOSITORY
require = content.require


def stamp(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))


def bind(report, artifact, run, job, log):
    require(report.get('accepted_archive_content') is True and report.get('status') == 'accepted_actual_archive_content',
            'Actual archive content has not been accepted')
    require(report.get('verifier_sha256') == content.file_sha(HERE / 'verify-audition-archive.py'),
            'Content report came from another verifier')
    require(report.get('source_commit') == SOURCE and report.get('source_tree') == TREE,
            'Content source mismatch')
    require(str(report.get('run_id')) == str(RUN) and str(report.get('run_attempt')) == str(ATTEMPT)
            and report.get('expected_ci_identity_checked') is True, 'Content run/attempt mismatch')
    for key, expected in [('wav_count', 225), ('selected_program_count', 89), ('actual_audio_seconds', 1276.2),
                          ('actual_raw_audio_bytes', 490060800), ('regular_files', 276), ('checksum_count', 275),
                          ('nonfinite_samples', 0), ('samples_over_full_scale', 0)]:
        require(report.get(key) == expected, 'Content count/safety mismatch: ' + key)
    require(report.get('inventory_match', {}).get('values') == 106692, 'Content inventory size mismatch')
    require(run['id'] == RUN and run['run_attempt'] == ATTEMPT and run['head_sha'] == SOURCE
            and run['repository']['full_name'] == REPO, 'Run API identity mismatch')
    require(job['id'] == JOB and job['run_id'] == RUN and job['run_attempt'] == ATTEMPT
            and job['head_sha'] == SOURCE and job['name'] == 'Linux / VST3 + Standalone',
            'Producer job identity mismatch')
    require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Producer is not completed successfully')
    steps = job['steps']
    required = ['Run actions/checkout@v5', 'Build', 'Test', 'Capture curated preset auditions', 'Upload curated preset auditions']
    chosen = []
    for name in required:
        found = [step for step in steps if step['name'] == name]
        require(len(found) == 1 and found[0]['status'] == 'completed' and found[0]['conclusion'] == 'success',
                'Producer required step missing/failed: ' + name)
        chosen.append(found[0])
    require(all(a['number'] < b['number'] and stamp(a['completed_at']) <= stamp(b['started_at'])
                for a,b in zip(chosen, chosen[1:])), 'Producer step order mismatch')
    name = 'cs80-curated-preset-audition-' + SOURCE
    require(artifact['name'] == name and artifact['expired'] is False, 'Artifact name/expiry mismatch')
    require(artifact['workflow_run']['id'] == RUN and artifact['workflow_run']['head_sha'] == SOURCE,
            'Artifact source/run mismatch')
    require(artifact['digest'] == 'sha256:' + report['zip_sha256']
            and artifact['size_in_bytes'] == report['zip_bytes'], 'API ZIP digest/size mismatch')
    require(artifact['url'] == f'https://api.github.com/repos/{REPO}/actions/artifacts/{artifact["id"]}',
            'Artifact repository URL mismatch')
    capture, upload = chosen[-2:]
    require(stamp(capture['completed_at']) <= stamp(artifact['created_at']) <= stamp(job['completed_at']),
            'Artifact time outside completed producer interval')
    lines = log.splitlines()
    section = []
    for line in lines:
        match = re.match(r'^(\S+) (.*)$', line)
        if not match:
            continue
        try:
            time = stamp(match[1])
        except ValueError:
            continue
        # API timestamps have second precision; include the complete end second.
        if stamp(upload['started_at']) <= time and int(time.timestamp()) <= int(stamp(upload['completed_at']).timestamp()):
            section.append(match[2])
    text = '\n'.join(section)
    success = f'Artifact {name} has been successfully uploaded! Final size is {artifact["size_in_bytes"]} bytes. Artifact ID is {artifact["id"]}'
    require(text.count(success) == 1, 'Actual upload record missing or duplicated')
    digest_records = re.findall(r'SHA256 digest of uploaded artifact zip is ([0-9a-f]{64})', text)
    require(digest_records == [report['zip_sha256']], 'Upload digest missing/ambiguous/mismatched')
    require(f'Artifact download URL: https://github.com/{REPO}/actions/runs/{RUN}/artifacts/{artifact["id"]}' in text,
            'Upload artifact/run URL mismatch')
    require('python3 scripts/capture-factory-auditions.py --capture build/cs80_preset_capture --out preset-audition' in log,
            'Original producer capture command absent')
    return dict(schema=1, status='actual_audition_content_and_ci_bound', audition_qualified=True,
                release_qualified=False, source_commit=SOURCE, source_tree=TREE,
                repository=REPO, run_id=RUN, run_attempt=ATTEMPT, producer_job_id=JOB,
                artifact_id=artifact['id'], artifact_name=name, zip_bytes=report['zip_bytes'],
                zip_sha256=report['zip_sha256'], artifact_created_at=artifact['created_at'],
                artifact_expires_at=artifact['expires_at'], producer_steps=chosen,
                wav_count=225, selected_program_count=89, folder_count=46, audio_seconds=1276.2,
                raw_audio_bytes=490060800, inventory_programs=523, inventory_parameters=204,
                capture_binary_sha256=report['capture_binary_sha256'],
                capture_binary_bytes_independently_available=False,
                limitations=['Caller must retain the authenticated GitHub retrieval receipts.',
                             'Executable hash and unchanged pre/post identity come from the complete authenticated producer manifest; executable bytes are not uploaded.',
                             'One note-on, fixed pressure and note-off per clip; no actual arpeggiator patterns, pedal gestures, repeated strikes or live bends.',
                             'Content and provenance acceptance does not establish acoustic equivalence or human listening approval.',
                             'Overall release and every platform host-validation gate remain separate.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ['content-report', 'artifact-api', 'run-api', 'job-api', 'job-log', 'out']:
        parser.add_argument('--' + flag, required=True, type=Path)
    args = parser.parse_args()
    raw = {key: content.bounded_snapshot(getattr(args, key), 32 * 1024 * 1024, key)
           for key in ['content_report', 'artifact_api', 'run_api', 'job_api', 'job_log']}
    report = bind(*[content.parse_json(raw[key]) for key in ['content_report', 'artifact_api', 'run_api', 'job_api']], raw['job_log'].decode())
    report['retained_inputs'] = {key: dict(path=str(getattr(args,key)), bytes=len(data), sha256=content.sha(data)) for key,data in raw.items()}
    report['binder_sha256'] = content.file_sha(Path(__file__))
    content.write_fresh_report(args.out, report)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError) as error:
        print('Audition CI binding failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
