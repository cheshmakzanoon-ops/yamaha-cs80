#!/usr/bin/env python3
"""Offline integrity and exact frozen arithmetic replay. Never builds, loads DSP or reads a clock."""
from pathlib import Path,PurePosixPath
import argparse,ast,copy,hashlib,json,math,sys
import frozen_math as calc
ROOT=Path(__file__).resolve().parent

def need(ok,message):
 if not ok:raise ValueError(message)
def digest(data):return hashlib.sha256(data).hexdigest()
def pairs(items):
 d={}
 for k,v in items:
  need(k not in d,'duplicate JSON key '+k);d[k]=v
 return d
def loads(text):return json.loads(text,object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError('nonfinite JSON')))
def read(name):return loads((ROOT/name).read_text())
def jsonl(name):
 lines=(ROOT/name).read_text().splitlines();need(all(x.strip() for x in lines),'blank JSONL record');return [loads(x) for x in lines]
def positive_int(x):return type(x) is int and x>0

def verify_manifest():
 m=read('MANIFEST.json');need(m['schema']==1,'manifest schema');expected=set(m['files'])
 actual={str(p.relative_to(ROOT)) for p in ROOT.rglob('*') if p.is_file() and p.name!='MANIFEST.json' and '__pycache__' not in p.parts and p.suffix!='.pyc'}
 need(actual==expected,'missing or additional bundle file')
 for name,v in m['files'].items():
  pp=PurePosixPath(name);need(not pp.is_absolute() and '..' not in pp.parts and str(pp)==name,'nonlocal manifest path');p=ROOT/name
  need(not p.is_symlink() and p.resolve().is_relative_to(ROOT),'symlink/path escape');data=p.read_bytes();need(len(data)==v['bytes'] and digest(data)==v['sha256'],'manifest digest '+name)
 return m

def fixed_plan():
 roles=[('sham',0),('compare',0),('positive',0),('compare',1),('sham',1),('compare',2),('positive',1),('compare',3),('sham',2),('compare',4)];slots=[];batches=[]
 for rnd,(kind,rep) in enumerate(roles):
  for scene_index in ([0,1] if rnd%2==0 else [1,0]):
   scene=['default','dense'][scene_index];order='ABBA' if (scene_index+rep)%2==0 else 'BAAB';ids=[];bid=len(batches);extra_label=('A' if rep==0 else 'B') if kind=='positive' else None
   for label in order:
    i=len(slots);ids.append(i);slots.append({'id':i,'batch':bid,'scene':scene,'label':label,'extra':label==extra_label,'image':'images/image-53' if kind=='compare' and label=='B' else 'images/image-17'})
   batches.append({'id':bid,'round':rnd,'kind':kind,'replicate':rep,'scene':scene,'order':order,'extra_label':extra_label,'slots':ids})
 return {'schema':1,'iterations':131072,'slots':slots,'batches':batches,'process_cpu_limit_s':3,'process_wall_limit_s':5,'whole_cpu_limit_s':120,'whole_wall_limit_s':180,'method_scope':'settled hot repeated mirror only'}

RECORD_KEYS={'slot','image_sha256','process_instance','process_cpu_ns','process_wall_begin_ns','process_wall_end_ns','exit','error','integrity','stderr','original_receipt_sha256','stdout_sha256','observed'}
def validate_records(records,plan,pins,ledger,terminal):
 need(plan==fixed_plan(),'fixed ABBA/control schedule differs');need(len(records)==80,'not all80 records')
 need(len(ledger)==162,'ledger retry/missing event');need(set(ledger[0])=={'event','wall_ns'} and ledger[0]['event']=='start' and positive_int(ledger[0]['wall_ns']),'ledger start')
 need(terminal['status']=='complete' and terminal['completed_slots']==80 and terminal['error'] is None and terminal['no_resume'] is True,'incomplete/failed terminal')
 need(0<terminal['cpu_elapsed_ns']<=120_000_000_000 and 0<terminal['wall_elapsed_ns']<=180_000_000_000,'whole trial budget')
 need(ledger[-1]=={'event':'terminal',**terminal},'terminal/ledger disagreement')
 previous_end=ledger[0]['wall_ns'];cpu_sum=0;instances=set();observations=[]
 for i,(record,slot) in enumerate(zip(records,plan['slots'])):
  need(set(record)==RECORD_KEYS,'record schema');need(record['slot']==slot,'record order or role mismatch');need(record['image_sha256']==pins['images'][slot['image']]['sha256'],'wrong physical image')
  need(record['process_instance']==i+1 and record['process_instance'] not in instances,'fresh normalized process identity');instances.add(record['process_instance'])
  need(record['exit']==0 and record['error'] is None and record['integrity']=='PASS' and record['stderr']=='','process did not complete cleanly')
  begin,end,cpu=record['process_wall_begin_ns'],record['process_wall_end_ns'],record['process_cpu_ns']
  need(all(positive_int(x) for x in (begin,end,cpu)) and previous_end<=begin<end and end-begin<=5_000_000_000 and cpu<=3_000_000_000,'process ordering/budget');previous_end=end;cpu_sum+=cpu
  need(record['original_receipt_sha256']==pins['original_receipt_sha256'][str(i)],'original receipt identifier');need(record['stdout_sha256']==pins['stdout_sha256'][str(i)],'stdout identifier')
  observed=record['observed'];raw=json.dumps(observed,separators=(',',':'))+'\n';need(digest(raw.encode())==record['stdout_sha256'],'original stdout reconstruction differs')
  calc.validate_output(observed,slot)
  times=observed['empty_clocks']+[observed['timing']]
  need(all(begin<=t['wall_begin_ns']<t['wall_end_ns']<=end for t in times),'timing outside process')
  need(sum(t['cpu_end_ns']-t['cpu_begin_ns'] for t in times)<=cpu,'measured CPU exceeds process CPU')
  need(ledger[1+2*i]=={'event':'slot-start','slot':i,'wall_ns':begin},'ledger slot start')
  need(ledger[2+2*i]=={'event':'slot-end','slot':i,'receipt_sha256':record['original_receipt_sha256'],'integrity':'PASS'},'ledger slot finish')
  observations.append(observed)
 need(cpu_sum<=terminal['cpu_elapsed_ns'],'child CPU not covered by terminal');need(previous_end<=ledger[0]['wall_ns']+terminal['wall_elapsed_ns'],'terminal wall does not cover process window')
 need(sum(o['main_calls'] for o in observations)==10_485_760 and sum(o['shadow_calls'] for o in observations)==131_072,'loop count conservation')
 need(sum(o['clock_reads'] for o in observations)==1280 and sum(len(o['empty_clocks']) for o in observations)==240,'clock/control conservation')
 return observations

