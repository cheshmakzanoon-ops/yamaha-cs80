#!/usr/bin/env python3
"""Bind actual Linux archives to original successful jobs; never qualify a release.

All provenance is explicit. The script can be used for a later source without
relabeling this db6b checkpoint. It does not execute downloaded product bytes.
Native counts, lane contents/exits/retries and payload identity remain separate.
"""
import argparse, hashlib, json, re, subprocess
from datetime import datetime, timedelta, timezone
from pathlib import Path

REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'

def need(condition, reason):
    if not condition:
        raise ValueError(reason)

def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result

def read(path):
    return json.loads(path.read_bytes(), object_pairs_hook=unique)

def sha(raw):
    return hashlib.sha256(raw).hexdigest()

def stamp(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))

def records(path):
    raw = path.read_bytes()
    text = raw.decode('utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')
    text = re.sub(r'\x1b\[[0-9;]*m', '', text.replace('\r\n', '\n'))
    output = []
    for line in text.splitlines():
        m = re.match(r'^(\d{4}-\d\d-\d\dT[^ ]+Z) (.*)$', line)
        output.append((m[1], m[2]) if m else (None, line))
    return output

def successful_step(job, name):
    matches = [s for s in job['steps'] if s['name'] == name]
    need(len(matches) == 1, 'Original step absent/nonunique: ' + name)
    step = matches[0]
    need(step['status'] == 'completed' and step['conclusion'] == 'success', 'Original step not successful: ' + name)
    return step

def within(timestamp, step):
    return timestamp is not None and stamp(step['started_at']) <= stamp(timestamp) < stamp(step['completed_at']) + timedelta(seconds=1)

def file_record(path):
    raw = path.read_bytes()
    return {'path': str(path), 'bytes': len(raw), 'sha256': sha(raw)}

