#!/usr/bin/env python3
"""Offline evidence replay only. Never imports a module, starts DSP, or reads clocks."""
from pathlib import Path
import csv,hashlib,io,json,math,struct,tarfile,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def f32(bits):return struct.unpack('<f',struct.pack('<I',int(bits)))[0]
def f64(bits):return struct.unpack('<d',struct.pack('<Q',int(bits)))[0]
def rows(blob):return list(csv.DictReader(io.StringIO(blob.decode())))
def load(root=ROOT):
    with tarfile.open(root/'captures.tar.gz','r:gz')as tar:
        entries=tar.getmembers();assert len(entries)==12 and len({t.name for t in entries})==12
        assert all(t.isfile()and '/'not in t.name and 0<t.size<600000 for t in entries)
        blobs={t.name:tar.extractfile(t).read()for t in entries}
    return blobs,json.loads((root/'evidence.json').read_text())
def identity(blobs,e):
    assert e['schema']=='native-factory8-held-eight-filter-V2-portable'and e['timing']is False
    assert set(blobs)==set(e['common_files'])
    for n,b in blobs.items():assert e['common_files'][n]==dict(bytes=len(b),sha256=sha(b))
    common=set(blobs)-{'observer-counts.jsonl'};assert len(common)==11 and len(e['arms'])==4
    assert [a['name']for a in e['arms']]==['stock-a','stock-b','V2-normal','V2-k8-observer']
    for a in e['arms']:
        assert a['exit']==0 and set(a['files'])==common
        assert all(a['files'][n]==e['common_files'][n]for n in common)
    assert sum(len(blobs[n])for n in common)==e['ordinary_bytes_per_arm']==933293
    assert e['build']['retained_header_dependent_original_TUs']==20 and e['build']['full_clean_closure_TUs']==23
    assert not e['workload']['private_active_voice_counts_observed']and not e['workload']['private_source_route_counts_observed']
    return True

def public(blobs,e):
    controls=rows(blobs['controls.csv']);assert len(controls)==192 and len({r['id']for r in controls})==192;byname={r['name']:r for r in controls}
    key={'hardware_source_1':0,'hardware_source_2':0,'source_performance':1,'parity_mode':0,'voice_mode':0,'drift':f32(0x3ebd70a4),'voice_dispersion':f32(0x3e3851ec),'drift_model':0,'mpe_on':0,'arp_on':0,'oversampling':1,'volume':f32(0x3df5c28f),'mix':.5,'analog_noise':0,'fx1_type':0,'fx2_type':0,'fx3_type':0,'fx1_mix':0,'fx2_mix':0,'fx3_mix':0}
    for k,x in key.items():assert f32(byname[k]['desired_physical_f32_bits'])==x
    states={};projection=[]
    for n in ['initial.state','block-0.state','block-1023.state','block-1039.state','block-1040.state']:
        b=blobs[n];end=b.index(b'</CS80_PARAMS>')+len(b'</CS80_PARAMS>');assert b[end:].endswith(b'JUCEPrivateData');tree=ET.fromstring(b[:end])
        values={p.get('id'):struct.unpack('<f',struct.pack('<f',float(p.get('value'))))[0]for p in tree.findall('PARAM')}
        for k,x in key.items():assert values[k]==x
        states[n]=values
    for values in states.values():assert values==states['initial.state']
    for k,r in byname.items():
        actual=struct.unpack('<I',struct.pack('<f',states['initial.state'][k]))[0];desired=int(r['desired_physical_f32_bits'])
        if actual!=desired:projection.append(dict(name=k,requested_f32_bits=desired,serialized_f32_bits=actual))
    assert projection==e['public_control_projection_differences']
    for label in ['initial','final']:
        records=rows(blobs[label+'-metadata.csv']);meta={int(r['id']):r for r in records};assert len(records)==len(meta)==2378
        for k,r in byname.items():assert int(r['id'])in meta
        for k,x in key.items():assert f64(meta[int(byname[k]['id'])]['current_f64_bits'])==f64(byname[k]['normalized_f64_bits'])
        for k in ['hardware_source_1','hardware_source_2']:assert int(meta[int(byname[k]['id'])]['step_count'])==13
    events=list(map(json.loads,blobs['inputs.jsonl'].splitlines()));assert len(events)==1049
    for n,(event,pitch)in enumerate(zip(events[:8],[36,43,48,55,60,64,67,72])):
        assert event==dict(event='note-on',block=0,offset=0,sdk_channel=0,note_id=n,pitch=pitch,velocity_f32_bits=event['velocity_f32_bits'],decoded_midi7=102)
        velocity=f32(event['velocity_f32_bits']);assert velocity==struct.unpack('<f',struct.pack('<f',102/127))[0]and int(velocity*127+.5)==102
    for b,event in enumerate(events[8:]):assert event['event']=='callback'and event['block']==b and event['event_reads']==(8 if b==0 else 0)and event['parameter_points']==0 and event['project_samples']==b*64
    pcm=blobs['pcm.bin'];assert len(pcm)==1041*512;values=struct.unpack('<133248f',pcm);assert all(math.isfinite(x)for x in values)
    assert sum(values[n]!=0 or values[n+1]!=0 for n in range(0,len(values),2))==66624
    assert sha(pcm)==e['PCM_all_sha256']and sha(pcm[1024*512:1040*512])==e['PCM_window_sha256']and sha(pcm[1040*512:])==e['PCM_continuation_sha256']
    raw=blobs['callbacks.bin'];assert len(raw)==1041*72;hist={}
    for b in range(1041):
        n,mode,rate,result,silence=struct.unpack_from('<IIdiQ',raw,b*72);assert n==b and mode==0 and rate==48000 and result==0 and silence<=3
        fp=[struct.unpack_from('<IHHi',raw,b*72+28+i*12)for i in range(3)];assert all((f[0]&0xffc0)==0x9fc0 and f[1]==0x037f for f in fp)
        assert fp[1][3]==fp[2][3]and struct.unpack_from('<d',raw,b*72+64)[0]==1.;hist[repr(fp)]=hist.get(repr(fp),0)+1
    assert hist==e['FP_histogram'];return True

