"""Read-only canonical PCM interpretation. Never launches a renderer or benchmark."""
from pathlib import Path
import hashlib,json,sys
import numpy as np
ROOT=Path(sys.argv[1]);OUT=Path(sys.argv[2]);N=2880000;RATE=48000
sha=lambda data:hashlib.sha256(data).hexdigest()
refs={};hashes={};receipts=[]
for path in sorted(ROOT.glob('run-[0-9][0-9][0-9]')):
    r=json.loads((path/'result.json').read_text());ref=json.loads((path/'audio-reference.json').read_text())
    rel=Path(ref['relative_path']);assert not rel.is_absolute() and '..' not in rel.parts
    canonical=ROOT/rel;assert not canonical.is_symlink() and canonical.is_file()
    assert str(rel)=='canonical-pcm/'+ref['sha256']+'.f32'
    if str(rel) not in hashes:
        b=canonical.read_bytes();assert len(b)==N*2*4==ref['bytes'];assert sha(b)==ref['sha256'];hashes[str(rel)]=sha(b)
    assert r['rate']==RATE and r['block']==256 and r['frames']==N
    arm=r['arm'];row=r['row'];assert r['latency']==(8 if arm=='draft' else 67)
    key=(arm,row);assert key not in refs or refs[key]==str(rel);refs[key]=str(rel)
    receipts.append({'run':path.name,'arm':arm,'row':row,'pcm_sha256':ref['sha256']})
assert len(receipts)==108
for row in range(6):assert refs['stock',row]==refs['off',row]
def basic(a):
    return {'finite_count':int(np.isfinite(a).sum()),'sample_count':int(a.size),'rms':np.sqrt(np.mean(a*a,axis=0)).tolist(),'peak':np.max(np.abs(a),axis=0).tolist(),'dc':a.mean(axis=0).tolist(),'max_adjacent_delta':np.max(np.abs(np.diff(a,axis=0)),axis=0).tolist() if len(a)>1 else [0.,0.]}
def compare(a,b):
    # Each column stays a distinct channel. No mono fold, gain fit, phase search or EQ.
    diff=b-a;den=np.sqrt(np.mean(a*a,axis=0));num=np.sqrt(np.mean(diff*diff,axis=0))
    relative=[float(n/d) if d else None for n,d in zip(num,den)]
    rms_ratio=[float(x/d) if d else None for x,d in zip(np.sqrt(np.mean(b*b,axis=0)),den)]
    return {'stock':basic(a),'draft':basic(b),'difference_rms_per_channel':num.tolist(),'difference_relative_rms_per_channel':relative,'draft_stock_rms_ratio_per_channel':rms_ratio,'stereo_difference_relative_rms':float(np.sqrt(np.mean(diff*diff))/np.sqrt(np.mean(a*a))) if np.any(a) else None}
rows=[]
for row in range(6):
    full={arm:np.fromfile(ROOT/refs[arm,row],dtype='<f4').reshape(N,2).astype(np.float64) for arm in ('stock','draft')}
    assert all(np.isfinite(a).all() for a in full.values())
    common=N-67;a=full['stock'][67:67+common];b=full['draft'][8:8+common]
    r={'row':row,'selector':[0,4,7,10,4,4][row],'raw':{arm:basic(x) for arm,x in full.items()},'common_logical_frames':common,'comparison':compare(a,b),'segments':[],'event_windows':[]}
    windows=[]
    if row<4:windows=[('held_all',0,N)]
    elif row==4:windows=[(f'pressure_cycle_{i}',i*192000,(i+1)*192000) for i in range(15)]
    else:
        for i in range(10):
            start=i*288000
            windows.extend([(f'held_{i}',start,start+96000),(f'release_{i}',start+96000,start+288000),(f'late_release_{i}',start+240000,start+288000)])
        for event in [i*288000 for i in range(10)]+[i*288000+96000 for i in range(10)]:
            lo=max(0,event-256);hi=min(common,event+256)
            r['event_windows'].append({'event_frame':event,'logical_start':lo,'logical_end':hi,'comparison':compare(a[lo:hi],b[lo:hi])})
    for name,lo,hi in windows:
        end=min(common,hi);r['segments'].append({'name':name,'logical_start':lo,'logical_end':end,'omitted_end_frames':hi-end,'comparison':compare(a[lo:end],b[lo:end])})
    rows.append(r)
report={'scope':'Read-only interpretation of frozen native-study PCM; no synthesis or timing execution','verified_timed_receipts':len(receipts),'unique_pcm_files':len(hashes),'stock_off_exact_all_six':True,'alignment':'stock[t+67] vs draft[t+8], integer nominal latency only, no fitting; common logical frames 0..2879932','limitations':['Waveform differences are not THD, alias energy or perceptual acceptance.','Nominal latency alignment does not remove frequency-dependent phase or rate-dependent analog-model changes.','Eight-voice source-factory subset at48k only; no hardware truth, fullbank, hostPDC or plugin validation.','Fixed tails analyzed through every predetermined endpoint; final window loses67frames only for common alignment.'],'canonical_sha256':hashes,'receipts':receipts,'rows':rows}
OUT.mkdir(exist_ok=True);(OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n')
for r in rows:print(r['row'],r['selector'],'relative difference',r['comparison']['difference_relative_rms_per_channel'],'RMS ratio',r['comparison']['draft_stock_rms_ratio_per_channel'],'rawpeak',r['raw']['stock']['peak'],r['raw']['draft']['peak'])
print('release final1s',[(x['name'],x['comparison']['stock']['rms'],x['comparison']['draft']['rms']) for x in rows[5]['segments'] if x['name'].startswith('late_')])
