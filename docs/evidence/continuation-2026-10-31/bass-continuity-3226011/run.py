from pathlib import Path
import subprocess,json,hashlib,datetime
root=Path('/workspace/scratch/2f805bffd21a/cs80-ui-publication')
out=Path('/tmp/cs80-bass-continuity-3226011')
manifest=json.loads((out/'manifest.json').read_text());manifest['started_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();manifest['steps']=[]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def save(): (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
def execute(name,command):
 log=out/(name+'.log')
 with log.open('w') as stream: result=subprocess.run(command,stdout=stream,stderr=stream,cwd=out)
 manifest['steps'].append({'name':name,'command':command,'exit':result.returncode,'log':str(log),'log_sha256':sha(log)});save();print(name,result.returncode,flush=True)
 if result.returncode:raise RuntimeError(name+' failed; '+str(log))
flags=['c++','-O3','-DNDEBUG','-std=gnu++20','-fPIC','-Wall','-Wextra','-Wpedantic','-I'+str(root/'src'),'-I'+str(root/'src/core')]
for name in ['probe.cpp','Engine-traced.cpp']:
 manifest[name+'_sha256']=sha(out/name)
manifest['driver_sha256']=sha(Path(__file__))
try:
 execute('compile-probe',flags+['-c',str(out/'probe.cpp'),'-o',str(out/'probe.o')])
 execute('compile-traced-engine',flags+['-c',str(out/'Engine-traced.cpp'),'-o',str(out/'Engine-traced.o')])
 for variant,engine in [('control','Engine-control.o'),('traced','Engine-traced.o')]:
  binary=out/('bass-'+variant)
  execute('link-'+variant,['c++',str(out/'probe.o'),str(out/engine),str(out/'FactoryPresets.o'),'-o',str(binary)])
  execute('run-'+variant,[str(binary),str(out/variant)])
  manifest[variant+'_binary_sha256']=sha(binary)
 for rate in [48000,96000]:
  a=out/'control'/('audio-'+str(rate)+'.bin');b=out/'traced'/a.name
  equal=a.read_bytes()==b.read_bytes()
  manifest.setdefault('audio_instrumentation_equivalence',[]).append({'rate':rate,'bytes':a.stat().st_size,'byte_identical':equal,'control_sha256':sha(a),'traced_sha256':sha(b)})
  if not equal:raise RuntimeError('Instrumentation changed audio at '+str(rate))
 manifest['status']='passed diagnostic neutrality and renders'
except BaseException as error:
 manifest['status']='failed';manifest['error']=repr(error);raise
finally:
 manifest['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();save()
print(json.dumps({'status':manifest['status'],'equivalence':manifest['audio_instrumentation_equivalence']},indent=2),flush=True)
