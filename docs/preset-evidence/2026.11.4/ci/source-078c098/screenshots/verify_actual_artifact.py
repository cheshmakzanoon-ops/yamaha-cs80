#!/usr/bin/env python3
"""Read-only qualification of the explicit final-source screenshot artifact."""
import argparse,hashlib,io,json,re,stat,subprocess,zipfile
from pathlib import Path,PurePosixPath
from datetime import datetime
import numpy as np
from PIL import Image
SOURCE='078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
RUN=45246863473
JOB=137945802978

def require(ok,message):
 if not ok:raise ValueError(message)
def sha(b):return hashlib.sha256(b).hexdigest()
def date(s):return datetime.fromisoformat(s.replace('Z','+00:00'))

a=argparse.ArgumentParser()
for name in ['archive','artifact-api','run-api','jobs-api','job-log','repo','out','extract-to']:a.add_argument('--'+name,required=True)
x=a.parse_args(); api=json.loads(Path(x.artifact_api).read_text());run=json.loads(Path(x.run_api).read_text());jobs=json.loads(Path(x.jobs_api).read_text());log=Path(x.job_log).read_text()
job=next(j for j in jobs['jobs'] if j['id']==JOB)
require(run['id']==RUN and run['head_sha']==SOURCE and run['run_attempt']==1 and run['conclusion']=='success' and run['status']=='completed','Final run identity or outcome')
require(job['run_id']==RUN and job['head_sha']==SOURCE and job['run_attempt']==1 and job['status']=='completed' and job['conclusion']=='success','Final job identity or outcome')
steps={s['name']:s for s in job['steps']}
for name in ['Run actions/checkout@v5','Configure exact source','Build screenshot renderer and core tests','Verify core','Capture actual editor at every supported size and page','Publish screenshot evidence']:
 require(steps[name]['conclusion']=='success','Unsuccessful required producer step '+name)
