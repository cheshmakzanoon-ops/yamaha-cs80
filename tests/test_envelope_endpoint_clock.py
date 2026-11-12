#!/usr/bin/env python3
"""Independent bounded checks of one retained valid endpoint-clock diagnostic."""
import argparse
import csv
from fractions import Fraction as Q
import hashlib
import json
import math
from pathlib import Path
import struct
import unittest

PARSER=argparse.ArgumentParser()
PARSER.add_argument('--bundle',required=True)
ARGS,REST=PARSER.parse_known_args()
BUNDLE=Path(ARGS.bundle)
TAU=1/(1/180000+1/560000+3/270000)*1e-8
AC=.1
F32=lambda x:struct.unpack('f',struct.pack('f',x))[0]
OFFSET,FEED=F32(.0015),F32(.001)

def csv_rows(path):
    with path.open() as f:return list(csv.DictReader(f))

def pwl_oracle(rows,rate,subdivisions):
    """Exact LI, independent Simpson quadrature of integrating-factor forcing.

    Splits every endpoint interval; interpolates audio and envelope separately.
    No new orchestrator math helpers or native RK4 stages are reused.
    """
    initial=.1;lc=.8;ei=-.5;coupling=0.;h=1/rate
    li=lambda t:lc+(initial-lc)*math.exp(-t/TAU)
    def raw(level,audio,envelope):return (audio+OFFSET+FEED)*2**ei*level*envelope
    result=[(initial,0.,raw(initial,float(rows[0]['input']),float(rows[0]['envelope'])))]
    for i,(a,b) in enumerate(zip(rows,rows[1:])):
        x0,x1=float(a['input']),float(b['input']);e0,e1=float(a['envelope']),float(b['envelope'])
        def integrand(s):
            phase=s/h
            return math.exp(-(h-s)/AC)/AC*raw(li(i*h+s),x0+(x1-x0)*phase,e0+(e1-e0)*phase)
        dt=h/subdivisions
        total=integrand(0)+integrand(h)
        total+=sum((4 if j%2 else 2)*integrand(j*dt) for j in range(1,subdivisions))
        coupling=coupling*math.exp(-h/AC)+total*dt/3
        level=li((i+1)*h)
        result.append((level,coupling,raw(level,x1,e1)-coupling))
    return result

class EndpointClockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report=json.loads((BUNDLE/'report.json').read_text())
        cls.manifest=json.loads((BUNDLE/'manifest.json').read_text())
        cls.common=csv_rows(BUNDLE/'common.csv')

    def test_complete_inventory_and_sources(self):
        self.assertIs(self.manifest['complete'],True)
        self.assertEqual(self.manifest['schema'],'cs80-envelope-endpoint-clock-bundle-v1')
        self.assertFalse(self.manifest['compare_realtime'])
        self.assertEqual({str(p.relative_to(BUNDLE)) for p in BUNDLE.rglob('*') if p.is_file()},set(self.manifest['artifacts'])|{'manifest.json'})
        for name,meta in self.manifest['artifacts'].items():
            data=(BUNDLE/name).read_bytes()
            self.assertEqual(len(data),meta['bytes'],name)
            self.assertEqual(hashlib.sha256(data).hexdigest(),meta['sha256'],name)
        for name,meta in self.manifest['source_snapshots'].items():
            self.assertEqual(hashlib.sha256((BUNDLE/meta['artifact']).read_bytes()).hexdigest(),meta['sha256'],name)

    def test_native_contract_and_refinement_separate(self):
        for rate in (8000,16000,32000):
            native=json.loads((BUNDLE/f'native_{rate}/manifest.json').read_text())
            report=self.report['rate_results'][str(rate)]
            self.assertFalse(native['compare_realtime'])
            self.assertEqual(native['initial_li'],.1)
            self.assertEqual(native['samples'],rate//50+1)
            self.assertEqual((native['coarse_steps_per_interval'],native['fine_steps_per_interval']),(8,16))
            self.assertEqual(report['native_refinement'],native['max_abs_h_h2_difference'])
            self.assertEqual((report['coarse_steps'],report['fine_steps']),(8,16))
            self.assertNotIn('--compare-realtime',str(self.manifest['commands']))

    def test_common_endpoint_values_and_deltas(self):
        self.assertEqual(len(self.common),483)
        self.assertEqual(self.report['common_endpoint_count'],161)
        analytic_reference=None
        for rate in (8000,16000,32000):
            selected=[r for r in self.common if int(r['rate'])==rate]
            self.assertEqual(len(selected),161)
            native=csv_rows(BUNDLE/f'native_{rate}/trace.csv')
            analytic=csv_rows(BUNDLE/f'analytic_{rate}/trace.csv')
            values=[]
            for j,row in enumerate(selected):
                n=j*(rate//8000)
                self.assertEqual((int(row['sample_8000']),int(row['sample_native'])),(j,n))
                self.assertEqual(float(row['time_s']),float(Q(j,8000)))
                for key in ('li','coupling','output'):
                    a,b=float(analytic[n][key]),float(native[n][key])
                    self.assertEqual(float(row['analytic_'+key]),a)
                    self.assertEqual(float(row['native_'+key]),b)
                    self.assertEqual(float(row['delta_'+key]),b-a)
                values.append(tuple(float(analytic[n][k]) for k in ('li','coupling','output')))
            if analytic_reference is None:analytic_reference=values
            else:self.assertEqual(values,analytic_reference)

    def test_bucket_membership_and_metrics_recomputed(self):
        boundaries={Q(t['numerator'],t['denominator']) for t in self.report['boundaries']}
        self.assertEqual(len(boundaries),len(self.report['boundaries']))
        for rate in (8000,16000,32000):
            rows=[r for r in self.common if int(r['rate'])==rate]
            for row in rows:
                t=Q(int(row['sample_8000']),8000)
                expected='t0' if t==0 else ('boundary_near' if any(t-Q(1,8000)<b<=t for b in boundaries) else 'smooth')
                self.assertEqual(row['bucket'],expected)
            for group in ('all','t0','boundary_near','smooth'):
                chosen=rows if group=='all' else [r for r in rows if r['bucket']==group]
                reported=self.report['rate_results'][str(rate)]['metrics'][group]
                self.assertEqual(reported['count'],len(chosen))
                for key in ('li','coupling','output'):
                    numbers=[float(r['delta_'+key]) for r in chosen]
                    self.assertEqual(reported[key]['max_abs'],max(map(abs,numbers),default=0))
                    if numbers:self.assertAlmostEqual(reported[key]['rms'],math.sqrt(sum(x*x for x in numbers)/len(numbers)),delta=1e-15)
                    else:self.assertIsNone(reported[key]['rms'])

    def test_native_forcing_matches_analytic_endpoints(self):
        for rate in (8000,16000,32000):
            forcing=csv_rows(BUNDLE/f'inputs/native_{rate}.csv')
            analytic=csv_rows(BUNDLE/f'analytic_{rate}/trace.csv')
            self.assertEqual(len(forcing),len(analytic))
            for i,(a,b) in enumerate(zip(forcing,analytic)):
                self.assertEqual(int(a['sample']),i)
                for key in ('input','envelope'):self.assertEqual(float(a[key]),float(b[key]))
                self.assertEqual((float(a['lc']),float(a['ei'])),(.8,-.5))

    def test_native_fine_vs_independent_pwl_quadrature(self):
        for rate in (8000,16000,32000):
            forcing=csv_rows(BUNDLE/f'inputs/native_{rate}.csv')
            native=csv_rows(BUNDLE/f'native_{rate}/trace.csv')
            coarse=pwl_oracle(forcing,rate,32)
            fine=pwl_oracle(forcing,rate,64)
            for row,a,b in zip(native,coarse,fine):
                for key,x,y in zip(('li','coupling','output'),a,b):
                    self.assertLess(abs(x-y),1e-12,key+' independent quadrature refinement')
                    self.assertAlmostEqual(float(row[key]),y,delta=3e-9,msg=f'{rate} {key}')
            # This check concerns the same PWL forcing only. It is deliberately
            # separate from native-minus-event-aware discrepancy and does not
            # assert monotonic convergence across sample rates.

if __name__=='__main__':unittest.main(argv=['test_envelope_endpoint_clock.py']+REST)
