#!/usr/bin/env python3
"""Analyze A44's bounded actual-engine traces; no perceptual-quality score."""
import csv, hashlib, json, math, subprocess, sys
from pathlib import Path
import numpy as np
from scipy import signal

SOURCE = '6094bce18c8b19da804c2fbc9a00e31969a7f579'
RATE = 48000
BASE = Path('docs/preset-evidence/2026.11.5/verification')
ROOT = Path('/tmp/cs80-a44/renders')
checks = []
def check(ok, label):
    checks.append({'passed': bool(ok), 'check': label})

def db(x):
    return float(20*np.log10(max(float(x),1e-30)))

def rms(x):
    return float(np.sqrt(np.mean(np.asarray(x,dtype=np.float64)**2)))

def segment(a,start,end):
    return a[round(start*RATE):round(end*RATE)]

def window_rms(a,start,end,seconds):
    n=round(seconds*RATE); x=segment(a,start,end)
    x=x[:len(x)//n*n].reshape(-1,n,2).astype(np.float64)
    return np.sqrt(np.mean(x*x,axis=(1,2)))

def motion(a,b,start=12,end=36,step=.025):
    return 20*np.log10(np.maximum(window_rms(a,start,end,step),1e-30)/np.maximum(window_rms(b,start,end,step),1e-30))

def lag_metrics(x,seconds,step=.025):
    lag=seconds/step
    k=np.arange(math.ceil(lag),len(x),dtype=float)
    earlier=np.interp(k-lag,np.arange(len(x)),x)
    later=x[k.astype(int)]
    return {'lag_seconds':seconds,'lagged_difference_rms_db':rms(later-earlier),'correlation':float(np.corrcoef(later,earlier)[0,1])}

inventory_raw=subprocess.check_output(['git','show',SOURCE+':docs/preset-evidence/2026.11.5/inventory.json'])
inventory=json.loads(inventory_raw)
ids=inventory['parameter_ids']; programs={p['name']:p for p in inventory['programs']}
rows=list(csv.DictReader((ROOT/'summary.csv').open()))
settings={r['case']:r for r in csv.DictReader((ROOT/'settings.csv').open())}
check(len(rows)==13 and len(settings)==13,'Thirteen complete diagnostic cases and settings')
audio={}; records=[]
for r in rows:
    name=r['case']; p=programs[r['name']]
    expected=np.array(p['values'],dtype='<f4')
    if (name.endswith('-flat') and name!='rain-fine-flat') or name=='wind-periodic-control':
        for i in (1,2,3):expected[ids.index(f'lfo{i}_depth')]=0
        if name=='wind-periodic-control':
            expected[ids.index('lfo2_depth')]=p['values'][ids.index('lfo2_depth')]
            expected[ids.index('lfo2_wave')]=1
            expected[ids.index('lfo2_rate')]=.7
    if name=='rain-fine-flat':expected[ids.index('lfo1_depth')]=0
    actual=np.array([float(settings[name][k]) for k in ids],dtype='<f4')
    check(expected.tobytes()==actual.tobytes(),name+': exact factory vector or explicitly scoped causal-control vector')
    check(int(r['host_index'])==p['host_index'],name+': existing host identity')
    path=ROOT/(name+'.f32'); raw=path.read_bytes();a=np.frombuffer(raw,dtype='<f4').reshape(-1,2);audio[name]=a
    check(len(a)==int(r['frames'])==int(r['seconds'])*RATE,name+': exact frame count')
    check(bool(np.isfinite(a).all()) and int(r['nonfinite'])==0,name+': all samples finite')
    peak=float(np.max(np.abs(a)));value=rms(a)
    check(peak<1 and int(r['over_full_scale'])==0,name+': all samples below full scale')
    check(abs(db(peak)-db(float(r['peak'])))<1e-7 and abs(db(value)-db(float(r['rms'])))<1e-7,name+': independent PCM peak/RMS match probe')
    records.append({'case':name,'program':p['name'],'host_index':p['host_index'],'seconds':int(r['seconds']),'frames':len(a),'pressure':float(r['pressure']),'peak_dbfs':db(peak),'rms_dbfs':db(value),'active_at_end':int(r['active_at_end']),'pcm_sha256':hashlib.sha256(raw).hexdigest(),'pcm_bytes':len(raw),'trace_sha256':hashlib.sha256((ROOT/(name+'-trace.csv')).read_bytes()).hexdigest(),'parameter_vector_sha256':hashlib.sha256(actual.tobytes()).hexdigest()})

weather={}
for prefix in ('wind','rain'):
    gain=motion(audio[prefix+'-moving'],audio[prefix+'-flat'])
    weather[prefix]={'window_seconds':.025,'start_seconds':12,'end_seconds':36,'window_level_difference_std_db':float(np.std(gain)),'range_db':[float(gain.min()),float(gain.max())],'nominal_07_hz_lag':lag_metrics(gain,1/.7)}
    check(float(np.std(gain))>.25,prefix+': long-window authored modulation remains observable')
    a=segment(audio[prefix+'-moving'],12,36).astype(np.float64)
    weather[prefix]['stereo']={'correlation':float(np.corrcoef(a.T)[0,1]),'side_to_mid_db':db(rms((a[:,0]-a[:,1])/2)/rms((a[:,0]+a[:,1])/2))}
    windows=BASE/('a44-'+prefix+'-long-motion.csv')
    with windows.open('w') as f:
        f.write('start_seconds,factory_minus_flat_window_rms_db\n')
        for i,v in enumerate(gain):f.write(f'{12+i*.025:.6f},{v:.12f}\n')
control=motion(audio['wind-periodic-control'],audio['wind-flat'])
weather['wind']['periodic_positive_control']={'description':'Only modern LFO2 amplitude is enabled, retaining factory depth but using a nominal 0.7 Hz triangle; all other modern LFO depths are zero.','window_level_difference_std_db':float(np.std(control)),**lag_metrics(control,1/.7)}
wind_error=weather['wind']['nominal_07_hz_lag']['lagged_difference_rms_db']
control_error=weather['wind']['periodic_positive_control']['lagged_difference_rms_db']
check(wind_error>.25 and wind_error>10*control_error,'Wind: former-period mismatch remains much larger than an intentionally periodic whole-patch control')
fine=motion(audio['rain-moving'],audio['rain-fine-flat'],.2,36,.002)
weather['rain']['fine_continuity']={'scope':'Only LFO1 depth is removed in the control; slower filter/amplitude trajectories remain identical.','start_seconds':.2,'end_seconds':36,'window_seconds':.002,'window_level_difference_std_db':float(np.std(fine)),'largest_adjacent_window_change_db':float(np.max(np.abs(np.diff(fine)))),'p99_adjacent_window_change_db':float(np.percentile(np.abs(np.diff(fine)),99))}
check(weather['rain']['fine_continuity']['largest_adjacent_window_change_db']<.10,'Rain: fine random modulation stays continuous over the extended sampled trajectory')

shots={}
for prefix in ('surf','thunder'):
    a=audio[prefix+'-repeated']; episodes=[]
    # Same broad split as the production FX suite, but filter all 40 seconds
    # continuously so the second note is not given a fresh analysis filter.
    k=1-np.exp(-2*np.pi*1500/RATE)
    low=signal.lfilter([k],[1,-(1-k)],a.astype(np.float64),axis=0);high=a-low
    for start in (0,16):
        onset=window_rms(a,start,start+5,.05)
        crest=float(onset.max()); crest_at=float(np.argmax(onset)*.05)
        tail=rms(segment(a,start+8.5,start+11.5))
        x=segment(a,start+.002,start+.055); y=segment(high,start+.002,start+.055)
        x2=segment(a,start+.4,start+.9);y2=segment(high,start+.4,start+.9)
        ep={'onset_seconds':start,'crest_window_start_seconds':crest_at,'crest_window_rms_dbfs':db(crest),'late_held_relative_to_crest_db':db(tail/crest),'crack_upper_energy_share':float(np.sum(y*y)/np.sum(x.astype(np.float64)**2)),'roll_upper_energy_share':float(np.sum(y2*y2)/np.sum(x2.astype(np.float64)**2))}
        episodes.append(ep)
        check(ep['late_held_relative_to_crest_db']<-12,prefix+f': note at {start} s decays during the held key instead of repeating')
        if prefix=='surf':check(.1<crest_at<4,prefix+f': note at {start} s retains a delayed crest')
        else:check(ep['crack_upper_energy_share']>5*ep['roll_upper_energy_share'],prefix+f': note at {start} s restores crack-to-rumble spectral contrast')
    check(next(r for r in records if r['case']==prefix+'-repeated')['active_at_end']==0,prefix+': ordinary final key release retires all voices')
    shots[prefix]={'note_on_seconds':[0,16],'note_off_seconds':[12,28],'seconds':40,'engine_reset_between_notes':False,'episodes':episodes}

mechanical={}
for prefix in ('helicopter','siren'):
    target={}
    for label in ('soft-pressure','full-pressure'):
        name=prefix+'-'+label
        trace=np.genfromtxt(ROOT/(name+'-trace.csv'),delimiter=',',names=True)
        t=trace['frame']/RATE;v=trace['sub_value'];index=np.flatnonzero((v[1:]>=0)&(v[:-1]<0))+1
        crossings=t[index];crossings=crossings[crossings>=2];periods=np.diff(crossings)
        result={'pressure':0 if label=='soft-pressure' else 1,'positive_crossings_after_2_seconds':len(crossings),'positive_crossing_times_seconds':crossings.tolist(),'measured_sub_cycle_hz':float(1/np.mean(periods)),'cycle_period_cv':float(np.std(periods)/np.mean(periods)),'final_rate_readout_hz':float(trace['sub_rate'][-1]),'trace_sampling_seconds':256/RATE}
        if prefix=='helicopter':
            a=audio[name].astype(np.float64); power=np.mean(a*a,axis=1)
            sos=signal.butter(4,30,fs=RATE,output='sos'); smooth=signal.sosfilt(sos,power)
            envelope=smooth[2*RATE::240]; envelope=envelope-np.mean(envelope)
            env_times=2+np.arange(len(envelope))/200
            hz=result['measured_sub_cycle_hz'];design=np.column_stack([np.sin(2*np.pi*hz*env_times),np.cos(2*np.pi*hz*env_times)])
            fitted=design@np.linalg.lstsq(design,envelope,rcond=None)[0]
            result['audio_power_variance_fraction_at_sub_fundamental']=float(np.var(fitted)/np.var(envelope))
        check(len(periods)>=5 and result['cycle_period_cv']<.08,name+': intentional modulation stays cyclic over the longer hold')
        target[label]=result
    check(target['full-pressure']['measured_sub_cycle_hz']>1.1*target['soft-pressure']['measured_sub_cycle_hz'],prefix+': pressure accelerates the sustained periodic modulation')
    mechanical[prefix]=target

clock=audio['clockwork-long'];early=[];body=[];late=[]
for beat in range(6,66):
    t=beat/6
    early.append(rms(segment(clock,t+.005,t+.035)))
    body.append(rms(segment(clock,t+.055,t+.075)))
    late.append(rms(segment(clock,t+.105,t+.145)))
within=20*np.log10(np.array(body)/np.array(early)); gaps=20*np.log10(np.array(late)/np.array(early))
clock_result={'full_ticks_measured':60,'rate_hz':6,'first_tick_number':6,'last_tick_number':65,'within_tick_body_relative_to_onset_db_range':[float(within.min()),float(within.max())],'between_tick_relative_to_onset_db_range':[float(gaps.min()),float(gaps.max())],'ordinary_note_off_seconds':12,'end_active_voices':next(r['active_at_end'] for r in records if r['case']=='clockwork-long')}
check(float(within.max())<-10,'Clockwork: all sixty late-held ticks decay before their gate ends')
check(float(gaps.max())<-25,'Clockwork: all sixty late-held gaps remain strongly attenuated')
check(clock_result['end_active_voices']==0,'Clockwork: ordinary key-up stops generated notes and retires voices')

result={'schema':1,'source':SOURCE,'scope':'Bounded long-window software audio behavior. Signal statistics are not acoustic-realism or listening-preference scores; native plugin qualification is separate.','source_inventory_sha256':hashlib.sha256(inventory_raw).hexdigest(),'sample_rate':RATE,'render_cases':len(records),'distinct_existing_programs':len({r['program'] for r in records}),'stereo_frames':sum(r['frames'] for r in records),'seconds':sum(r['seconds'] for r in records),'records':records,'weather':weather,'repeated_one_shots':shots,'intentional_periodic_effects':mechanical,'clockwork':clock_result,'checks':checks,'checks_passed':sum(x['passed'] for x in checks),'checks_failed':sum(not x['passed'] for x in checks),'limitations':['One deterministic noise trajectory per case and one played note/register; not an exhaustive random/physical environment test.','Weather LFOs still draw new targets at authored fixed update rates; the tested trajectories do not repeat each update period.','Stereo measurements describe the authored narrow image; they are not spatial recordings.','No production code or preset changed by A44.']}
(BASE/'a44-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k in ('render_cases','distinct_existing_programs','stereo_frames','seconds','checks_passed','checks_failed','weather','repeated_one_shots','intentional_periodic_effects','clockwork')},indent=2))
for c in checks:
    if not c['passed']:print('FAIL:',c['check'])
raise SystemExit(1 if result['checks_failed'] else 0)
