#!/usr/bin/env python3
"""One new-host, correctness-gated cost study. Never patches production."""
import argparse,csv,hashlib,json,math,os,pathlib,platform,shutil,subprocess,time
P=pathlib.Path
ROOT=P(__file__).resolve().parent
OBJECTS={'src':'702bf5ee4e375c7b13da7ec95b01b56587bbf75c','tests':'64b7f72ed0139d846e8107c47c62f190cb13eefa','CMakeLists.txt':'288b80b965363547c0851815c95e6bbc17fa84c2','cmake':'22c04e90f3245ccd8a17e7640122fa949948cec3'}
IGNORE={'min_us','median_us','p95_us','p99_us','max_us','mean_rtf','median_rtf','p99_rtf','max_rtf','budget_exceedances','event_median_us','event_max_us','process_cpu_seconds_including_checks','thread_cpu_seconds_including_checks','callback_cpu_seconds','callback_cpu_rtf'}
KEY=('rate','block','mode','scenario','initial_selector','phase')
def digest(p):return hashlib.sha256(P(p).read_bytes()).hexdigest()
def save(p,x):P(p).write_text(json.dumps(x,indent=2)+'\n')
def require(ok,message):
 if not ok:raise ValueError(message)
def read_rows(d,expected_rows,clock=False):
 completed=json.loads((d/'completed.json').read_text())
 with (d/'results.csv').open() as stream:rows=list(csv.DictReader(stream))
 require(completed.get('completed') is True and completed.get('rows')==expected_rows and len(rows)==expected_rows,'completion/row count')
 if expected_rows==36:require(completed.get('cases')==15,'case count')
 keyed={tuple(r[k] for k in KEY):r for r in rows};require(len(keyed)==len(rows),'duplicate rows')
 for r in rows:
  require(r['rate']=='48000' and r['block']=='128','declared rate/block')
  require(r['nonfinite_count']=='0','nonfinite PCM')
  if clock:
   require(r['callback_cpu_status']=='available','callback clock unavailable')
   require(int(r['callback_cpu_call_count'])==int(r['callbacks']),'callback count')
   cpu,audio=float(r['callback_cpu_seconds']),float(r['audio_seconds'])
   require(math.isfinite(cpu) and cpu>0 and math.isfinite(audio) and audio>0,'invalid time')
   require(math.isclose(float(r['callback_cpu_rtf']),cpu/audio,rel_tol=2e-5,abs_tol=1e-8),'CPU denominator')
 return keyed

def exact(a,b):
 require(a.keys()==b.keys(),'case inventory mismatch')
 for key,r in a.items():
  require(r.keys()==b[key].keys(),'CSV schema mismatch')
  for field in r:
   if field not in IGNORE:require(r[field]==b[key][field],f'output field mismatch: {key} {field}')

def ratio(a,b,group):
 keys=[k for k in a if group=='all' or (a[k]['mode']==group and a[k]['phase']=='warmed_held')]
 require(bool(keys),'empty metric group')
 def score(rows):return sum(float(rows[k]['callback_cpu_seconds']) for k in keys)/sum(float(rows[k]['audio_seconds']) for k in keys)
 return score(b)/score(a)

def calibration(pairs):
 records=[]
 for a,b in pairs:
  exact(a,b)
  groups={g:ratio(a,b,g) for g in ('source_performance','all','isolation')}
  rows={str(k):float(b[k]['callback_cpu_seconds'])/float(a[k]['callback_cpu_seconds']) for k in a}
  ok=abs(groups['source_performance']-1)<=.02 and abs(groups['all']-1)<=.02 and abs(groups['isolation']-1)<=.03 and all(.9<=v<=1.1 for v in rows.values())
  records.append({'groups':groups,'rows':rows,'pass':ok})
 return len(records)==3 and all(r['pass'] for r in records),records

def assess(pairs):
 require(len(pairs)==3,'three cost pairs required')
 for a,b in pairs:exact(a,b)
 gm=lambda xs:math.exp(sum(math.log(x) for x in xs)/len(xs))
 groups={g:gm([ratio(a,b,g) for a,b in pairs]) for g in ('source_performance','all','isolation')}
 rows={str(k):gm([float(b[k]['callback_cpu_seconds'])/float(a[k]['callback_cpu_seconds']) for a,b in pairs]) for k in pairs[0][0]}
 passed=groups['source_performance']<=.97 and groups['all']<=1.02 and groups['isolation']<=1.02 and all(v<=1.05 for v in rows.values())
 return {'decision':'SCOPED_COST_PASS' if passed else 'REJECT_MATERIAL_GAIN_OR_REGRESSION_GATE','groups':groups,'rows':rows,'candidate_cost_executions':3,'no_retry':True}