upload=steps['Publish screenshot evidence']
require(api['id']>0 and api['name']=='cs80-panel-'+SOURCE and not api['expired'],'Artifact name/expiry')
require(api['workflow_run']['id']==RUN and api['workflow_run']['head_sha']==SOURCE,'Artifact run/source binding')
require(date(job['started_at'])<=date(api['created_at'])<=date(job['completed_at']),'Artifact outside producer job')
require(date(upload['started_at'])<=date(api['created_at'])<=date(upload['completed_at']),'Artifact outside upload step')
blob=Path(x.archive).read_bytes();digest=sha(blob)
require(len(blob)==api['size_in_bytes'] and 'sha256:'+digest==api['digest'],'Actual outer ZIP size or digest')
require(SOURCE in log and f"Artifact ID {api['id']}" in log and f"Final size is {len(blob)} bytes. Artifact ID is {api['id']}" in log and 'SHA256 digest of uploaded artifact zip is '+digest in log,'Original producer upload binding')
records=[];goldens=[];ex=Path(x.extract_to);ex.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(io.BytesIO(blob)) as z:
 names=set();files=set()
 for e in z.infolist():
  n=e.filename;c=n[:-1] if e.is_dir() else n
  require(c and '\\' not in c and ':' not in c and not c.startswith('/') and PurePosixPath(c).as_posix()==c and all(p not in ('','.','..') for p in c.split('/')),'Noncanonical archive path')
  require(c not in names,'Duplicate canonical archive path');names.add(c)
  kind=stat.S_IFMT(e.external_attr>>16)
  require(kind in ((0,stat.S_IFDIR) if e.is_dir() else (0,stat.S_IFREG)),'Unsupported archive member type')
  if not e.is_dir():files.add(n)
 require(sum(e.file_size for e in z.infolist())<1024**3,'Expanded archive bound')
 provenance=json.loads(z.read('provenance.json'))
 require(provenance['commit']==SOURCE and str(provenance['run'])==str(RUN),'Embedded source/run provenance')
 smoke=z.read('run.log').decode();require('UI SMOKE TEST PASSED' in smoke and 'FAIL:' not in smoke and not re.search(r'UI SMOKE TEST: \d+ FAILURE',smoke),'UI smoke outcome')
 require('golden refreshed:' not in smoke,'Golden refresh cannot qualify comparison')
 entries=z.read('SHA256SUMS').decode().splitlines();checks={}
 for line in entries:
  m=re.fullmatch(r'([0-9a-f]{64})  artifacts/ui/([^/]+\.png)',line);require(m is not None,'Malformed PNG checksum line')
  h,n=m.groups();require(n not in checks,'Duplicate PNG checksum');checks[n]=h;require(sha(z.read(n))==h,'PNG checksum mismatch '+n)
 top_png={n for n in files if '/' not in n and n.endswith('.png')};require(set(checks)==top_png,'Top-level PNG checksum inventory')
 for width,height in [(1600,1000),(1120,700)]:
  name=f'main-{width}x{height}';member='golden/golden-candidate-'+name+'.png';candidate=z.read(member)
  baseline=subprocess.check_output(['git','show',SOURCE+':tests/golden/'+name+'.png'],cwd=x.repo)
  before=np.asarray(Image.open(io.BytesIO(baseline)).convert('RGB'),dtype=np.int16);after=np.asarray(Image.open(io.BytesIO(candidate)).convert('RGB'),dtype=np.int16)
  require(before.shape==after.shape==(height,width,3),'Golden image dimensions')
  delta=np.abs(before-after).max(axis=2);s=np.float32(width)/np.float32(1600)
  rnd=lambda v:int(np.floor(np.float32(v)*s+np.float32(.5)))
  xx,yy,ww,hh=rnd(1118),0,rnd(342),rnd(48)
  mask=np.ones((height,width),dtype=bool);mask[yy:yy+hh,xx:xx+ww]=False
  masked=delta.copy();masked[~mask]=0
  fraction=float((masked>40).sum())/int(mask.sum())
  crop=masked[:height//8*8,:width//8*8];blocks=crop.reshape(height//8,8,width//8,8).mean(axis=(1,3));worst=float(blocks.max())
  require(fraction<=.003 and worst<=8.0,'Independent golden comparison failed '+name)
  expected=f'golden {name}: changed {fraction*100:.4f} % of pixels (limit 0.30 %), worst 8x8 block mean {worst:.2f} (limit 8.0)'
  require(expected in smoke,'Reported golden metrics differ from actual pixels')
  (ex/(name+'.png')).write_bytes(candidate);(ex/('diff-'+name+'.png')).write_bytes(z.read('golden/golden-diff-'+name+'.png'))
  goldens.append({'size':[width,height],'candidate_sha256':sha(candidate),'source_golden_sha256':sha(baseline),'all_pixels_equal':bool(np.array_equal(before,after)),'outside_mask_changed_fraction':fraction,'worst_8x8_block_mean':worst,'masked_rectangle':[xx,yy,ww,hh],'unchanged_limits_passed':True})
 (ex/'run.log').write_text(smoke);(ex/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
 for n in sorted(files):
  b=z.read(n);records.append({'path':n,'bytes':len(b),'sha256':sha(b)})
summary={'source':SOURCE,'run_id':RUN,'run_attempt':1,'job_id':JOB,'run_and_job_conclusion':'success','artifact':api,'actual_zip':{'bytes':len(blob),'sha256':digest},'producer_upload_bound':True,'provenance':provenance,'file_count':len(records),'top_level_png_count':len(checks),'all_archive_entries_reviewed':True,'top_level_png_checksums_passed':True,'ui_smoke':'UI SMOKE TEST PASSED','no_failed_assertions_in_full_smoke_log':True,'golden_refresh_used':False,'goldens':goldens,'producer_log_sha256':sha(Path(x.job_log).read_bytes()),'scope':'Actual artifact/source/UI qualification only. Not native plugin package validation or acoustic evidence.'}
Path(x.out).write_text(json.dumps(summary,indent=2)+'\n');Path(x.out).with_name('actual-artifact-members.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps({k:summary[k] for k in ['source','file_count','top_level_png_count','ui_smoke','goldens']},indent=2))
