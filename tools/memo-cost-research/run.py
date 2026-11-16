"""Prepared runner. Exact reviewed binding is mandatory before all opt-in work."""
import argparse,hashlib,json,os,pathlib,shlex,subprocess,sys,time,traceback,gzip,re,shutil
from metrics import ORDERS,inventory,read,check,calibration,analyze
HERE=pathlib.Path(__file__).resolve().parent
BASE='5836041138bc1708c3fd2d4240cf63a3e1def4b3'
CANDIDATE='46812652364337aec1c8d51443427d1b63df5351'
FORBIDDEN_ENV=('LD_PRELOAD','LD_AUDIT','LD_LIBRARY_PATH','CPATH','CPLUS_INCLUDE_PATH','C_INCLUDE_PATH','LIBRARY_PATH','GCC_EXEC_PREFIX','COMPILER_PATH','CXXFLAGS','CFLAGS','LDFLAGS','GLIBC_TUNABLES','LD_HWCAP_MASK')
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def fingerprint(result,compiler_sha256=None):
 assert result['memo_enabled'] is False and result['timing_executed'] is False and result['sample_calls']==2097168
 return {'libraries':{k:{n:result['libraries'][k][n] for n in ('sha256','build_id')} for k in ('libm','libc','loader')},'implementation_offset':result['binding']['implementation_offset'],'symbol_version':re.search(r'powf@(GLIBC_[0-9.]+)',result['binding']['relocation']).group(1),'compiler_receipt_sha256':compiler_sha256,'proof_source_sha256':result['source_sha256'],'proof_executable_sha256':result['proof_executable_sha256']}
def approve(entries,observed,tool_hashes):
 assert entries,'EMPTY ALLOWLIST: no memoON build/run or timing permitted'
 matched=[x for x in entries if x.get('candidate')==CANDIDATE and x.get('reviewed') is True and x.get('review_receipt_sha256') and x.get('inspection_tool_hashes')==tool_hashes and x.get('fingerprint')==observed]
 assert len(matched)==1,'unknown/drifted binding or inspection tool; stop unqualified'
 return matched[0]
def validate_patch_paths(src,allowed):
 tracked=subprocess.check_output(['git','diff','HEAD','--name-only','-z'],cwd=src).decode().split('\0')
 untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard','-z'],cwd=src).decode().split('\0')
 paths=set(filter(None,tracked+untracked))
 assert paths==set(allowed),('unexpected patch inventory',sorted(paths),sorted(allowed))
 return sorted(paths)
