import csv,json,hashlib,pathlib,collections
root=pathlib.Path(__file__).resolve().parent
baseline=list(csv.DictReader((root.parent/'a05/metrics.csv').open()))
current=list(csv.DictReader((root/'metrics.csv').open()))
parity=list(csv.DictReader((root/'baseline-parity-metrics.csv').open()))
key=lambda r:(r['name'],int(r['note']),round(float(r['velocity']),3))
before={key(r):r for r in baseline}
after={key(r):r for r in current}
fields=['peak_dbfs','rms_025_250_dbfs','rms_300_800_dbfs','fund_025_250_dbfs']
assert len(before)==len(after)==90 and before.keys()==after.keys()
rows=[]
for name,note in sorted({(r['name'],int(r['note'])) for r in current}):
 pairs=sorted((k,before[k],after[k]) for k in after if k[:2]==(name,note))
 entry={'name':name,'midi_note':note,'measurements':{}}
 for field in fields:
  b=[float(x[1][field]) for x in pairs];a=[float(x[2][field]) for x in pairs]
  entry['measurements'][field]={'baseline_soft_to_hard_db':b[-1]-b[0],'candidate_soft_to_hard_db':a[-1]-a[0],'span_change_db':(a[-1]-a[0])-(b[-1]-b[0]),'baseline_top_step_db':b[-1]-b[-2],'candidate_top_step_db':a[-1]-a[-2],'candidate_monotonic':all(y>x for x,y in zip(a,a[1:])),'hard_endpoint_delta_db_rounded_baseline':a[-1]-b[-1],'soft_endpoint_delta_db_rounded_baseline':a[0]-b[0]}
 rows.append(entry)
identical=[];different=[]
for r in parity:
 k=key(r)
 target=after[k]
 (identical if r['audio_fnv1a64']==target['audio_fnv1a64'] else different).append({'name':k[0],'note':k[1],'velocity':k[2]})
summary={'scope':'Matched 90-case full-factory-preset sweep. Body model refinement only; preset parameter vectors unchanged. Exact full-strike and non-tine endpoint checks use identical float-audio FNV1a64 hashes; rounded-baseline metrics are labelled separately.','renders':len(current),'all_peak_and_rms_monotonic':all(all(p['measurements'][f]['candidate_monotonic'] for f in fields[:3]) for p in rows),'all_nominal_fundamental_monotonic':all(p['measurements'][fields[3]]['candidate_monotonic'] for p in rows),'exact_audio_parity':{'compared_cases':len(parity),'samples_per_case':96000,'identical_cases':len(identical),'different_cases':different,'note':'Same executable probe source, fresh Engine per capture, 48 kHz stereo float, 128-frame blocks, one-second hold. FNV1a64 checks byte identity, not cryptographic provenance; files additionally SHA256-bound.'},'per_preset_register':rows}
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print('Compared',len(current),'cases; monotonic peak/RMS:',summary['all_peak_and_rms_monotonic'],'exact parity',len(identical),'of',len(parity))
for name in sorted({p['name'] for p in rows}):
 p=[r for r in rows if r['name']==name]
 f='rms_300_800_dbfs'
 ranges=lambda k:[min(r['measurements'][f][k] for r in p),max(r['measurements'][f][k] for r in p)]
 print(name,'body span baseline',ranges('baseline_soft_to_hard_db'),'candidate',ranges('candidate_soft_to_hard_db'),'top step',ranges('candidate_top_step_db'))
assert not different,'Full-strike or unrelated-model audio changed'
assert summary['all_peak_and_rms_monotonic'],'Non-monotonic full-preset level response'
