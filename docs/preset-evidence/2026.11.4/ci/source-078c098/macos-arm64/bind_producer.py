#!/usr/bin/env python3
"""Bind the two actual arm64 artifacts to their original successful producer."""
from pathlib import Path
from datetime import datetime, timezone, timedelta
import hashlib, json, re

ROOT = Path('/workspace/scratch/dde4ad85ed5b')
HERE = ROOT / 'a51'
INPUT = ROOT / 'release-078c098/input'
SOURCE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
RUN = 45246863788
JOB = 137945805542

def read(p): return json.loads(p.read_text())
def dt(s): return datetime.fromisoformat(s.replace('Z', '+00:00'))
def record(p):
    b=p.read_bytes()
    return {'path': str(p), 'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest()}

run, job = read(HERE/'run.json'), read(HERE/'job.json')
assert run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == 1
assert job['id'] == JOB and job['run_id'] == RUN and job['head_sha'] == SOURCE and job['run_attempt'] == 1
assert job['name'] == 'macOS / macos-15 / VST3 + AU + Standalone'
assert job['status'] == 'completed' and job['conclusion'] == 'success'
steps={x['name']:x for x in job['steps']}
required=['Run actions/checkout@v5', 'Record native platform', 'Configure',
          'Verify configured JUCE source', 'Build', 'Test',
          'Package native macOS VST3, AU and Standalone',
          'Upload native macOS install bundle', 'Upload test and JUCE source evidence']
for name in required: assert steps[name]['conclusion'] == 'success', name
log=(HERE/'job.log').read_text()
assert SOURCE in log, 'Exact source absent from original job log'

artifacts=[]
for stem,name,step in [
    ('macos-arm64-install','cs80-macos-15-install-bundle','Upload native macOS install bundle'),
    ('macos-arm64-test-evidence','cs80-macos-15-test-evidence','Upload test and JUCE source evidence')]:
    api=read(INPUT/(stem+'-api.json'))
    archive=INPUT/(stem+'.zip')
    assert api['name']==name and api['expired'] is False
    assert api['workflow_run']['id']==RUN and api['workflow_run']['head_sha']==SOURCE
    identity=record(archive)
    assert identity['bytes']==api['size_in_bytes']
    assert api['digest']=='sha256:'+identity['sha256']
    assert dt(api['expires_at']) > datetime.now(timezone.utc)
    start,end=dt(steps[step]['started_at']),dt(steps[step]['completed_at'])
    # API artifact times have second precision; allow only that rounding margin.
    assert start-timedelta(seconds=1)<=dt(api['created_at'])<=end+timedelta(seconds=1)
    final=re.search(r'Artifact '+re.escape(name)+r'\.zip successfully finalized\. Artifact ID '+str(api['id'])+r'\b',log)
    assert final, 'Original upload finalization identity missing'
    prefix=log[:final.start()]
    preceding_digest=list(re.finditer(r'SHA256 digest of uploaded artifact zip is ([0-9a-f]{64})',prefix))[-1].group(1)
    assert preceding_digest==identity['sha256'], 'Original upload digest mismatch'
    summary=re.search(r'Artifact '+re.escape(name)+r' has been successfully uploaded! Final size is (\d+) bytes\. Artifact ID is (\d+)',log)
    assert summary and int(summary[1])==identity['bytes'] and int(summary[2])==api['id']
    artifacts.append({'api':api,'download':identity,'api_file':record(INPUT/(stem+'-api.json')),
                      'upload_step':steps[step], 'original_upload_digest':preceding_digest,
                      'producer_created_at_within_upload_step':True})

out={'status':'original_arm64_producer_and_download_bytes_bound', 'source':SOURCE,
     'run_id':RUN,'run_attempt':1,'native_job_id':JOB,'native_job_conclusion':job['conclusion'],
     'run_conclusion_at_snapshot':run['conclusion'], 'required_steps':[steps[n] for n in required],
     'artifacts':artifacts,'original_job_log':record(HERE/'job.log'),
     'run_api':record(HERE/'run.json'),'job_api':record(HERE/'job.json'),
     'scope':'Installation and native Test producer only. Validator lanes and AU before/after payload identity are qualified separately.'}
path=HERE/'producer-binding.json'
assert not path.exists(), 'Refusing stale output'
path.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'status':out['status'],'artifact_ids':[a['api']['id'] for a in artifacts]}))
