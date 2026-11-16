"""One frozen read-only research experiment. No network, installs or promotion."""
import argparse,hashlib,json,os,pathlib,shlex,subprocess,sys,time,traceback
from metrics import read,check,analyze
HERE=pathlib.Path(__file__).resolve().parent
BASE='5836041138bc1708c3fd2d4240cf63a3e1def4b3'
CANDIDATE='18e1c1da25842b7bbacfc871067835db735d1230'
PATCH='7d9c73919d31330f35c9aad10b21a86d56b86526044a1f75b6398c694a1fe1ec'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def expected(low):
 result=set()
 for sel in ['0','1','10']:
  for mode in ['legacy','isolation']:
   for phase in ['warmed_held','release_tail']:result.add((mode,'paired_tones',sel,phase))
  if not low:
   for scenario in ['paired_tones','mpe_chorus_reverb','mpe_phaser_delay_drive']:
    for phase in ['warmed_held','release_tail']+([] if scenario=='paired_tones' else ['sustain_pedal_hold']):result.add(('source_performance',scenario,sel,phase))
 if low:
  for mode in ['legacy','isolation']:
   for phase in ['warmed_held','release_tail']:result.add((mode,'moving_tabs_pressure_wah','0',phase))
   result.add((mode,'idle_quiet','0','warmed_held'))
  result.add(('legacy','idle_noise','0','warmed_held'))
 return result
