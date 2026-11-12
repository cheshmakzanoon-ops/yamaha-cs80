#!/usr/bin/env python3
"""Independent bounded valid-input sine/envelope/VCA composition checks."""
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

def rk4_sine(duration,steps,envelope,start=0.,li=.2,coupling=0.,lc=.8,ei=.3,amplitude=.4,frequency=230.,phase=.125):
    """Independent real-valued differential equations, not complex convolution."""
    h=duration/steps
    def raw(t,level):
        audio=amplitude*math.sin(2*math.pi*(frequency*(start+t)+phase))
        return (audio+OFFSET+FEED)*2**ei*level*envelope(t)
    def rhs(t,state):return ((lc-state[0])/TAU,(raw(t,state[0])-state[1])/AC)
    for i in range(steps):
        t=i*h;a=rhs(t,(li,coupling))
        b=rhs(t+h/2,(li+h*a[0]/2,coupling+h*a[1]/2))
        c=rhs(t+h/2,(li+h*b[0]/2,coupling+h*b[1]/2))
        d=rhs(t+h,(li+h*c[0],coupling+h*c[1]))
        li+=h*(a[0]+2*b[0]+2*c[0]+d[0])/6
        coupling+=h*(a[1]+2*b[1]+2*c[1]+d[1])/6
    return li,coupling,raw(duration,li)-coupling

SCRIPT=ROOT/'scripts/render_event_aware_envelope_sine_vca.py'
def load(path,name):
    sys.path.insert(0,str(ROOT/'scripts'))
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module

class EnvelopeSineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.ref=load(SCRIPT,'sine_vca_under_test')
    def model(self,**changes):
        params=dict(attack=Q('.01'),decay=Q('.02'),sustain=Q('.4'),release=Q('.03'),
                    input_amplitude=Q('.4'),input_frequency=Q(230),input_phase=Q('.125'),
                    lc=Q('.8'),ei=Q('.3'),initial_li=Q('.2'),initial_coupling=Q(0))
        params.update(changes)
        return self.ref.Composition(**params)

    def test_idle_and_zero_lc_controls(self):
        model=self.model(initial_coupling=Q('.02'))
        for t in [Q(0),Q('.00037'),Q('.004')]:
            s=model.advance(t)
            self.assertEqual(s['envelope'],0)
            self.assertAlmostEqual(s['coupling'],.02*math.exp(-float(t)/AC),delta=3e-15)
            self.assertEqual(s['output'],-s['coupling'])
        model=self.model(lc=Q(0),initial_li=Q(0));model.event(Q(0),'note_on')
        s=model.advance(Q('.004'))
        self.assertEqual((s['li'],s['coupling'],s['output']),(0,0,0))

    def test_smooth_and_short_segments_refined_rk4(self):
        for duration,steps,phase in [(Q('.004'),256,Q('.125')),(Q('.0001'),32,Q(0))]:
            model=self.model(input_phase=phase,initial_coupling=Q('.013'));model.event(Q(0),'note_on');got=model.advance(duration)
            env=lambda t:1.5*(1-3**(-t/.01))
            coarse=rk4_sine(float(duration),steps,env,phase=float(phase),coupling=.013)
            fine=rk4_sine(float(duration),steps*2,env,phase=float(phase),coupling=.013)
            for key,a,b in zip(('li','coupling','output'),coarse,fine):
                self.assertLess(abs(a-b),2e-9,key+' refinement')
                self.assertAlmostEqual(got[key],b,delta=2e-10,msg=key)

    def test_constant_sustain_signed_sine_and_offset_response(self):
        model=self.model(lc=Q('.8'),initial_li=Q('.8'))
        model.event(Q(0),'note_on')
        t0=Q('.04');start=model.advance(t0)
        self.assertEqual(start['stage'],'sustain')
        gain=.8*2**.3*.4; b=gain*.4; dc=gain*(OFFSET+FEED);w=2*math.pi*230;q=10.
        def particular(t):
            angle=w*t+2*math.pi*.125
            return dc+b*q/(q*q+w*w)*(q*math.sin(angle)-w*math.cos(angle))
        for dt in [Q('.0001'),Q('.0013'),Q('.004')]:
            t=t0+dt;s=model.advance(t)
            cap=particular(float(t))+(start['coupling']-particular(float(t0)))*math.exp(-q*float(dt))
            raw=dc+b*math.sin(w*float(t)+2*math.pi*.125)
            self.assertAlmostEqual(s['coupling'],cap,delta=3e-14)
            self.assertAlmostEqual(s['output'],raw-cap,delta=3e-13)

    def test_reset_split_rk4_and_continuous_states(self):
        model=self.model();model.event(Q(0),'note_on')
        before=model.advance(Q('.004'));model.event(Q('.004'),'reset');after=model.advance(Q('.004'))
        for key in ('li','coupling'):self.assertEqual(before[key],after[key])
        self.assertEqual(after['output'],-after['coupling'])
        self.assertNotEqual(before['output'],after['output'])
        env=lambda t:1.5*(1-3**(-t/.01))
        results=[]
        for steps in (256,512):
            first=rk4_sine(.004,steps,env)
            results.append(rk4_sine(.002,steps,lambda t:0.,start=.004,li=first[0],coupling=first[1]))
        got=model.advance(Q('.006'))
        for key,a,b in zip(('li','coupling','output'),*results):
            self.assertLess(abs(a-b),2e-9)
            self.assertAlmostEqual(got[key],b,delta=2e-10,msg=key)

    def test_near_equal_decay_and_coupling_rate_refined_rk4(self):
        for rate in (10.,10.0001):
            duration=math.log(1000)/rate
            model=self.model(decay=Q(duration),sustain=Q(0),lc=Q('.8'),initial_li=Q('.8'))
            model.event(Q(0),'note_on');start=model.advance(Q('.01'))
            env=lambda t:1000**(-t/duration)
            coarse=rk4_sine(.004,256,env,start=.01,li=.8,coupling=start['coupling'])
            fine=rk4_sine(.004,512,env,start=.01,li=.8,coupling=start['coupling'])
            got=model.advance(Q('.014'))
            for key,a,b in zip(('li','coupling','output'),coarse,fine):
                self.assertLess(abs(a-b),2e-9)
                self.assertAlmostEqual(got[key],b,delta=2e-10,msg=key)

    def test_exact_shared_grids_high_frequency_and_event_order(self):
        events=[(Q('.00037'),'note_on'),(Q('.003'),'note_off'),(Q('.003'),'retrigger'),
                (Q('.00619'),'reset'),(Q('.00677'),'note_on')]
        def grid(rate):
            model=self.model(input_frequency=Q(1990));cursor=0;states=[]
            for i in range(rate//100+1):
                t=Q(i,rate)
                while cursor<len(events) and events[cursor][0]<=t:model.event(*events[cursor]);cursor+=1
                states.append(model.advance(t))
            return states
        a,b=grid(8000),grid(16000)
        for x,y in zip(a,b[::2]):
            for key in ('li','coupling','input','input_cycles','envelope','output','stage'):self.assertEqual(x[key],y[key],key)
        self.assertEqual(a[24]['stage'],'attack')

    def test_offgrid_retrigger_retains_level_and_audio_phase(self):
        model=self.model();model.event(Q('.00037'),'note_on');t=Q('.00311');before=model.advance(t)
        model.event(t,'note_off');model.event(t,'retrigger');after=model.advance(t)
        for key in ('li','coupling','envelope','input','output'):self.assertAlmostEqual(before[key],after[key],delta=3e-15)
        self.assertAlmostEqual(after['input'],.4*math.sin(2*math.pi*(230*float(t)+.125)),delta=2e-15)
        self.assertEqual(self.model().advance(Q(0))['input_cycles'],.125)

    def test_zero_audio_amplitude_matches_prior_constant_offset_model(self):
        previous=load(ROOT/'scripts/render_event_aware_envelope_vca.py','prior_constant_model')
        old=previous.Composition(Q('.01'),Q('.02'),Q('.4'),Q('.03'),Q(0),Q('.8'),Q('.3'),Q('.2'),Q(0))
        new=self.model(input_amplitude=Q(0))
        for model in (old,new):model.event(Q('.00037'),'note_on')
        for t in [Q('.001'),Q('.004'),Q('.013')]:
            a,b=old.advance(t),new.advance(t)
            for key in ('li','coupling','output'):self.assertAlmostEqual(a[key],b[key],delta=3e-15)

    def test_ordinary_cli_export_provenance_and_endpoints(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);events=root/'events.csv';out=root/'export'
            data=b'event,time_s,action\n0,0.00037,note_on\n1,0.00311,note_off\n2,0.00473,retrigger\n3,0.00619,reset\n4,0.01,note_on\n'
            events.write_bytes(data)
            cmd=[sys.executable,str(SCRIPT),'--events',str(events),'--attack','.01','--decay','.02','--sustain','.4','--release','.03',
                 '--input-amplitude','.4','--input-frequency','230','--input-phase','.125','--lc','.8','--ei','.3',
                 '--initial-li','.2','--initial-coupling','0','--rate','8000','--samples','81','--out',str(out)]
            result=subprocess.run(cmd,capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            manifest=json.loads((out/'manifest.json').read_text())
            self.assertIs(manifest['complete'],True)
            self.assertEqual(manifest['schema'],'cs80-event-aware-envelope-sine-vca-v1')
            self.assertEqual((manifest['samples'],manifest['intervals'],manifest['rate_hz']),(81,80,8000))
            endpoint=manifest['endpoint_span'];self.assertEqual(Q(endpoint['numerator'],endpoint['denominator']),Q('.01'))
            with (out/'trace.csv').open() as f:rows=list(csv.DictReader(f))
            self.assertEqual(len(rows),81)
            self.assertEqual([int(row['sample']) for row in rows],list(range(81)))
            self.assertEqual(float(rows[0]['time_s']),0)
            self.assertEqual(float(rows[-1]['time_s']),.01)
            self.assertEqual(rows[-1]['last_event'],'4')
            self.assertEqual(rows[-1]['stage'],'attack')
            self.assertEqual((out/'input_events.csv').read_bytes(),data)
            for name,source in [('renderer.py',SCRIPT),('render_event_aware_envelope_vca.py',ROOT/'scripts/render_event_aware_envelope_vca.py'),
                                ('render_amplitude_envelope_reference.py',ROOT/'scripts/render_amplitude_envelope_reference.py')]:
                self.assertEqual((out/name).read_bytes(),source.read_bytes())
            self.assertEqual({p.name for p in out.iterdir()},set(manifest['artifacts'])|{'manifest.json'})
            for name,meta in manifest['artifacts'].items():
                payload=(out/name).read_bytes();self.assertEqual(len(payload),meta['bytes'],name)
                self.assertEqual(hashlib.sha256(payload).hexdigest(),meta['sha256'],name)

if __name__=='__main__':unittest.main()
