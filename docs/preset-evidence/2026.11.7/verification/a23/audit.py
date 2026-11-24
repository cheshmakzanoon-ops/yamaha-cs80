#!/usr/bin/env python3
"""A23: full Lead structure, subset gestures, and two pressure-only .4 gaps.

Default reruns the compact captures. --reuse-captures analyses only this exact
session's enumerated files. Requires numpy and the source-matched executables.
"""
import argparse, csv, hashlib, json, math, re, struct, subprocess
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
SOURCE = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
OLD6 = '18711f93a39aa49f3d6f9a41784be456224facbe'
OLD4 = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
INVENTORY4 = '8685e9ed39d6347b67401da7590b7e1e8e95472d'
CURRENT = Path('/tmp/cs80-build-1cee5a')
PUBLISHED = Path('/tmp/cs80-published-4-build-1cee5a')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reuse-captures', action='store_true')
parser.add_argument('--raw', type=Path, default=Path('/tmp/cs80-a23-1cee5a'))
args = parser.parse_args()
RAW = args.raw
RAW.mkdir(parents=True, exist_ok=True)
commands, checks, manifest = [], [], []

def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def blob(commit, path): return subprocess.check_output(['git','show',commit+':'+path], cwd=ROOT)
def write(name, data): (OUT/name).write_text(json.dumps(data, indent=2, allow_nan=False)+'\n')
def check(label, passed): checks.append({'contract': label, 'passed': bool(passed)})
def run(cmd, log=None):
    cmd = [str(x) for x in cmd]; commands.append(cmd)
    result = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if log: Path(log).write_text(result.stdout)
    result.check_returncode()
    return result.stdout
def db(value): return 20*math.log10(max(float(value), 1e-12))
def rms(a): return float(np.sqrt(np.mean(np.asarray(a, dtype=np.float64)**2)))
def file_receipt(path, source, **extra):
    manifest.append({'path':str(path),'source':source,'bytes':path.stat().st_size,'sha256':digest(path),**extra})

# Authenticate the inspected frozen source and every complete Lead record.
subprocess.run(['git','diff','--quiet',SOURCE,'--','src','tests','CMakeLists.txt'], cwd=ROOT, check=True)
current_inventory = ROOT/'docs/preset-evidence/2026.11.7/verification/a12/inventory.json'
now = json.loads(current_inventory.read_text())
six = json.loads(blob(OLD6,'docs/preset-evidence/2026.11.6/verification/a15/inventory.json'))
four = json.loads(blob(INVENTORY4,'docs/preset-evidence/2026.11.4/continuation/final-inventory/inventory.json'))
ids = now['parameter_ids']
assert len(ids)==204 and ids==six['parameter_ids']
leads = [p for p in now['programs'] if p['category']=='Leads']
six_by = {p['host_index']:p for p in six['programs']}
four_by = {p['host_index']:p for p in four['programs']}
mechanisms = json.loads((ROOT/'docs/preset-evidence/2026.11.7/verification/a12/mechanism-coverage.json').read_text())
mechanism_by = {p['host_index']:p['eligible_mechanisms'] for p in mechanisms['programs']}
original = (ROOT/'src/core/OriginalCircuitLibrary.inc').read_text()
original_sources = re.findall(r'"([^"]+)"',original.split('{ "Leads", {',1)[1].split('} }',1)[0])
rows=[]
for p in leads:
    assert p==six_by[p['host_index']], p['name']
    b=four_by[p['host_index']]
    assert all(p[k]==b[k] for k in ('index','host_index','name','category'))
    changes=[key for i,key in enumerate(four['parameter_ids']) if key!='volume'
             and struct.pack('<f',p['values'][ids.index(key)])!=struct.pack('<f',b['values'][i])]
    assert changes
    values=dict(zip(ids,p['values']))
    bodies=[{'rank':k,'model':values[f'body{k}_model'],'mix':values[f'body{k}_mix']}
            for k in (1,2) if values[f'body{k}_model'] and values[f'body{k}_mix']]
    mapped={36:'Ribbon Glide Lead',37:'Ring Lead'}.get(p['host_index'])
    if 158<=p['host_index']<=179: mapped=original_sources[p['host_index']-158]
    rows.append({'host_index':p['host_index'],'name':p['name'],'category':p['category'],
        'collection':'original' if p['host_index']<222 else 'curated',
        'recipe_file':'OriginalCircuitLibrary.inc' if p['host_index']<222 else
                      ('curated/Modern.inc' if p['name'] in ('Drive Lead','Space Lead') else 'curated/Leads.inc'),
        'inherited_recipe':mapped,'description':p['description'],'tags':p['tags'],
        'complete_record_equal_to_6':True,'parameter_values_verified':204,
        'complete_record_sha256':hashlib.sha256(json.dumps(p,sort_keys=True).encode()).hexdigest(),
        'changed_prior_non_master_controls':changes,'active_bodies':bodies,
        'character_scope':'electronic lead with acoustic-inspired body color' if bodies else 'electronic circuit lead',
        'eligible_7_mechanisms':mechanism_by[p['host_index']],
        'voice_mode':values['voice_mode'],'source_performance':values['source_performance'],
        'portamento_seconds':values['portamento'],'glide_mode':values['glide_mode'],
        'material_selectors':[values['body1_material'],values['body2_material']]})
