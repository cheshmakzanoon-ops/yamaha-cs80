"""Inspect supporting-DSP ZIPs against authentic API/job evidence; no extraction."""
from pathlib import Path, PurePosixPath
import hashlib,json,zipfile,stat,subprocess,re,datetime
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parent/'yamaha-cs80'
SOURCE='078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
RUN=45246863831
UPLOAD_NAMES={'candidate':'Run actions/upload-artifact@v5','live-adr':'Upload live envelope candidate trace','dsp':'Upload source-traceable graphs and raw traces'}
NAMES={
 'synthetic':('cs80-synthetic-comparison-',137945816716,'synthetic-job.log'),
 'legacy':('cs80-legacy-compatibility-',137945817147,'legacy-job.log'),
 'source':('cs80-source-factory-compatibility-',137945817476,'source-job.log'),
 'candidate':('cs80-engine-candidate-',137945817176,'measure-job.log'),
 'live-adr':('cs80-engine-live-adr-',137945817176,'measure-job.log'),
 'dsp':('cs80-dsp-',137945817176,'measure-job.log'),
}
sha=lambda b:hashlib.sha256(b).hexdigest()
def gitbytes(p):return subprocess.check_output(['git','show',f'{SOURCE}:{p}'],cwd=REPO)
def dt(s):return datetime.datetime.fromisoformat(s.replace('Z','+00:00'))
run=json.loads((ROOT/'run.json').read_text()); jobs=json.loads((ROOT/'jobs.json').read_text())['jobs']; api=json.loads((ROOT/'artifacts.json').read_text())['artifacts']
assert run['id']==RUN and run['head_sha']==SOURCE and run['run_attempt']==1
results=[]
for key,(prefix,jobid,logname) in NAMES.items():
 path=ROOT/f'{key}.zip'
 if not path.exists():continue
 a=next(a for a in api if a['name']==prefix+SOURCE); j=next(j for j in jobs if j['id']==jobid)
 assert a['workflow_run']['id']==RUN and a['workflow_run']['head_sha']==SOURCE and not a['expired']
 assert j['run_id']==RUN and j['run_attempt']==1 and j['head_sha']==SOURCE and j['conclusion']=='success'
 assert dt(run['created_at'])<=dt(j['started_at'])<=dt(a['created_at'])<=dt(j['completed_at'])
 upload=next(s for s in j['steps'] if s['name']==UPLOAD_NAMES.get(key,'Run actions/upload-artifact@v5'))
 assert upload['conclusion']=='success' and dt(upload['started_at'])<=dt(a['created_at'])<=dt(upload['completed_at'])
 blob=path.read_bytes(); assert len(blob)==a['size_in_bytes'] and 'sha256:'+sha(blob)==a['digest']
 log=(ROOT/logname).read_text()
 assert f"Artifact ID {a['id']}" in log and a['digest'][7:] in log and f"Final size is {a['size_in_bytes']} bytes. Artifact ID is {a['id']}" in log
 finalize=next(x for x in log.splitlines() if f"Artifact {a['name']}.zip successfully finalized. Artifact ID {a['id']}" in x)
 assert dt(j['started_at'])<=dt(finalize.split()[0])<=dt(j['completed_at'])
 with zipfile.ZipFile(path) as z:
  names=set(); contents={}
  for e in z.infolist():
   n=e.filename; canon=n[:-1] if e.is_dir() else n
   assert canon and not canon.startswith('/') and '\\' not in canon and ':' not in canon and all(p not in ('','.','..') for p in canon.split('/')) and PurePosixPath(canon).as_posix()==canon
   assert canon not in names;names.add(canon)
   kind=stat.S_IFMT(e.external_attr>>16); assert kind in (0,stat.S_IFDIR if e.is_dir() else stat.S_IFREG)
   assert e.file_size<64*1024*1024
   if not e.is_dir():contents[n]=z.read(e)
  assert sum(map(len,contents.values()))<256*1024*1024
  checks={'archive_entries':len(names),'files':len(contents),'safe_unique_regular_entries':True}
  if key=='synthetic':
   r=json.loads(contents['report.json']); m=json.loads(contents['manifest.json'])
   assert r['passed'] and r['analog_evidence'] is False and r['evidence_kind']=='synthetic'
   assert r['provenance']['source_revision']==SOURCE and not r['provenance']['working_tree_dirty']
   assert r['manifest_sha256']==sha(contents['manifest.json'])
   for p,h in r['provenance']['source_hashes'].items():assert sha(gitbytes(p))==h
   for stage in m['stages']:
    for which in ('candidate','reference'):
     q=stage[which];assert sha(contents[q['path']])==q['sha256']
   checks.update({'report_passed':True,'source_hashes_matched':True,'trace_hashes_matched':8,'analog_evidence':False})
  elif key in ('legacy','source'):
   r=json.loads(contents['report.json']);assert r['candidate']==SOURCE and r['analog_evidence'] is False
   recorded_sums={}
   for line in contents['SHA256SUMS'].decode().splitlines():
    h,n=line.split(maxsplit=1);assert n not in recorded_sums and re.fullmatch('[0-9a-f]{64}',h);recorded_sums[n]=h
   if key=='legacy':
    assert r['baseline']==r['corpus_source']=='d56911a7c5f6953cd244457226b126cc567a538b' and r['default_off_byte_identical']
    assert sha(contents['baseline-corpus.txt'])==r['corpus_sha256']==recorded_sums['artifacts/compatibility/baseline-corpus.txt']
    assert len(recorded_sums)==5
    for scene in ('default','tails'):assert recorded_sums[f'artifacts/compatibility/baseline-{scene}.f32']==recorded_sums[f'artifacts/compatibility/current-{scene}.f32']
   else:
    assert r['circuit_baseline']=='22d6a32c48c85f9b813506edcd856f73f4293c2d' and r['audio_baseline']=='00ceccfacb3cb9845436701da01c4387d53a361a'
    assert r['v1_circuit_bytes_unchanged'] and r['v2_audio_and_numeric_state_bytes_unchanged']
    assert len(recorded_sums)==4
    assert recorded_sums['artifacts/source-factory-compatibility/audio-current.bin']==recorded_sums['artifacts/source-factory-compatibility/audio-v2.bin']
    assert recorded_sums['artifacts/source-factory-compatibility/circuit-current.bin']==recorded_sums['artifacts/source-factory-compatibility/circuit-v1.bin']
   checks.update({'report':r,'recorded_comparison_digests':recorded_sums,'recorded_digest_pairs_equal':True,'raw_audio_binaries_distributed':False,'byte_comparison_evidence':'Original successful producer cmp steps, with retained digest report; raw comparison bytes are not uploaded.'})
  elif key in ('candidate','live-adr'):
   r=json.loads(contents['provenance.json']);assert r['commit']==SOURCE and int(r['run'])==RUN and r['analog_evidence'] is False
   sums={}
   for line in contents['SHA256SUMS'].decode().splitlines():
    h,n=line.split(maxsplit=1);n=n.lstrip('*');assert n not in sums and n in contents and sha(contents[n])==h;sums[n]=h
   assert set(sums)==set(contents)-{'SHA256SUMS'}
   source_sums={}
   for line in contents['SOURCE_SHA256SUMS'].decode().splitlines():
    h,p=line.split(maxsplit=1);assert sha(gitbytes(p))==h;source_sums[p]=h
   checks.update({'manifest_checksums_matched':len(sums),'source_hashes_matched':len(source_sums),'analog_evidence':False})
  elif key=='dsp':
   r=json.loads(contents['measurements.json']);assert r['git_head']==SOURCE and r['probe_failures']==0 and r['comparative_regression_passed']
   for p,h in r['analysis_time_source_sha256'].items():assert sha(gitbytes(p))==h
   checks.update({'comparative_regression_passed':True,'probe_failures':0,'source_hashes_matched':len(r['analysis_time_source_sha256']),'analysis_working_tree_status':r['working_tree_status'],'scope':'Software numerical reference, not a physical instrument comparison'})
  results.append({'kind':key,'artifact_id':a['id'],'name':a['name'],'bytes':len(blob),'sha256':sha(blob),'created_at':a['created_at'],'expires_at':a['expires_at'],'producer_job_id':j['id'],'run_id':RUN,'run_attempt':1,'source':SOURCE,'producer_upload_line':finalize,'upload_step':upload,'artifact_id_ordering_used_as_provenance':False,'entry_inventory':[{ 'path':n,'bytes':len(b),'sha256':sha(b)} for n,b in sorted(contents.items())],'checks':checks})
(ROOT/'artifact-inspection.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps({'verified_artifacts':len(results),'kinds':[r['kind'] for r in results]},indent=2))