def main(args):
    need(re.fullmatch(r'[0-9a-f]{40}', args.source) is not None, 'Expected complete source SHA')
    root, downloads = args.evidence.resolve(), args.downloads.resolve()
    run = read(root / 'run-attempt-api.json')
    jobs = read(root / 'jobs-api.json')['jobs']
    artifacts = read(root / 'artifacts-api.json')['artifacts']
    need(run['id'] == args.run_id and run['run_attempt'] == args.attempt and run['head_sha'] == args.source, 'Run provenance mismatch')
    need(run['path'] == '.github/workflows/build.yml', 'Wrong native workflow')
    need(run['repository']['full_name'] == run['head_repository']['full_name'] == REPOSITORY, 'Wrong repository')
    producers = [j for j in jobs if j['id'] == args.producer_job_id and j['name'] == 'Linux / VST3 + Standalone']
    consumers = [j for j in jobs if j['name'] == 'pluginval / Linux VST3 (strictness 10)']
    need(len(producers) == len(consumers) == 1, 'Missing unique Linux producer/consumer')
    producer, consumer = producers[0], consumers[0]
    required_producer = ['Install build dependencies', 'Configure', 'Verify configured JUCE source', 'Build', 'Test', 'Capture curated preset auditions', 'Upload curated preset auditions', 'Upload Linux VST3', 'Render demo and screenshots', 'Package Linux VST3 and Standalone', 'Upload Linux install bundle', 'Upload test and JUCE source evidence']
    required_consumer = ['Download Linux VST3', 'Install pluginval runtime', 'Fetch pluginval 1.0.4', 'Verify diagnostic exit propagation', 'Validate VST3 headless', 'Validate VST3 GUI under Xvfb', 'Validate GUI state regression seed 0x2da72db', 'Show first-attempt core diagnostics', 'Select failed validator log for diagnostics', 'Build and test fault-thread diagnostic shim', 'Upload pluginval reports']
    for job, names in [(producer, required_producer), (consumer, required_consumer)]:
        need(job['run_id'] == args.run_id and job['run_attempt'] == args.attempt and job['head_sha'] == args.source, 'Job provenance mismatch')
        need(job['status'] == 'completed' and job['conclusion'] == 'success', 'Original job not successful')
        for name in names:
            successful_step(job, name)
    producer_log = records(root / 'producer-job.log')
    consumer_log = records(root / 'validator-job.log')
    for log in (producer_log, consumer_log):
        need(any(message == args.source for _, message in log), 'Original checkout SHA absent')
        need(not any(re.search(r'^##\[error\]|Process completed with exit code [1-9]', message) for _, message in log), 'Original error marker present')
    bindings = []
    specifications = [('install', 'cs80-linux-install-bundle', producer, producer_log, 'Upload Linux install bundle'), ('vst3', 'cs80-linux-vst3', producer, producer_log, 'Upload Linux VST3'), ('test-evidence', 'cs80-linux-test-evidence', producer, producer_log, 'Upload test and JUCE source evidence'), ('pluginval', 'cs80-pluginval-reports', consumer, consumer_log, 'Upload pluginval reports')]
    for stem, name, job, log, upload_name in specifications:
        api = read(root / (stem + '-api.json'))
        need([a for a in artifacts if a['name'] == name] == [api], 'API collection mismatch: ' + name)
        raw = (downloads / (stem + '.zip')).read_bytes()
        need(0 < len(raw) <= 512 * 1024 ** 2, 'Archive size bound')
        digest = sha(raw)
        need(api['size_in_bytes'] == len(raw) and api['digest'] == 'sha256:' + digest, 'Actual ZIP/API hash or size mismatch')
        need(api['expired'] is False and stamp(api['expires_at']) > datetime.now(timezone.utc), 'Expired archive')
        need(api['workflow_run']['id'] == args.run_id and api['workflow_run']['head_sha'] == args.source, 'Artifact source/run mismatch')
        need(api['workflow_run']['repository_id'] == api['workflow_run']['head_repository_id'] == run['repository']['id'], 'Artifact repository mismatch')
        upload = successful_step(job, upload_name)
        exact = f"Artifact {name} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {api['id']}"
        indices = [i for i, (_, message) in enumerate(log) if message == exact]
        need(len(indices) == 1, 'Original upload name/ID/size absent or duplicated')
        index = indices[0]
        begin = max(i for i in range(index) if log[i][1] == '##[group]Run actions/upload-artifact@v5')
        section = [message for _, message in log[begin:index + 1]]
        need('  name: ' + name in section and 'SHA256 digest of uploaded artifact zip is ' + digest in section, 'Original upload name/digest mismatch')
        need(within(log[index][0], upload) and within(api['created_at'], upload) and within(api['updated_at'], upload), 'Artifact not within original upload step')
        bindings.append({'name': name, 'artifact_id': api['id'], 'api': api, 'archive': file_record(downloads / (stem + '.zip')), 'producer_job_id': job['id'], 'attempt': args.attempt, 'upload_step': upload, 'logged_upload_utc': log[index][0], 'original_upload_id_size_digest_and_time_bound': True})
    vst3 = read(root / 'vst3-api.json')
    download_step = successful_step(consumer, 'Download Linux VST3')
    downloaded = [(timestamp, message) for timestamp, message in consumer_log if within(timestamp, download_step)]
    receipt = f"- cs80-linux-vst3 (ID: {vst3['id']}, Size: {vst3['size_in_bytes']}, Expected Digest: {vst3['digest']})"
    need(sum(message == receipt for _, message in downloaded) == 1, 'Original consumer artifact receipt mismatch')
    need(sum(message == 'SHA256 digest of downloaded artifact is ' + vst3['digest'].split(':')[1] for _, message in downloaded) == 1, 'Original consumer downloaded digest mismatch')
    need(any(message == 'Artifact download completed successfully.' for _, message in downloaded), 'Original consumer download incomplete')
    source_files = {}
    for path in ['.github/workflows/build.yml', 'scripts/run-pluginval-linux.sh', 'scripts/run-pluginval-with-core.py', 'scripts/verify-ci-juce-source.cmake', 'scripts/package-ci-artifacts.py', 'scripts/verify-ci-bundle.py']:
        raw = subprocess.check_output(['git', '-C', str(args.repo), 'show', args.source + ':' + path])
        source_files[path] = {'bytes': len(raw), 'sha256': sha(raw)}
    result = {'schema': 1, 'status': 'original_linux_execution_and_artifact_binding_verified', 'platform_qualified': False, 'release_qualified': False, 'source': args.source, 'tree': subprocess.check_output(['git', '-C', str(args.repo), 'rev-parse', args.source + '^{tree}']).decode().strip(), 'run_id': args.run_id, 'attempt': args.attempt, 'producer_job_id': producer['id'], 'consumer_job_id': consumer['id'], 'original_required_producer_steps': [successful_step(producer, n) for n in required_producer], 'original_required_consumer_steps': [successful_step(consumer, n) for n in required_consumer], 'artifact_bindings': bindings, 'original_consumer_input_artifact_bound': True, 'source_files': source_files, 'input_records': [file_record(root / n) for n in ['run-attempt-api.json', 'jobs-api.json', 'artifacts-api.json', 'producer-job.log', 'validator-job.log']], 'remaining_external_gates': ['Actual complete JUnit/LastTest native roster, all fifteen new tests, all skips and JUCE provenance.', 'All three validator console/report lanes, original core exits and exact starts; preserve and independently review each prior retry against the source wrapper.', 'A41 full installation/native-header acceptance and A36 complete packaged/consumed VST3 identity.', 'Other platforms, actual audition acceptance and publication gates.'], 'scope': 'Original execution/API/hash/upload/consumer binding only. Archive member inspection, native results, lane acceptance, retries and full payload identity are not inferred.'}
    with args.out.open('x') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'status': result['status'], 'artifacts': len(bindings), 'platform_qualified': False, 'release_qualified': False}))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--run-id', type=int, required=True)
    parser.add_argument('--attempt', type=int, required=True)
    parser.add_argument('--producer-job-id', type=int, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--downloads', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    main(parser.parse_args())
