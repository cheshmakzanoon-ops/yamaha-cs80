from pathlib import Path
import hashlib,json
p=Path(__file__).resolve().parent
m=json.loads((p/'manifest.json').read_text());assert set(m)=={o.name for o in p.iterdir() if o.is_file() and o.name!='manifest.json'}
for k,v in m.items():assert hashlib.sha256((p/k).read_bytes()).hexdigest()==v
matrix=json.loads((p.parent/'evidence/engine.json').read_text())['rows'];want=[x for x in matrix if x['arm']=='witness']
rr=json.loads((p/'new-dispatch.json').read_text());assert len(rr)==20
for i,q in enumerate(rr):
 r=want[i%10];assert q['arm']==('prefilter' if i<10 else 'prefilter-witness') and q['row']==i%10
 assert (q['scene'],q['rate'],q['filterKernel'])==(r['scene'],r['rate'],r['filterKernel'])
 assert q['correct_reference_sha256']==r['sha256'] and q['original_and_new_shadow_fast']==r['fast'] and q['original_fp_read_pairs']==r['fast']+r['stock']
 if i<10:assert q['new_observed_fp_read_pairs'] is None
 else:assert 0<=q['new_observed_fp_read_pairs']<=q['original_fp_read_pairs']
assert sum(x['original_fp_read_pairs'] for x in rr[10:])==3972576 and sum(x['new_observed_fp_read_pairs'] for x in rr[10:])==2037699
assert [r['new_observed_fp_read_pairs'] for r in rr[10:]]==[523730,165207,0,524272,0,0,300760,0,0,523730]
print('Verified original/new per-scene receipts, 1,934,877 fewer control-read pairs, unchanged admitted count. No timing.')
