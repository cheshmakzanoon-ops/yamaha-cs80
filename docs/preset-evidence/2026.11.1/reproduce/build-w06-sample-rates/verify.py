from pathlib import Path
import concurrent.futures, csv, hashlib, json, shutil, subprocess
import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parent.parent
OUT = Path(__file__).resolve().parent
CAP = OUT / 'capture-snapshot'
INV = OUT / 'inventory.json'
if not CAP.exists(): shutil.copy2(ROOT / 'build-audit/cs80_preset_capture', CAP)
if not INV.exists(): shutil.copy2(ROOT / 'build-audit/inventory-final.json', INV)
inventory = json.loads(INV.read_text())
presets = {p['name']: p for p in inventory['programs']}
mix_index = inventory['parameter_ids'].index('mix')
cases = [('Flute',67,.75),('Breathy Flute',67,.75),('Harp',60,1.2),('Kalimba',60,1.2),('Sine Bass',48,1.2),('Upright Pluck',48,1.2)]
rates = [44100,48000,96000]
commands = []
for name,note,hold in cases:
    for rate in rates:
        for rank in [0,1,2]:
            folder = OUT / 'captures' / f'{name.replace(" ","-")}-{rate}-r{rank}'
            cmd = [str(CAP),'--out',str(folder),'--preset',name,'--wav','--rate',str(rate),'--note',str(note),'--velocity','.8','--pressure','0','--hold',str(hold),'--tail','0']
            if rank: cmd += ['--rank',str(rank)]
            commands.append(cmd)

def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if p.returncode: raise RuntimeError(p.stdout+p.stderr)
    return p.stdout
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    logs = list(pool.map(run, commands))
(OUT / 'capture.log').write_text(''.join(logs))

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def rms(x): return float(np.sqrt(np.mean(x*x)))
def db(x): return float(20*np.log10(max(float(x),1e-24)))
def read(name,rate,rank):
    folder=OUT/'captures'/f'{name.replace(" ","-")}-{rate}-r{rank}'
    path=next(folder.glob('*.wav'))
    sr,data=wavfile.read(path)
    assert sr == rate and data.ndim == 2 and data.shape[1] == 2
    data=data.astype(np.float64)
    return data, {'file':str(path.relative_to(OUT)), 'sha256':sha(path), 'frames':len(data), 'finite':bool(np.isfinite(data).all()), 'peak_dbfs':db(np.max(np.abs(data))), 'over_full_scale_samples':int(np.sum(np.abs(data)>1))}
rows=[]; checks=[]
def check(test,condition): checks.append({'test':test,'pass':bool(condition)})
for name,note,hold in cases:
    mix=np.float32(presets[name]['values'][mix_index]); weights=[float(np.float32(1)-mix),float(mix)]
    for rate in rates:
        readout=[read(name,rate,r) for r in [0,1,2]]
        full,body,detail=[item[0] for item in readout]
        body*=weights[0]; detail*=weights[1]
        residual=full-body-detail
        windows=[(0.,hold),(.3,.5),(.5,.7)] if name in ['Flute','Breathy Flute'] else [(0.,hold),(.004,.020),(.8,1.1)]
        metrics=[]
        for start,end in windows:
            sl=slice(round(start*rate),round(end*rate))
            b,d,f,e=[rms(a[sl]) for a in [body,detail,full,residual]]
            metrics.append({'start_seconds':start,'end_seconds':end,'body_rms':b,'body_dbfs':db(b),'detail_rms':d,'detail_body_ratio':d/max(b,1e-24),'detail_body_db':db(d/max(b,1e-24)),'full_dbfs':db(f),'reconstruction_error_relative':e/max(f,1e-24),'reconstruction_error_db':db(e/max(f,1e-24))})
            check(f'{name}@{rate}: reconstruction {start}-{end}s within1%',e/max(f,1e-24)<.01)
        for a,m in readout: check(f'{name}@{rate}: {m["file"]} finite',m['finite'])
        check(f'{name}@{rate}: full remains belowfullscale',readout[0][1]['over_full_scale_samples']==0)
        onset,late=metrics[1:]
        check(f'{name}@{rate}: body remains',onset['body_rms']>.001 and late['body_rms']>.001)
        if name in ['Flute','Breathy Flute']:
            low,high=(.025,.16) if name=='Flute' else (.055,.25)
            check(f'{name}@{rate}: supporting sustainedair',low<onset['detail_body_ratio']<high)
            check(f'{name}@{rate}: air sustained',late['detail_rms']>.5*onset['detail_rms'])
        else:
            low=.08 if name in ['Harp','Kalimba'] else .05
            check(f'{name}@{rate}: supporting strike',low<onset['detail_body_ratio']<.5)
            check(f'{name}@{rate}: strike decaysbeforebody',late['detail_body_ratio']<.25*onset['detail_body_ratio'])
        rows.append({'name':name,'note':note,'sample_rate':rate,'velocity':.8,'pressure':0,'hold_seconds':hold,'tail_seconds':0,'weights':weights,'captures':[m for a,m in readout],'windows':metrics})
