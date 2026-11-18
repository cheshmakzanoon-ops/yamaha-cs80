#!/usr/bin/env python3
"""Offline only. Recheck lossless public traces, 80 callback records and count witnesses."""
from pathlib import Path
import csv,hashlib,io,json,math,struct,tarfile,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
SNAPSHOTS=[0,8,16,24,32,40,48,49,56,64,72,79]
def mode(b):return int(16<=b<32 or 48<=b<72)
def load(root=ROOT):
    with tarfile.open(root/'captures.tar.gz','r:gz')as tar:
        entries=tar.getmembers()
        assert len(entries)==31 and all(t.isfile()and '/'not in t.name and t.size<20000 or t.isfile()and t.name=='pcm.bin'and t.size==40960 for t in entries)
        assert len({t.name for t in entries})==len(entries)
        blobs={t.name:tar.extractfile(t).read()for t in entries}
    return blobs,json.loads((root/'evidence.json').read_text()),list(csv.DictReader(io.StringIO((root/'callbacks.csv').read_text())))
def parse_state(raw):
    end=raw.index(b'</CS80_PARAMS>')+len(b'</CS80_PARAMS>')
    tail=raw[end:];assert tail.endswith(b'JUCEPrivateData')
    return ET.fromstring(raw[:end]),tail
def canonical(n):return (n.tag,tuple(sorted(n.attrib.items())),(n.text or '').strip(),tuple(canonical(c)for c in n))
def config(tree,v):
    layer=tree.find('MODERN_LAYER');assert layer is not None and int(layer.get('version'))==1
    shapes=layer.findall('MSEG');assert len(shapes)==3 and {int(s.get('index'))for s in shapes}=={0,1,2}
    for s in shapes:
        points=[list(map(float,p.strip().split()))for p in s.get('points').split(';')if p.strip()]
        assert points==[[0.,0.,0.],[.5,.25 if v%2 else .75,0.],[1.,0.,0.]]
        assert all(int(s.get(k))==x for k,x in dict(loopStart=0,loopEnd=2,loop=0,trigger=v%3,sync=v%2,bipolar=0).items())
    slots=layer.find('MATRIX').findall('SLOT');assert len(slots)==16 and {int(s.get('index'))for s in slots}==set(range(16))
    for s in slots:
        n=int(s.get('index'));expected=dict(source=(1 if v%2 else 14)if n==0 else 0,via=0,destination=(1 if v%2 else 2)if n==0 else 0,target=v%3 if n==0 else 0)
        assert all(int(s.get(k))==x for k,x in expected.items())
def validate_states(blobs):
    events=list(map(json.loads,blobs['events.jsonl'].splitlines()))
    expected=[dict(event='config',block=b,variant=v,stem=f'{b}-config-{v}')for b,v in [(-1,0),(8,1),(24,2),(40,3)]]+[dict(event='reprepare',block=56,rate=96000),dict(event='config',block=64,variant=4,stem='64-config-4')]
    assert events==expected
    for e in events:
        if e['event']!='config':continue
        stem=e['stem'];v=e['variant'];before,bt=parse_state(blobs[stem+'-before.state']);input,it=parse_state(blobs[stem+'-input.state']);after,at=parse_state(blobs[stem+'-after.state'])
        config(input,v);config(after,v);assert bt==it==at
        for tree in [before,input]:
            for child in list(tree):
                if child.tag=='MODERN_LAYER':tree.remove(child)
        assert canonical(before)==canonical(input)
    for b in SNAPSHOTS:
        tree,_=parse_state(blobs[f'block-{b}.state']);params={p.get('id'):p.get('value')for p in tree.findall('PARAM')}
        assert float(params['voice_mode'])==mode(b)
    return True

