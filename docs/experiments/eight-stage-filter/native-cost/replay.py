#!/usr/bin/env python3
"""Portable complete-data replay. No native loading, subprocesses or clocks."""
from pathlib import Path
import argparse,copy,csv,gzip,hashlib,io,json,math
import schedule
from study_statistics import analyze,PLAN
ROOT=Path(__file__).resolve().parent
PRIVATE={'authorization','module_path','loaded_module_path','module_device','module_inode','module_size','pid'}
CLOCK_FIELDS=['slot','window_index','global_block','wall_start_relative_ns','thread_cpu_start_relative_ns','thread_cpu_end_relative_ns','wall_end_relative_ns']
def strict(s):
 def pairs(items):
  d={}
  for k,v in items:
   if k in d:raise ValueError('duplicate JSON key '+k)
   d[k]=v
  return d
 return json.loads(s,object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError(x)))
def read(name):return strict((ROOT/name).read_bytes())
def sha(b):return hashlib.sha256(b).hexdigest()
def require(ok,why):
 if not ok:raise ValueError(why)
def close(a,b,path='root'):
 if type(a) is float and type(b) is float:
  require(math.isfinite(a) and math.isfinite(b) and math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-10),'numeric drift '+path)
 elif isinstance(a,dict) and isinstance(b,dict):
  require(set(a)==set(b),'keys '+path)
  for k in a:close(a[k],b[k],path+'.'+k)
 elif isinstance(a,list) and isinstance(b,list):
  require(len(a)==len(b),'length '+path)
  for i,(x,y) in enumerate(zip(a,b)):close(x,y,path+'.'+str(i))
 else:require(type(a) is type(b) and a==b,'mismatch '+path)
def manifest():
 m=read('MANIFEST.json');actual={str(p.relative_to(ROOT)) for p in ROOT.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='MANIFEST.json'}
 require(actual==set(m['files']),'manifest exact file set')
 for name,pin in m['files'].items():
  path=ROOT/name;require(not Path(name).is_absolute() and '..' not in Path(name).parts and not path.is_symlink(),'safe path')
  data=path.read_bytes();require(len(data)==pin['bytes'] and sha(data)==pin['sha256'],'manifest digest '+name)
def load():
 records=[strict(x) for x in gzip.decompress((ROOT/'records.jsonl.gz').read_bytes()).splitlines()]
 meta=read('clock-shards.json');require(meta['total_rows']==49920 and len(meta['shards'])==6,'six complete clock shards')
 clocks=[];combined=[]
 for i,part in enumerate(meta['shards']):
  require(part['set']==i and part['file']==f'callback-clocks-set-{i}.csv.gz' and (part['first_slot'],part['last_slot'])==(16*i,16*i+15),'shard order')
  data=gzip.decompress((ROOT/part['file']).read_bytes());require(sha(data)==part['uncompressed_sha256'],'shard raw digest');lines=data.splitlines(keepends=True)
  combined.extend(lines if i==0 else lines[1:]);reader=csv.reader(io.StringIO(data.decode()));require(next(reader)==CLOCK_FIELDS,'clock header');count=0
  for row in reader:
   require(len(row)==7 and all(x.isascii() and x.isdigit() and str(int(x))==x for x in row),'canonical integer endpoint');v=[int(x) for x in row];require(16*i<=v[0]<16*i+16,'clock in declared set');clocks.append(v);count+=1
  require(count==part['rows']==8320,'complete set callback rows')
 require(sha(b''.join(combined))==meta['combined_uncompressed_sha256'],'lossless strict shard concatenation')
 ledger=[strict(x) for x in (ROOT/'ledger.jsonl').read_text().splitlines()]
 return records,clocks,ledger

