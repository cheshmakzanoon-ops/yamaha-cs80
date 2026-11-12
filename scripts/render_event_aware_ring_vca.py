#!/usr/bin/env python3
"""Independent fixed-carrier ring multiplier into event-aware envelope/VCA."""
import argparse
from fractions import Fraction
import math
import os
from pathlib import Path
import sys

import render_event_aware_envelope_vca as constant
import render_event_aware_envelope_sine_vca as sine
import render_ring_reference as ring

helpers = constant.envelope_reference
require = helpers.require
PARAMETERS = ('attack','decay','sustain','release','input_amplitude','input_frequency',
              'input_phase','carrier_frequency','carrier_phase','amount','lc','ei','initial_li','initial_coupling')
MAX_SAMPLES = 20001
MAX_EVENTS = 1024
MAX_BUNDLE_BYTES = 32 * 1024 * 1024


def ring_tones(amplitude,frequency,phase,carrier_frequency,carrier_phase,amount):
    """Return DC and positive-Hz tones; exact coincidence folds the lower term."""
    fi,fc,pi,pc=map(Fraction,(frequency,carrier_frequency,phase,carrier_phase))
    dc=amount*(ring.SIGNAL_OFFSET*ring.CARRIER_OFFSET+ring.OUTPUT_OFFSET)
    tones=((amplitude*((1-amount)+amount*ring.CARRIER_OFFSET),fi,pi),
           (amount*ring.SIGNAL_OFFSET,fc,pc))
    if fi==fc:
        # Equality and cycle reduction are rational, never tolerance decisions.
        lower_dc=amount*amplitude/2*math.cos(math.tau*float((pi-pc)%1))
        dc=math.fsum((dc,lower_dc))
    else:
        delta=pi-pc if fi>fc else pc-pi
        tones+=((amount*amplitude/2,abs(fi-fc),delta+Fraction(1,4)),)
    tones+=((-amount*amplitude/2,fi+fc,pi+pc+Fraction(1,4)),)
    return dc,tones


def short_forcing(li,lc,e0,target,a,b,dc,tones,dt,q):
    """Total-degree40 product; tones contain amplitude, anchor radians, omega."""
    left,right=[li],[e0]
    audio=[math.fsum([dc]+[amp*math.sin(phase) for amp,phase,omega in tones])]
    lx=ex=1.0
    powers=[1.0]*len(tones)
    for n in range(1,41):
        lx*=-a*dt/n
        ex*=-b*dt/n
        left.append((li-lc)*lx)
        right.append((e0-target)*ex)
        terms=[]
        for j,(amp,phase,omega) in enumerate(tones):
            powers[j]*=omega*dt/n
            sinp,cosp=math.sin(phase),math.cos(phase)
            terms.append(amp*powers[j]*(sinp,cosp,-sinp,-cosp)[n%4])
        audio.append(math.fsum(terms))
    product=[math.fsum(left[j]*right[n-j] for j in range(n+1)) for n in range(41)]
    terms=[]
    z=q*dt
    for n in range(41):
        coefficient=math.fsum(product[j]*audio[n-j] for j in range(n+1))
        term=1.0
        moments=[1/(n+1)]
        for k in range(1,41):
            term*=z/k
            moments.append(term/(n+k+1))
        terms.append(coefficient*math.exp(-z)*math.fsum(moments))
    return q*dt*math.fsum(terms)


