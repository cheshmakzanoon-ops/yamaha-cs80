#!/usr/bin/env python3
"""Offline evidence replay. Never runs compilers, DSP, subprocesses or clocks."""
from pathlib import Path
import argparse,collections,copy,gzip,hashlib,json
from work_math import validate,totals
ROOT=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def strict(s):
 def pairs(items):
  out={}
  for k,v in items:
   if k in out:raise ValueError('duplicate key '+k)
   out[k]=v
  return out
 return json.loads(s,object_pairs_hook=pairs,parse_constant=lambda v:(_ for _ in ()).throw(ValueError(v)))
def phase(stock,candidate,pairs,declines,pred):
 a=list(filter(pred,stock));b=list(filter(pred,candidate));pp=list(filter(pred,pairs));dd=list(filter(pred,declines))
 return {'stock':totals(a),'candidate':totals(b),'candidate_four':totals([x for x in b if x['n']==4]),'candidate_eight':totals([x for x in b if x['n']==8]),'matched_pair_pass_histogram':dict(sorted(collections.Counter(f"{x['four_passes'][0]}+{x['four_passes'][1]}->{x['eight_passes']}" for x in pp).items())),'eight_decline_matched_stock_statuses':dict(sorted(collections.Counter(str(x['stock_statuses']) for x in dd).items())),'uneven_eight_groups':sum(x['four_passes'][0]!=x['four_passes'][1] for x in pp)}
def projection(rows):return [{**{k:v for k,v in x.items() if k!='lanes'},'stage_ids':[v[0] for v in x['lanes']]} for x in rows]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--raw-dir',type=Path);parser.add_argument('--self-test',action='store_true');args=parser.parse_args()
 manifest=strict((ROOT/'MANIFEST.json').read_bytes());actual={p.name for p in ROOT.iterdir() if p.is_file() and p.name!='MANIFEST.json'};assert actual==set(manifest['files'])
 for name,rec in manifest['files'].items():
  assert Path(name).name==name and not (ROOT/name).is_symlink();data=(ROOT/name).read_bytes();assert len(data)==rec['bytes'] and sha(data)==rec['sha256'],name
 obs=strict((ROOT/'observations.json').read_bytes());counts={};thin={}
 for arm in ['stock','candidate']:
  data=gzip.decompress((ROOT/(arm+'-counts.jsonl.gz')).read_bytes());assert sha(data)==obs['raw_artifacts'][arm+'.jsonl']['count_projection_sha256']
  counts[arm]=[strict(x) for x in data.splitlines()];thin[arm]=[{**{k:v for k,v in x.items() if k!='stage_ids'},'lanes':[[i] for i in x['stage_ids']]} for x in counts[arm]]
 a,b=thin['stock'],thin['candidate'];pairs,declines=validate(a,b,full=False)
 for name,pred in [('window',lambda x:x['block']<1040),('continuation',lambda x:x['block']==1040),('combined',lambda x:True)]:assert phase(a,b,pairs,declines,pred)==obs['phases'][name],name
 assert obs['full_raw_matching']['matched_quartets']==len(a)
 assert sum(len(x['lanes']) for x in a)==sum(len(x['lanes']) for x in b if x['n']==4 or x['ok'])
 tests=[]
 if args.self_test:
  for name,change in [('removed_event',lambda x:x.pop()),('wrong_stage',lambda x:x[0]['lanes'][0].__setitem__(0,31)),('forged_pass_count',lambda x:x[0].__setitem__('passes',5)),('forged_active_mask',lambda x:x[0].__setitem__('masks',0)),('discard_declines',lambda x:x.__setitem__(slice(None),[y for y in x if y['ok']]))]:
   mutant=copy.deepcopy(b);change(mutant)
   try:validate(a,mutant,full=False)
   except (AssertionError,IndexError):tests.append(name)
   else:raise AssertionError('accepted mutation '+name)
  try:strict('{"x":1,"x":2}')
  except ValueError:tests.append('duplicate_json_key')
  else:raise AssertionError('duplicate json')
 full=False;traces=[]
 if args.raw_dir:
  raw={}
  for arm in ['stock','candidate']:
   name=arm+'.jsonl';data=(args.raw_dir/name).read_bytes();pin=obs['raw_artifacts'][name];assert len(data)==pin['bytes'] and sha(data)==pin['sha256'];raw[arm]=[strict(x) for x in data.splitlines()];assert projection(raw[arm])==counts[arm]
   name=arm+'.bin'
   if (args.raw_dir/name).exists():
    data=(args.raw_dir/name).read_bytes();pin=obs['raw_artifacts'][name];assert len(data)==pin['bytes'] and sha(data)==pin['sha256'];traces.append(name)
  validate(raw['stock'],raw['candidate']);full=True
  if args.self_test:
   for field in [1,2]:
    mutant=copy.deepcopy(raw['candidate']);mutant[0]['lanes'][0][field]^=1
    try:validate(raw['stock'],mutant)
    except AssertionError:tests.append('wrong_input_bits' if field==1 else 'wrong_state_fingerprint')
    else:raise AssertionError('accepted raw mutation')
 print(json.dumps({'status':'PASS','timing':False,'stock_events':len(a),'candidate_events':len(b),'ordered_quartet_matches':len(a),'all_phase_counts_and_active_masks_replayed':True,'full_raw_input_and_stage_fingerprints_replayed':full,'trace_files_hashed':traces,'negative_controls_passed':tests,'scope':'deterministic work counts only; no CPU or speed admission'},indent=2))
if __name__=='__main__':main()