assert len(rows)==56 and all(r['voice_mode']==0 and r['source_performance']==1 for r in rows)
assert all(r['material_selectors']==[0,0] for r in rows)
assert len({tuple(v for k,v in zip(ids,p['values']) if k!='volume') for p in leads})==56
structural={'source':SOURCE,'baseline_6_source':OLD6,'published_4_source':OLD4,
    'published_4_inventory_commit':INVENTORY4,'inventory_sha256':digest(current_inventory),
    'scope':'All 56 Leads: complete identity/metadata and 11,424 exact parameter values; not full fresh audio coverage.',
    'program_count':56,'original_count':sum(r['collection']=='original' for r in rows),
    'curated_count':sum(r['collection']=='curated' for r in rows),'body_enabled':sum(bool(r['active_bodies']) for r in rows),
    'changed_prior_non_master_control_range':[min(len(r['changed_prior_non_master_controls']) for r in rows),max(len(r['changed_prior_non_master_controls']) for r in rows)],
    'mono_priority':'Not implemented. Mode 0=8 voices, 1=16 voices, 2/3/4=unison x2/x4/x8; no mono claim.',
    'programs':rows}
write('structural-audit.json',structural)

# Cross-check each capture executable's actual inventory against authenticated data.
for label,build,expected,source in [('current7',CURRENT,now,SOURCE),('published4',PUBLISHED,four,OLD4)]:
    target=RAW/(label+'-inventory.json')
    run([build/'cs80_preset_capture','--inventory',target,'--library','all'])
    assert json.loads(target.read_text())==expected
    file_receipt(target,source,role='authenticated executable inventory')

compile_command=['c++','-std=c++20','-O2','-DNDEBUG','-DCS80_ENABLE_BORROWED_HELD=1','-I','src',
    OUT/'gesture_probe.cpp',CURRENT/'libcs80_core.a','-o',RAW/'gesture_probe']
gesture_command=[RAW/'gesture_probe',RAW/'current-gestures','all',SOURCE]
if not args.reuse_captures:
    run(compile_command,RAW/'compile.log')
    run(gesture_command,RAW/'gesture-probe.log')
else: commands.extend([[str(x) for x in compile_command],[str(x) for x in gesture_command]])
gesture_dir=RAW/'current-gestures'
case_rows=list(csv.DictReader((gesture_dir/'cases.csv').open()))
trace=list(csv.DictReader((gesture_dir/'trace.csv').open()))
assert len(case_rows)==12
audio={}; measured=[]
offs={'overlap':1.6,'ribbon':1.8,'glide':3.1,'vibrato':2.4,'pressure':2.5,'modern_release':1.6,'body_rate':2.0}
for row in case_rows:
    path=gesture_dir/(row['id']+'.f32'); a=np.fromfile(path,dtype='<f4').reshape(-1,2)
    rate=int(row['rate']); duration=float(row['duration']); off=offs[row['role']]
    assert row['source']==SOURCE and len(a)==int(row['frames'])
    audio[row['id']]=(a,rate)
    sample=lambda s,e:a[round(s*rate):round(e*rate)]
    before=rms(sample(off-.3,off-.1)); tail=rms(sample(duration-.3,duration-.05))
    finite=bool(np.isfinite(a).all()); peak=float(np.max(np.abs(a)))
    check(row['id']+': finite and below full scale',finite and peak<1)
    check(row['id']+': late phrase clears held level by at least 16 dB',db(tail/max(before,1e-12)) < -16)
    file_receipt(path,SOURCE,case=row['id'],rate=rate,frames=len(a),channels=2,format='little-endian float32 interleaved')
    measured.append({'id':row['id'],'preset':row['name'],'source':SOURCE,'role':row['role'],
        'sample_rate':rate,'duration_seconds':duration,'peak_dbfs':db(peak),'nonfinite':int((~np.isfinite(a)).sum()),
        'samples_over_full_scale':int((np.abs(a)>1).sum()),'late_to_pre_release_db':db(tail/max(before,1e-12)),
        'late_rms_dbfs':db(tail),'patch_overrides':[]})