for rate in rates:
    f=next(r for r in rows if r['name']=='Flute' and r['sample_rate']==rate)['windows'][1]
    b=next(r for r in rows if r['name']=='Breathy Flute' and r['sample_rate']==rate)['windows'][1]
    check(f'wind roleordering@{rate}',b['detail_body_ratio']>1.5*f['detail_body_ratio'])
variations=[]
for name,note,hold in cases:
    group=[r for r in rows if r['name']==name]
    role=[r['windows'][1]['detail_body_db'] for r in group]
    late=[r['windows'][2]['detail_body_db'] for r in group]
    variations.append({'name':name,'role_window_seconds':[group[0]['windows'][1]['start_seconds'],group[0]['windows'][1]['end_seconds']],'role_db_by_rate':dict(zip(map(str,rates),role)),'max_role_span_db':max(role)-min(role),'late_db_by_rate':dict(zip(map(str,rates),late))})
summary={'assignment':30,'status':'PASS' if all(c['pass'] for c in checks) else 'FAIL','library_revision':inventory['library_revision'],'sample_rates':rates,'case_count':len(rows),'capture_count':len(commands),'generated_seconds':sum(c[2] for c in cases)*len(rates)*3,'checks_passed':sum(c['pass'] for c in checks),'checks_total':len(checks),'full_capture_peak_dbfs':max(r['captures'][0]['peak_dbfs'] for r in rows),'nonfinite_captures':sum(not c['finite'] for r in rows for c in r['captures']),'over_full_scale_samples_all_ranks':sum(c['over_full_scale_samples'] for r in rows for c in r['captures']),'worst_reconstruction_relative_db':max(w['reconstruction_error_db'] for r in rows for w in r['windows']),'role_comparisons':variations,'hashes':{'capture_binary':sha(CAP),'inventory':sha(INV),'script':sha(Path(__file__))},'conditions':'Final calibrated actual factory values; one held note; velocity0.8, pressure0, no release tail. Rank isolation changes only actual mix control, with float32-weighted reconstruction. All outputs unnormalized stereo IEEEfloat. Core role thresholds match the existing character suite, with velocity0.8 for this matrix rather than its strike velocity0.9.','scope':'Tests air/strike roles at representative notes across rates, not arbitrary notes/polyphony or perceptual identity. Sample-rate waveforms are not expected to be bit-identical; noise and filter responses may differ.'}
(OUT/'measurements.json').write_text(json.dumps({'summary':summary,'rows':rows,'checks':checks,'commands':commands},indent=2)+'\n')
(OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
for c in checks:
    if not c['pass']:print('FAIL',c['test'])
if not all(c['pass'] for c in checks):raise SystemExit(1)