def validate_callbacks(blobs):
    raw=blobs['callbacks.bin'];pcm=blobs['pcm.bin'];assert len(raw)==80*72 and len(pcm)==40960
    floats=struct.unpack('<10240f',pcm);assert all(math.isfinite(x)for x in floats)
    assert sum(floats[n]!=0 or floats[n+1]!=0 for n in range(0,len(floats),2))==5120
    counts=list(map(json.loads,blobs['observer-counts.jsonl'].splitlines()));assert len(counts)==80
    rows=[];last=[0,0,0]
    for b in range(80):
        n,m,rate,result,silence=struct.unpack_from('<IIdiQ',raw,b*72)
        assert n==b and m==mode(b)and rate==(96000. if b>=56 else 48000.)and result==0 and silence<=3
        fps=[struct.unpack_from('<IHHi',raw,b*72+28+i*12)for i in range(3)]
        assert all((f[0]&0xffc0)==0x9fc0 and f[1]==0x037f for f in fps)
        assert fps[1][3]==fps[2][3]and struct.unpack_from('<d',raw,b*72+64)[0]==1.
        c=counts[b];assert c['block']==b and c['mode']==m and c['before']==last
        assert [c['after'][i]-c['before'][i]for i in range(3)]==[m]*3;last=c['after']
        row=dict(block=b,mode=m,sample_rate=int(rate),process_result=result,silence_mask=silence,note_on_inputs=8 if b in [0,16,49]else 16 if b==56 else 0,config_variant={8:1,24:2,40:3,64:4}.get(b,''),reprepare=int(b==56),pcm_sha256=sha(pcm[b*512:(b+1)*512]),public_state_sha256=sha(blobs[f'block-{b}.state'])if b in SNAPSHOTS else '')
        for prefix,fp in zip(['before','after','next_probe'],fps):
            for label,v in zip(['csr','cw','sw','errno'],fp):row[prefix+'_'+label]=v
        row['next_probe_double_bits']=struct.unpack_from('<Q',raw,b*72+64)[0]
        for prefix,cs in [('count_before',c['before']),('count_after',c['after'])]:
            for label,v in zip(['entry','accept','transport'],cs):row[prefix+'_'+label]=v
        rows.append({k:str(v)for k,v in row.items()})
    assert last==[40,40,40]
    return rows

def validate_identity(blobs,evidence):
    assert evidence['schema']=='modern-mirror-native-portable-v1'and evidence['timing']is False
    assert len(evidence['arms'])==4 and [a['name']for a in evidence['arms']]==['stock-a','stock-b','candidate-normal','candidate-observer']
    assert set(blobs)==set(evidence['common_files'])
    for name,data in blobs.items():assert evidence['common_files'][name]==dict(bytes=len(data),sha256=sha(data))
    common=set(blobs)-{'observer-counts.jsonl'};assert len(common)==30
    for arm in evidence['arms']:
        assert arm['exit']==0 and set(arm['files'])==common
        assert all(arm['files'][n]==evidence['common_files'][n]for n in common)
    assert sum(len(blobs[n])for n in common)==evidence['normal_comparison_bytes']==474170
    assert evidence['coverage']['private_active_voice_counts_observed']is False and evidence['coverage']['private_ModernLayer_raw_bytes_observed']is False
    assert evidence['build']['retained_header_affected_TUs']==19
    return True

def verify(root=ROOT):
    manifest=json.loads((root/'manifest.json').read_text())
    actual={p.name for p in root.iterdir()if p.is_file()and p.name!='manifest.json'}
    assert actual==set(manifest['files'])
    for name,record in manifest['files'].items():
        b=(root/name).read_bytes();assert record==dict(bytes=len(b),sha256=sha(b))
    blobs,evidence,rows=load(root);validate_identity(blobs,evidence);validate_states(blobs)
    assert rows==validate_callbacks(blobs)
    assert sha((root/'host.cpp').read_bytes())==evidence['source_hashes']['native/host.cpp']
    for name in ['README.md','evidence.json','host.cpp','observer.patch','callbacks.csv']:
        t=(root/name).read_bytes();assert not any(p in t for p in [b'/workspace/',b'/home/agent/',b'/tmp/'])
    return dict(passed=True,processes=4,callbacks_each=80,normal_files=30,exact_bytes_per_arm=474170,PCM_frames_each=5120,public_snapshot_states=12,config_edits=5,actual_entries=40,actual_accepted=40,actual_transport=40,zero8_callbacks=40,hybrid_only=True,timing=False)
if __name__=='__main__':print(json.dumps(verify(),indent=2))