def cost_phase(run):
 pairs=[]
 for n in range(3):pairs.append((run(f'cal-{n}-a','baseline'),run(f'cal-{n}-b','baseline')))
 ok,records=calibration(pairs)
 if not ok:return {'decision':'TERMINAL_UNUSABLE_CALIBRATION','calibration':records,'candidate_cost_executions':0,'no_retry':True}
 pairs=[]
 for n,order in enumerate((('baseline','candidate'),('candidate','baseline'),('baseline','candidate'))):
  values={arm:run(f'cost-{n}-{arm}',arm) for arm in order};pairs.append((values['baseline'],values['candidate']))
 return {**assess(pairs),'calibration':records}

def qualification_source(text):
 replacements=[('namespace {\n','namespace {\nstd::ofstream qualificationPcm;\n'),
 ('    void observe(const std::vector<float>& audio, int voices) {\n','    void observe(const std::vector<float>& audio, int voices) {\n        qualificationPcm.write(reinterpret_cast<const char*>(audio.data()), audio.size() * sizeof(float));\n        if (!qualificationPcm) throw std::runtime_error("qualification PCM write failed");\n'),
 ('        require(fs::create_directories(directory), "Cannot create output directory");\n','        require(fs::create_directories(directory), "Cannot create output directory");\n        qualificationPcm.open(directory / "qualification.pcm", std::ios::binary);\n        require(bool(qualificationPcm), "Cannot create qualification PCM");\n')]
 for old,new in replacements:
  require(text.count(old)==1,'qualification source anchor');text=text.replace(old,new,1)
 return text

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--repo',type=P,required=True);parser.add_argument('--out',type=P,required=True);parser.add_argument('--work',type=P,required=True);args=parser.parse_args()
 repo,out,work=args.repo.resolve(),args.out.resolve(),args.work.resolve()
 out.mkdir(parents=True,exist_ok=False);work.mkdir(parents=True,exist_ok=False)
 started=time.monotonic();deadline=started+900;cpu=min(os.sched_getaffinity(0));inputs={};ledger=[];decision={'decision':'STOP_NO_RETRY','candidate_cost_executions':0}
 def command(label,cmd):
  remaining=deadline-time.monotonic();require(remaining>0,'total deadline')
  cmd=list(map(str,cmd))
  save(out/(label+'-command.json'),cmd)
  entry={'label':label,'command':cmd,'started':True,'exit':None};ledger.append(entry);save(out/'execution-ledger.json',ledger)
  # Observe affinity in the child immediately before exec, not merely parent.
  wrap='import os,sys; c=int(sys.argv[1]);os.sched_setaffinity(0,{c});assert os.sched_getaffinity(0)=={c};print("verified_child_affinity="+str(c),flush=True);os.execvp(sys.argv[2],sys.argv[2:])'
  try:
   with (out/(label+'.log')).open('w') as log:
    p=subprocess.run(['python3','-c',wrap,str(cpu),*cmd],stdout=log,stderr=subprocess.STDOUT,timeout=remaining)
   entry['exit']=p.returncode
  except BaseException as error:
   entry['error']=repr(error);raise
  finally:
   save(out/'execution-ledger.json',ledger);save(out/(label+'-exit.json'),entry)
  require(p.returncode==0,label+' failed')
 try:
  require(platform.machine()=='x86_64' and platform.system()=='Linux','qualified platform')
  for path,expected in OBJECTS.items():require(subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD:'+path],text=True).strip()==expected,'source drift '+path)
  require(not subprocess.check_output(['git','-C',str(repo),'status','--porcelain','--',*OBJECTS],text=True).strip(),'dirty source')
  commit=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
  compiler=subprocess.check_output(['g++','--version'],text=True)
  save(out/'metadata.json',{'commit':commit,'compiler':compiler,'platform':platform.platform(),'cpuinfo':P('/proc/cpuinfo').read_text(),'affinity':sorted(os.sched_getaffinity(0)),'selected_cpu':cpu,'protocol_sha256':digest(ROOT/'PROTOCOL.md'),'runner_sha256':digest(ROOT/'run.py'),'probe_sha256':digest(ROOT/'probe.cpp'),'patch_sha256':digest(ROOT/'candidate.patch'),'runner_name':os.environ.get('RUNNER_NAME'),'image':os.environ.get('ImageVersion'),'started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())})
  for arm in ('baseline','candidate'):
   (work/arm).mkdir();shutil.copytree(repo/'src',work/arm/'src');(work/arm/'build').mkdir()
  command('apply-isolated-patch',['patch','--batch','--fuzz=0','--forward','-p1','-d',work/'candidate','-i',ROOT/'candidate.patch'])
  h=(work/'candidate/src/core/SourceFactoryTones.h').read_text();begin=h.index('inline bool evaluateShared(');end=h.index('\n} // namespace cs80::source_factory',begin)
  (work/'candidate-function.h').write_text('namespace cs80::source_factory {\n'+h[begin:end].replace('inline bool evaluateShared(','inline bool evaluateSharedCandidate(',1)+'\n}\n')
  command('compile-probe',['g++','-std=c++20','-O3','-DNDEBUG','-frounding-math','-fsignaling-nans','-I'+str(repo/'src'),'-I'+str(work),ROOT/'probe.cpp','-o',work/'probe'])
  command('component-probe',[work/'probe'])
  driver=qualification_source((repo/'src/tools/source_factory_benchmark.cpp').read_text());(work/'qualification.cpp').write_text(driver)
  for arm in ('baseline','candidate'):
   source=work/arm/'src';build=work/arm/'build'
   for unit in ('Engine','FactoryPresets','Ig00156Batch'):
    extra=['-ffp-contract=off'] if unit=='Ig00156Batch' else []
    command('compile-'+arm+'-'+unit,['g++','-std=c++20','-O3','-DNDEBUG','-I'+str(source),*extra,'-c',source/'core'/(unit+'.cpp'),'-o',build/(unit+'.o')])
   for kind,main in [('benchmark',source/'tools/source_factory_benchmark.cpp'),('qualification',work/'qualification.cpp')]:
    command('link-'+arm+'-'+kind,['g++','-std=c++20','-O3','-DNDEBUG','-I'+str(source),'-I'+str(source/'tools'),main,*[build/(u+'.o') for u in ('Engine','FactoryPresets','Ig00156Batch')],'-o',build/kind])
    inputs[arm+'-'+kind]=digest(build/kind)
  save(out/'binary-hashes.json',inputs)
  for suite,count in [('baseline',19),('performance',36)]:
   results={}
   for arm in ('baseline','candidate'):
    d=out/('qualification-'+suite+'-'+arm)
    command('qualification-'+suite+'-'+arm,[work/arm/'build/qualification','--out',d,'--source-commit',commit,'--suite',suite,'--quick','--callbacks','16'])
    results[arm]=read_rows(d,count)
   exact(results['baseline'],results['candidate'])
   a=(out/('qualification-'+suite+'-baseline')/'qualification.pcm').read_bytes();b=(out/('qualification-'+suite+'-candidate')/'qualification.pcm').read_bytes();require(a==b and len(a)==({'baseline':3916800,'performance':11563008}[suite]),'PCM mismatch or size')
  qualified_keys=results['baseline'].keys()
  reference=None
  def run(name,arm):
   nonlocal reference
   d=out/name
   command(name,[work/arm/'build/benchmark','--out',d,'--source-commit',commit,'--suite','performance','--quick','--callbacks','512','--callback-cpu'])
   rows=read_rows(d,36,True)
   require(rows.keys()==qualified_keys,'qualified case roster drift')
   if reference is None:reference=rows
   else:exact(reference,rows)
   return rows
  decision=cost_phase(run)
  require(all(digest(work/arm/'build'/kind)==inputs[arm+'-'+kind] for arm in ('baseline','candidate') for kind in ('benchmark','qualification')),'binary drift')
  require(time.monotonic()<=deadline,'final deadline')
 except BaseException as error:
  decision={'decision':'STOP_NO_RETRY','error':repr(error),'no_retry':True}
 finally:
  candidate=[x for x in ledger if x['label'].startswith('cost-') and x['label'].endswith('-candidate')]
  decision['candidate_cost_started']=len(candidate)
  decision['candidate_cost_completed']=sum(x['exit']==0 for x in candidate)
  save(out/'decision.json',decision)
  print(json.dumps(decision),flush=True)
 return 0 if decision['decision'] in ('SCOPED_COST_PASS','TERMINAL_UNUSABLE_CALIBRATION','REJECT_MATERIAL_GAIN_OR_REGRESSION_GATE') else 1
if __name__=='__main__':raise SystemExit(main())
