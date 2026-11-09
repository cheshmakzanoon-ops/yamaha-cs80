from pathlib import Path
import copy, hashlib, json, os, shutil, stat, subprocess, tempfile, zipfile
import yaml
import argparse
parser=argparse.ArgumentParser(description='Requires PyYAML plus the exact original ZIP and artifact metadata directory.')
parser.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[3])
parser.add_argument('--evidence',type=Path,required=True)
args=parser.parse_args()
ROOT=args.repo
EVIDENCE=args.evidence
v=yaml.load((ROOT/'.github/workflows/validator-6c560f1-diagnostic.yml').read_text(), Loader=yaml.BaseLoader)
job=v['jobs']['diagnose'];steps=job['steps'];base_env=job['env']
assert set(v['on'])=={'push','workflow_dispatch'}
assert v['on']['push']['paths']==['.github/workflows/validator-6c560f1-diagnostic.yml']
assert v['permissions']=={'contents':'read','actions':'read'}
assert steps[0]['with']['ref']==base_env['ORIGINAL_SOURCE']
assert 'GH_TOKEN' not in base_env
for step in steps:
 if 'run' in step:
  r=subprocess.run(['bash','-n'],input=step['run'],text=True,capture_output=True);assert not r.returncode,(step['name'],r.stderr)
verify=next(s for s in steps if s.get('id')=='artifact')['run']
source=verify.split("python3 - <<'PY'\n",1)[1].rsplit('\nPY',1)[0]
metadata=json.loads((EVIDENCE/'original-artifact-metadata.json').read_text())
archive=(EVIDENCE/'exact-plugin.zip').read_bytes()
results=[]
def run(name,mutate_metadata=None,mutate_archive=None,mutate_env=None,want=1):
 with tempfile.TemporaryDirectory(prefix='cs80-artifact-guard-') as directory:
  p=Path(directory);(p/'diagnostic-evidence').mkdir();m=copy.deepcopy(metadata);env=dict(base_env);data=archive
  if mutate_metadata:mutate_metadata(m)
  if mutate_archive:data=mutate_archive(data, m, env)
  if mutate_env:mutate_env(env)
  (p/'diagnostic-evidence/artifact.json').write_text(json.dumps(m));(p/'exact-plugin.zip').write_bytes(data)
  result=subprocess.run(['python3','-c',source],cwd=p,env={**os.environ,**env},capture_output=True,text=True)
  assert result.returncode==want,(name,result.returncode,result.stderr)
  results.append({'case':name,'exit':result.returncode,'expected':want})
run('exact_original',want=0)
run('wrong_artifact_id',lambda m:m.update(id=1))
run('wrong_artifact_name',lambda m:m.update(name='other'))
run('expired',lambda m:m.update(expired=True))
run('wrong_run',lambda m:m['workflow_run'].update(id=1))
run('wrong_source',lambda m:m['workflow_run'].update(head_sha='0'*40))
run('wrong_metadata_digest',lambda m:m.update(digest='sha256:'+'0'*64))
run('wrong_zip_bytes',mutate_archive=lambda b,m,e:b+b'tamper')
run('wrong_plugin_hash',mutate_env=lambda e:e.update(PLUGIN_SHA256='0'*64))
def changed_zip(data,m,e,kind):
 import io
 result=io.BytesIO()
 with zipfile.ZipFile(io.BytesIO(data)) as old,zipfile.ZipFile(result,'w') as new:
  for info in old.infolist():
   if kind=='symlink' and info.filename.endswith('.so'):info.external_attr=(stat.S_IFLNK|0o777)<<16
   new.writestr(info,old.read(info))
  if kind=='extra':new.writestr('../escape',b'untrusted')
  if kind=='duplicate':new.writestr(old.namelist()[0],b'duplicate')
 data=result.getvalue();e['ARTIFACT_SHA256']=hashlib.sha256(data).hexdigest();m['digest']='sha256:'+e['ARTIFACT_SHA256'];m['size_in_bytes']=len(data)
 return data
for kind in ('extra','duplicate','symlink'):run(kind,mutate_archive=lambda b,m,e,k=kind:changed_zip(b,m,e,k))
replays=[s for s in steps if s.get('id') in ('gdb','trace')]
assert len(replays)==2
for replay in replays:
 code=replay['run'];assert code.count('timeout --signal=TERM --kill-after=10s 240s')==1
 assert code.count('--random-seed "$REPLAY_SEED"')==1 and '--skip-gui-tests' not in code
 assert 'continue-on-error' not in replay and '--strictness-level 10 --validate-in-process --timeout-ms 120000' in code
 assert 'Original gate remains FAILED.' in code
print(json.dumps({'guards':results,'workflow':'YAML/shell, pinned original checkout, read-only permissions, bounded two GUI diagnostics and exact seed verified'},indent=2))
