"""Isolated bounded timing-study decoder. Never a qualification admission path.
Requires a separately frozen materialized lifecycle helper; new timing schemas
cannot pass the ordinary process-lifetime parser.
"""
import hashlib,math,os,random,statistics,struct
from pathlib import Path
from identity import require
from process_lifetime_transaction import _Reader,_maps,digest,sha,BODY_CAP,PAYLOAD_CAP,CASE_CAP,PROFILE
import source_parameter_proof as proof
SCHEMA='cs80-process-lifetime-timing-v1'
HISTORY='cs80-instance-timing-A-D-steady-v1'
CPU_CAP=25_000_000_000
B=lambda b:struct.pack('<Q',len(b))+b
U=lambda n:struct.pack('<Q',n)
V=lambda n:struct.pack('<I',n)

def decode_timing(data,c,nonce,pid,module_stat,lifecycle_decoder):
    require(c['schema']==SCHEMA and c['history_schema']==HISTORY and c['audio_input_topology']==2 and c['parameter_binding_policy']==2,'timing frozen schema/profile')
    # Caller pins the separately materialized helper source hash before launch.
    # Its outer framing, source metadata/readbacks, lifecycle and mapping checks
    # are all mandatory; this function adds independent inner timing semantics.
    require(lifecycle_decoder.SCHEMA==SCHEMA and lifecycle_decoder.SOURCE_HISTORY==HISTORY,'isolated timing lifecycle decoder required')
    result=lifecycle_decoder._parse(data,c,nonce,pid,module_stat)
    payload=_Reader(result['payload']);require(payload.text()==HISTORY,'timing payload schema')
    case=payload.blob(CASE_CAP);require(case==bytes.fromhex(c['case_identity_hex']),'timing payload case')
    spans=[payload.blob(PAYLOAD_CAP) for _ in range(8)]
    renders=[payload.blob(PAYLOAD_CAP) for _ in range(6)]
    require([digest(v) for v in spans[:3]]==c['protocol_sha256'],'timing protocol hashes')
    return {**result,**decode_segments(renders,case,c)}

def source_protocols(case):
    fields,_=proof.case_controls(case)
    require(fields[1:]==[48000,0,16,64,4096],'fixed timing study case required')
    # Consume the entire case, including source-derived event tail (the general
    # control proof intentionally does not consume the event section).
    r=_Reader(case)
    def txt():return r.raw(r.u32()).tobytes().decode('ascii')
    for _ in range(3):txt()
    for _ in range(6):r.u32()
    require(r.u32()==192,'source timing case control count')
    for _ in range(192):txt();r.u32();r.raw(8)
    require(r.u32()==32,'source timing case event count')
    for on in (True,False):
        for n in range(16):
            expected=[(17 if on else 3072)+n,int(on),0,0,48+n,48+n,struct.unpack('<I',struct.pack('<f',.75 if on else 0))[0]]
            require([r.u32() for _ in range(7)]==expected,'source timing case events')
    r.end()
    def protocol(frames,kind):
        p=b'CS80RPRT'+V(2)+V(2)+struct.pack('<diiq',48000.,0,64,frames)+V(0)
        events=[]
        if kind!='measured':
            for n in range(16):
                events.append(struct.pack('<qiidIIiiffii',17+n,0,0,0.,0,0,0,48+n,0.,.75,0,(96 if kind=='warmup' else 48)+n))
            if kind=='history':
                for n in range(16):events.append(struct.pack('<qiidIIiiffi',3072+n,0,0,0.,0,1,0,48+n,0.,0.,48+n))
        return p+U(len(events))+b''.join(events)+U(0)
    return [protocol(4096,'history')]*4+[protocol(65536,'warmup'),protocol(524288,'measured')]