def window(case,start,end):
    a,rate=audio[case]; return a[round(start*rate):round(end*rate)]
def pitch(case,start,duration,expected):
    a,rate=audio[case]; mono=a.mean(axis=1,dtype=np.float64)
    first=round(start*rate); count=round(duration*rate)
    lo=math.floor(rate/(1.19*expected)); hi=math.ceil(rate/(.83*expected))
    base=mono[first:first+count]; aa=float(base@base); corr={}
    for lag in range(lo-1,hi+2):
        b=mono[first+lag:first+lag+count]
        corr[lag]=float(base@b)/math.sqrt(max(aa*float(b@b),1e-24))
    best=max(range(lo,hi+1),key=corr.get)
    left,mid,right=corr[best-1],corr[best],corr[best+1]
    den=left-2*mid+right; delta=.5*(left-right)/den if abs(den)>1e-12 else 0
    return rate/(best+max(-.5,min(.5,delta)))
def snapshot(case,seconds):
    rate=audio[case][1]; frame=round(seconds*rate)
    return [r for r in trace if r['id']==case and int(r['frame'])==frame]
def held(rows): return sorted(int(r['note']) for r in rows if r['held']=='1')

gesture_details={}
for case in ['overlap_touch','overlap_original']:
    before=snapshot(case,.59); overlap=snapshot(case,.61); released=snapshot(case,1.16); off=snapshot(case,1.61)
    before60=next(r for r in before if r['note']=='60' and r['held']=='1')
    after60=next(r for r in overlap if r['note']=='60' and r['held']=='1')
    envelope_ratio=float(after60['amp0'])/float(before60['amp0'])
    check(case+': two held notes retain ownership; newer key-up leaves older key held',held(overlap)==[60,67] and held(released)==[60] and held(off)==[])
    check(case+': older gate stays on its circuit without an amplitude-envelope restart',before60['slot']==after60['slot'] and envelope_ratio>.8)
    gesture_details[case]={'held_after_overlap':held(overlap),'held_after_newer_key_up':held(released),
        'held_after_both_keys_up':held(off),'older_note_same_circuit':before60['slot']==after60['slot'],
        'older_amp_envelope_after_over_before':envelope_ratio,'interpretation':'polyphonic overlap; not monophonic priority or mono legato'}

pre=pitch('ribbon_return',.35,.1,261.625565); bent=pitch('ribbon_return',.85,.1,261.625565*1.35); returned=pitch('ribbon_return',1.5,.1,261.625565)
return_cents=1200*math.log2(returned/pre)
check('ribbon: audible travel ratio agrees with 1.35 and return is within 5 cents',abs(bent/pre-1.35)<.015 and abs(return_cents)<5)
gesture_details['ribbon_return']={'pre_hz':pre,'bent_hz':bent,'return_hz':returned,'travel_ratio':bent/pre,'return_cents':return_cents}
up_early=pitch('glide_both_directions',.75,.05,315); up_late=pitch('glide_both_directions',1.55,.08,392)
down_early=pitch('glide_both_directions',2.05,.05,240); down_late=pitch('glide_both_directions',2.9,.08,196)
check('glide: ascending and descending audio moves toward each target',up_early<.9*up_late and down_early>1.1*down_late and 380<up_late<400 and 190<down_late<205)
gesture_details['glide_both_directions']={'ascending_early_hz':up_early,'ascending_settled_hz':up_late,'descending_early_hz':down_early,'descending_settled_hz':down_late}
spans={}
for label,start,end in [('dry',.25,.5),('pressed',.85,1.5),('returned',1.95,2.3)]:
    freqs=[pitch('pressure_vibrato_return',float(at),.025,392) for at in np.arange(start,end,.035)]
    spans[label]=1200*math.log2(max(freqs)/min(freqs))