def advance_segment(li,coupling,lc,ei,e0,target,rate,dc,tones,dt):
    require(dt>=0 and math.isfinite(dt),'Invalid segment duration')
    if dt==0: return li,coupling
    a,b,q=1/constant.TAU_LI,rate,1/constant.TAU_AC
    next_li=li+(lc-li)*(-math.expm1(-a*dt))
    if max([a,b,q]+[omega for amp,phase,omega in tones])*dt<=.5:
        forcing=short_forcing(li,lc,e0,target,a,b,dc,tones,dt,q)
    else:
        coefficients=((lc*target,0),((li-lc)*target,a),
                      (lc*(e0-target),b),((li-lc)*(e0-target),a+b))
        contributions=[]
        for c,r in coefficients:
            integrated=[dc*constant.convolution_kernel(r,dt,q)]
            for amp,phase,omega in tones:
                phasefactor=complex(math.cos(phase),math.sin(phase))
                integrated.append(amp*(phasefactor*sine.sine_kernel(r,omega,dt,q)).imag)
            contributions.append(c*math.fsum(integrated))
        forcing=q*math.fsum(contributions)
    next_coupling=math.fsum((coupling*math.exp(-q*dt),(2**ei)*forcing))
    require(math.isfinite(next_li) and next_li>=0 and math.isfinite(next_coupling),'Invalid state')
    return next_li,next_coupling


class Composition(constant.Composition):
    def __init__(self,attack,decay,sustain,release,input_amplitude,input_frequency,
                 input_phase,carrier_frequency,carrier_phase,amount,lc,ei,initial_li,initial_coupling):
        self.input_amplitude=float(input_amplitude)
        self.amount=float(amount)
        self.input_frequency,self.input_phase,self.carrier_frequency,self.carrier_phase=map(
            Fraction,(input_frequency,input_phase,carrier_frequency,carrier_phase))
        require(math.isfinite(self.input_amplitude) and 0<=self.input_amplitude<=1 and
                math.isfinite(self.amount) and 0<=self.amount<=1,'Amplitude/amount outside bounds')
        require(self.input_frequency>=Fraction(1,10) and self.carrier_frequency>=Fraction(1,10) and
                (self.input_frequency==self.carrier_frequency or
                 abs(self.input_frequency-self.carrier_frequency)>=Fraction(1,10)) and
                self.input_frequency+self.carrier_frequency<=2000,'Frequency combination outside bounded scope')
        require(0<=self.input_phase<1 and 0<=self.carrier_phase<1,'Phase outside bounds')
        dc,self.tones=ring_tones(self.input_amplitude,self.input_frequency,self.input_phase,
                                self.carrier_frequency,self.carrier_phase,self.amount)
        self.dc=dc+constant.OFFSET+constant.FEEDTHROUGH
        super().__init__(attack,decay,sustain,release,0,lc,ei,initial_li,initial_coupling)

    @staticmethod
    def phase_at(frequency,phase,t):
        return math.tau*float((phase+frequency*t)%1)

    def _evaluate(self,t):
        tones=[(amp,self.phase_at(freq,phase,self.anchor),math.tau*float(freq))
               for amp,freq,phase in self.tones]
        return advance_segment(self.anchor_li,self.anchor_coupling,self.lc,self.ei,
            self.e0,self.target,self.rate,self.dc,tones,float(t-self.anchor))

    def _state(self,value=None):
        if value is None: value=self.env.value(self.now)
        audio=self.input_amplitude*math.sin(self.phase_at(self.input_frequency,self.input_phase,self.now))
        carrier=math.sin(self.phase_at(self.carrier_frequency,self.carrier_phase,self.now))
        wet=(audio+ring.SIGNAL_OFFSET)*(carrier+ring.CARRIER_OFFSET)+ring.OUTPUT_OFFSET
        ring_output=(1-self.amount)*audio+self.amount*wet
        gain=self.li*(2**self.ei)*value
        raw=(ring_output+constant.OFFSET+constant.FEEDTHROUGH)*gain
        return dict(time=helpers.exact_time(self.now),stage=self.env.stage,gate=self.env.gate,
                    envelope=value,li=self.li,coupling=self.coupling,gain=gain,raw=raw,output=raw-self.coupling,
                    input=audio,carrier=carrier,ring_output=ring_output,
                    input_cycles=float(self.input_phase+self.input_frequency*self.now),
                    carrier_cycles=float(self.carrier_phase+self.carrier_frequency*self.now))


