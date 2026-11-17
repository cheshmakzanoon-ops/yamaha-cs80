"""Independent CSV accounting; imports no study module, executes no study binary."""
import csv,json,struct,pathlib
p=pathlib.Path(__file__).parent/'native-study';runs=sorted(x for x in p.glob('dry-*') if x.is_dir())+sorted(x for x in p.glob('run-*') if x.is_dir());assert len(runs)==126
ncallbacks=nevents=0
for run in runs:
 r=json.loads((run/'result.json').read_text());row=r['row'];arm=r['arm'];phase=r['phase']
 assert row in range(6) and arm in ['stock','off','draft'] and phase in ['dry','timed']
 assert r['selector']==[0,4,7,10,4,4][row] and r['rate']==48000 and r['block']==256
 assert r['factor']==(1 if arm=='draft' else 2) and r['latency']==(8 if arm=='draft' else 67)
 assert (r['callbacks'],r['frames'],r['warmup_callbacks'],r['warmup_frames'])==(11250,2880000,375,96000)
 with (run/'callbacks.csv').open() as f:rows=list(csv.DictReader(f))
 assert len(rows)==11250;total=0
 for k,v in enumerate(rows):
  assert int(v['callback'])==k;ns=int(v['cpu_ns']);assert ns>=0;total+=ns
 assert total==r['cpu_ns'];assert (total==0 if phase=='dry' else total>0)
 with (run/'events.csv').open() as f:events=list(csv.DictReader(f))
 assert len(events)==11250;counts=[0,0,0]
 for k,v in enumerate(events):
  assert int(v['callback'])==k and int(v['frame'])==k*256
  on=8 if row==5 and k in range(0,11250,1125) else 0
  off=8 if row==5 and k in range(375,11250,1125) else 0
  press=int(row==4);q=k%750;value=min(q,750-q)/375.0 if press else 0.0
  bits=int.from_bytes(struct.pack('<f',value),'little')
  assert (int(v['noteons']),int(v['noteoffs']),int(v['pressure_event']),int(v['pressure_bits']))==(on,off,press,bits)
  counts=[a+b for a,b in zip(counts,[on,off,press])]
 assert counts==[r['measured_noteons'],r['measured_noteoffs'],r['pressure_events']]
 ncallbacks+=len(rows);nevents+=len(events)
summary={'independent_csv_validation':'PASS','receipt_count':len(runs),'dry_receipts':18,'timed_receipts':108,'callback_rows_checked':ncallbacks,'event_rows_checked':nevents,'shared_study_validator_imported':False,'checks':['all callback sequence/count/nonnegative time and total CPU sums','all event callback/frame positions','all release note counts and exact375/1125 callback pattern','all pressure float32 bit patterns and event counts','all selector/rate/block/factor/latency/warmup/frame receipt constants'],'limits':'PCM finiteness and cross-run hash checks remain from earlier audit using shared study receipt validator; gate arithmetic and this complete CSV supplement are independently implemented.'}
print(json.dumps(summary,indent=2))