check('pressure vibrato: stable dry/returned pitch; controlled pressed span',spans['dry']<4 and spans['returned']<4 and 25<spans['pressed']<65)
gesture_details['pressure_vibrato_return']={'pitch_span_cents':spans,
    'pressed_minus_dry_rms_db':db(rms(window('pressure_vibrato_return',1.1,1.5))/rms(window('pressure_vibrato_return',.25,.5)))}
zero=rms(window('pressure_required_original',.2,.45)); light=rms(window('pressure_required_original',.8,1)); full=rms(window('pressure_required_original',1.35,1.7)); lifted=rms(window('pressure_required_original',2.1,2.4))
check('pressure-required original: initial silence, audible pressure and fading after pressure removal',zero<1e-6 and full>2e-4 and db(full/light)>7 and db(lifted/full)<-20)
gesture_details['pressure_required_original']={'zero_pressure_rms_dbfs':db(zero),'light_pressure_rms_dbfs':db(light),
    'full_pressure_rms_dbfs':db(full),'light_to_full_db':db(full/light),'pressure_removed_rms_dbfs':db(lifted),
    'removal_relative_to_full_db':db(lifted/full),'note':'Removal is a fade through the pressure/touch path, not an instantaneous mute.'}

# Two pressure-required presets are silent in the root's zero-pressure bank run.
# This is a matched .4/current full-patch comparison, not another bank sweep.
pressure_rows={}
for label,build,source in [('published4',PUBLISHED,OLD4),('current7',CURRENT,SOURCE)]:
    for pressure in ('0.30','1.00'):
        target=RAW/(label+'-pressure-'+pressure)
        cmd=[build/'cs80_preset_capture','--out',target,'--library','all','--preset','Electric Leads 136',
            '--preset','Pressure Lead','--rate','48000','--note','67','--velocity','0.55','--pressure',pressure,'--hold','1.8','--tail','0.6','--wav']
        if not args.reuse_captures: run(cmd,RAW/(target.name+'.log'))
        else: commands.append([str(x) for x in cmd])
        metrics=list(csv.DictReader((target/'metrics.csv').open()))
        assert {r['name'] for r in metrics}=={'Electric Leads 136','Pressure Lead'}
        for r in metrics:
            check(label+' '+r['name']+' pressure '+pressure+': finite and unclipped',int(r['nonfinite'])==0 and int(r['samples_over_full_scale'])==0 and float(r['peak_dbfs'])<0)
            pressure_rows[label,pressure,r['name']]=r
            wav=target/(r['index']+'-'+r['name'].replace(' ','-')+'.wav')
            file_receipt(wav,source,role='pressure-gap comparison',pressure=float(pressure),preset=r['name'])
        file_receipt(target/'metrics.csv',source,role='pressure-gap metrics',pressure=float(pressure))

def wav_audio(path):
    data=path.read_bytes(); assert data[:4]==b'RIFF' and data[8:12]==b'WAVE'
    at=12; fmt=pcm=None
    while at+8<=len(data):
        name=data[at:at+4]; size=struct.unpack_from('<I',data,at+4)[0]; value=data[at+8:at+8+size]
        if name==b'fmt ': fmt=struct.unpack_from('<HHIIHH',value)
        if name==b'data': pcm=value
        at+=8+size+(size&1)
    assert fmt[0]==3 and fmt[1]==2 and fmt[2]==48000 and fmt[5]==32
    return np.frombuffer(pcm,dtype='<f4').astype(np.float64)

comparisons=[]
for name,index in [('Electric Leads 136',179),('Pressure Lead',444)]:
    for pressure in ('0.30','1.00'):
        b=pressure_rows['published4',pressure,name]; a=pressure_rows['current7',pressure,name]
        filename=str(index)+'-'+name.replace(' ','-')+'.wav'
        old=wav_audio(RAW/('published4-pressure-'+pressure)/filename)
        new=wav_audio(RAW/('current7-pressure-'+pressure)/filename)
        assert old.shape==new.shape
        difference=new-old
        check(name+' pressure '+pressure+': published/current waveforms differ',bool(np.any(difference)))
        comparisons.append({'preset':name,'host_index':index,'pressure':float(pressure),'note':67,'velocity':.55,'sample_rate':48000,
            'published_source':OLD4,'current_source':SOURCE,'published_metrics':b,'current_metrics':a,
            'current_minus_published_peak_db':float(a['peak_dbfs'])-float(b['peak_dbfs']),
            'current_minus_published_body_rms_db':float(a['body_rms_dbfs'])-float(b['body_rms_dbfs']),
            'waveform_difference_rms_dbfs':db(rms(difference)),
            'waveform_difference_relative_to_published_rms_db':db(rms(difference)/rms(old)),
            'max_abs_sample_difference':float(np.max(np.abs(difference)))})
