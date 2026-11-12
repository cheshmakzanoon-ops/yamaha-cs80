#!/usr/bin/env python3
"""Bounded ordinary-input checks; independent ODE quadrature, not convolution reuse."""
from fractions import Fraction as Q
import csv
import hashlib
import json
import subprocess
import tempfile
import importlib.util
import math
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts/render_event_aware_envelope_vca.py'
TAU = 1 / (1 / 180000 + 1 / 560000 + 3 / 270000) * 1e-8
AC = .1
F32 = lambda x: struct.unpack('f', struct.pack('f', x))[0]
OFFSET, FEED = F32(.0015), F32(.001)


def rk4_ode(duration, steps, envelope, lc=.8, initial_li=.2, signal=.25, ei=.3, initial_coupling=0.):
    """Independent two-state differential equation; no renderer state/term helpers."""
    h = duration / steps
    li, coupling = initial_li, initial_coupling
    def rhs(t, state):
        level, cap = state
        raw = (signal + OFFSET + FEED) * 2**ei * level * envelope(t)
        return ((lc-level)/TAU, (raw-cap)/AC)
    for i in range(steps):
        t = i*h
        a = rhs(t, (li, coupling))
        b = rhs(t+h/2, (li+h*a[0]/2, coupling+h*a[1]/2))
        c = rhs(t+h/2, (li+h*b[0]/2, coupling+h*b[1]/2))
        d = rhs(t+h, (li+h*c[0], coupling+h*c[1]))
        li += h*(a[0]+2*b[0]+2*c[0]+d[0])/6
        coupling += h*(a[1]+2*b[1]+2*c[1]+d[1])/6
    raw = (signal+OFFSET+FEED)*2**ei*li*envelope(duration)
    return li, coupling, raw-coupling