class Runner:
 def __init__(self,args):
  assert json.loads((HERE/'request.json').read_text())=={'request_id': 'memo-three-arm-4681265-02', 'baseline': '5836041138bc1708c3fd2d4240cf63a3e1def4b3', 'candidate': '46812652364337aec1c8d51443427d1b63df5351', 'timed_runs': 52, 'timeout_seconds': 5400},'request drift'
  self.args=args;self.repo=args.repo.resolve();self.out=args.out.resolve();self.work=args.work.resolve();self.out.mkdir(exist_ok=False);self.work.mkdir(exist_ok=False)
  self.cpu=min(os.sched_getaffinity(0));self.tooling_hashes={p.name:sha(p) for p in HERE.iterdir() if p.is_file() and p.name!='SOURCE-HASHES.json'};self.deadline=time.monotonic()+90*60;self.binding_approved=False;self.references={}
 def save(self,name,value):(self.out/name).write_text(json.dumps(value,indent=2)+'\n')
 def command(self,cmd,name,cwd=None,pin=False):
  cmd=list(map(str,cmd));cmd=(['taskset','-c',str(self.cpu)]+cmd) if pin else cmd;self.save(name+'-command.json',cmd)
  remaining=self.deadline-time.monotonic();assert remaining>0,'fixed90-minute budget exhausted'
  with (self.out/(name+'.log')).open('w') as f:p=subprocess.run(cmd,cwd=cwd,stdout=f,stderr=subprocess.STDOUT,timeout=min(900,remaining))
  (self.out/(name+'.exit')).write_text(str(p.returncode)+'\n');assert p.returncode==0,name
 def qualify_binding(self):
  assert os.environ.get('GITHUB_RUN_ATTEMPT','1')=='1','no automatic retries'
  for key in FORBIDDEN_ENV:assert not os.environ.get(key),'unexpected '+key
  entries=json.loads((HERE/'binding-allowlist.json').read_text());assert entries,'EMPTY ALLOWLIST: inspection approval required before any build or timing'
  tool=self.repo/'tools/memo-libm-inspection';names=('inspect.py','Proof.cpp','request.json');hashes={n:sha(tool/n) for n in names}
  assert any(x.get('reviewed') is True and x.get('candidate')==CANDIDATE and x.get('inspection_tool_hashes')==hashes for x in entries),'inspection source unapproved'
  self.command([sys.executable,tool/'inspect.py','--out',self.out/'binding-inspection'],'binding-inspection',pin=True)
  result=json.loads((self.out/'binding-inspection/RESULT.json').read_text());observed=fingerprint(result,sha(self.out/'binding-inspection/compiler.txt'));entry=approve(entries,observed,hashes)
  self.approved_binding=observed
  self.save('binding-approved.json',{'allowlist_sha256':sha(HERE/'binding-allowlist.json'),'matched_entry':entry,'observed':observed});self.binding_approved=True
 def prepare(self):
  assert self.binding_approved,'no memoON until fresh host qualified'
  assert sha(HERE/'INDEPENDENT-BINDING-AUDIT.md')==json.loads((HERE/'binding-allowlist.json').read_text())[0]['review_receipt_sha256'],'audit receipt drift'
  source=json.loads((HERE/'source.json').read_text());assert source['baseline']==BASE and source['candidate']==CANDIDATE and sha(HERE/'candidate.patch')==source['patch_sha256']
  self.save('environment.json',{'uname':list(os.uname()),'allowed_affinity':sorted(os.sched_getaffinity(0)),'selected_cpu':self.cpu,'loadavg':os.getloadavg(),'clock':vars(time.get_clock_info('thread_time')),'timeout_seconds':5400,'timed_runs_if_all_gates_pass':52})
  self.command(['g++','--version'],'compiler')
  self.executables={};self.hashes={};self.sources={};self.builds={}
  for arm in 'ABC':
   src=self.work/arm;b=self.work/(arm+'-build');self.sources[arm]=src;self.builds[arm]=b
   self.command(['git','worktree','add','--detach',src,BASE],arm+'-source',self.repo)
   if arm!='A':
    self.command(['git','apply','--check',HERE/'candidate.patch'],arm+'-patch-check',src);self.command(['git','apply',HERE/'candidate.patch'],arm+'-patch',src)
    validate_patch_paths(src,source['allowed_paths'])
   from binding import instrument
   benchmark_source=src/'src/tools/source_factory_benchmark.cpp'
   benchmark_source.write_text(instrument(benchmark_source.read_text()))
   option=[] if arm=='A' else ['-DCS80_EXPERIMENTAL_PITCH_MEMO_SSE_LIBM='+('ON' if arm=='C' else 'OFF')]
   self.command(['cmake','-S',src,'-B',b,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_COMPILER=g++','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCS80_BUILD_PLUGIN=OFF','-DCS80_BUILD_TESTS=ON','-DCS80_ENABLE_AVX2_FILTER_BATCH=ON','-DCS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=OFF',*option],arm+'-configure')
   targets=['cs80_filter_batch_engine_tests','cs80_filter_batch_scheduling_tests','cs80_ig00156_batch_tests','cs80_source_factory_benchmark']+([] if arm=='A' else ['cs80_pitch_memo_tests','cs80_pitch_memo_lifecycle_tests'])
   self.command(['cmake','--build',b,'--parallel','2','--target',*targets],arm+'-build')
   self.command([b/'cs80_filter_batch_engine_tests','--write-corpus',self.out/(arm+'-engine.bin')],arm+'-engine',pin=True)
   regex='^(cs80_filter_batch_scheduling_tests|cs80_ig00156_batch_tests'+('' if arm=='A' else '|cs80_pitch_memo_tests|cs80_pitch_memo_lifecycle_tests')+')$'
   self.command(['ctest','--test-dir',b,'--output-on-failure','-R',regex],arm+'-tests',pin=True)
   for name,probe in [('history',src/'src/tools/source_factory_compatibility_probe.cpp'),('dispatch',HERE/'DispatchProbe.cpp')]:
    exe=self.work/(arm+'-'+name);defs=['-DCS80_EXPERIMENTAL_PITCH_MEMO_SSE_LIBM=1'] if arm=='C' else []
    self.command(['g++','-O3','-DNDEBUG','-std=c++20',*defs,'-I'+str(src/'src'),probe,b/'libcs80_core.a','-o',exe],arm+'-'+name+'-build');self.command([exe,self.out/(arm+'-'+name+'.bin')],arm+'-'+name,pin=True)
   exe=b/'cs80_source_factory_benchmark';self.executables[arm]=exe;self.hashes[arm]=sha(exe)
  for name in ('engine','history','dispatch'):
   baseline=(self.out/('A-'+name+'.bin')).read_bytes()
   for arm in 'BC':assert baseline==(self.out/(arm+'-'+name+'.bin')).read_bytes(),(arm,name,'exact corpus mismatch')
  self.save('corpus-hashes.json',{arm:{name:sha(self.out/(arm+'-'+name+'.bin')) for name in ('engine','history','dispatch')} for arm in 'ABC'})
  assert sha(self.out/'A-history.bin')=='08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195'
  assert len({sha(src/'src/tools/source_factory_benchmark.cpp') for src in self.sources.values()})==1
  self.save('common-driver-hashes.json',{arm:sha(src/'src/tools/source_factory_benchmark.cpp') for arm,src in self.sources.items()})
  self.match_commands();self.save('benchmark-hashes.json',self.hashes)
  retained=self.out/'timed-benchmarks';retained.mkdir()
  for arm,exe in self.executables.items():
   dest=retained/('benchmark-'+arm);shutil.copyfile(exe,dest);assert sha(dest)==self.hashes[arm]
  self.save('retained-benchmarks.json',{arm:{'path':'timed-benchmarks/benchmark-'+arm,'sha256':self.hashes[arm],'bytes':(retained/('benchmark-'+arm)).stat().st_size} for arm in 'ABC'})
  self.codegen()
  from reach import qualify
  qualify(self)
 def check_tooling(self):
  assert self.tooling_hashes=={p.name:sha(p) for p in HERE.iterdir() if p.is_file() and p.name!='SOURCE-HASHES.json'},'tooling drift after freeze'
 def codegen(self):
  records={}
  for arm in 'ABC':
   exe=self.executables[arm];self.command(['readelf','-h',exe],arm+'-elf-header')
   assert re.search(r'Type:\s+DYN\b',(self.out/(arm+'-elf-header.log')).read_text()),'runtime binding receipt requires PIE'
   self.command(['readelf','-Wr',exe],arm+'-dynamic-relocations');self.command(['ldd',exe],arm+'-loader-dependencies')
   rel=(self.out/(arm+'-dynamic-relocations.log')).read_text();assert any('powf@' in line and ('JUMP_SLOT' in line or 'GLOB_DAT' in line) for line in rel.splitlines())
   deps=(self.out/(arm+'-loader-dependencies.log')).read_text();paths=re.findall(r'libm\.so[^ ]*\s+=>\s+(\S+)',deps);assert len(paths)==1
   assert sha(pathlib.Path(paths[0]).resolve())==self.approved_binding['libraries']['libm']['sha256'],'benchmark loader resolved different libm'
   self.command(['objdump','-d','-Mintel',exe],arm+'-full-codegen')
   raw=self.out/(arm+'-full-codegen.log');data=raw.read_bytes();packed=raw.with_suffix('.log.gz');packed.write_bytes(gzip.compress(data,mtime=0));raw.unlink()
   symbols=subprocess.check_output(['nm','--defined-only',str(exe)],text=True)
   from binding import codegen
   linkage=codegen(data.decode(),rel,arm)
   record={'linkage':linkage,'full_assembly_sha256':hashlib.sha256(data).hexdigest(),'compressed_assembly_sha256':sha(packed),'libm_sha256':sha(pathlib.Path(paths[0]).resolve())}
   if arm=='C':
    names=[line.split()[-1] for line in symbols.splitlines() if 'PitchPowMemo2hz' in line];assert len(names)==1
    self.command(['objdump','-d','-Mintel','--disassemble='+names[0],exe],arm+'-memo-pow-callsite')
    helper=(self.out/(arm+'-memo-pow-callsite.log')).read_text();assert 'powf@plt' in helper and 'exp2f' not in helper,'unknown memo libm call lowering'
    record['helper_symbol']=names[0]
   records[arm]=record
  self.codegen_records=records
  self.save('actual-codegen-bindings.json',records)
 def match_commands(self):
  results={}
  for file,target in [('src/core/Engine.cpp','cs80_core'),('src/core/FactoryPresets.cpp','cs80_core'),('src/core/Ig00156Batch.cpp','cs80_core'),('src/tools/source_factory_benchmark.cpp','cs80_source_factory_benchmark')]:
   commands=[]
   for arm in 'ABC':
    src=self.sources[arm];b=self.builds[arm];entries=json.loads((b/'compile_commands.json').read_text());selected=[e for e in entries if e['file']==str(src/file) and f'CMakeFiles/{target}.dir/' in e['command']];assert len(selected)==1
    cmd=shlex.split(selected[0]['command']);macro='-DCS80_EXPERIMENTAL_PITCH_MEMO_SSE_LIBM=1';assert cmd.count(macro)==(1 if arm=='C' else 0)
    cmd=[x for x in cmd if x!=macro];commands.append([x.replace(str(b),'<BUILD>').replace(str(src),'<SOURCE>') for x in cmd])
   assert commands[0]==commands[1]==commands[2],file;results[file]=commands
  self.save('matched-commands.json',results)
 def run_one(self,stage,index,role,arm,low):
  self.check_tooling()
  assert self.binding_approved;exe=self.executables[arm];assert sha(exe)==self.hashes[arm]
  name=f'{stage}-{index}-{role}';dest=self.out/name
  self.command([exe,'--out',dest,'--source-commit',BASE if arm=='A' else CANDIDATE,'--suite','baseline' if low else 'performance','--rate','48000','--block','128','--callbacks','512' if low else '256','--quick','--callback-cpu'],name,pin=True)
  from binding import verify
  receipt=verify((self.out/(name+'.log')).read_text(),(self.out/(arm+'-dynamic-relocations.log')).read_text(),self.approved_binding)
  assert sha(pathlib.Path(receipt['library_path']).resolve())==self.approved_binding['libraries']['libm']['sha256']
  assert receipt['relative_got']==self.codegen_records[arm]['linkage']['relative_got'],'runtime GOT differs from Engine call target'
  self.save(name+'-binding.json',receipt)
  rows=read(dest,low)
  if low not in self.references:self.references[low]=rows
  else:check(self.references[low],rows)
 def calibrate(self,low):
  suite='lowload' if low else 'active'
  for i in range(1,5):
   order=('left','right') if i%2 else ('right','left')
   for role in order:self.run_one('calibration-'+suite,i,role,'A',low)
  result=calibration(self.out,low);self.save('calibration-'+suite+'.json',result);return result
 def triplets(self,low,cal):
  suite='lowload' if low else 'active'
  for i,order in enumerate(ORDERS,1):
   for arm in order:self.run_one(suite,i,arm,arm,low)
   self.save(suite+'-progress.json',{'triplets_complete':i,'order':order})
  result=analyze(self.out,low,cal);self.save(suite+'-analysis.json',result);return result
 def execute(self):
  self.qualify_binding();self.save('tooling-hashes.json',self.tooling_hashes);self.prepare()
  self.check_tooling()
  a=self.calibrate(False)
  if not a['usable']:return self.save('DECISION.json',{'candidate_timed':False,'reason':'active calibration unusable'})
  b=self.calibrate(True)
  if not b['usable']:return self.save('DECISION.json',{'candidate_timed':False,'reason':'lowload calibration unusable'})
  active=self.triplets(False,a);low=self.triplets(True,b)
  self.save('DECISION.json',{'candidate_timed':True,'material_signal':active['material_signal'] and not low['hard_losses'] and not low['cautions'],'promotion_authorized':False,'requires_independent_audit':True})
def main():
 p=argparse.ArgumentParser();p.add_argument('--repo',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);p.add_argument('--work',type=pathlib.Path,required=True);args=p.parse_args();r=Runner(args)
 try:r.execute()
 except BaseException:(r.out/'FAILED.txt').write_text(traceback.format_exc());(r.out/'run.exit').write_text('1\n');raise
 else:(r.out/'run.exit').write_text('0\n')
if __name__=='__main__':main()