def parse(argv):
    parser=argparse.ArgumentParser(description=__doc__,allow_abbrev=False)
    for name in ('events',*[n.replace('_','-') for n in PARAMETERS],'rate','samples','out'):
        parser.add_argument('--'+name,required=True)
    options=[x.split('=',1)[0] for x in argv if x.startswith('--')]
    require(len(options)==len(set(options)),'Duplicate option')
    args=parser.parse_args(argv)
    args.rate=helpers.integer(args.rate,8000,384000)
    args.samples=helpers.integer(args.samples,2,MAX_SAMPLES)
    require(Fraction(args.samples-1,args.rate)<=2,'Endpoint span exceeds two seconds')
    bounds=((.001,1),(.01,10),(0,1),(.01,10),(0,1),(.1,2000),(0,1),(.1,2000),(0,1),(0,1),(0,8),(-8,8),(0,8),(-8,8))
    for name,(low,high) in zip(PARAMETERS,bounds):
        value=helpers.number(getattr(args,name))
        require(Fraction(str(low))<=value<=Fraction(str(high)),name+' outside bounds')
        if name in ('input_phase','carrier_phase'): require(value<1,'Phase must be below one cycle')
        setattr(args,name,value)
    require((args.input_frequency==args.carrier_frequency or
             abs(args.input_frequency-args.carrier_frequency)>=Fraction(1,10)) and
            args.input_frequency+args.carrier_frequency<=2000,'Frequency combination outside bounded scope')
    return args


def solve(args,events):
    require(len(events)<=MAX_EVENTS,'Event budget exceeded')
    model=Composition(*(getattr(args,name) for name in PARAMETERS))
    rows=[]
    cursor=0
    for i in range(args.samples):
        t=Fraction(i,args.rate)
        while cursor<len(events) and events[cursor][0]<=t:
            model.event(*events[cursor]);cursor+=1
        rows.append(dict(sample=i,time_s=float(t),**model.advance(t),last_event=cursor-1))
    require(cursor==len(events),'Unconsumed event')
    segments=list(model.segments)
    if model.now!=model.anchor:
        segments.append(dict(start=helpers.exact_time(model.anchor),end=helpers.exact_time(model.now),
            stage=model.env.stage,envelope_start=model.e0,envelope_target=model.target,envelope_rate=model.rate,
            li_start=model.anchor_li,coupling_start=model.anchor_coupling,li_end=model.li,coupling_end=model.coupling,
            end_reason='final output endpoint (query only)'))
    return dict(rows=rows,event_audit=model.event_audit,transitions=model.transition_audit,
                segments=segments,envelope_transitions=model.env.transitions)