def counts(blobs,e):
    records=list(map(json.loads,blobs['observer-counts.jsonl'].splitlines()));assert len(records)==1041;last=[0,0,0]
    for b,r in enumerate(records):
        assert r['block']==b and r['before']==last and all(r['after'][i]>=r['before'][i]for i in range(3))
        assert r['after'][1]<=r['after'][0];last=r['after']
    labels=['returned_eight_calls','accepted_eight_calls','cached_quartet_consumptions'];assert dict(zip(labels,last))==e['attribution']['total']
    phases={}
    for label,a,z in [('warm',0,1024),('window',1024,1040),('continuation',1040,1041)]:
        delta=[records[z-1]['after'][i]-records[a]['before'][i]for i in range(3)];phases[label]=dict(callbacks=z-a,**dict(zip(labels,delta)))
        assert delta[0]>=delta[1]>0 and delta[1]==delta[2]
    assert phases==e['attribution']['phases'];return phases

def verify(root=ROOT):
    manifest=json.loads((root/'manifest.json').read_text());actual={p.name for p in root.iterdir()if p.is_file()and p.name!='manifest.json'};assert actual==set(manifest['files'])
    for name,r in manifest['files'].items():
        data=(root/name).read_bytes();assert r==dict(bytes=len(data),sha256=sha(data))
        if name!='captures.tar.gz':assert not any(x in data for x in [b'/' + b'workspace' + b'/',b'/' + b'home' + b'/agent/',b'/' + b'tmp' + b'/'])
    blobs,e=load(root);identity(blobs,e);public(blobs,e);phases=counts(blobs,e)
    assert sha((root/'host.cpp').read_bytes())==e['source']['host_sha256']
    return dict(passed=True,processes=4,callbacks_each=1041,ordinary_bytes_each=933293,held_note_events_read=8,public_states=5,phases=phases,hybrid_only=True,timing=False)
if __name__=='__main__':print(json.dumps(verify(),indent=2))