def compare(a,b,path='result'):
 if isinstance(a,float) or isinstance(b,float):need(type(a) in (int,float) and type(b) in (int,float) and math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-9),'descriptive numeric replay differs '+path)
 elif isinstance(a,dict):
  need(isinstance(b,dict) and a.keys()==b.keys(),'keys differ '+path)
  for k in a:compare(a[k],b[k],path+'.'+k)
 elif isinstance(a,list):
  need(isinstance(b,list) and len(a)==len(b),'length differs '+path)
  for i,(x,y) in enumerate(zip(a,b)):compare(x,y,path+'.'+str(i))
 else:need(type(a)==type(b) and a==b,'value differs '+path)

def replay_data():
 pins=read('provenance/source-pins.json')
 for name,key in [('plan.json','original_plan_sha256'),('PROTOCOL-frozen.md','original_protocol_sha256'),('terminal.json','original_terminal_sha256'),('analysis-frozen.json','original_analysis_sha256'),('sources/host.cpp','original_host_source_sha256'),('sources/study-original.py','original_collector_source_sha256')]:need(digest((ROOT/name).read_bytes())==pins[key],'original source/data pin '+name)
 # Verify the pure calculation functions are verbatim, not a retuned post-result analysis.
 original=(ROOT/'sources/study-original.py').read_text();tree=ast.parse(original);pure=(ROOT/'frozen_math.py').read_text();puretree=ast.parse(pure)
 for name,h in pins['original_function_source_sha256'].items():
  old=ast.get_source_segment(original,next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name==name));new=ast.get_source_segment(pure,next(n for n in puretree.body if isinstance(n,ast.FunctionDef) and n.name==name));need(old==new and digest(old.encode())==h,'frozen mathematical function changed '+name)
 for scene in ('default','dense'):
  for mode in ('plain','extra'):
   outputs=[]
   for arm,image in [('stock','images/image-17'),('candidate','images/image-53')]:
    r=read(f'untimed/{scene}-{mode}-{arm}.json');need(r['exit']==0 and r['stderr']=='' and r['binary_sha256']==pins['images'][image]['sha256'],'untimed receipt identity');o=loads(r['stdout']);need(o['timed'] is False and o['clock_reads']==0 and o['main_calls']==4096,'untimed scope');outputs.append(o)
   need(outputs[0]==outputs[1],'untimed stock/candidate public outputs differ')
 plan=read('plan.json');records=jsonl('records.jsonl');ledger=jsonl('ledger.jsonl');terminal=read('terminal.json')
 observations=validate_records(records,plan,pins,ledger,terminal);result=calc.analyze_data(plan,observations);result.update(terminal=terminal,freeze_sha256=pins['original_private_freeze_sha256'],plan_sha256=pins['original_plan_sha256']);compare(result,read('analysis-frozen.json'))
 need(result['control_pass'] is False and len(result['failures'])==10,'original no-admission outcome changed')
 return {'evidence_replay':'PASS','performance_admission':False,'records':80,'independent_ABBA_batches':20,'all_original_control_failures':result['failures'],'new_DSP_executions':0,'new_timing_runs':0},(records,plan,pins,ledger,terminal,observations)