def load_reference():
    sys.path.insert(0, str(ROOT/'scripts'))
    spec = importlib.util.spec_from_file_location('event_aware_reference_under_test', SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class EventAwareCompositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ref = load_reference()

    def model(self, **changes):
        params = dict(attack=Q('.01'), decay=Q('.02'), sustain=Q('.4'), release=Q('.03'),
                      lc=Q('.8'), ei=Q('.3'), audio_input=Q('.25'), initial_li=Q('.2'), initial_coupling=Q(0))
        params.update(changes)
        return self.ref.Composition(**params)

    def test_idle_zero_envelope_and_exact_li(self):
        model = self.model()
        for t in [Q(0), Q('.00037'), Q('.004')]:
            s = model.advance(t)
            expected = .8+(.2-.8)*math.exp(-float(t)/TAU)
            self.assertAlmostEqual(s['li'], expected, delta=3e-15)
            self.assertEqual(s['envelope'], 0)
            self.assertEqual(s['coupling'], 0)
            self.assertEqual(s['output'], 0)

    def test_zero_controls_silence_with_envelope_activity(self):
        model = self.model(lc=Q(0), initial_li=Q(0))
        model.event(Q('.00037'), 'note_on')
        for t in [Q('.001'), Q('.005'), Q('.02')]:
            s = model.advance(t)
            self.assertEqual(s['li'], 0)
            self.assertEqual(s['coupling'], 0)
            self.assertEqual(s['output'], 0)

    def test_smooth_attack_against_independent_refined_rk4(self):
        model = self.model()
        model.event(Q(0), 'note_on')
        got = model.advance(Q('.004'))
        env = lambda t: 1.5*(1-3**(-t/.01))
        coarse = rk4_ode(.004, 128, env)
        fine = rk4_ode(.004, 256, env)
        for key, a, b in zip(('li', 'coupling', 'output'), coarse, fine):
            self.assertLess(abs(a-b), 2e-9, key+' refinement')
            self.assertAlmostEqual(got[key], b, delta=2e-10, msg=key)

    def test_offgrid_events_and_reset_continuous_vca_states(self):
        model = self.model()
        model.event(Q('.00037'), 'note_on')
        for t, action in [(Q('.00311'),'note_off'), (Q('.00473'),'retrigger')]:
            before = model.advance(t)
            model.event(t, action)
            after = model.advance(t)
            for key in ('li','coupling','envelope','output'):
                self.assertAlmostEqual(before[key], after[key], delta=3e-15, msg=key)
        t = Q('.00619')
        before = model.advance(t)
        self.assertGreater(before['envelope'], 0)
        model.event(t, 'reset')
        after = model.advance(t)
        self.assertAlmostEqual(before['li'], after['li'], delta=3e-15)
        self.assertAlmostEqual(before['coupling'], after['coupling'], delta=3e-15)
        self.assertEqual(after['envelope'], 0)
        self.assertEqual(after['output'], -after['coupling'])
        self.assertNotEqual(before['output'], after['output'])

    def test_same_time_file_order_and_attack_boundary(self):
        a, b = self.model(), self.model()
        t = Q('.00037')
        for action in ['note_on','reset']: a.event(t,action)
        for action in ['reset','note_on']: b.event(t,action)
        self.assertEqual(a.advance(t+Q('.005'))['envelope'],0)
        self.assertGreater(b.advance(t+Q('.005'))['envelope'],0)
        boundary = t+Q('.01')
        state = b.advance(boundary)
        self.assertEqual(state['envelope'],1)
        self.assertEqual(state['stage'],'decay')
        b.event(boundary,'note_off')
        self.assertEqual(b.advance(boundary)['stage'],'release')

    def test_shared_exact_endpoints_across_grids(self):
        events=[(Q('.00037'),'note_on'),(Q('.00311'),'note_off'),
                (Q('.00473'),'retrigger'),(Q('.00619'),'reset'),(Q('.00677'),'note_on')]
        def grid(rate):
            model=self.model(); cursor=0; states=[]
            for i in range(rate//100+1):
                t=Q(i,rate)
                while cursor<len(events) and events[cursor][0]<=t:
                    model.event(*events[cursor]);cursor+=1
                states.append(model.advance(t))
            return states
        coarse,fine=grid(8000),grid(16000)
        for a,b in zip(coarse,fine[::2]):
            for key in ('li','coupling','envelope','output','stage'):
                self.assertEqual(a[key],b[key],key)

    def test_constant_sustain_vca_and_autonomous_boundaries(self):
        model = self.model(lc=Q('.8'), initial_li=Q('.8'))
        model.event(Q(0), 'note_on')
        decay_end = Q('.01') + Q(.02/math.log(1000)*math.log(.6/.0005))
        prior = model.advance(decay_end-Q(1,10**8))
        self.assertEqual(prior['stage'], 'decay')
        at = model.advance(decay_end)
        self.assertEqual(at['stage'], 'sustain')
        self.assertEqual(at['envelope'], .4)
        end = decay_end+Q('.004')
        after = model.advance(end)
        raw = (.25+OFFSET+FEED)*2**.3*.8*.4
        expected_cap = raw+(at['coupling']-raw)*math.exp(-.004/AC)
        self.assertAlmostEqual(after['coupling'], expected_cap, delta=3e-15)
        self.assertAlmostEqual(after['output'], raw-expected_cap, delta=3e-15)
        model.event(end,'note_off')
        release_end = end+Q(.03/math.log(1000)*math.log(.4/.0002))
        self.assertEqual(model.advance(release_end-Q(1,10**8))['stage'],'release')
        stopped = model.advance(release_end)
        self.assertEqual(stopped['stage'],'idle')
        self.assertEqual(stopped['envelope'],0)
        self.assertEqual(stopped['output'],-stopped['coupling'])

    def test_offgrid_event_differs_from_sampled_piecewise_linear_envelope(self):
        model=self.model(lc=Q('.8'), initial_li=Q('.8'))
        onset=Q('.00037')
        model.event(onset,'note_on')
        exact=model.advance(Q('.001'))
        # An independently constructed sampled-envelope PWL alternative smears
        # onset across [0,.0005]. This is not the old broad CLI test suite.
        value=lambda t: 0 if t<float(onset) else 1.5*(1-3**(-(t-float(onset))/.01))
        a,b=value(.0005),value(.001)
        def pwl(t):
            return a*t/.0005 if t<=.0005 else a+(b-a)*(t-.0005)/.0005
        alternative=rk4_ode(.001,1024,pwl,initial_li=.8)
        refined=rk4_ode(.001,2048,pwl,initial_li=.8)
        self.assertLess(abs(alternative[1]-refined[1]),1e-12)
        self.assertGreater(abs(exact['coupling']-refined[1]),1e-7)
        self.assertGreater(abs(exact['output']-refined[2]),1e-7)

    def test_rk4_reset_discontinuity_is_split_explicitly(self):
        model=self.model()
        model.event(Q(0),'note_on')
        # Integrate only the smooth attack, stop exactly at reset, then restart
        # integration with unchanged continuous states and zero envelope.
        attack=lambda t: 1.5*(1-3**(-t/.01))
        for steps in (128,256):
            first=rk4_ode(.004,steps,attack)
            second=rk4_ode(.002,steps,lambda t:0.,initial_li=first[0],initial_coupling=first[1])
            if steps==128: coarse=second
            else: fine=second
        model.event(Q('.004'),'reset')
        got=model.advance(Q('.006'))
        for key,a,b in zip(('li','coupling','output'),coarse,fine):
            self.assertLess(abs(a-b),2e-9)
            self.assertAlmostEqual(got[key],b,delta=2e-10,msg=key)

    def test_decay_rate_equal_to_coupling_rate(self):
        model=self.model(lc=Q('.8'),initial_li=Q('.8'),sustain=Q(0),decay=Q(math.log(1000)/10))
        model.event(Q(0),'note_on')
        start=model.advance(Q('.01'))
        got=model.advance(Q('.014'))
        amplitude=(.25+OFFSET+FEED)*2**.3*.8
        expected=math.exp(-.04)*(start['coupling']+amplitude*10*.004)
        self.assertAlmostEqual(got['coupling'],expected,delta=5e-15)
        self.assertAlmostEqual(got['output'],amplitude*math.exp(-.04)-expected,delta=5e-15)

    def test_short_segment_against_independent_refined_rk4(self):
        model=self.model()
        model.event(Q(0),'note_on')
        got=model.advance(Q('.0001'))
        envelope=lambda t:1.5*(1-3**(-t/.01))
        coarse=rk4_ode(.0001,32,envelope)
        fine=rk4_ode(.0001,64,envelope)
        for key,a,b in zip(('li','coupling','output'),coarse,fine):
            self.assertLess(abs(a-b),2e-11,key+' refinement')
            self.assertAlmostEqual(got[key],b,delta=2e-12,msg=key)

    def test_ordinary_cli_export_and_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); events=root/'events.csv'; out=root/'new-export'
            event_bytes=(b'event,time_s,action\n0,0.00037,note_on\n1,0.00311,note_off\n'
                         b'2,0.00473,retrigger\n3,0.00619,reset\n')
            events.write_bytes(event_bytes)
            command=[sys.executable,str(SCRIPT),'--events',str(events),
                     '--attack','.01','--decay','.02','--sustain','.4','--release','.03',
                     '--audio-input','.25','--lc','.8','--ei','.3',
                     '--initial-li','.2','--initial-coupling','0',
                     '--rate','8000','--samples','81','--out',str(out)]
            result=subprocess.run(command,capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            manifest=json.loads((out/'manifest.json').read_text())
            self.assertIs(manifest['complete'],True)
            self.assertEqual(manifest['schema'],'cs80-event-aware-envelope-vca-v1')
            self.assertEqual((manifest['samples'],manifest['intervals'],manifest['rate_hz']),(81,80,8000))
            span=manifest['endpoint_span']
            self.assertEqual(Q(span['numerator'],span['denominator']),Q(1,100))
            with (out/'trace.csv').open() as stream: rows=list(csv.DictReader(stream))
            self.assertEqual(len(rows),81)
            self.assertEqual([int(row['sample']) for row in rows],list(range(81)))
            self.assertEqual(float(rows[0]['time_s']),0.)
            self.assertEqual(float(rows[-1]['time_s']),.01)
            self.assertEqual((out/'input_events.csv').read_bytes(),event_bytes)
            self.assertEqual((out/'renderer.py').read_bytes(),SCRIPT.read_bytes())
            self.assertEqual((out/'render_amplitude_envelope_reference.py').read_bytes(),
                             (ROOT/'scripts/render_amplitude_envelope_reference.py').read_bytes())
            self.assertEqual({p.name for p in out.iterdir()},set(manifest['artifacts'])|{'manifest.json'})
            for name,meta in manifest['artifacts'].items():
                data=(out/name).read_bytes()
                self.assertEqual(len(data),meta['bytes'],name)
                self.assertEqual(hashlib.sha256(data).hexdigest(),meta['sha256'],name)


if __name__ == '__main__':
    unittest.main()