def render(args):
    out=Path(args.out).absolute()
    require(not os.path.lexists(out),'Output path must be new (including symlinks)')
    require(out.parent.is_dir(),'Output parent must exist')
    event_bytes=helpers.snapshot(args.events)
    result=solve(args,helpers.parse_events(event_bytes,Fraction(args.samples-1,args.rate)))
    columns=('sample','time_s','input_cycles','carrier_cycles','input','carrier','ring_output','envelope','stage','gate','li','gain','raw','coupling','output','last_event')
    lines=[','.join(columns)+'\n']
    for row in result['rows']:
        lines.append(','.join(format(row[k],'.17g') if isinstance(row[k],float) else
                              str(int(row[k])) if isinstance(row[k],bool) else str(row[k]) for k in columns)+'\n')
    artifacts={'trace.csv':''.join(lines).encode('ascii'),'input_events.csv':event_bytes,
        'events.json':helpers.json_bytes(result['event_audit']),'transitions.json':helpers.json_bytes(result['transitions']),
        'segments.json':helpers.json_bytes(result['segments']),'envelope_transitions.json':helpers.json_bytes(result['envelope_transitions']),
        'renderer.py':helpers.snapshot(__file__),'render_event_aware_envelope_vca.py':helpers.snapshot(constant.__file__),
        'render_amplitude_envelope_reference.py':helpers.snapshot(helpers.__file__),
        'render_event_aware_envelope_sine_vca.py':helpers.snapshot(sine.__file__),
        'render_ring_reference.py':helpers.snapshot(ring.__file__)}
    manifest=dict(schema='cs80-event-aware-ring-vca-v1',complete=True,rate_hz=args.rate,
        samples=args.samples,intervals=args.samples-1,endpoint_span=helpers.exact_time(Fraction(args.samples-1,args.rate)),
        parameters={n:dict(decimal_rational=str(getattr(args,n)),binary64=float(getattr(args,n))) for n in PARAMETERS},
        constants=dict(tau_li_seconds=constant.TAU_LI,tau_ac_seconds=constant.TAU_AC,
                       offset_promoted_float32=constant.OFFSET,feedthrough_promoted_float32=constant.FEEDTHROUGH,ring_signal_offset=ring.SIGNAL_OFFSET,
                       ring_carrier_offset=ring.CARRIER_OFFSET,ring_output_offset=ring.OUTPUT_OFFSET),
        equations=['input=A*sin(2*pi*(phase+frequency*t))','carrier=sin(2*pi*(carrier_phase+carrier_frequency*t))',
                   'ring_output=(1-amount)*input+amount*((input+.0005)*(carrier+.005)+.001)',
                   'LI=LC+(LI0-LC)*exp(-t/tauLI)',
                   'raw=2**EI*(ring_output+offset+feedthrough)*LI*envelope','dCoupling/dt=(raw-Coupling)/tauAC','output=raw-Coupling'],
        clock='Exact Fraction samples/events and input/carrier/sideband phase clocks; autonomous boundaries precede same-time events in file order; no t=0 tick',
        integration='Boundary-only anchors; real/complex exponential convolution; total-degree40 local forcing series when max rate*duration<=.5; binary64, not certified error bounds',
        scope='Fixed sine input and carrier offset multiplier into event-aware envelope/conditional VCA. Explicit numeric unit mapping 1:1, fixed LC/EI, nonnegative LI. No production equivalence or hardware calibration.',
        coincident_semantics='Exact rational frequency equality folds +amount*A/2*cos(2*pi*(input_phase-carrier_phase)) into DC once and omits the lower tone; no nearzero expansion',
        jump_semantics='LI/coupling continuous; envelope reset/snap raw/output jumps retained at their true endpoints',
        units='Phase cycles, frequency Hz, EI octaves, envelope unitless; uncalibrated engine input/output/control units',
        limitations=['No converter or ring envelope generation','No strictly nonzero frequency difference below0.1Hz or sum above2000Hz','No arbitrary audio','No live controls or ADSR','No negative LI floor crossings','No filter or whole voice','No alias qualification','No WAV/listening claim'],
        limits=dict(max_samples=MAX_SAMPLES,max_events=MAX_EVENTS,max_seconds=2,max_input_bytes=helpers.MAX_INPUT_BYTES,max_bundle_bytes=MAX_BUNDLE_BYTES),
        provenance='RingModulator.h offset multiplier via existing ring reference plus IG00151Reference.h conditional laws via existing constant reference and existing amplitude envelope inferences. Snapshots identify source bytes, not authenticated execution; platform libm bits may vary.',
        artifacts={n:helpers.metadata(data) for n,data in sorted(artifacts.items())})
    artifacts['manifest.json']=helpers.json_bytes(manifest)
    require(sum(map(len,artifacts.values()))<=MAX_BUNDLE_BYTES,'Bundle byte budget exceeded')
    owned=helpers.Owned()
    try:
        owned.mkdir(out)
        for name,data in artifacts.items(): owned.write(out/name,data)
    except BaseException:
        owned.cleanup()
        raise
    return result


def main(argv=None):
    try: render(parse(sys.argv[1:] if argv is None else argv))
    except (ValueError,OSError,UnicodeError,OverflowError) as error:
        print('event-aware ring/envelope/VCA: '+str(error),file=sys.stderr)
        return 1
    return 0


if __name__=='__main__': sys.exit(main())
