#!/usr/bin/env python3
"""Fresh, untimed, finite cutoff/provenance-pinned stock vs. WSC interior arms."""
import argparse,hashlib,io,json,pathlib,shutil,subprocess,sys,tarfile
p=pathlib.Path(__file__).resolve().parent;a=argparse.ArgumentParser()
a.add_argument('checkout',type=pathlib.Path)
a.add_argument('work',type=pathlib.Path)
a.add_argument('--stage',choices=['prepare','build','run'],required=True)
args=a.parse_args();repo=args.checkout.resolve();w=args.work.resolve()
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def invoke(args):subprocess.run(list(map(str,args)),check=True,cwd=w,timeout=240)
baseline='c037a8970f939bdcd7d98ea56348006f82f2412cc64cd912fbf0f85b5ffa9373'
base='db02cd8fa56892c6ea2cf7df084bcf190204968e'
assert subprocess.check_output(['git','-C',str(repo),'rev-parse',base+'^{commit}'],text=True).strip()==base
arms={'stock':[], 'normal':[], 'witness':['-DCS80_WSC_HORIZON_VERIFY','-DCS80_WSC_HORIZON_OBSERVE'], 'off':['-DCS80_WSC_HORIZON_FORCE_OFF','-DCS80_WSC_HORIZON_OBSERVE']}
if args.stage=='prepare':
 w.mkdir(parents=True,exist_ok=False)
 # Never consume concurrent working-tree edits: take exactly the named Git tree.
 snap=w/'immutable';snap.mkdir()
 archive=subprocess.check_output(['git','-C',str(repo),'archive',base,'src','docs/experiments/wsc-terminal-occupancy/census.cpp'])
 with tarfile.open(fileobj=io.BytesIO(archive),mode='r:') as tf:
  for m in tf:
   if m.isdir(): continue
   assert m.isfile() and not m.name.startswith('/') and '..' not in pathlib.PurePosixPath(m.name).parts
   q=snap/m.name;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(tf.extractfile(m).read())
 assert sha(snap/'src/core/ReconstructedOscillator.h')==baseline
 assert sha(snap/'src/core/Engine.cpp')=='27418070012931bc96ebb8937f016d4ae4fe1e239e97f6dc8959d5cc63bc994f'
 invoke([sys.executable,p/'make.py',snap/'src/core/ReconstructedOscillator.h',w/'gen'])
 stock=snap/'docs/experiments/wsc-terminal-occupancy/census.cpp';x=stock.read_text()
 a=x.index('extern "C" void wsc_census_sample_begin');b=x.index('namespace census {\nvoid publicTrace(',a);c=x.index('void run(unsigned which,',b)
 x=x[:a]+'}\n'+x[b:c]+(p/'engine_tail.inc').read_text();x=x.replace('fixture/source_factory_benchmark.cpp','tools/source_factory_benchmark.cpp')
 # The stock census writes only current events. This new, shorter qualification
 # writes all 64 retained physical events at each snapshot like the direct suite.
 x=x.replace('for(std::size_t i=0;i<d.eventCount;++i)','for(std::size_t i=0;i<d.events.size();++i)')
 (w/'engine.cpp').write_text(x)
 for arm in arms:
  dst=w/arm/'src';(dst/'core').mkdir(parents=True,exist_ok=False)
  # Copy only in-repo source code. No installed plugin or external dependencies.
  shutil.copytree(snap/'src',dst,dirs_exist_ok=True)
  h=dst/'core/Engine.h';s=h.read_text();assert s.count('class Engine\n{')==1;h.write_text(s.replace('class Engine\n{','class Engine\n{\n    friend struct WscCensusAccess;'))
  h=dst/'core/ReconstructedOscillator.h';s=(w/'gen'/('stock.h' if arm=='stock' else 'candidate.h')).read_text()
  assert s.count('    friend struct HorizonObserver;')==1;h.write_text(s.replace('    friend struct HorizonObserver;','    friend struct WscCensusAccess;'))
 print('Prepared isolated source, no binaries run.')
elif args.stage=='build':
 flags=['-std=gnu++20','-O3','-DNDEBUG','-fPIC','-Wall','-Wextra','-Wpedantic']
 for arm,defs in arms.items():
  dst=w/arm;src=dst/'src';print('Build',arm,flush=True)
  for m in ['Engine','FactoryPresets','Ig00156Batch','harness']:
   source=src/('core/'+m+'.cpp') if m!='harness' else w/'engine.cpp'
   ob=dst/(m+'.o');invoke(['g++',*flags,*defs,'-I'+str(src),*(['-ffp-contract=off'] if m=='Ig00156Batch' else []),'-c',source,'-o',ob])
  invoke(['g++',dst/'harness.o',dst/'Engine.o',dst/'FactoryPresets.o',dst/'Ig00156Batch.o','-o',dst/'probe'])
else:
 # 7 actual musical/lifecycle fixtures plus 44.1/96k alternate rates and
 # explicit runtime scalar filter selection. All arms, fixed order, no timing.
 scenes=[(s,48000,1) for s in range(7)]+[(0,44100,1),(1,96000,1),(0,48000,0)]
 logs=[];totals={arm:{'fast':0,'stock':0,'certificates':0,'falseWitness':0} for arm in arms}
 for n,(scene,rate,simd) in enumerate(scenes):
  control=None
  for arm in arms:
   trace=w/f'{arm}-{n}.trace'; assert not trace.exists()
   r=subprocess.run([str(w/arm/'probe'),str(scene),str(trace),str(rate),str(simd)],check=True,timeout=120,capture_output=True,text=True)
   row=json.loads(r.stdout); row.update(arm=arm,traceBytes=trace.stat().st_size,sha256=sha(trace));logs.append(row);print(json.dumps(row),flush=True)
   assert row['filterKernel']==bool(simd);assert not row['falseWitness']
   expected=0 if rate!=48000 or scene in(4,5) else None
   if arm=='witness' and expected is not None:assert row['fast']==expected
   if arm=='witness' and rate==48000 and scene in (0,1,3,6):assert row['fast']>0
   if arm=='off':assert row['fast']==0
   if control is None:control=trace
   else:
    assert control.read_bytes()==trace.read_bytes(),f'{arm} diverged: preserve failing traces'
   for k in totals[arm]:totals[arm][k]+=row[k]
  # Finish each exact byte comparison before discarding that fresh, locally
  # generated trace. Rows retain SHA/size. Never delete failed/different traces.
  for arm in arms:(w/f'{arm}-{n}.trace').unlink()
 (w/'engine-results.json').write_text(json.dumps({'rows':logs,'totals':totals},indent=2)+'\n')
