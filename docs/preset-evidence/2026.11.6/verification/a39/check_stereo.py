#!/usr/bin/env python3
"""A39 full-preset stereo-to-mid review; not a perceptual or alias test."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import numpy as np

RATE = 48000
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
MATERIALS = ['Tubular Bells', 'Gong', 'Temple Bell', 'Agogo', 'Music Box',
             'Harp', 'Koto', 'Banjo', 'Kalimba']
KEYS = ['Analog Piano', 'Stage Keys', 'Electric Piano I', 'Electric Piano II', 'Reed Piano']
WINDOWS = {'onset': (.025, .35), 'held': (.6, 1.2), 'release': (1.55, 2.1)}

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def db(x):
    return float(20 * np.log10(max(float(x), 1e-30)))

def wav(path):
    raw = path.read_bytes(); at = 12; fmt = None; audio = None
    assert raw[:4] == b'RIFF' and raw[8:12] == b'WAVE'
    while at + 8 <= len(raw):
        tag, size = struct.unpack_from('<4sI', raw, at); start = at + 8
        if tag == b'fmt ': fmt = struct.unpack_from('<HHIIHH', raw, start)
        if tag == b'data': audio = np.frombuffer(raw[start:start+size], dtype='<f4').astype(np.float64)
        at = start + size + (size & 1)
    assert fmt and fmt[0] == 3 and fmt[1] == 2 and fmt[2] == RATE and fmt[5] == 32
    assert audio is not None and len(audio) == int(2.2 * RATE) * 2
    return audio.reshape(-1, 2)

def window_metrics(audio, first, last):
    a = audio[round(first*RATE):round(last*RATE)]
    powers = np.mean(a*a, axis=0); stereo = float(np.mean(powers))
    if stereo < 1e-10:
        return {'qualified_energy': False, 'stereo_rms_dbfs': db(np.sqrt(stereo))}
    mid = a.mean(axis=1); side = .5*(a[:, 0]-a[:, 1])
    mp = float(np.mean(mid*mid)); sp = float(np.mean(side*side))
    hann = np.hanning(len(a))
    bins = np.fft.rfftfreq(len(a), 1/RATE)
    f = np.fft.rfft(a*hann[:, None], axis=0)
    stereo_spectrum = .5*(np.abs(f[:, 0])**2 + np.abs(f[:, 1])**2)
    mid_spectrum = np.abs(.5*(f[:, 0]+f[:, 1]))**2
    valid = (bins >= 30) & (bins <= 16000)
    idx = np.flatnonzero(valid)[np.argmax(stereo_spectrum[valid])]
    dominant = np.abs(bins-bins[idx]) <= max(3*RATE/len(a), .04*bins[idx])
    total = float(stereo_spectrum.sum())
    upper = (bins >= 1000) & (bins <= 16000)
    def band(mask):
        st = float(stereo_spectrum[mask].sum()); mo = float(mid_spectrum[mask].sum())
        relative = 10*np.log10(max(st/total, 1e-30))
        return {'relative_stereo_power_db': float(relative),
                'qualified_energy': bool(relative >= -45),
                'mono_loss_db': float(10*np.log10(st/max(mo, 1e-30))) if relative >= -45 else None}
    return {'qualified_energy': True, 'stereo_rms_dbfs': db(np.sqrt(stereo)),
            'mono_loss_db': float(10*np.log10(stereo/max(mp, 1e-30))),
            'left_right_balance_db': float(10*np.log10(powers[0]/max(powers[1], 1e-30))),
            'uncentered_channel_correlation': float(np.mean(a[:, 0]*a[:, 1])/np.sqrt(powers.prod())),
            'side_mid_ratio_db': float(10*np.log10(max(sp, 1e-30)/max(mp, 1e-30))),
            'dominant_hz': float(bins[idx]), 'dominant_band': band(dominant), 'upper_band': band(upper)}

def main():
    p = argparse.ArgumentParser(); p.add_argument('--repo', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True); p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--audio', type=Path, required=True); p.add_argument('--out', type=Path, required=True)
    args = p.parse_args(); args.out.mkdir(parents=True, exist_ok=True)
    exes = {'candidate': args.candidate.resolve(), 'baseline': args.baseline.resolve()}
    before = {k: sha(v) for k,v in exes.items()}
    sources = ['src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/Params.h',
               'src/core/InstrumentBody.h', 'src/core/InstrumentBodyMaterials.h',
               'src/core/FactoryPresets.cpp', 'src/core/CuratedLibrary.inc',
               'src/core/OriginalCircuitLibrary.inc', 'src/core/curated/Bells.inc',
               'src/core/curated/Plucked.inc', 'src/core/curated/Keys.inc']
    hashes = {}
    for name in sources:
        raw = (args.repo/name).read_bytes()
        assert raw == subprocess.check_output(['git','-C',str(args.repo),'show',SOURCE+':'+name])
        hashes[name] = hashlib.sha256(raw).hexdigest()
    jobs = [('candidate',n,v,MATERIALS+KEYS) for n in (48,60,72) for v in (.25,.85)]
    jobs += [('baseline',n,.85,['Analog Piano','Stage Keys']) for n in (48,60,72)]
    def capture(job):
        label,note,velocity,names = job
        dest = args.audio/label/f'n{note}-v{round(velocity*100)}'
        cmd = [str(exes[label]),'--out',str(dest),'--wav','--library','all','--rate',str(RATE),
               '--note',str(note),'--velocity',str(velocity),'--pressure','0','--hold','1.4','--tail','.8']
        for name in names: cmd += ['--preset',name]
        run = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=True)
        (args.out/f'{label}-n{note}-v{round(velocity*100)}.log').write_text(run.stdout)
        rows = []
        for name in names:
            files = list(dest.glob('*-'+name.replace(' ','-')+'.wav')); assert len(files)==1,(name,files)
            a = wav(files[0]); assert np.isfinite(a).all() and np.abs(a).max()<1
            rows.append({'version':label,'name':name,'note':note,'velocity':velocity,
                         'wav_sha256':sha(files[0]),'peak_dbfs':db(np.abs(a).max()),
                         'windows':{k:window_metrics(a,*limits) for k,limits in WINDOWS.items()}})
        print(label,note,velocity,'captures',len(rows),flush=True)
        return rows
    with ThreadPoolExecutor(max_workers=2) as pool: rows = sum(pool.map(capture,jobs),[])
    assert before == {k:sha(v) for k,v in exes.items()}
    current=[x for x in rows if x['version']=='candidate']
    summary={}
    for name in MATERIALS+KEYS:
        values=[(x,k,m) for x in current if x['name']==name for k,m in x['windows'].items() if m['qualified_energy']]
        summary[name]={'captures':sum(x['name']==name for x in current),
                       'max_mono_loss_db':max(m['mono_loss_db'] for _,_,m in values),
                       'max_absolute_balance_db':max(abs(m['left_right_balance_db']) for _,_,m in values),
                       'max_dominant_band_loss_db':max(m['dominant_band']['mono_loss_db'] for _,_,m in values),
                       'max_upper_band_loss_db':max(m['upper_band']['mono_loss_db'] for _,_,m in values if m['upper_band']['qualified_energy'])}
    piano=[]
    for old in rows:
        if old['version']!='baseline': continue
        new=next(x for x in current if all(x[k]==old[k] for k in ('name','note','velocity')))
        for key in WINDOWS:
            if old['windows'][key]['qualified_energy'] and new['windows'][key]['qualified_energy']:
                piano.append({'name':old['name'],'note':old['note'],'window':key,
                              'baseline_mono_loss_db':old['windows'][key]['mono_loss_db'],
                              'candidate_mono_loss_db':new['windows'][key]['mono_loss_db']})
    result={'source_commit':SOURCE,'baseline_commit':'c76ad288e02fbeca48f39652bf4fcdd99837f793',
            'settings':{'sample_rate':RATE,'hold_seconds':1.4,'tail_seconds':.8,'pressure':0,
                        'notes':[48,60,72],'velocities':[.25,.85]},
            'qualification':{'minimum_window_rms_dbfs':-100,'minimum_band_relative_energy_db':-45,
                             'meaning':'These are analysis inclusion floors, not changed production gates.'},
            'captures':len(rows),'candidate_captures':len(current),'generated_seconds':2.2*len(rows),
            'binary_sha256':before,'candidate_source_sha256':hashes,'summary':summary,
            'piano_baseline_comparison':piano,'measurements':rows,
            'scope':'Complete-patch mono fold-down at (L+R)/2; no acoustic, perceptual or alias-equivalence claim.'}
    (args.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(summary,indent=2))

if __name__ == '__main__': main()
