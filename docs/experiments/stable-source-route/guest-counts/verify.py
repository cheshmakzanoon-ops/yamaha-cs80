#!/usr/bin/env python3
"""Read-only verification of a complete source-route diagnostic directory (no plugin is executed)."""
from pathlib import Path
import json, re, sys, hashlib, struct
P=Path(__file__).resolve().parent
ARM={'A':('first4',4),'A2':('first4',4),'P':('all5',5),'S':('fifth',1),'C':('first4',4)}
def check(x,m):
 if not x:raise ValueError(m)
def exactly(rows,prefix):
 a=[r[len(prefix):] for r in rows if r.startswith(prefix)];check(len(a)==1,'expected exactly one '+prefix);return a[0]
def scalar3(line):return tuple(map(int,line.strip().split()[:3]))
def parse(file,k):
 r=file.read_text().splitlines()
 check('creator: callgrind-3.24.0' in r and 'desc: Trigger: Client Request: selected_callbacks' in r,'unrecognized dump')
 check(exactly(r,'events: ').split()[:3]==['Ir','Dr','Dw'],'unrecognized event order')
 totals=scalar3(exactly(r,'summary: '));check(totals==scalar3(exactly(r,'totals: ')),'event conservation')
 names={}
 patt=re.compile(r'(c?fn)=\((\d+)\)(?: (.*))?$')
 for l in r:
  m=patt.match(l)
  if m and m.group(3):
   idx,name=m.group(2,3);check(idx not in names or names[idx]==name,'inconsistent name');names[idx]=name
 fun=None; trg=None; edges={}; own={}; inputs={}; i=0
 while i<len(r):
  l=r[i]; t=patt.match(l)
  if t:
   if t.group(1)=='fn':fun=names[t.group(2)]
   else:trg=names[t.group(2)]
  elif l.startswith('calls='):
   n=int(l[6:].split()[0]);i+=1;e=r[i].split()[1:];v=tuple(map(int,(e+['0']*3)[:3]));key=(fun,trg)
   if key not in edges:edges[key]=[0,0,0,0]
   for x,y in enumerate((n,)+v):edges[key][x]+=y
   trg=None
  elif fun and l and re.match(r'^[-+*\d]',l):
   e=l.split()[1:];v=tuple(map(int,(e+['0']*3)[:3]));a=own.setdefault(fun,[0,0,0])
   for x,y in enumerate(v):a[x]+=y
  i+=1
 wrapper='cs80_counted_process';target='non-virtual thunk to juce::JuceVST3Component::process(Steinberg::Vst::ProcessData&)'
 gr=edges[(wrapper,target)];check(gr[0]==k,'not the expected number of function boundary entries')
 check(own[wrapper]==[k*5,k*3,k],'unexpected common indirect call stub count')
 check(tuple(gr[t+1]+own[wrapper][t] for t in range(3))==totals,'no exclusive caller isolation')
 route='cs80::Engine::setControlFrame(cs80::ControlFrame const&)';route_edges=[v for (f,t),v in edges.items() if t==route];check(route_edges,'route absent')
 route_total=[sum(x[j] for x in route_edges) for j in range(1,4)]
 batch='cs80::Ig00156BatchAccess::process('
 return {'stub_plus_plugin':totals,'plugin_edge':gr[1:],'route_inclusive':route_total,'batch_self_ir':sum(v[0] for n,v in own.items() if n.startswith(batch))}
def summarize():
 summary={}
 for scene in ('fresh-quiet','held-short'):
  d={a:parse(P/f'cg-{scene}-{a}.1',n) for a,(s,n) in ARM.items()}
  for sig in ['stub_plus_plugin','plugin_edge','route_inclusive']:
   R={a:v[sig] for a,v in d.items()}
   check(tuple(R['A'])==tuple(R['A2']),'same stock differs '+sig)
   check(tuple(R['P'])==tuple(a+b for a,b in zip(R['A'],R['S'])) and all(v>0 for v in R['S']),'positive control did not exactly add '+sig)
  c=d['C']['plugin_edge'];s=d['A']['plugin_edge']
  delta=[y-x for x,y in zip(s,c)];route_delta=[y-x for x,y in zip(d['A']['route_inclusive'],d['C']['route_inclusive'])]
  check(delta==route_delta,'change not wholly attributed to route inclusive')
  paths={'guest-'+a:P/f'guest-{scene}-{a}' for a in ARM}|{'native-stock':P/f'native-{scene}-s','native-candidate':P/f'native-{scene}-c'}
  ref=paths['guest-A'];native=paths['native-stock']
  files={'pcm.bin','fp.bin','initial.state','postwarm.state','window.state','continuation.state'}
  hashf=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  for key,p in paths.items():
   check({f.name for f in p.iterdir()}==files|{'outcome.json'},'unexpected artifacts')
   a=json.loads((p/'outcome.json').read_text())
   check(a['genuine_calls']==38 and a['warm']==32 and a['window']==5 and a['scene']==scene and a['note_event_reads']==(8 if scene=='held-short' else 0),'inconsistent callback census')
   check((a['window_peak']>1e-5 if scene=='held-short' else a['window_peak']<=1e-6),'scene nonvacuity failed')
   if key.startswith('guest-'):
    k=key.split('-')[1];check(a['under_valgrind']==1 and (a['arm_window'],a['counted_calls'])==ARM[k],'guest arm attribution')
    parent=ref
   else:
    check(a['under_valgrind']==0 and a['arm_window']=='first4','native reference scope');parent=native
   for f in files:check(hashf(p/f)==hashf(parent/f),'within-runtime public evidence differs')
  for f in files-{'fp.bin'}:check(hashf(ref/f)==hashf(native/f),'guest vs native public evidence differs '+f)
  layout=struct.Struct('<iQII'+3*'IHHi'+'d'); natively=list(layout.iter_unpack((native/'fp.bin').read_bytes()));virt=list(layout.iter_unpack((ref/'fp.bin').read_bytes()));labels=['result','silence','event_reads','event_count_reads']+[f'{stage}.{field}' for stage in ('before','after','next') for field in ('mxcsr','x87cw','x87sw','errno')]+['probe']
  check(len(virt)==len(natively)==38,'FP record count')
  changed={name:sum(a[i]!=b[i] for a,b in zip(virt,natively)) for i,name in enumerate(labels) if any(a[i]!=b[i] for a,b in zip(virt,natively))}
  summary[scene]={'events':['Ir','Dr','Dw'],'counts':d,'candidate_minus_stock':delta,'per_callback_delta':[v/4 for v in delta],'percent_change':[v*100/base for v,base in zip(delta,s)],'cross_runtime_differing_fields':changed,'sha256':{'pcm':hashf(native/'pcm.bin'),'guest_fp':hashf(ref/'fp.bin'),'native_fp':hashf(native/'fp.bin'),'states':{f:hashf(native/f) for f in files if f.endswith('.state')}},'guest_abi':{x:json.loads((ref/'outcome.json').read_text())[x] for x in ('cpu_avx2','cpu_avx512f','xcr0')},'native_abi':{x:json.loads((native/'outcome.json').read_text())[x] for x in ('cpu_avx2','cpu_avx512f','xcr0')}}
 return summary
if __name__=='__main__':print(json.dumps(summarize(),indent=2,sort_keys=True))
