#!/usr/bin/env python3
"""Read-only reconstruction of saved captures. Never runs native code.
Usage: python independent-capture-audit.py [CAPTURE_DIRECTORY]
Phase integrates exact float inputs as integers in 2^-19 Hz units.
"""
from pathlib import Path
import hashlib,json,math,struct,sys
CAP=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[3]/'build-lfo-random-switching/captures'
def read(p):return [v[0] for v in struct.iter_unpack('<f',p.read_bytes())]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
def rng(seed):
    x=seed
    while True:
        x=(x^(x*8192))&0xffffffff;x=x^(x//131072);x=(x^(x*32))&0xffffffff
        yield (x//256)/8388608-1
cases=[];inputs={};total=0
for sr in [88200,96000,192000,384000]:
    rates=read(CAP/f'rates-{sr}.f32');N=sr//4;assert len(rates)==N
    inputs[f'rates-{sr}.f32']=digest(CAP/f'rates-{sr}.f32')
    den=(1<<19)*sr;acc=0;phases=[];cycles=[];wraps=[];old=0;clearance=1.
    for n,r in enumerate(rates):
        assert math.isfinite(r) and 16<=r<=20
        q=int(r*(1<<19));assert q/(1<<19)==r
        acc+=q;cycle,rem=divmod(acc,den);phases.append(rem/den);cycles.append(cycle)
        clearance=min(clearance,min(rem,den-rem)/den)
        if cycle!=old:assert cycle==old+1;wraps.append(n);old=cycle
    assert len(wraps)==4 and clearance>1e-10
    events=sorted([13*N//100,wraps[0],wraps[0]+1,61*N//100,wraps[2],87*N//100])
    assert len(set(events))==6 and 0<events[0]<events[-1]<N
    assert len(set(events)&set(wraps))==2
    for start in [5,6]:
        wavefile=CAP/f'waves-{sr}-start{start}.f32';waves=read(wavefile);assert len(waves)==N
        inputs[wavefile.name]=digest(wavefile)
        state=start;actual_events=[]
        for n,w in enumerate(waves):
            if n in events:state=11-state
            assert w==state
            if n and w!=waves[n-1]:actual_events.append(n)
        assert actual_events==events
        last_seed=None
        for seed in [0x6c8e9cf5,0x3b9e5d21,0xa1c4f00d]:
            identity=f'{sr}-start{start}-seed{seed}'
            ap=CAP/f'{identity}-actual.f32';rp=CAP/f'{identity}-reference.f32'
            actual=read(ap);saved=read(rp);assert len(actual)==len(saved)==N
            stream=rng(seed);previous_wave=None;previous_cycle=0;draws=0;left=right=0.;worst=refworst=0.
            for phase,cycle,w,v,s in zip(phases,cycles,waves,actual,saved):
                # Selection initialization wins over a coincident cycle edge.
                if w!=previous_wave:
                    left=next(stream);draws+=1
                    if w==6:right=next(stream);draws+=1
                    else:right=left
                elif cycle!=previous_cycle:
                    if w==5:left=next(stream)
                    else:left,right=right,next(stream)
                    draws+=1
                previous_wave=w;previous_cycle=cycle
                truth=f32(left if w==5 else left+(right-left)*(1-math.cos(math.pi*phase))/2)
                assert math.isfinite(v) and abs(v)<=1 and math.isfinite(s) and abs(s)<=1
                error=abs(v-truth);referror=abs(s-truth);assert error<=1e-6 and referror<=1e-6
                if w==5:assert struct.pack('<f',v)==struct.pack('<f',truth)==struct.pack('<f',s)
                worst=max(worst,error);refworst=max(refworst,referror);total+=1
            assert draws==(12 if start==5 else 13)
            if last_seed is not None:assert last_seed!=actual
            last_seed=actual
            cases.append(dict(case=identity,samples=N,switch_indices=events,wrap_indices=wraps,coincident_switch_wraps=2,rng_draws=draws,max_actual_error=worst,max_saved_reference_error=refworst,minimum_boundary_clearance=clearance,actual_sha256=digest(ap),reference_sha256=digest(rp)))
assert len(cases)==24 and total==1140300
print(json.dumps(dict(result='PASS',method='Independent exact integer phase (2^-19 Hz quantum), rational RNG, saved waveform timeline, initialization-over-wrap priority',cases=24,compared_samples=total,switches=144,wraps_per_case=4,max_actual_error=max(c['max_actual_error'] for c in cases),max_saved_reference_error=max(c['max_saved_reference_error'] for c in cases),minimum_boundary_clearance=min(c['minimum_boundary_clearance'] for c in cases),sample_hold_bit_exact=True,input_sha256=inputs,case_results=cases,limits='Saved-data audit, no native rerun. Observed production phase and reset replay are fixture/log checks, not independently reconstructed measurements here.'),indent=2))
