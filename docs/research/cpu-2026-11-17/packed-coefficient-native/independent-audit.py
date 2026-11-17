import pathlib,json,hashlib,csv,math,struct
p=pathlib.Path(__file__).parent/'extracted'
def load(n):return json.loads((p/n).read_text())
def sha(f):
 h=hashlib.sha256()
 with open(f,'rb') as x:
  for b in iter(lambda:x.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def gm(v):return math.exp(sum(map(math.log,v))/len(v))
m=load('evidence-manifest.json')
for f,h in m.items():assert sha(p/f)==h,f
assert sha(p/'stock-component.bin')==sha(p/'candidate-component.bin')
assert sha(p/'stock-engine.corpus')==sha(p/'candidate-engine.corpus')
s=load('schedule.json');assert len(s)==84
vals={};pcm={};n=0
for i,x in enumerate(s):
 assert x['index']==i
 folder=p/f'run-{i:03}'
 r=json.loads((folder/'result.json').read_text()); ref=json.loads((folder/'audio-reference.json').read_text())
 assert r['row']==x['row'] and r['arm']==x['arm']
 assert r['callbacks']==11250 and r['frames']==2880000 and r['rate']==48000 and r['block']==256
 assert sha(p/ref['relative_path'])==ref['sha256'];assert (p/ref['relative_path']).stat().st_size==ref['bytes']==23040000
 if r['row'] in pcm:assert pcm[r['row']]==ref['sha256']
 pcm[r['row']]=ref['sha256']
 total=0; count=0
 with open(folder/'callbacks.csv') as f:
  for row in csv.DictReader(f):
   assert int(row['callback'])==count;count+=1;v=int(row['cpu_ns']);assert v>0;total+=v
 assert count==11250 and total==r['cpu_ns'];n+=count
 vals[x['phase'],x['round'],x['row'],x['role']]=total
ratios={phase:[[vals[phase,rd,row,'B']/vals[phase,rd,row,'A'] for row in range(7)] for rd in range(3)] for phase in ['aa','candidate']}
aa=[gm(row[:4]) for row in ratios['aa']]; ca=[gm(row[:4]) for row in ratios['candidate']]
noise=max(abs(x-1) for x in aa);gain=1-gm(ca);threshold=max(.005,2*noise)
assert all(.97<=x<=1.03 for row in ratios['aa'] for x in row)
assert all(.985<=x<=1.015 for x in aa)
assert all(.97<=row[6]<=1.03 for row in ratios['candidate'])
assert all(x<1 for x in ca) and gain>threshold
assert all(x<=1.03 for row in ratios['candidate'] for x in row)
t=load('terminal.json');assert math.isclose(t['primary_gain'],gain,abs_tol=1e-14)
for arm in ['stock','candidate']:
 for row in range(7):
  ref=load(f'dry-{arm}-{row}/audio-reference.json');assert ref['sha256']==pcm[row]
links=load('binary-arm-links.json')
for v in links.values():
 assert sha(p/v['binary'].split('packed-coefficient-cpu-results/')[1])==v['sha256']
 for obj,h in v['objects'].items():assert sha(p/obj.split('packed-coefficient-cpu-results/')[1])==h
report={'artifact_sha256':sha(p.parent/'artifact.zip'),'manifest_files_verified':len(m),'timed_runs':len(s),'callback_rows_independently_summed':n,'dry_runs':14,'exact_pcm_workloads':len(pcm),'component_bytes_exact':(p/'stock-component.bin').stat().st_size,'engine_corpus_bytes_exact':(p/'stock-engine.corpus').stat().st_size,'aa_primary_ratios':aa,'candidate_primary_ratios':ca,'primary_gain_percent':gain*100,'minimum_gain_percent':threshold*100,'candidate_all_row_ratios':ratios['candidate'],'decision':'MEASURED_USEFUL_GAIN','scope':'GNU13 Linux x86_64 standalone PIC core, not actual plugin or other hardware/platform qualification'}
(p.parent/'independent-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
