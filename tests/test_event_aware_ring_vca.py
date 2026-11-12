#!/usr/bin/env python3
"""Bounded independent direct-ring/real-ODE checks; no tone-decomposition reuse."""
from fractions import Fraction as Q
import csv
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
TAU=1/(1/180000+1/560000+3/270000)*1e-8
AC=.1
F32=lambda x:struct.unpack('f',struct.pack('f',x))[0]
OFFSET,FEED=F32(.0015),F32(.001)

def direct_ring(t,amplitude=.4,frequency=230.,phase=.125,carrier_frequency=200.,carrier_phase=.25,amount=.6):
    x=amplitude*math.sin(2*math.pi*(frequency*t+phase))
    carrier=math.sin(2*math.pi*(carrier_frequency*t+carrier_phase))
    return (1-amount)*x+amount*((x+.0005)*(carrier+.005)+.001)

def rk4_direct(duration,steps,envelope,start=0.,li=.2,coupling=.013,lc=.8,ei=.3,**ring):
    h=duration/steps
    def raw(t,level):return (direct_ring(start+t,**ring)+OFFSET+FEED)*2**ei*level*envelope(t)
    def rhs(t,state):return ((lc-state[0])/TAU,(raw(t,state[0])-state[1])/AC)
    for i in range(steps):
        t=i*h;a=rhs(t,(li,coupling))
        b=rhs(t+h/2,(li+h*a[0]/2,coupling+h*a[1]/2))
        c=rhs(t+h/2,(li+h*b[0]/2,coupling+h*b[1]/2))
        d=rhs(t+h,(li+h*c[0],coupling+h*c[1]))
        li+=h*(a[0]+2*b[0]+2*c[0]+d[0])/6
        coupling+=h*(a[1]+2*b[1]+2*c[1]+d[1])/6
    return li,coupling,raw(duration,li)-coupling

def load(path,name):
    sys.path.insert(0,str(ROOT/'scripts'))
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module