def integrity(records,clocks,ledger,terminal,plan,provenance,expected):
 require(len(records)==96 and len(clocks)==49920,'all96 processes and49920 calls required')
 require(terminal['status']=='complete' and terminal['completed_slots']==96 and terminal['error'] is None and terminal['no_resume'] is True and terminal['analysis_performed'] is False,'complete one-shot')
 require(terminal['wall_begin_ns']==0 and 0<terminal['wall_end_ns']<=360_000_000_000 and 0<terminal['cpu_elapsed_ns']<=240_000_000_000,'terminal budgets')
 require(len(ledger)==194,'complete ledger')
 original=provenance['original_private_bindings'];ph=original['original_plan_sha256'];fh=original['original_freeze_sha256'];ah=original['original_authorization_sha256']
 require(ledger[0]=={'event':'start','action':'native-eight-average-cost-v1-96','authorization_sha256':ah,'plan_sha256':ph,'freeze_sha256':fh,'wall_ns':0},'ledger start binding')
 require(ledger[-1]=={'event':'terminal',**terminal},'ledger terminal')
 roles=plan['roles'];require(roles['stock']['sha256']!=roles['candidate']['sha256'],'distinct C image')
 require(roles['stock']['physical_aliases']==['S0','S1','S+'],'same physical stock aliases')
 rows=[];position=0;last_wall=0;child_cpu=0;receipt_pins=[];absolute=[]
 for i,(r,s) in enumerate(zip(records,plan['schedule']['slots'])):
  require(r['slot']==s and s['slot']==i and r['process_ordinal']==i+1,'chronological slot/process identity')
  require(r['physical_module_role']==s['module_role'] and r['module_sha256']==roles[s['module_role']]['sha256'] and r['host_sha256']==roles['host']['sha256'],'physical module binding')
  require(r['exit']==0 and r['error'] is None and r['stderr']=='' and r['integrity']=='PASS' and r['initial_home_empty'] is True,'retained process status')
  before,after=r['outer_wall_start_relative_ns'],r['outer_wall_end_relative_ns'];require(last_wall<=before<after<=terminal['wall_end_ns'] and after-before<=6_000_000_000,'process ordering/budget');last_wall=after
  rcpu=r['resource_user_ns']+r['resource_system_ns'];require(0<rcpu<=4_000_000_000,'process CPU budget');child_cpu+=rcpu
  require(ledger[1+2*i]=={'event':'slot-start','slot':i,'wall_ns':before},'ledger slot start')
  require(ledger[2+2*i]=={'event':'slot-end','slot':i,'receipt_sha256':r['original_receipt_sha256'],'integrity':'PASS'},'ledger raw receipt pin');receipt_pins.append(r['original_receipt_sha256'])
  n=s['measured_callbacks'];ref=expected[str(n)];o=r['observed'];require(r['immutable_artifact_set']==str(n),'gold count binding')
  require(set(o)==set(ref['schema_keys'])-PRIVATE,'public host schema')
  for k,v in ref['constant_fields'].items():require(type(o[k]) is type(v) and o[k]==v,'gold PCM/FP/count '+k)
  require(o['mode']=='measure' and o['cpu_before']==o['cpu_after']==0 and o['affinity_before']==o['affinity_after']=='0' and o['clock_reads']==4*n,'clock mode/affinity/count')
  cpu=wall=0;lc=lw=None;first=last=None
  for j in range(n):
   slot,index,block,w0,c0,c1,w1=clocks[position];position+=1
   require((slot,index,block)==(i,j,1024+j),'raw callback order')
   require(0<=c0<c1 and 0<w0<w1 and c1-c0<=w1-w0,'raw endpoint validity')
   if j==0:require(c0==0,'per-process CPU origin');first=w0
   else:require(lc<=c0 and lw<=w0,'monotonic endpoint order')
   lc,lw=c1,w1;last=w1;cpu+=c1-c0;wall+=w1-w0
  require((o['cpu_ns_sum'],o['wall_ns_sum'],o['window_wall_start_ns'],o['window_wall_end_ns'],o['enclosing_wall_ns'])==(cpu,wall,first,last,last-first),'all endpoint sums')
  require(before<=first<last<=after and cpu<=rcpu,'native interval envelope')
  rows.append({**s,'cpu_ns':cpu,'pid':i+1,'module_sha256':r['module_sha256'],'integrity_pass':True})
  absolute.append({'slot':i,'wall_ns_sum':wall,'enclosing_wall_ns':last-first,'outer_process_wall_ns':after-before,'resource_cpu_ns':rcpu,'cpu_ns_per_baseline_callback':cpu/512,'cpu_ns_per_baseline_frame':cpu/(512*64),'rtf_cpu_baseline512':cpu/(512*64/48000*1e9)})
 require(position==49920 and child_cpu<=terminal['cpu_elapsed_ns'],'all work/resource conservation')
 return rows,receipt_pins,absolute

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--self-test',action='store_true');args=ap.parse_args();manifest()
 plan,prov,terminal=read('plan.json'),read('provenance.json'),read('terminal.json');records,clocks,ledger=load();expected={str(n):read(f'expected/{n}.json') for n in [512,544]}
 require(plan['schedule']==json.loads(json.dumps(schedule.make_schedule())) and plan['schedule']==read('schedule.json'),'seed/schedule reproduction')
 require(plan['schedule_sha256']==sha((ROOT/'schedule.json').read_bytes()) and plan['protocol_sha256']==sha((ROOT/'PROTOCOL-frozen.md').read_bytes()) and plan['statistical_plan_sha256']==sha((ROOT/'statistical-plan.json').read_bytes()),'prospective plan pins')
 require(plan['statistical_plan']==read('statistical-plan.json')==PLAN,'exact frozen statistical plan')
 for name,pin in prov['frozen_sources'].items():require(sha((ROOT/name).read_bytes())==pin,'frozen original source '+name)
 rows,pins,absolute=integrity(records,clocks,ledger,terminal,plan,prov,expected)
 result=analyze(rows,plan['schedule'])
 for row in result['observations']:row['process_ordinal']=row.pop('pid')
 official=read('official-decision.json');observed_integrity=official['integrity'];expected_math={k:v for k,v in official.items() if k!='integrity'};close(result,expected_math)
 require(observed_integrity['integrity_pass'] is True and observed_integrity['terminal']==terminal and observed_integrity['process_receipt_sha256']==pins,'original integrity receipt mapping')
 close(observed_integrity['wall_and_absolute_totals'],absolute)
 require(observed_integrity['plan_sha256']==prov['original_private_bindings']['original_plan_sha256'] and observed_integrity['freeze_sha256']==prov['original_private_bindings']['original_freeze_sha256'] and observed_integrity['authorization_sha256']==prov['original_private_bindings']['original_authorization_sha256'],'original private binding mapping')
 tests=[]
 if args.self_test:
  mutations=[('missing_process',lambda r,k,l:r.pop()),('changed_process_identity',lambda r,k,l:r[1].__setitem__('process_ordinal',1)),('wrong_module',lambda r,k,l:r[0].__setitem__('module_sha256','0'*64)),('wrong_total',lambda r,k,l:r[0]['observed'].__setitem__('cpu_ns_sum',r[0]['observed']['cpu_ns_sum']+1)),('wrong_FP',lambda r,k,l:r[0]['observed']['endpoint_fp'].__setitem__(0,0)),('fake_extra_calls',lambda r,k,l:next(x for x in r if x['slot']['treatment']=='S+')['observed'].__setitem__('window_calls',512)),('missing_clock',lambda r,k,l:k.pop()),('wrong_endpoint_order',lambda r,k,l:k[0].__setitem__(5,k[0][4])),('wrong_callback_index',lambda r,k,l:k[10].__setitem__(1,9)),('wrong_ledger_pin',lambda r,k,l:l[2].__setitem__('receipt_sha256','0'*64)),('dropped_ledger_event',lambda r,k,l:l.pop())]
  for name,mutate in mutations:
   r,k,l=copy.deepcopy(records),copy.deepcopy(clocks),copy.deepcopy(ledger);mutate(r,k,l)
   try:integrity(r,k,l,terminal,plan,prov,expected)
   except (ValueError,IndexError):tests.append(name)
   else:raise ValueError('negative escaped '+name)
  try:strict('{"x":1,"x":2}')
  except ValueError:tests.append('duplicate_JSON_key')
  else:raise ValueError('duplicate JSON escaped')
 print(json.dumps({'evidence_replay':'PASS','processes':96,'blocks':24,'independent_randomized_sets':6,'callback_clock_rows':49920,'callback_clock_reads':199680,'official_admission':result['admission'],'calibration_pass':result['calibration_pass'],'failed_gates':len(result['failures']),'negative_controls_passed':tests,'measurement_or_DSP_reruns':0,'scope':'complete normalized timing evidence/statistics replay; omitted native binaries and raw identity/PCM artifacts are hash-bound receipts, not re-proved here'},indent=2))
if __name__=='__main__':main()