def decode_render(data,expected,frames):
    r=_Reader(data);require(r.raw(8)==b'CS80TIME' and r.u32()==1,'timing render schema')
    require(r.blob(CASE_CAP)==expected,'timing render source protocol differs')
    count=r.count(8192);require(count==frames//64,'timing callback count');cpu=[]
    for i in range(count):
        start,n,result,silence,ns=r.u64(),r.u32(),r.u32(),r.u64(),r.u64()
        require(start==i*64 and n==64 and result==0 and silence<=3,'timing callback partition/result/mask')
        require(0<ns<=CPU_CAP,'timing CPU callback bound');cpu.append(ns)
    require(r.u64()==frames*2,'complete timing stereo PCM');pcm=r.raw(frames*8)
    nonzero=False;peak=0.;nonzero_count=0;block_energy=0.;energies=[]
    for sample,(v,) in enumerate(struct.iter_unpack('<f',pcm)):
        require(math.isfinite(v),'nonfinite timing PCM');nonzero|=v!=0;nonzero_count+=v!=0;peak=max(peak,abs(v));block_energy+=v*v
        if (sample+1)%128==0:energies.append(block_energy);block_energy=0.
    if frames==524288:require(all(e>0 for e in energies),'silent measured callback')
    require(nonzero,'timing PCM all zero')
    outcount=r.count(65536);lastblock=-1;lastid=None;lastframe=-1;ids=set();blockcount=0
    for _ in range(outcount):
        frame,id=r.u64(),r.u32();value=struct.unpack('<d',r.raw(8))[0]
        require(frame<frames and id!=0xffffffff and math.isfinite(value) and 0<=value<=1,'timing output point bounds')
        block=frame//64;require(block>=lastblock,'timing output block order')
        if block!=lastblock:lastblock=block;lastid=None;lastframe=-1;ids=set();blockcount=0
        blockcount+=1;require(blockcount<=4096,'timing output block count')
        if id!=lastid:require(id not in ids,'timing output queue reentry');ids.add(id);lastid=id;lastframe=-1
        require(frame>=lastframe,'timing output queue order');lastframe=frame
    require(r.u64()==128,'timing baseline count');baseline=[r.u64() for _ in range(128)]
    require(all(0<x<=CPU_CAP for x in baseline),'timing baseline CPU bound');r.end()
    require(sum(cpu)+sum(baseline)<=CPU_CAP,'timing render CPU total bound')
    return {'frames':frames,'callback_count':count,'callback_cpu_ns':cpu,'callback_cpu_total_ns':sum(cpu),'clock_pair_ns':baseline,'pcm_sha256':digest(pcm),'pcm_bytes':len(pcm),'pcm_peak':peak,'pcm_rms':math.sqrt(math.fsum(energies)/(frames*2)),'pcm_nonzero_count':nonzero_count,'nonzero_callback_fraction':sum(e>0 for e in energies)/len(energies),'minimum_callback_rms':math.sqrt(min(energies)/128),'protocol_sha256':digest(expected),'render_sha256':digest(data)}

def decode_segments(renders,case,c):
    require(len(renders)==6,'six timing observations required');protocols=source_protocols(case)
    require(digest(protocols[0])==c['protocol_sha256'][2] and digest(protocols[4])==c['warmup_protocol_sha256'] and digest(protocols[5])==c['measured_protocol_sha256'],'timing independently expected protocol hashes')
    segments=[decode_render(data,p,frames) for data,p,frames in zip(renders,protocols,[4096]*4+[65536,524288])]
    require(sum(s['callback_cpu_total_ns']+sum(s['clock_pair_ns']) for s in segments)<=CPU_CAP,'timing invocation CPU total bound')
    measured=segments[5]
    return {'timing_segments':segments,'measured_cpu_ns_per_frame':measured['callback_cpu_total_ns']/524288,'measured_clock_overhead_fraction':statistics.mean(measured['clock_pair_ns'])*measured['callback_count']/measured['callback_cpu_total_ns'],'clock_subtracted':False,'production_qualified':False}

def paired_summary(blocks,aa_blocks,*,seed=20261117,bootstrap_count=10000):
    """Whole ABBA blocks are the independent resampling units, not callbacks.
    Blocks contain four positive CPU/frame values in stock,candidate,candidate,
    stock order. A/A blocks contain stock in all four slots. No subtraction.
    """
    require(len(blocks)==6 and len(aa_blocks)==2,'six ABBA plus two A/A blocks required')
    require(type(bootstrap_count) is int and 1000<=bootstrap_count<=100000,'bounded bootstrap count')
    def ratio(block):
        require(len(block['cpu_per_frame'])==4,'complete ABBA block')
        v=block['cpu_per_frame'];require(all(type(x) in (int,float) and math.isfinite(x) and x>0 for x in v),'positive finite CPU samples')
        require(len(block['overhead_fraction'])==4 and all(type(x) in (int,float) and math.isfinite(x) and 0<=x<1 for x in block['overhead_fraction']),'clock overhead fractions')
        return (math.log(v[0])+math.log(v[3])-math.log(v[1])-math.log(v[2]))/2
    values=[ratio(b) for b in blocks];aa=[ratio(b) for b in aa_blocks];effect=statistics.mean(values);speedup=math.exp(effect);gain=1-math.exp(-effect)
    noise=max(abs(1-math.exp(-v)) for v in aa)
    overhead=max(x for b in blocks+aa_blocks for x in b['overhead_fraction']);floor=max(noise,3*overhead)
    rng=random.Random(seed);boots=sorted(statistics.mean(rng.choices(values,k=6)) for _ in range(bootstrap_count))
    def quantile(p):
        index=(len(boots)-1)*p;lo=int(index);return boots[lo]+(boots[min(lo+1,len(boots)-1)]-boots[lo])*(index-lo)
    low,high=map(math.exp,(quantile(.025),quantile(.975)))
    return {'schema':'cs80-timing-paired-summary-v1','paired_blocks':6,'aa_blocks':2,'bootstrap_seed':seed,'bootstrap_samples':bootstrap_count,'resampling_unit':'complete_ABBA_block','speedup':speedup,'speedup_ci95':[low,high],'gain_fraction':gain,'gain_ci95':[1-1/low,1-1/high],'aa_noise_fraction':noise,'clock_overhead_fraction_max':overhead,'claim_floor_fraction':floor,'gain_claim_supported':low>1 and (1-1/low)>floor,'clock_subtracted':False,'production_qualified':False}

def matched_pcm_histories(left,right):
    """Exact retained-PCM identity only, not perceptual or general equivalence."""
    a,b=left['timing_segments'],right['timing_segments']
    require(len(a)==len(b)==6,'complete paired histories required')
    require(all(x['frames']==y['frames'] and x['protocol_sha256']==y['protocol_sha256'] and x['pcm_bytes']==y['pcm_bytes'] for x,y in zip(a,b)),'paired history protocol/shape mismatch')
    return {'exact_pcm_all_six':all(x['pcm_sha256']==y['pcm_sha256'] for x,y in zip(a,b)),'matching_segments':[x['pcm_sha256']==y['pcm_sha256'] for x,y in zip(a,b)],'equivalence_scope':'exact retained six-segment PCM only','production_qualified':False}