SCRIPT=ROOT/'scripts/render_event_aware_ring_vca.py'
class RingVcaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.ref=load(SCRIPT,'ring_vca_under_test')
    def model(self,**changes):
        args=dict(attack=Q('.01'),decay=Q('.02'),sustain=Q('.4'),release=Q('.03'),input_amplitude=Q('.4'),
                  input_frequency=Q(230),input_phase=Q('.125'),carrier_frequency=Q(200),carrier_phase=Q('.25'),amount=Q('.6'),
                  lc=Q('.8'),ei=Q('.3'),initial_li=Q('.2'),initial_coupling=Q('.013'))
        args.update(changes);return self.ref.Composition(**args)

    def test_direct_ring_waveform_and_offsets_counted_once(self):
        for frequency in (130,230):
            model=self.model(input_frequency=Q(frequency));model.event(Q(0),'note_on')
            for t in (Q('.0001'),Q('.00137'),Q('.004')):
                s=model.advance(t);ring=direct_ring(float(t),frequency=frequency)
                self.assertAlmostEqual(s['ring_output'],ring,delta=3e-15)
                self.assertAlmostEqual(s['raw'],(ring+OFFSET+FEED)*2**.3*s['li']*s['envelope'],delta=3e-15)
                self.assertAlmostEqual(s['output'],s['raw']-s['coupling'],delta=3e-15)

    def test_idle_and_nonzero_initial_coupling_once(self):
        idle=self.model()
        for t in (Q(0),Q('.0001'),Q('.004')):
            s=idle.advance(t);expected=.013*math.exp(-float(t)/AC)
            self.assertAlmostEqual(s['coupling'],expected,delta=3e-15)
            self.assertEqual(s['output'],-s['coupling'])
        a,b=self.model(),self.model(initial_coupling=Q(0))
        a.event(Q(0),'note_on');b.event(Q(0),'note_on')
        for t in (Q('.0001'),Q('.004')):
            x,y=a.advance(t),b.advance(t)
            self.assertAlmostEqual(x['coupling']-y['coupling'],.013*math.exp(-float(t)/AC),delta=3e-15)
            self.assertAlmostEqual(x['output']-y['output'],-.013*math.exp(-float(t)/AC),delta=3e-15)

    def test_smooth_short_and_negative_difference_frequency_rk4(self):
        for duration,steps,frequency,phase in [(Q('.004'),256,230,.125),(Q('.0001'),32,130,0.)]:
            model=self.model(input_frequency=Q(frequency),input_phase=Q(phase));model.event(Q(0),'note_on')
            env=lambda t:1.5*(1-3**(-t/.01))
            coarse=rk4_direct(float(duration),steps,env,frequency=frequency,phase=phase)
            fine=rk4_direct(float(duration),steps*2,env,frequency=frequency,phase=phase)
            got=model.advance(duration)
            for key,x,y in zip(('li','coupling','output'),coarse,fine):
                self.assertLess(abs(x-y),2e-9,key+' refinement')
                self.assertAlmostEqual(got[key],y,delta=2e-10,msg=key)

    def test_zero_amount_identity_with_prior_sine_exporter(self):
        prior=load(ROOT/'scripts/render_event_aware_envelope_sine_vca.py','prior_sine')
        old=prior.Composition(Q('.01'),Q('.02'),Q('.4'),Q('.03'),Q('.4'),Q(230),Q('.125'),Q('.8'),Q('.3'),Q('.2'),Q('.013'))
        new=self.model(amount=Q(0))
        for m in (old,new):m.event(Q('.00037'),'note_on')
        for t in (Q('.001'),Q('.004'),Q('.013')):
            x,y=old.advance(t),new.advance(t)
            for key in ('li','coupling','raw','output'):self.assertAlmostEqual(x[key],y[key],delta=5e-14,msg=key)

    def test_zero_amplitude_carrier_feedthrough_and_dc(self):
        model=self.model(input_amplitude=Q(0));model.event(Q(0),'note_on')
        for t in (Q('.0001'),Q('.00137'),Q('.004')):
            s=model.advance(t)
            expected=.6*(.0005*(math.sin(2*math.pi*(200*float(t)+.25))+.005)+.001)
            self.assertEqual(s['input'],0)
            self.assertAlmostEqual(s['ring_output'],expected,delta=2e-18)
        coarse=rk4_direct(.004,256,lambda t:1.5*(1-3**(-t/.01)),amplitude=0.)
        fine=rk4_direct(.004,512,lambda t:1.5*(1-3**(-t/.01)),amplitude=0.)
        for key,x,y in zip(('li','coupling','output'),coarse,fine):
            self.assertLess(abs(x-y),2e-9)
            self.assertAlmostEqual(s[key],y,delta=2e-10,msg=key)

    def test_events_continuity_and_explicit_reset_rk4_split(self):
        model=self.model();model.event(Q(0),'note_on')
        before=model.advance(Q('.004'));model.event(Q('.004'),'reset');after=model.advance(Q('.004'))
        for key in ('li','coupling','ring_output'):self.assertEqual(before[key],after[key])
        self.assertEqual(after['output'],-after['coupling'])
        self.assertNotEqual(before['output'],after['output'])
        results=[]
        for steps in (256,512):
            first=rk4_direct(.004,steps,lambda t:1.5*(1-3**(-t/.01)))
            results.append(rk4_direct(.002,steps,lambda t:0.,start=.004,li=first[0],coupling=first[1]))
        got=model.advance(Q('.006'))
        for key,x,y in zip(('li','coupling','output'),*results):
            self.assertLess(abs(x-y),2e-9)
            self.assertAlmostEqual(got[key],y,delta=2e-10,msg=key)

    def test_shared_exact_grid_and_same_time_events(self):
        events=[(Q('.00037'),'note_on'),(Q('.003'),'note_off'),(Q('.003'),'retrigger'),(Q('.00619'),'reset')]
        def grid(rate):
            model=self.model(input_frequency=Q(900),carrier_frequency=Q(1000));states=[];cursor=0
            for i in range(rate//100+1):
                t=Q(i,rate)
                while cursor<len(events) and events[cursor][0]<=t:model.event(*events[cursor]);cursor+=1
                states.append(model.advance(t))
            return states
        a,b=grid(8000),grid(16000)
        for x,y in zip(a,b[::2]):
            for key in ('li','coupling','input','carrier','ring_output','envelope','output','stage'):self.assertEqual(x[key],y[key],key)
        self.assertEqual(a[24]['stage'],'attack')
        self.assertEqual(self.model().advance(Q(0))['carrier_cycles'],.25)

    def test_offgrid_noteoff_retrigger_retains_state(self):
        model=self.model();model.event(Q('.00037'),'note_on');t=Q('.00311');before=model.advance(t)
        model.event(t,'note_off');model.event(t,'retrigger');after=model.advance(t)
        for key in ('li','coupling','envelope','ring_output','raw','output'):self.assertAlmostEqual(before[key],after[key],delta=3e-15,msg=key)
        self.assertEqual(after['stage'],'attack')

    def test_valid_cli_export_and_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);events=root/'events.csv';out=root/'export'
            data=b'event,time_s,action\n0,0.00037,note_on\n1,0.00311,note_off\n2,0.00473,retrigger\n3,0.00619,reset\n4,0.01,note_on\n'
            events.write_bytes(data)
            cmd=[sys.executable,str(SCRIPT),'--events',str(events),'--attack','.01','--decay','.02','--sustain','.4','--release','.03',
                 '--input-amplitude','.4','--input-frequency','130','--input-phase','.125','--carrier-frequency','200','--carrier-phase','.25',
                 '--amount','.6','--lc','.8','--ei','.3','--initial-li','.2','--initial-coupling','.013','--rate','8000','--samples','81','--out',str(out)]
            result=subprocess.run(cmd,capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            m=json.loads((out/'manifest.json').read_text());self.assertIs(m['complete'],True)
            self.assertEqual(m['schema'],'cs80-event-aware-ring-vca-v1')
            self.assertEqual((m['samples'],m['intervals'],m['rate_hz']),(81,80,8000))
            e=m['endpoint_span'];self.assertEqual(Q(e['numerator'],e['denominator']),Q('.01'))
            with (out/'trace.csv').open() as f:rows=list(csv.DictReader(f))
            self.assertEqual(len(rows),81);self.assertEqual([int(r['sample']) for r in rows],list(range(81)))
            self.assertEqual(float(rows[0]['time_s']),0);self.assertEqual(float(rows[-1]['time_s']),.01)
            self.assertEqual(rows[-1]['last_event'],'4');self.assertEqual(rows[-1]['stage'],'attack')
            self.assertEqual((out/'input_events.csv').read_bytes(),data)
            for name in ('render_event_aware_envelope_vca.py','render_amplitude_envelope_reference.py','render_event_aware_envelope_sine_vca.py','render_ring_reference.py'):
                self.assertEqual((out/name).read_bytes(),(ROOT/'scripts'/name).read_bytes())
            self.assertEqual((out/'renderer.py').read_bytes(),SCRIPT.read_bytes())
            self.assertEqual({p.name for p in out.iterdir()},set(m['artifacts'])|{'manifest.json'})
            for name,meta in m['artifacts'].items():
                payload=(out/name).read_bytes();self.assertEqual(len(payload),meta['bytes'],name)
                self.assertEqual(hashlib.sha256(payload).hexdigest(),meta['sha256'],name)

if __name__=='__main__':unittest.main()
