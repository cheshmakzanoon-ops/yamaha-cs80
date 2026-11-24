#!/usr/bin/env python3
"""A27: complete Leads structure and complementary full-Engine captures."""
import csv, hashlib, json, math, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[5]
OUT=Path(__file__).resolve().parent
SOURCE='fcc3c8e055223fe4f786ca162ff46076c33471c8'
before=json.loads((ROOT/'docs/preset-evidence/2026.11.5/baseline/inventory.json').read_text())
after=json.loads((ROOT/'docs/preset-evidence/2026.11.6/verification/a15/inventory.json').read_text())
ids=after['parameter_ids']; old={p['host_index']:p for p in before['programs']}
programs=[p for p in after['programs'] if p['category']=='Leads']
assert len(programs)==56
rows=[]
for p in programs:
    b=old[p['host_index']]
    assert all(p[k]==b[k] for k in ('index','host_index','name','category'))
    changes=[ids[i] for i in range(200) if p['values'][i]!=b['values'][i] and ids[i]!='volume']
    assert changes,p['name']
    active=[]
    for rank in (1,2):
        model=p['values'][ids.index(f'body{rank}_model')]; mix=p['values'][ids.index(f'body{rank}_mix')]
        if model and mix: active.append({'rank':rank,'model':model,'mix':mix})
    assert p['values'][202:]==[0,0]
    rows.append({'name':p['name'],'host_index':p['host_index'],'collection':'original' if p['host_index']<222 else 'curated','changed_prior_non_volume_controls':changes,'active_bodies':active})
assert len({tuple(v for i,v in enumerate(p['values']) if ids[i]!='volume') for p in programs})==56
(OUT/'structural-audit.json').write_text(json.dumps({'program_count':56,'programs':rows},indent=2)+'\n')
tests=(ROOT/'tests/LeadRefinementTests.cpp').read_text().split('constexpr const char* names[] {',1)[1].split('};',1)[0]
covered=set(re.findall(r'"([^"\n]+)"',tests)); assert len(covered)==30
extra=[p for p in programs if p['name'] not in covered]
assert len(extra)==26
ordinary=[p for p in extra if p['name']!='Electric Leads 136']
exe=ROOT/'build-preset/cs80_preset_capture'; fingerprint=hashlib.sha256(exe.read_bytes()).hexdigest()
def capture(label, names, velocity, pressure):
    args=[str(exe),'--out',str(OUT/label),'--library','all','--rate','48000','--note','67','--velocity',str(velocity),'--pressure',str(pressure),'--hold','1.3','--tail','.5']
    for name in names:args+=['--preset',name]
    with (OUT/f'{label}.log').open('w') as stream:subprocess.run(args,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,check=True)
    return {r['name']:r for r in csv.DictReader((OUT/label/'metrics.csv').open())}
soft=capture('soft',[p['name'] for p in ordinary],.25,0)
firm=capture('firm',[p['name'] for p in ordinary],.85,0)
special={label:capture(label,['Electric Leads 136'],v,p)['Electric Leads 136'] for label,v,p in [('pressure-zero',.55,0),('pressure-light',.55,.3),('pressure-full',.55,1),('pressure-full-soft-key',.25,1),('pressure-full-firm-key',.85,1)]}
assert hashlib.sha256(exe.read_bytes()).hexdigest()==fingerprint
all_metrics=list(soft.values())+list(firm.values())+list(special.values())
assert len(all_metrics)==55
for m in all_metrics:
    assert int(m['nonfinite'])==0 and int(m['samples_over_full_scale'])==0
    assert math.isfinite(float(m['peak_dbfs'])) and float(m['peak_dbfs'])<0
measured=[{'name':p['name'],'onset_touch_db':float(firm[p['name']]['early_rms_dbfs'])-float(soft[p['name']]['early_rms_dbfs']),'firm_peak_dbfs':float(firm[p['name']]['peak_dbfs'])} for p in ordinary]
assert float(special['pressure-zero']['peak_dbfs']) < -110
assert float(special['pressure-full']['body_rms_dbfs']) > -60
pressure_span=float(special['pressure-full']['body_rms_dbfs'])-float(special['pressure-light']['body_rms_dbfs'])
fixed_pressure_velocity=abs(float(special['pressure-full-soft-key']['body_rms_dbfs'])-float(special['pressure-full-firm-key']['body_rms_dbfs']))
assert pressure_span>10 and fixed_pressure_velocity<.1
sources={}
for name in ['src/core/InstrumentBody.h','src/core/Engine.cpp','src/core/Engine.h','src/core/Params.h','src/core/FactoryPresets.cpp','src/core/FactoryPresets.h','src/core/CuratedLibrary.inc','src/core/OriginalCircuitLibrary.inc','src/core/curated/Leads.inc','tests/LeadRefinementTests.cpp']:
    data=(ROOT/name).read_bytes(); assert data==subprocess.check_output(['git','show',SOURCE+':'+name],cwd=ROOT)
    sources[name]=hashlib.sha256(data).hexdigest()
report={'source_commit':SOURCE,'source_hashes':sources,'capture_executable_sha256':fingerprint,'scope':'local complete-Engine evidence; not native installation qualification','program_count':56,'original_count':sum(p['host_index']<222 for p in programs),'curated_count':sum(p['host_index']>=222 for p in programs),'body_enabled':sum(bool(r['active_bodies']) for r in rows),'changed_control_range':[min(len(r['changed_prior_non_volume_controls']) for r in rows),max(len(r['changed_prior_non_volume_controls']) for r in rows)],'complementary_programs':26,'captures':55,'audio_seconds':99,'note':67,'rate':48000,'hold_seconds':1.3,'tail_seconds':.5,'all_finite_unclipped':True,'maximum_peak_dbfs':max(float(m['peak_dbfs']) for m in all_metrics),'ordinary_contrast_range_db':[min(r['onset_touch_db'] for r in measured),max(r['onset_touch_db'] for r in measured)],'electric_leads_136':{'zero_pressure_peak_dbfs':float(special['pressure-zero']['peak_dbfs']),'pressure_03_to_1_body_span_db':pressure_span,'fixed_full_pressure_velocity_difference_db':fixed_pressure_velocity},'programs':measured}
(OUT/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('programs','source_hashes')},indent=2))