def self_tests(bundle):
 records,plan,pins,ledger,terminal,observations=bundle;tests=[]
 def rejects(name,fn):
  try:fn()
  except (ValueError,KeyError,TypeError,IndexError):tests.append({'test':name,'pass':True});return
  raise ValueError('negative was accepted: '+name)
 for name,mut in [('missing slot',lambda z:z.pop()),('duplicate slot',lambda z:z.__setitem__(1,copy.deepcopy(z[0]))),('reordered slots',lambda z:z.reverse()),('wrong physical image',lambda z:z[0].update(image_sha256='0'*64)),('reused process identity',lambda z:z[1].update(process_instance=1)),('wrong extra call count',lambda z:next(x for x in z if x['slot']['extra'])['observed'].update(shadow_calls=0)),('changed PCM identity',lambda z:z[0]['observed'].update(pcm_hash=0)),('changed FP flags',lambda z:z[0]['observed']['after_fp'].update(mxcsr=0)),('zero CPU interval',lambda z:z[0]['observed']['timing'].update(cpu_end_ns=z[0]['observed']['timing']['cpu_begin_ns'])),('missing clock control',lambda z:z[0]['observed']['empty_clocks'].pop()),('wrong receipt binding',lambda z:z[0].update(original_receipt_sha256='0'*64))]:
  z=copy.deepcopy(records);mut(z);rejects(name,lambda:validate_records(z,plan,pins,ledger,terminal))
 t=copy.deepcopy(terminal);t['completed_slots']=79;rejects('partial terminal',lambda:validate_records(records,plan,pins,ledger,t))
 rejects('duplicate JSON key',lambda:loads('{"x":1,"x":2}'))
 rejects('nonfinite JSON',lambda:loads('{"x":NaN}'))
 # Separate invented numbers test the predeclared gate itself, not measured evidence.
 synth=copy.deepcopy(observations)
 for o,slot in zip(synth,plan['slots']):
  b=plan['batches'][slot['batch']];duration=22_500_000 if slot['extra'] else 15_000_000 if b['kind']=='compare' and slot['label']=='B' else 20_000_000
  cb=o['timing']['cpu_begin_ns'];wb=o['timing']['wall_begin_ns'];o['timing'].update(cpu_end_ns=cb+duration,wall_end_ns=wb+duration+1000)
  o['empty_clocks']=[{'cpu_begin_ns':cb-3_000_000+i*1_000_000,'cpu_end_ns':cb-3_000_000+i*1_000_000+100,'wall_begin_ns':wb-3_000_000+i*1_000_000,'wall_end_ns':wb-3_000_000+i*1_000_000+200} for i in range(3)]
 need(calc.analyze_data(plan,synth)['control_pass'],'synthetic known-good gate');tests.append({'test':'synthetic valid80 including both positive directions','pass':True})
 z=copy.deepcopy(synth)
 for o in z:
  if o['extra']:o['timing']['cpu_end_ns']=o['timing']['cpu_begin_ns']+20_000_000
 need(any(x.startswith('positive batch') for x in calc.analyze_data(plan,z)['failures']),'fake known-work benefit not caught');tests.append({'test':'synthetic real-count but no extra duration fails positive gate','pass':True})
 z=copy.deepcopy(synth)
 for slot,o in zip(plan['slots'],z):
  if plan['batches'][slot['batch']]['kind']=='sham' and slot['label']=='B':o['timing']['cpu_end_ns']=o['timing']['cpu_begin_ns']+20_400_000
 need('mean sham default' in calc.analyze_data(plan,z)['failures'],'mean sham floor omitted');tests.append({'test':'synthetic2pct shams fail1.5pct mean floor','pass':True})
 z=copy.deepcopy(synth);z[0]['empty_clocks'][0]['cpu_end_ns']=z[0]['empty_clocks'][0]['cpu_begin_ns']+100000;need('clock overhead slot 0' in calc.analyze_data(plan,z)['failures'],'strict overhead maximum');tests.append({'test':'synthetic overhead maximum equality fails strict bound','pass':True})
 z=copy.deepcopy(synth)
 for t in z[0]['empty_clocks']:t['cpu_end_ns']=t['cpu_begin_ns']+20000
 need('clock overhead slot 0' in calc.analyze_data(plan,z)['failures'],'strict overhead mean');tests.append({'test':'synthetic overhead mean equality fails strict bound','pass':True})
 return tests

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--self-test',action='store_true');a=p.parse_args()
 try:
  verify_manifest();report,bundle=replay_data()
  if a.self_test:report['software_self_tests']=self_tests(bundle)
  print(json.dumps(report,indent=2,allow_nan=False))
 except (OSError,ValueError,KeyError,TypeError,IndexError) as e:print('FAIL evidence integrity/replay: '+str(e),file=sys.stderr);raise SystemExit(1)