write('published-pressure-comparison.json',{'scope':'Only the two pressure-required fixed-setting bank exceptions; nonzero differences do not establish perceived improvement.','captures':8,'audio_seconds':19.2,'comparisons':comparisons})

# Reuse prior and focused evidence with their original source tags.
focused_path=ROOT/'docs/preset-evidence/2026.11.7/development/focused-ctest-receipt.json'
focused=json.loads(focused_path.read_text()); assert focused['source']==SOURCE and focused['passed']==31 and focused['failed']==0
for name,sha in focused['files'].items(): assert digest(focused_path.parent/name)==sha
prior_path=ROOT/'docs/preset-evidence/2026.11.6/verification/a27/summary.json'
prior=json.loads(prior_path.read_text())
sources={}
for name in ['src/core/Engine.h','src/core/Engine.cpp','src/core/InstrumentBody.h','src/core/Params.h',
    'src/core/FactoryPresets.h','src/core/FactoryPresets.cpp','src/core/OriginalCircuitLibrary.inc',
    'src/core/curated/Leads.inc','src/core/curated/Modern.inc','src/core/VoiceModes.h',
    'src/plugin/KeyboardTuningPage.cpp','tests/LeadRefinementTests.cpp','tests/CoreTests.cpp','tests/TuningMidiTests.cpp']:
    assert (ROOT/name).read_bytes()==blob(SOURCE,name)
    sources[name]=digest(ROOT/name)
for path,source in [(CURRENT/'cs80_preset_capture',SOURCE),(CURRENT/'libcs80_core.a',SOURCE),
    (PUBLISHED/'cs80_preset_capture',OLD4),(RAW/'gesture_probe',SOURCE),
    (gesture_dir/'cases.csv',SOURCE),(gesture_dir/'trace.csv',SOURCE)]: file_receipt(path,source)
write('pcm-manifest.json',{'source':SOURCE,'scope':'Only exact newly generated A23 paths; raw files remain in the session scratch directory.','files':manifest})
summary={'source':SOURCE,'scope':'Full structural coverage of 56 Leads, 12 fresh gesture captures of 10 presets, plus 8 comparative pressure captures of 2 presets (11 unique current presets total).',
    'all_checks_passed':all(x['passed'] for x in checks),'check_count':len(checks),'checks':checks,
    'gesture_captures':12,'gesture_audio_seconds':sum(float(r['duration']) for r in case_rows),
    'gesture_unique_presets':len({r['name'] for r in case_rows}),
    'gesture_maximum_peak_dbfs':max(r['peak_dbfs'] for r in measured),
    'pressure_comparison_captures':8,'total_new_audio_seconds':sum(float(r['duration']) for r in case_rows)+19.2,
    'source_files_sha256':sources,'probe_source_sha256':digest(OUT/'gesture_probe.cpp'),
    'audit_source_sha256':digest(OUT/'audit.py'),'measurements':measured,'gestures':gesture_details,
    'reused_focused_ctest':{'source':SOURCE,'receipt_sha256':digest(focused_path),'passed':31,'failed':0,'lead_test':'cs80_lead_refinement_tests: passed; not rerun by A23'},
    'reused_6_lead_review':{'source':prior['source_commit'],'summary_sha256':digest(prior_path),'structural_programs':56,
        'existing_suite_checks':234,'complementary_captures':55,'scope':'Original .6 A27 evidence; not represented as new .7 captures.'},
    'commands':commands,'no_listening_or_native_host_claim':True}
write('summary.json',summary)
write('commands.json',{'source':SOURCE,'commands':commands,'reuse_captures':args.reuse_captures})
print(json.dumps({'source':SOURCE,'published_comparator_source':OLD4,
    'passed':summary['all_checks_passed'],'checks':len(checks),'structural_programs':56,
    'gesture_captures':12,'pressure_captures':8,'new_audio_seconds':summary['total_new_audio_seconds'],
    'maximum_gesture_peak_dbfs':summary['gesture_maximum_peak_dbfs'],'gestures':gesture_details,
    'pressure_comparisons':[{k:r[k] for k in ['preset','pressure','current_minus_published_peak_db','current_minus_published_body_rms_db','waveform_difference_relative_to_published_rms_db']} for r in comparisons]},indent=2))
if not summary['all_checks_passed']: raise SystemExit(1)
