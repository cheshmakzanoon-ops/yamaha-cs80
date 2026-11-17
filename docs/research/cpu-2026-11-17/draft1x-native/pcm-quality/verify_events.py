from pathlib import Path
import csv,json,struct,sys
root=Path(sys.argv[1]);out=Path(sys.argv[2]);count=0
for run in sorted(root.glob('run-[0-9][0-9][0-9]')):
 r=json.loads((run/'result.json').read_text());row=r['row'];assert r['phase']=='timed';assert r['selector']==[0,4,7,10,4,4][row];assert r['callbacks']==11250 and r['warmup_callbacks']==375 and r['warmup_frames']==96000
 with (run/'events.csv').open() as f:events=list(csv.DictReader(f))
 assert len(events)==11250
 for k,e in enumerate(events):
  phase=k%750;v=(phase if phase<=375 else 750-phase)/375 if row==4 else 0
  expected={'callback':str(k),'frame':str(k*256),'noteons':str(8 if row==5 and k%1125==0 else 0),'noteoffs':str(8 if row==5 and k%1125==375 else 0),'pressure_event':str(int(row==4)),'pressure_bits':str(struct.unpack('<I',struct.pack('<f',v))[0])}
  assert e==expected,(run.name,k)
 count+=1
assert count==108
(out/'events-verified.json').write_text(json.dumps({'timed_receipts':108,'event_rows_verified':108*11250,'release_noteon_callbacks':list(range(0,11250,1125)),'release_noteoff_callbacks':list(range(375,11250,1125)),'pressure_period_callbacks':750,'status':'PASS'},indent=2)+'\n')
