#!/usr/bin/env python3
"""A30 independent Hardware Studies identity/programming and render audit."""
import csv, hashlib, json, math, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[5]
OUT=Path(__file__).resolve().parent
before=json.loads((ROOT/'docs/preset-evidence/2026.11.5/baseline/inventory.json').read_text())
after=json.loads((ROOT/'docs/preset-evidence/2026.11.6/verification/a15/inventory.json').read_text())
ids=after['parameter_ids']; original={p['host_index']:p for p in before['programs']}
programs=[p for p in after['programs'] if p['category']=='Hardware Studies']
assert len(programs)==22
rows=[]
for p in programs:
 b=original[p['host_index']]
 assert all(p[k]==b[k] for k in ('index','host_index','name','category'))
 changes=[ids[i] for i in range(200) if p['values'][i]!=b['values'][i] and ids[i]!='volume']
 assert changes,p['name']
 assert p['values'][-2:]==[0,0],p['name']
 active=[]
 for rank in (1,2):
  model=p['values'][ids.index(f'body{rank}_model')]; mix=p['values'][ids.index(f'body{rank}_mix')]
  if model and mix:active.append({'rank':rank,'model':model,'mix':mix})
 rows.append({'name':p['name'],'host_index':p['host_index'],'collection':'original' if p['host_index']<222 else 'curated','changed_prior_non_volume_controls':changes,'active_bodies':active})
assert len({tuple(v for i,v in enumerate(p['values']) if ids[i]!='volume') for p in programs})==22
(OUT/'structural-audit.json').write_text(json.dumps({'program_count':22,'body_enabled':sum(bool(r['active_bodies']) for r in rows),'programs':rows},indent=2)+'\n')
exe=ROOT/'build-preset/cs80_preset_capture'
hash_before=hashlib.sha256(exe.read_bytes()).hexdigest()
for label,velocity in [('soft','.25'),('firm','.85')]:
 args=[str(exe),'--out',str(OUT/label),'--library','all','--rate','48000','--note','60','--velocity',velocity,'--pressure','0','--hold','1.4','--tail','.5']
 for p in programs:args += ['--preset',p['name']]
 with (OUT/f'{label}-capture.log').open('w') as stream:subprocess.run(args,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,check=True)
assert hashlib.sha256(exe.read_bytes()).hexdigest()==hash_before
metrics={label:{r['name']:r for r in csv.DictReader((OUT/label/'metrics.csv').open())} for label in ('soft','firm')}
assert all(len(v)==22 for v in metrics.values())
measured=[]
for p in programs:
 s,f=metrics['soft'][p['name']],metrics['firm'][p['name']]
 for m in (s,f):
  assert int(m['nonfinite'])==0 and int(m['samples_over_full_scale'])==0
  assert math.isfinite(float(m['peak_dbfs'])) and float(m['peak_dbfs'])<0
 measured.append({'name':p['name'],'onset_contrast_db':float(f['early_rms_dbfs'])-float(s['early_rms_dbfs']),'firm_peak_dbfs':float(f['peak_dbfs']),'firm_body_dbfs':float(f['body_rms_dbfs']),'firm_release_tail_dbfs':float(f['tail_rms_dbfs']),'firm_release_relative_body_db':float(f['tail_rms_dbfs'])-float(f['body_rms_dbfs'])})
report={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'capture_sha256':hash_before,'capture_source_scope':'locally built current source; not a native installation artifact','capture_count':44,'all_finite_unclipped':True,'note':60,'rate':48000,'hold_seconds':1.4,'tail_seconds':.5,'program_count':22,'body_enabled':sum(bool(r['active_bodies']) for r in rows),'changed_control_count_range':[min(len(r['changed_prior_non_volume_controls']) for r in rows),max(len(r['changed_prior_non_volume_controls']) for r in rows)],'programs':measured}
(OUT/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='programs'},indent=2))
print('contrast range',min(r['onset_contrast_db'] for r in measured),max(r['onset_contrast_db'] for r in measured))
print('peak',max(r['firm_peak_dbfs'] for r in measured))
