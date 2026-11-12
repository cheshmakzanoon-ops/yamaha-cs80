#!/usr/bin/env python3
"""Targeted valid coincident-frequency checks using the prior independent direct oracle."""
from fractions import Fraction as Q
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from test_event_aware_ring_vca import ROOT,SCRIPT,load,direct_ring,rk4_direct

class CoincidentRingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.ref=load(SCRIPT,'coincident_ring_under_test')
    def model(self,**changes):
        args=dict(attack=Q('.01'),decay=Q('.02'),sustain=Q('.4'),release=Q('.03'),input_amplitude=Q('.4'),
                  input_frequency=Q(200),input_phase=Q('.125'),carrier_frequency=Q(200),carrier_phase=Q('.125'),amount=Q('.6'),
                  lc=Q('.8'),ei=Q('.3'),initial_li=Q('.2'),initial_coupling=Q('.013'))
        args.update(changes);return self.ref.Composition(**args)

    def test_signed_and_quadrature_dc_direct_equation(self):
        #160 exclusive endpoints cover four200Hz cycles; no window leakage.
        for carrier_phase,product_dc in [(Q('.125'),.12),(Q('.625'),-.12),(Q('.375'),0.)]:
            model=self.model(carrier_phase=carrier_phase)
            values=[]
            for i in range(160):
                t=Q(i,8000);s=model.advance(t)
                expected=direct_ring(float(t),frequency=200.,carrier_frequency=200.,phase=.125,carrier_phase=float(carrier_phase))
                self.assertAlmostEqual(s['ring_output'],expected,delta=5e-15)
                values.append(s['ring_output'])
            self.assertAlmostEqual(math.fsum(values)/160,.6*(.0005*.005+.001)+product_dc,delta=2e-14)

    def test_matched_normal_and_short_real_rk4(self):
        for duration,steps in [(Q('.004'),256),(Q('.0001'),32)]:
            for phase in (Q('.125'),Q('.625'),Q('.375')):
                model=self.model(carrier_phase=phase);model.event(Q(0),'note_on')
                env=lambda t:1.5*(1-3**(-t/.01))
                coarse=rk4_direct(float(duration),steps,env,frequency=200.,carrier_frequency=200.,phase=.125,carrier_phase=float(phase))
                fine=rk4_direct(float(duration),2*steps,env,frequency=200.,carrier_frequency=200.,phase=.125,carrier_phase=float(phase))
                got=model.advance(duration)
                for key,x,y in zip(('li','coupling','output'),coarse,fine):
                    self.assertLess(abs(x-y),2e-9,key+' refinement')
                    self.assertAlmostEqual(got[key],y,delta=2e-10,msg=key)

    def test_initial_coupling_is_counted_once(self):
        a,b=self.model(),self.model(initial_coupling=Q(0))
        for m in (a,b):m.event(Q(0),'note_on')
        for t in [Q(0),Q('.0001'),Q('.004')]:
            x,y=a.advance(t),b.advance(t);difference=.013*math.exp(-float(t)/.1)
            self.assertAlmostEqual(x['coupling']-y['coupling'],difference,delta=5e-15)
            self.assertAlmostEqual(x['output']-y['output'],-difference,delta=5e-15)

    def test_reset_split_rk4(self):
        model=self.model();model.event(Q(0),'note_on')
        before=model.advance(Q('.004'));model.event(Q('.004'),'reset');after=model.advance(Q('.004'))
        for key in ('li','coupling','ring_output'):self.assertEqual(before[key],after[key])
        self.assertEqual(after['output'],-after['coupling'])
        answers=[]
        for steps in (256,512):
            first=rk4_direct(.004,steps,lambda t:1.5*(1-3**(-t/.01)),frequency=200.,carrier_frequency=200.,carrier_phase=.125)
            answers.append(rk4_direct(.002,steps,lambda t:0.,start=.004,li=first[0],coupling=first[1],frequency=200.,carrier_frequency=200.,carrier_phase=.125))
        got=model.advance(Q('.006'))
        for key,x,y in zip(('li','coupling','output'),*answers):
            self.assertLess(abs(x-y),2e-9)
            self.assertAlmostEqual(got[key],y,delta=2e-10,msg=key)

    def test_exact_shared_grid_with_events(self):
        events=[(Q('.00037'),'note_on'),(Q('.00311'),'note_off'),(Q('.00473'),'retrigger'),(Q('.00619'),'reset')]
        def grid(rate):
            model=self.model();cursor=0;rows=[]
            for i in range(rate//100+1):
                t=Q(i,rate)
                while cursor<len(events) and events[cursor][0]<=t:model.event(*events[cursor]);cursor+=1
                rows.append(model.advance(t))
            return rows
        a,b=grid(8000),grid(16000)
        for x,y in zip(a,b[::2]):
            for key in ('li','coupling','ring_output','envelope','output','stage'):self.assertEqual(x[key],y[key],key)

    def test_valid_matched_cli_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);events=root/'events.csv';out=root/'export'
            data=b'event,time_s,action\n0,0.00037,note_on\n1,0.00311,note_off\n2,0.00473,retrigger\n3,0.00619,reset\n4,0.01,note_on\n'
            events.write_bytes(data)
            cmd=[sys.executable,str(SCRIPT),'--events',str(events),'--attack','.01','--decay','.02','--sustain','.4','--release','.03',
                 '--input-amplitude','.4','--input-frequency','200','--input-phase','.125','--carrier-frequency','200','--carrier-phase','.625',
                 '--amount','.6','--lc','.8','--ei','.3','--initial-li','.2','--initial-coupling','.013','--rate','8000','--samples','81','--out',str(out)]
            result=subprocess.run(cmd,capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            m=json.loads((out/'manifest.json').read_text());self.assertIs(m['complete'],True)
            self.assertEqual((m['samples'],m['intervals'],m['rate_hz']),(81,80,8000))
            e=m['endpoint_span'];self.assertEqual(Q(e['numerator'],e['denominator']),Q('.01'))
            with (out/'trace.csv').open() as f:rows=list(csv.DictReader(f))
            self.assertEqual(len(rows),81);self.assertEqual(rows[-1]['last_event'],'4');self.assertEqual(rows[-1]['stage'],'attack')
            self.assertEqual((out/'input_events.csv').read_bytes(),data)
            self.assertEqual((out/'renderer.py').read_bytes(),SCRIPT.read_bytes())
            self.assertEqual({p.name for p in out.iterdir()},set(m['artifacts'])|{'manifest.json'})
            for name,meta in m['artifacts'].items():
                payload=(out/name).read_bytes();self.assertEqual(len(payload),meta['bytes'],name)
                self.assertEqual(hashlib.sha256(payload).hexdigest(),meta['sha256'],name)

if __name__=='__main__':unittest.main()
