from pathlib import Path
import hashlib,json,shutil,subprocess,sys
p=Path(__file__).resolve().parent;w=Path(sys.argv[1]).resolve();dst=Path(sys.argv[2]).resolve();assert not dst.exists();dst.mkdir()
base=w/'normal';obs=w/'witness'
rows=[r for r in json.loads((w/'engine-results.json').read_text())['rows'] if r['arm']=='witness'];out=[]
for arm,defs in [('prefilter',[]),('prefilter-witness',['-DCS80_WSC_HORIZON_OBSERVE','-DCS80_WSC_HORIZON_VERIFY'])]:
 d=dst/arm;d.mkdir();shutil.copytree(base/'src',d/'src');h=d/'src/core/ReconstructedOscillator.h';subprocess.run([sys.executable,p/'transform.py',h,h],check=True)
 x=(w/'engine.cpp').read_text()
 a='ReconstructedOscillator::horizonFast=ReconstructedOscillator::horizonSlow=ReconstructedOscillator::horizonGrants=0;'
 x=x.replace(a,a+'ReconstructedOscillator::prefilterFpReads=0;')
 a='    if(!nonzero&&scene!=5)std::abort();';x=x.replace(a,'#ifdef CS80_WSC_HORIZON_OBSERVE\n    std::printf("FP_reads=%llu\\n",(unsigned long long)ReconstructedOscillator::prefilterFpReads);\n#endif\n'+a)
 (d/'harness.cpp').write_text(x)
 flags=['-std=gnu++20','-O3','-DNDEBUG','-fPIC','-Wall','-Wextra','-Wpedantic',*defs,'-I'+str(d/'src')]
 for member,src in [('Engine',d/'src/core/Engine.cpp'),('harness',d/'harness.cpp')]:
  with (d/(member+'-build.log')).open('x') as log:subprocess.run(['g++',*flags,'-c',src,'-o',d/(member+'.o')],check=True,stdout=log,stderr=log)
 subprocess.run(['g++',d/'harness.o',d/'Engine.o',base/'FactoryPresets.o',base/'Ig00156Batch.o','-o',d/'probe'],check=True)
 for i,r in enumerate(rows):
  f=d/f'{i}.trace';run=subprocess.run([d/'probe',str(r['scene']),str(f),str(r['rate']),str(int(r['filterKernel']))],capture_output=True,text=True,check=True)
  ls=run.stdout.splitlines();j=json.loads(ls[0]); assert hashlib.sha256(f.read_bytes()).hexdigest()==r['sha256']
  if arm=='prefilter-witness':
   assert j['fast']==r['fast'] and j['stock']==r['stock'] and j['certificates']==r['certificates'] and j['falseWitness']==0
   count=int(ls[1].split('=')[1]);assert 0<=count<=r['fast']+r['stock'];assert count==0 if r['rate']!=48000 or r['scene'] in [2,4,5] else count>0
  else: count=None
  record={'arm':arm,'row':i,'scene':r['scene'],'rate':r['rate'],'filterKernel':r['filterKernel'],'original_fp_read_pairs':r['fast']+r['stock'],'new_observed_fp_read_pairs':count,'original_and_new_shadow_fast':r['fast'],'correct_reference_sha256':r['sha256']};out.append(record);print(json.dumps(record),flush=True);f.unlink()
(dst/'new-dispatch.json').write_text(json.dumps(out,indent=2)+'\n')
