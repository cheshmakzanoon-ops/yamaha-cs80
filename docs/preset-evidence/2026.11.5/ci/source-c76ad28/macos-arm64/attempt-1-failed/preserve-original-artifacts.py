#!/usr/bin/env python3
"""Preserve and bind failed original Mac artifacts; never accept a platform."""
from pathlib import Path
import base64, hashlib, importlib.util, json, zipfile
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[6]
DOWNLOADS=REPO.parent/'ci-c76-macos-arm64-downloads/attempt-1-failed'
spec=importlib.util.spec_from_file_location('a50_prepared_binder',ROOT.parent/'qualify-macos-arm64.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
require=b.require
log_raw=(ROOT/'original-producer-job.log').read_bytes()
require(hashlib.sha256(log_raw).hexdigest()=='fd381feec6c18ace447722f1e9da1d0917d45a0f80070e0d95edb8b31af05c9a','Original producer log changed')
log=b.rows(log_raw)
jobs=json.loads((ROOT/'jobs-api.json').read_text())['jobs']
job=next(j for j in jobs if j['id']==138299557178)
require(job['run_id']==45351731801 and job['run_attempt']==1 and job['head_sha']=='c76ad288e02fbeca48f39652bf4fcdd99837f793','Original job context')
require(job['status']=='completed' and job['conclusion']=='failure','Original failed job outcome')
artifacts=json.loads((ROOT/'artifacts-api.json').read_text())['artifacts']
records=[]
for stem,aid,size,digest,step_name in [
 ('test-evidence',14903471382,80433,'6900bb29e3d926d53802f9ebced7ebe0c0925cd67e26d29fe379ddc1ffb3f9ba','Upload test and JUCE source evidence'),
 ('pluginval',14903346660,6433,'2c88144dbabd2d437795f2c75365704e7e82d58ffc7896716f7898d6bc21bd85','Upload pluginval reports')]:
 api=next(a for a in artifacts if a['id']==aid)
 raw=(DOWNLOADS/(stem+'.zip')).read_bytes()
 require(api['size_in_bytes']==len(raw)==size and api['digest']=='sha256:'+hashlib.sha256(raw).hexdigest()=='sha256:'+digest,'Actual API ZIP identity')
 require(api['workflow_run']['id']==45351731801 and api['workflow_run']['head_sha']==job['head_sha'],'Artifact source/run')
 upload=b.step(job,step_name)
 exact=f"Artifact {api['name']} has been successfully uploaded! Final size is {size} bytes. Artifact ID is {aid}"
 indices=[i for i,(_,m) in enumerate(log) if m==exact];require(len(indices)==1,'Original unique upload identity')
 i=indices[0];start=max(k for k in range(i) if log[k][1]=='##[group]Run actions/upload-artifact@v5')
 section=[m for _,m in log[start:i+1]]
 require('  name: '+api['name'] in section and 'SHA256 digest of uploaded artifact zip is '+digest in section,'Original upload digest')
 require(b.in_step(log[i][0],upload) and b.in_step(api['created_at'],upload) and b.in_step(api['updated_at'],upload),'Original upload timestamps')
 inventory=b.inventory(raw,stem+'.zip')
 with (ROOT/(stem+'-api.json')).open('x') as f:json.dump(api,f,indent=2);f.write('\n')
 # Text encodings retain exact ZIP bytes after GitHub may replace same-named artifacts.
 encoded=base64.encodebytes(raw)
 require(base64.decodebytes(encoded)==raw,'Lossless ZIP preservation')
 with (ROOT/(stem+'.zip.base64')).open('xb') as f:f.write(encoded)
 with zipfile.ZipFile(DOWNLOADS/(stem+'.zip')) as z:
  for info in z.infolist():
   if info.is_dir():continue
   dest=ROOT/('contents-'+stem)/info.filename
   dest.parent.mkdir(parents=True,exist_ok=True)
   with dest.open('xb') as f:f.write(z.read(info))
 records.append({'artifact':api,'original_zip':{'path':str(DOWNLOADS/(stem+'.zip')),'bytes':size,'sha256':digest},'lossless_preservation':{'path':stem+'.zip.base64','bytes':len(encoded),'sha256':hashlib.sha256(encoded).hexdigest(),'decoded_bytes_verified':True},'original_upload_step':upload,'logged_upload_at':log[i][0],'original_upload_attempt_bound':True,'all_members':inventory})
result={'source':job['head_sha'],'run_id':45351731801,'attempt':1,'producer_job_id':job['id'],'job_conclusion':'failure','platform_qualified':False,'release_qualified':False,'actual_artifacts_preserved':records,'original_producer_log':{'bytes':len(log_raw),'sha256':hashlib.sha256(log_raw).hexdigest()},'scope':'Exact original failed attempt archives, API receipts, every member and upload bindings preserved before any same-name artifact replacement; not platform acceptance.'}
with (ROOT/'preservation-summary.json').open('x') as f:json.dump(result,f,indent=2);f.write('\n')
print(json.dumps({'artifacts_preserved':[r['artifact']['id'] for r in records],'all_members_read':sum(len(r['all_members']['members']) for r in records),'lossless_zip_encodings_retained':True,'platform_qualified':False}))
