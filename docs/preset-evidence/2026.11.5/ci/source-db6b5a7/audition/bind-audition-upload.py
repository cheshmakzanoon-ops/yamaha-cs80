#!/usr/bin/env python3
"""Bind accepted audition bytes to an explicitly selected original CI producer.

This does not download or execute a product, establish API authenticity, or
qualify the whole native release. Use authenticated original API/log inputs.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess

REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
VERIFIER_PATH = 'docs/preset-evidence/2026.11.5/verification/verify-audition-archive.py'
VERIFIER_SHA = '1dad81d5daf81900d36bfeaf6b18da5e85db04e88ef6a63613cc72c5f30a0583'
INVENTORY_PATH = 'docs/preset-evidence/2026.11.5/inventory.json'

def require(ok, message):
    if not ok: raise ValueError(message)

def sha(data): return hashlib.sha256(data).hexdigest()

def snapshot(path, limit):
    with Path(path).open('rb') as f:
        require(stat.S_ISREG(os.fstat(f.fileno()).st_mode), 'Nonregular input: '+str(path))
        data = f.read(limit)
    require(0 < len(data) < limit, 'Input outside bound: '+str(path))
    return data

def unique(pairs):
    out = {}
    for k,v in pairs:
        require(k not in out, 'Duplicate JSON key: '+k)
        out[k] = v
    return out

def parse(data):
    def reject(x): raise ValueError('Nonfinite JSON constant: '+x)
    return json.loads(data, object_pairs_hook=unique, parse_constant=reject)

def stamp(value): return datetime.fromisoformat(value.replace('Z','+00:00'))

def step(job, name):
    rows = [s for s in job['steps'] if s['name'] == name]
    require(len(rows) == 1 and rows[0]['status'] == 'completed'
            and rows[0]['conclusion'] == 'success', 'Original step did not succeed: '+name)
    return rows[0]

def inside(value, s):
    return stamp(s['started_at']) <= stamp(value) < stamp(s['completed_at']) + timedelta(seconds=1)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', type=Path, required=True)
    p.add_argument('--source', required=True)
    p.add_argument('--run-id', type=int, required=True)
    p.add_argument('--attempt', type=int, required=True)
    p.add_argument('--producer-job', type=int, required=True)
    for name in ('run-api','jobs-api','artifact-api','producer-log','content-report','archive','out'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args()
    require(re.fullmatch('[0-9a-f]{40}',a.source) is not None, 'Expected full source SHA')
    require(all(x > 0 for x in (a.run_id,a.attempt,a.producer_job)), 'Expected positive CI identities')
    require(not a.out.exists(), 'Output already exists')
    blobs = {name:snapshot(getattr(a,name),16*1024**2)
             for name in ('run_api','jobs_api','artifact_api','producer_log','content_report')}
    run,jobs,api,content = (parse(blobs[n]) for n in ('run_api','jobs_api','artifact_api','content_report'))
    archive = snapshot(a.archive,512*1024**2)
    digest = sha(archive)
    git = lambda path: subprocess.check_output(['git','-C',str(a.repo),'show',a.source+':'+path])
    tree = subprocess.check_output(['git','-C',str(a.repo),'rev-parse',a.source+'^{tree}']).decode().strip()
    require(sha(git(VERIFIER_PATH)) == VERIFIER_SHA == content['verifier_sha256'], 'Verifier identity')
    require(run['id'] == a.run_id and run['run_attempt'] == a.attempt
            and run['head_sha'] == a.source, 'Run identity')
    require(run['path'] == '.github/workflows/build.yml' and run['event'] == 'push', 'Workflow/event')
    require(run['repository']['full_name'] == run['head_repository']['full_name'] == REPOSITORY,
            'Repository identity')
    require(run['head_commit']['tree_id'] == tree, 'Run tree identity')
    matches = [j for j in jobs['jobs'] if j['id'] == a.producer_job]
    require(len(matches) == 1, 'Unique original producer required')
    job = matches[0]
    require(job['run_id'] == a.run_id and job['run_attempt'] == a.attempt
            and job['head_sha'] == a.source and job['name'] == 'Linux / VST3 + Standalone',
            'Producer identity')
    require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Producer incomplete or failed')
    needed = ['Run actions/checkout@v5','Configure','Verify configured JUCE source','Build','Test',
              'Capture curated preset auditions','Upload curated preset auditions']
    steps = {name:step(job,name) for name in needed}
    workflow = git('.github/workflows/build.yml').decode()
    linux = workflow.split('\n  linux:\n',1)[1].split('\n  pluginval:\n',1)[0]
    capture_block = linux.split('      - name: Capture curated preset auditions\n',1)[1].split('      - name:',1)[0]
    require('continue-on-error' not in capture_block and '|| true' not in capture_block
            and 'python3 scripts/capture-factory-auditions.py' in capture_block
            and '--capture build/cs80_preset_capture --out preset-audition' in capture_block,
            'Capture command or exit contract changed')
    for path, expected in content['source_files'].items():
        data = git(path)
        require(expected == {'bytes':len(data),'sha256':sha(data)}, 'Content source input mismatch: '+path)
    inventory_hash = sha(git(INVENTORY_PATH))
    require(content['expected_inventory_source_path'] == INVENTORY_PATH
            and content['expected_inventory_sha256'] == inventory_hash, 'Expected inventory binding')
    require(content['status'] == 'accepted_actual_archive_content'
            and content['accepted_archive_content'] is True and content['release_qualified'] is False,
            'Content inspection is not an accepted scoped result')
    require(content['source_commit'] == a.source and content['source_tree'] == tree
            and content['library_revision'] == '2026.11.5' and content['repository'] == REPOSITORY,
            'Content source/revision identity')
    require(content['expected_ci_identity_checked'] is True
            and str(content['run_id']) == str(a.run_id) and str(content['run_attempt']) == str(a.attempt),
            'Content run/attempt identity')
    require(content['zip_sha256'] == digest and content['zip_bytes'] == len(archive), 'Content/actual ZIP binding')
    expected_counts = {'wav_count':171,'selected_program_count':81,'regular_files':216,
                       'checksum_count':215,'actual_stereo_frames':49593600,'actual_audio_seconds':1033.2,
                       'nonfinite_samples':0,'samples_over_full_scale':0}
    require(all(content[k] == v for k,v in expected_counts.items()), 'Actual content totals')
    require(content['body_models'] == list(range(1,20)) and content['body_variants'] == list(range(5)),
            'Actual material/model coverage')
    identity = content['inventory_match']
    require(identity['programs'] == 523 and identity['parameters'] == 202 and identity['values'] == 105646
            and identity['all_ordered_identities_identical'] is True
            and identity['all_binary32_values_identical'] is True
            and identity['expected_sha256'] == inventory_hash, 'Delivered inventory identity')
    name = 'cs80-curated-preset-audition-'+a.source
    base = 'https://api.github.com/repos/'+REPOSITORY+'/actions/artifacts/'+str(api['id'])
    require(api['name'] == name and api['url'] == base and api['archive_download_url'] == base+'/zip',
            'Artifact identity/routes')
    require(api['size_in_bytes'] == len(archive) and api['digest'] == 'sha256:'+digest, 'API/actual ZIP bytes')
    require(api['expired'] is False and stamp(api['expires_at']) > datetime.now(timezone.utc), 'Artifact expired')
    require(api['workflow_run']['id'] == a.run_id and api['workflow_run']['head_sha'] == a.source
            and api['workflow_run']['head_branch'] == 'main', 'Artifact source/run')
    require(api['workflow_run']['repository_id'] == run['repository']['id']
            == api['workflow_run']['head_repository_id'], 'Artifact repository IDs')
    text = re.sub(r'\x1b\[[0-9;]*m','',blobs['producer_log'].decode('utf-8-sig')).replace('\r\n','\n')
    rows = [(m[1],m[2]) if (m:=re.match(r'^(\d{4}-\d\d-\d\dT[^ ]+Z) (.*)$',line)) else (None,line)
            for line in text.splitlines()]
    require(any(t and m == a.source and inside(t,steps['Run actions/checkout@v5']) for t,m in rows),
            'Original checked-out source absent')
    command = '##[group]Run python3 scripts/capture-factory-auditions.py --capture build/cs80_preset_capture --out preset-audition'
    commands = [(t,m) for t,m in rows if m == command]
    require(len(commands) == 1 and inside(commands[0][0],steps['Capture curated preset auditions']),
            'Original unique capture command absent')
    exact = f'Artifact {name} has been successfully uploaded! Final size is {len(archive)} bytes. Artifact ID is {api["id"]}'
    hits = [i for i,(_,m) in enumerate(rows) if m == exact]
    require(len(hits) == 1, 'Original unique upload receipt absent')
    i = hits[0]
    starts = [k for k in range(i) if rows[k][1] == '##[group]Run actions/upload-artifact@v5']
    require(starts, 'Original upload action boundary absent')
    section = [m for _,m in rows[starts[-1]:i+1]]
    require('  name: '+name in section and '  path: preset-audition/' in section
            and 'SHA256 digest of uploaded artifact zip is '+digest in section, 'Original upload name/path/hash')
    upload = steps['Upload curated preset auditions']
    require(all(inside(t,upload) for t in (rows[i][0],api['created_at'],api['updated_at'])), 'Original upload timestamps')
    inputs = {n:{'path':str(getattr(a,n)),'bytes':len(b),'sha256':sha(b)} for n,b in blobs.items()}
    inputs['archive'] = {'path':str(a.archive),'bytes':len(archive),'sha256':digest}
    report = dict(schema=1,status='accepted_actual_audition_with_original_upload_binding',
                  audition_artifact_qualified=True,release_qualified=False,native_plugin_qualified=False,
                  source=a.source,source_tree=tree,run_id=a.run_id,run_attempt=a.attempt,
                  producer_job_id=a.producer_job,artifact=api,inputs=inputs,
                  content_verifier_sha256=VERIFIER_SHA,binder_sha256=sha(Path(__file__).read_bytes()),
                  required_original_steps=steps,original_upload_timestamp=rows[i][0],
                  original_upload_id_size_digest_and_time_bound=True,
                  inventory_match=identity,body_models=content['body_models'],body_variants=content['body_variants'],
                  capture_binary_sha256=content['capture_binary_sha256'],max_peak_dbfs=content['max_peak_dbfs'],
                  **expected_counts,limits=[
                    'Authenticity of supplied API responses and original job log is established by external authenticated acquisition.',
                    'The unchanged content checker inspects the ZIP; this binder requires its exact hash and immutable same-hash archive input.',
                    'Capture executable stability is source-pinned producer provenance; the executable is not independently distributed in this archive.',
                    'Representative clips cover 81 existing programs; all 523 programs are inventoried.',
                    'No playback, human listening approval, acoustic equivalence, native-host or whole-release qualification is inferred.'])
    a.out.parent.mkdir(parents=True,exist_ok=True)
    with a.out.open('x') as f: json.dump(report,f,indent=2,allow_nan=False); f.write('\n')
    print(json.dumps({k:report[k] for k in ('status','source','run_id','run_attempt','producer_job_id','wav_count','release_qualified')}))

if __name__ == '__main__':
    main()