class Run:
 def __init__(self,args):
  self.a=args;self.repo=args.repo.resolve();self.out=args.out.resolve();self.work=args.work.resolve()
  self.out.mkdir(exist_ok=False);self.work.mkdir(exist_ok=False);self.cpu=min(os.sched_getaffinity(0));self.n=0
 def record(self,name,value):(self.out/name).write_text(json.dumps(value,indent=2)+'\n')
 def cmd(self,args,name,cwd=None,pin=False):
  args=[str(x) for x in args];args=(['taskset','-c',str(self.cpu)]+args) if pin else args
  self.record(name+'-command.json',args)
  with (self.out/(name+'.log')).open('w') as f:
   p=subprocess.run(args,cwd=cwd,stdout=f,stderr=subprocess.STDOUT,timeout=900)
  (self.out/(name+'.exit')).write_text(str(p.returncode)+'\n');assert p.returncode==0,name
 def prepare(self):
  assert os.environ.get('GITHUB_RUN_ATTEMPT','1')=='1','reruns prohibited'
  req=json.loads((HERE/'request.json').read_text());assert req==dict(request_id='pitch-18e1c1d-research-01',baseline=BASE,candidate=CANDIDATE,patch_sha256=PATCH)
  assert sha(HERE/'candidate.patch')==PATCH
  self.record('environment.json',{'affinity':sorted(os.sched_getaffinity(0)),'selected_cpu':self.cpu,'uname':list(os.uname()),'loadavg':os.getloadavg(),'clocks':{k:vars(time.get_clock_info(k)) for k in ['monotonic','thread_time']},'run_id':os.environ.get('GITHUB_RUN_ID'),'attempt':os.environ.get('GITHUB_RUN_ATTEMPT'),'workflow_sha':os.environ.get('GITHUB_SHA')})
  (self.out/'cpuinfo.txt').write_text(pathlib.Path('/proc/cpuinfo').read_text())
  for command,name in [(['g++','--version'],'compiler'),(['cmake','--version'],'cmake'),(['lscpu'],'lscpu')]:self.cmd(command,name)
  self.cmd(['git','rev-parse','HEAD'],'tooling-head',self.repo)
  self.record('tooling-hashes.json',{str(p.relative_to(HERE)):sha(p) for p in HERE.rglob('*') if p.is_file() and '__pycache__' not in p.parts})
  # Local immutable object only: no checkout credentials or network needed.
  assert subprocess.check_output(['git','-C',str(self.repo),'rev-parse',BASE],text=True).strip()==BASE
  for arm in ['baseline','candidate']:
   src=self.work/arm
   self.cmd(['git','worktree','add','--detach',src,BASE],arm+'-source',self.repo)
   if arm=='candidate':
    self.cmd(['git','apply','--check',HERE/'candidate.patch'],arm+'-patch-check',src)
    self.cmd(['git','apply',HERE/'candidate.patch'],arm+'-patch',src)
    paths=subprocess.check_output(['git','diff','--name-only'],cwd=src,text=True).splitlines();assert paths==['src/core/Engine.cpp']
   b=self.work/(arm+'-build')
   self.cmd(['cmake','-S',src,'-B',b,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_COMPILER=g++','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCS80_BUILD_PLUGIN=OFF','-DCS80_BUILD_TESTS=ON','-DCS80_ENABLE_AVX2_FILTER_BATCH=ON','-DCS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=OFF'],arm+'-configure')
   self.cmd(['cmake','--build',b,'--parallel','2','--target','cs80_filter_batch_engine_tests','cs80_filter_batch_scheduling_tests','cs80_ig00156_batch_tests','cs80_source_factory_benchmark'],arm+'-build')
   self.cmd([b/'cs80_filter_batch_engine_tests','--write-corpus',self.out/(arm+'-engine.bin')],arm+'-engine',pin=True)
   self.cmd(['ctest','--test-dir',b,'--output-on-failure','-R','^(cs80_filter_batch_scheduling_tests|cs80_ig00156_batch_tests)$'],arm+'-tests',pin=True)
   for name,probe in [('history',src/'src/tools/source_factory_compatibility_probe.cpp'),('dispatch',HERE/'DispatchProbe.cpp')]:
    exe=self.work/(arm+'-'+name)
    self.cmd(['g++','-O3','-DNDEBUG','-std=c++20','-I'+str(src/'src'),probe,b/'libcs80_core.a','-o',exe],arm+'-'+name+'-build')
    self.cmd([exe,self.out/(arm+'-'+name+'.bin')],arm+'-'+name,pin=True)
  for name in ['engine','history','dispatch']:assert (self.out/('baseline-'+name+'.bin')).read_bytes()==(self.out/('candidate-'+name+'.bin')).read_bytes(),name+' mismatch'
  assert sha(self.out/'baseline-history.bin')=='08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195'
  matched={}
  for file,target in [('src/core/Engine.cpp','cs80_core'),('src/core/FactoryPresets.cpp','cs80_core'),('src/core/Ig00156Batch.cpp','cs80_core'),('src/tools/source_factory_benchmark.cpp','cs80_source_factory_benchmark')]:
   normalized=[]
   for arm in ['baseline','candidate']:
    b=self.work/(arm+'-build');src=self.work/arm
    entries=json.loads((b/'compile_commands.json').read_text());x=[e for e in entries if e['file']==str(src/file) and f'CMakeFiles/{target}.dir/' in e['command']];assert len(x)==1
    normalized.append(x[0]['command'].replace(str(b),'<BUILD>').replace(str(src),'<SRC>'))
   assert normalized[0]==normalized[1],file;matched[file]=normalized
  self.record('matched-commands.json',matched)
  self.reach()
  self.hashes={a:sha(self.work/(a+'-build/cs80_source_factory_benchmark')) for a in ['baseline','candidate']};self.record('benchmark-hashes.json',self.hashes)
 def reach(self):
  # Separate instrumented copy, never a timed artifact.
  src=self.work/'candidate';b=self.work/'candidate-build';s=(src/'src/core/Engine.cpp').read_text();needle='return tuningRef * std::exp2(exponent);';assert s.count(needle)==1
  s=s.replace('namespace cs80\n{','extern "C" { unsigned long long pitchHits[4] = {}; }\nnamespace cs80\n{',1).replace(needle,'{ const auto m=_mm_getcsr(); ++pitchHits[((m&0x40)?1:0)|((m&0x8000)?2:0)]; '+needle+' }')
  p=self.work/'reach';p.mkdir();(p/'Engine.cpp').write_text(s)
  entries=json.loads((b/'compile_commands.json').read_text());x=[e for e in entries if e['file']==str(src/'src/core/Engine.cpp') and 'CMakeFiles/cs80_core.dir/' in e['command']];assert len(x)==1
  cmd=shlex.split(x[0]['command']);cmd[cmd.index('-o')+1]=str(p/'Engine.cpp.o');cmd[cmd.index('-c')+1]=str(p/'Engine.cpp');cmd+=['-I'+str(src/'src/core')]
  if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(p/'Engine.d')
  self.cmd(cmd,'reach-core',cwd=b)
  import shutil
  shutil.copyfile(b/'libcs80_core.a',p/'lib.a');self.cmd(['ar','r',p/'lib.a',p/'Engine.cpp.o'],'reach-archive')
  assert subprocess.check_output(['ar','t',str(p/'lib.a')],text=True).splitlines().count('Engine.cpp.o')==1
  code='#define main originalMain\n#include "'+str(src/'tests/FilterBatchEngineTests.cpp')+'"\n#undef main\n#include <xmmintrin.h>\nextern "C" unsigned long long pitchHits[4];\nint main(int argc,char**argv){_mm_setcsr(0x9fc0);int r=originalMain(argc,argv);std::printf("PITCH_HITS %llu %llu %llu %llu\\n",pitchHits[0],pitchHits[1],pitchHits[2],pitchHits[3]);return r?r:(pitchHits[3]?0:1);}\n'
  (p/'Probe.cpp').write_text(code)
  self.cmd(['g++','-O3','-DNDEBUG','-std=c++20','-I'+str(src/'src'),p/'Probe.cpp',p/'lib.a','-o',p/'probe'],'reach-build')
  self.cmd([p/'probe'],'reach-run',pin=True)
 def suite(self,stage,count,low=False,cal=False):
  inventory=(11,19) if low else (15,36)
  ref=self.references.get(low)
  for i in range(1,count+1):
   order=['baseline','candidate'] if i%2 else ['candidate','baseline']
   for role in order:
    arm='baseline' if cal else role;exe=self.work/(arm+'-build/cs80_source_factory_benchmark');assert sha(exe)==self.hashes[arm]
    name=f'{stage}-{i}-{role}';out=self.out/name
    self.cmd([exe,'--out',out,'--source-commit',BASE if arm=='baseline' else CANDIDATE,'--suite','baseline' if low else 'performance','--rate','48000','--block','128','--callbacks','512' if low else '256','--quick','--callback-cpu'],name,pin=True)
    rows=read(out,inventory);assert set(rows)==expected(low)
    if ref is None:ref=rows;self.references[low]=rows
    else:check(ref,rows)
   self.record(stage+'-progress.json',{'pairs_complete':i,'order':order,'cross_run_exact':True})
 def execute(self):
  self.prepare();self.references={}
  self.suite('calibration',4,cal=True);active=analyze(self.out,'calibration',4)
  if not active['usable']:return self.record('DECISION.json',{'status':'calibration unusable','candidate_timed':False})
  self.suite('calibration-lowload',4,low=True,cal=True);low=analyze(self.out,'calibration-lowload',4)
  if not low['usable']:return self.record('DECISION.json',{'status':'lowload calibration unusable','candidate_timed':False})
  self.suite('performance',6);a=analyze(self.out,'performance',6,active)
  self.suite('lowload',4,low=True);b=analyze(self.out,'lowload',4,low)
  self.record('DECISION.json',{'status':'independent review required, no automatic promotion','candidate_timed':True,'material_signal':a['material_signal'] and not b['hard_losses'] and not b['cautions']})
def main():
 p=argparse.ArgumentParser();p.add_argument('--dry-run',action='store_true');p.add_argument('--repo',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);p.add_argument('--work',type=pathlib.Path,required=True);a=p.parse_args();r=Run(a)
 try:
  if a.dry_run:
   assert json.loads((HERE/'request.json').read_text())==dict(request_id='pitch-18e1c1d-research-01',baseline=BASE,candidate=CANDIDATE,patch_sha256=PATCH)
   assert sha(HERE/'candidate.patch')==PATCH;assert len(expected(False))==36 and len(expected(True))==19
   r.record('PLAN.json',{'baseline':BASE,'candidate':CANDIDATE,'patch_sha256':PATCH,'stages':['correctness','4 active A/A pairs','4 lowload A/A pairs','6 active B/C pairs','4 lowload B/C pairs'],'candidate_requires_both_calibrations':True,'timing_executed':False})
  else:r.execute()
 except BaseException:
  (r.out/'FAILED.txt').write_text(traceback.format_exc());(r.out/'run.exit').write_text('1\n');raise
 else:(r.out/'run.exit').write_text('0\n')
if __name__=='__main__':main()
