#!/usr/bin/env python3
"""Bounded valid-input phase/quadrature/spectrum/export checks, no broad suites."""
import csv
from fractions import Fraction as Q
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SCRIPT=ROOT/'scripts/render_ring_reference.py'

def simpson_voltage_cycles(v0,v1,duration,steps):
    # Independent numerical integral of the voltage-to-frequency law, not the
    # renderer's expm1 integral or phase_increment function.
    h=duration/steps
    def f(t): return (1/.15)*30**((v0+(v1-v0)*t/duration-5)/5)
    total=f(0)+f(duration)
    total+=sum((4 if j%2 else 2)*f(j*h) for j in range(1,steps))
    return total*h/3

class RingTimeReferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sys.path.insert(0,str(ROOT/'scripts'))
        spec=importlib.util.spec_from_file_location('ring_time_under_test',SCRIPT)
        cls.ref=importlib.util.module_from_spec(spec);spec.loader.exec_module(cls.ref)

    def model(self,knots=None,**changes):
        args=dict(input_amplitude=Q('.4'),input_frequency=Q(700),input_phase=Q('.125'),
                  amount=Q('.6'),carrier_initial_phase=Q('.25'))
        args.update(changes)
        return self.ref.Composition(knots or [(Q(0),Q(10)),(Q('.2'),Q(10))],**args)

    def test_converter_anchors(self):
        self.assertAlmostEqual(self.ref.hz_for_volts(5),1/.15,delta=2e-15)
        self.assertAlmostEqual(self.ref.hz_for_volts(10),200,delta=2e-13)

    def test_constant_voltage_phase_and_no_initial_tick(self):
        model=self.model()
        for t in [Q(0),Q('.00037'),Q('.01'),Q('.2')]:
            s=model.advance(t)
            self.assertAlmostEqual(s['carrier_cycles'],.25+200*float(t),delta=2e-13)
            self.assertAlmostEqual(s['carrier'],math.sin(2*math.pi*(.25+200*float(t))),delta=2e-12)
        self.assertEqual(self.model().advance(Q(0))['carrier_cycles'],.25)

    def test_linear_voltage_phase_against_refined_quadrature(self):
        duration=.023
        coarse=simpson_voltage_cycles(5,8,duration,64)
        fine=simpson_voltage_cycles(5,8,duration,128)
        got=self.ref.phase_increment(Q(5),Q(8),Q('.023'))
        self.assertLess(abs(coarse-fine),2e-8)
        self.assertAlmostEqual(got,fine,delta=2e-9)
        halfway=self.ref.phase_increment(Q(5),Q(8),Q('.023'),Q('.0115'))
        self.assertAlmostEqual(halfway,simpson_voltage_cycles(5,6.5,.0115,128),delta=2e-10)

    def test_zero_and_nearzero_voltage_slopes(self):
        duration=Q('.01')
        self.assertAlmostEqual(self.ref.phase_increment(Q(5),Q(5),duration),float(duration)/.15,delta=2e-16)
        for endpoint in [Q('5.000000001'),Q('4.999999999')]:
            got=self.ref.phase_increment(Q(5),endpoint,duration)
            expected=simpson_voltage_cycles(5,float(endpoint),float(duration),64)
            self.assertAlmostEqual(got,expected,delta=3e-16)

    def test_knots_and_exact_shared_sample_grid(self):
        knots=[(Q(0),Q(5)),(Q('.0037'),Q(8)),(Q('.0061'),Q(6)),(Q('.01'),Q(7))]
        a,b=self.model(knots),self.model(knots)
        coarse=[a.advance(Q(i,8000)) for i in range(81)]
        fine=[b.advance(Q(i,16000)) for i in range(161)]
        for x,y in zip(coarse,fine[::2]):
            for key in ('carrier_cycles','carrier','input','wet','output','volts'):
                self.assertEqual(x[key],y[key],key)
        phase=.25
        for (t0,v0),(t1,v1) in zip(knots,knots[1:]):
            phase+=simpson_voltage_cycles(float(v0),float(v1),float(t1-t0),256)
            state=self.model(knots).advance(t1)
            self.assertEqual(state['volts'],float(v1))
            self.assertAlmostEqual(state['carrier_cycles'],phase,delta=2e-10)

    def test_interior_knot_has_continuous_phase_and_output(self):
        knots=[(Q(0),Q(5)),(Q('.0037'),Q(8)),(Q('.01'),Q(6))]
        t=Q('.0037'); epsilon=Q('.0000001')
        left=self.model(knots).advance(t-epsilon)
        at=self.model(knots).advance(t)
        right=self.model(knots).advance(t+epsilon)
        expected_hz=(1/.15)*30**(.6)
        for x,y in [(left,at),(at,right)]:
            self.assertAlmostEqual((y['carrier_cycles']-x['carrier_cycles'])/float(epsilon),expected_hz,delta=.02)
            self.assertLess(abs(y['output']-x['output']),.001)

    def test_zero_amount_is_exact_dry_signal(self):
        model=self.model(amount=Q(0))
        for t in [Q(0),Q('.00037'),Q('.013')]:
            s=model.advance(t)
            self.assertEqual(s['output'],s['input'])

    def test_integer_cycle_spectrum_signed_coefficients(self):
        model=self.model(); rate=8000; n=1600
        output=[model.advance(Q(i,rate))['output'] for i in range(n)]
        def projection(hz):
            return (2/n*sum(y*math.cos(2*math.pi*hz*i/rate) for i,y in enumerate(output)),
                    2/n*sum(y*math.sin(2*math.pi*hz*i/rate) for i,y in enumerate(output)))
        amount,amp=.6,.4; pi,pc=2*math.pi*.125,2*math.pi*.25
        self.assertAlmostEqual(sum(output)/n,amount*(.0005*.005+.001),delta=2e-13)
        expected={200:(amount*.0005*math.sin(pc),amount*.0005*math.cos(pc)),
                  700:(amp*(1-amount+amount*.005)*math.sin(pi),amp*(1-amount+amount*.005)*math.cos(pi)),
                  500:(amount*amp/2*math.cos(pi-pc),-amount*amp/2*math.sin(pi-pc)),
                  900:(-amount*amp/2*math.cos(pi+pc),amount*amp/2*math.sin(pi+pc))}
        for hz,want in expected.items():
            for got,value in zip(projection(hz),want):self.assertAlmostEqual(got,value,delta=3e-12,msg=str(hz))

    def test_ordinary_cli_export_hashes_and_endpoints(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);knots=root/'knots.csv';out=root/'new-export'
            data=b'knot,time_s,volts\n0,0,5\n1,0.0037,8\n2,0.0061,6\n3,0.01,7\n'
            knots.write_bytes(data)
            command=[sys.executable,str(SCRIPT),'--knots',str(knots),'--input-amplitude','.4',
                     '--input-frequency','700','--input-phase','.125','--amount','.6',
                     '--carrier-initial-phase','.25','--rate','8000','--samples','81','--out',str(out)]
            result=subprocess.run(command,capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            manifest=json.loads((out/'manifest.json').read_text())
            self.assertIs(manifest['complete'],True)
            self.assertEqual(manifest['schema'],'cs80-ring-time-reference-v1')
            self.assertEqual((manifest['samples'],manifest['intervals'],manifest['rate_hz']),(81,80,8000))
            end=manifest['endpoint_span']
            self.assertEqual(Q(end['numerator'],end['denominator']),Q('.01'))
            with (out/'trace.csv').open() as f:rows=list(csv.DictReader(f))
            self.assertEqual(len(rows),81)
            self.assertEqual([int(row['sample']) for row in rows],list(range(81)))
            self.assertEqual(float(rows[0]['time_s']),0)
            self.assertEqual(float(rows[-1]['time_s']),.01)
            self.assertEqual(float(rows[0]['carrier_cycles']),.25)
            self.assertEqual(float(rows[-1]['volts']),7)
            self.assertEqual((out/'input_knots.csv').read_bytes(),data)
            for name,source in [('renderer.py',SCRIPT),('render_amplitude_envelope_reference.py',ROOT/'scripts/render_amplitude_envelope_reference.py'),('RingModulator.h',ROOT/'src/reference/RingModulator.h')]:
                self.assertEqual((out/name).read_bytes(),source.read_bytes())
            self.assertEqual({p.name for p in out.iterdir()},set(manifest['artifacts'])|{'manifest.json'})
            for name,meta in manifest['artifacts'].items():
                payload=(out/name).read_bytes()
                self.assertEqual(len(payload),meta['bytes'],name)
                self.assertEqual(hashlib.sha256(payload).hexdigest(),meta['sha256'],name)

if __name__=='__main__':unittest.main()
