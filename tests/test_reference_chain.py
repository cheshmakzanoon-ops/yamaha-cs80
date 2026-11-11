#!/usr/bin/env python3
"""Independent linear/convolution oracle, route separation and failure contracts."""
import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from test_truth_render import solve

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts/render_reference_chain.py'
SPEC = importlib.util.spec_from_file_location('chain', SCRIPT)
CHAIN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHAIN)
FILTER = VCA = None
F = lambda x: struct.unpack('<f', struct.pack('<f', x))[0]


def rows(path):
    with path.open() as stream:
        return [{k: float(v) for k, v in row.items()} for row in csv.DictReader(stream)]


def linear_lp(values, rate, substeps, hp=200, lp=2000, q=1):
    """Independent 8x8 linear nodal matrix, solved by Gaussian elimination.

    Physical adapters, zero states; uses declared constants, no renderer values.
    """
    networks = []
    for resistance, cutoff in [(75000, hp), (150000, lp)]:
        gs, gg, gf = 1/resistance, 1/33000, 1/1.2e6
        total = gs + gg + gf
        w = 2*math.pi*cutoff
        beta = w*w/(total/180e-12)
        rate_constant = beta + math.sqrt(beta*beta + w*w)
        networks.append((gs, gg, gf, total/(2*q)-gf, rate_constant))
    def derivative(state, source):
        result = []
        for offset, u, (gs, gg, gf, gq, w) in zip((0, 4), (source, 2*(state[1]+state[2])-state[3]), networks):
            vin, x, bp, lp_state = state[offset:offset+4]
            ai = x + bp
            current = (u - vin - ai)*gs
            result += [current/1e-6, (current - ai*gg - x*gf + gq*bp)/180e-12,
                       -w*(2*ai-lp_state), -w*bp]
        return result
    a = list(zip(*(derivative([float(i == j) for i in range(8)], 0) for j in range(8))))
    b = derivative([0.]*8, 1)
    h = 1/(rate*substeps)
    lhs = [[float(i == j)-h*.5*a[i][j] for j in range(8)] for i in range(8)]
    state, output = [0.]*8, [0.]
    for left, right in zip(values, values[1:]):
        for sub in range(substeps):
            u0, u1 = [left+(right-left)*k/substeps for k in (sub, sub+1)]
            rhs = [state[i]+h*.5*(sum(a[i][j]*state[j] for j in range(8))+b[i]*(u0+u1)) for i in range(8)]
            state = solve(lhs, rhs)
        output.append(state[7])
    return output


def ac_oracle(signal, rate, lc=.8, ei=-.5, envelope=.9):
    """Exact AC exponential convolution of PWL raw signal at constant gain."""
    gain = lc * 2**ei * envelope
    raw = [gain*(v+F(.0015))+F(.001)*gain for v in signal]
    dt, tau, coupling = 1/rate, 1e-6*100000, 0.
    e = math.exp(-dt/tau)
    one_minus = -math.expm1(-dt/tau)
    output = [raw[0]]
    for start, end in zip(raw, raw[1:]):
        coupling = coupling*e + start*one_minus + (end-start)/dt*(dt-tau*one_minus)
        output.append(end-coupling)
    return output


def dynamic_lp(values, controls, rate, substeps):
    """Independent LTV nodal matrices with declared physical adapters/constants.

    Requested Hz/Q are interpolated before nonlinear conductance mapping. Each
    trapezoid uses different old/new endpoint matrices, never a midpoint matrix.
    """
    def derivative(state, source, ctl):
        result = []
        for offset, resistance, hz, u in [(0, 75000, ctl[0], source),
                (4, 150000, ctl[1], 2*(state[1]+state[2])-state[3])]:
            gs, gg, gf = 1/resistance, 1/33000, 1/1.2e6
            total, w = gs+gg+gf, 2*math.pi*hz
            beta = w*w*180e-12/total
            rate_constant = beta + math.sqrt(beta*beta+w*w)
            gq = total/(2*ctl[2])-gf
            vin, x, bp, lp = state[offset:offset+4]
            ai = x+bp
            current = (u-vin-ai)*gs
            result += [current/1e-6, (current-ai*gg-x*gf+gq*bp)/180e-12,
                       -rate_constant*(2*ai-lp), -rate_constant*bp]
        return result
    state, result = [0.]*8, [0.]
    h = 1/(rate*substeps)
    for i, (left, right) in enumerate(zip(values, values[1:])):
        for sub in range(substeps):
            c0, c1 = [[a+(b-a)*k/substeps for a,b in zip(controls[i], controls[i+1])]
                      for k in (sub, sub+1)]
            u0, u1 = [left+(right-left)*k/substeps for k in (sub, sub+1)]
            d0 = derivative(state, u0, c0)
            a1 = list(zip(*(derivative([float(i == j) for i in range(8)], 0, c1) for j in range(8))))
            forcing = derivative([0.]*8, u1, c1)
            lhs = [[float(i == j)-h*.5*a1[i][j] for j in range(8)] for i in range(8)]
            state = solve(lhs, [state[i]+h*.5*(d0[i]+forcing[i]) for i in range(8)])
        result.append(state[7])
    return result


def envelope_oracle(signal, envelopes, rate, lc=.8, ei=-.5):
    """Independent integrating-factor convolution, not the VCA RK4 solver.

    Constant LI/EI, independently PWL LP and attack/release envelope yield a
    quadratic raw drive inside each interval. Integrate its exponential kernel
    by composite Simpson quadrature, without linearizing endpoint products.
    """
    gain, dt, tau, coupling = lc*2**ei, 1/rate, .1, 0.
    def raw(signal, envelope):
        return gain*envelope*(signal+F(.0015)) + F(.001)*gain*envelope
    result = [raw(signal[0], envelopes[0])]
    for start, end, env0, env1 in zip(signal, signal[1:], envelopes, envelopes[1:]):
        def integrand(i):
            phase = i/64
            return math.exp(-(1-phase)*dt/tau)*raw(start+(end-start)*phase, env0+(env1-env0)*phase)/tau
        integral = dt/64/3*(integrand(0)+integrand(64)
                            + sum((4 if i%2 else 2)*integrand(i) for i in range(1,64)))
        coupling = coupling*math.exp(-dt/tau)+integral
        result.append(raw(end,env1)-coupling)
    return result


class ChainTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.signal = self.root / 'signal.csv'
        self.controls = self.root / 'controls.csv'
        self.filter_controls = self.root / 'filter-controls.csv'
        self.out = self.root / 'out'
        self.write([.2*math.sin(i*.13) for i in range(81)],
                   [(.8+.05*math.sin(i*.17), .2*math.sin(i*.07), min(i/20, (80-i)/30, 1)) for i in range(81)])

    def write(self, signal, controls, crlf=False):
        self.count = len(signal)
        nl = '\r\n' if crlf else '\n'
        self.signal.write_bytes(('sample,value'+nl+''.join(f'{i},{v:.17g}'+nl for i,v in enumerate(signal))).encode())
        self.controls.write_bytes(('sample,lc,ei,envelope'+nl+''.join(str(i)+','+','.join(format(v,'.17g') for v in values)+nl for i,values in enumerate(controls))).encode())

    def args(self, **changes):
        options = {'filter': FILTER, 'vca': VCA, 'input': self.signal, 'controls': self.controls,
                   'out': self.out, 'rate': 48000, 'samples': self.count, 'hp-hz': 200, 'lp-hz': 2000,
                   'q': 1, 'ota-level': 2.5, 'gain': 'unity'}
        options.update(changes)
        return [str(v) for k,value in options.items() if value is not None for v in ('--'+k, value)]

    def dynamic_options(self, values=None, crlf=False):
        values = values if values is not None else [(200+i*3, 2000-i*7, 1+i*.01) for i in range(self.count)]
        nl = '\r\n' if crlf else '\n'
        self.filter_controls.write_bytes(('sample,hp_hz,lp_hz,q'+nl+''.join(
            str(i)+','+','.join(format(v,'.17g') for v in c)+nl for i,c in enumerate(values))).encode())
        return {'hp-hz':None, 'lp-hz':None, 'q':None, 'filter-controls':self.filter_controls}

    def run_chain(self, success=True, **changes):
        result = subprocess.run([sys.executable, str(SCRIPT), *self.args(**changes)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode == 0, success, result.stdout+result.stderr)
        self.assertEqual(list(self.root.glob('.cs80-chain-*')), [], result.stderr)
        if not success:
            self.assertNotIn('Wrote', result.stdout)
            self.assertFalse(self.out.exists())
            return result
        return Path(changes.get('out', self.out))

    def shim(self, filename, code):
        path = self.root / filename
        path.write_text('#!'+sys.executable+'\n'+code)
        path.chmod(0o700)
        return path

    def test_independent_linear_and_dc_chain(self):
        for name, signal in [('dc', [.25]*33), ('signed', [.3*math.sin(i*.37)-.1 for i in range(49)]), ('zero', [0.]*17)]:
            self.write(signal, [(.8, -.5, .9)]*len(signal))
            output = self.run_chain(out=self.root/name, gain='physical', **{'ota-level': 0})
            expected_lp = linear_lp(signal, 48000, 32)
            expected = ac_oracle(expected_lp, 48000)
            actual = rows(output/'chain.csv')
            filtered = rows(output/'filter/trace.csv')
            for row, filt, lp, value in zip(actual, filtered, expected_lp, expected):
                self.assertAlmostEqual(filt['lp'], lp, delta=3e-12)
                self.assertAlmostEqual(row['reference_output'], value, delta=3e-12)
            self.assertNotEqual(actual[0]['reference_output'], 0)  # Offset has no invented prehistory.

    def test_dynamic_independent_matrix_and_envelope_convolution(self):
        signal = [.4*math.sin(i*.31)-.1 for i in range(25)]
        controls = [(200+45*i, 2800-65*i, .6+.12*i) for i in range(len(signal))]
        envelope = [max(0, min(i/7, (24-i)/11, 1)) for i in range(len(signal))]
        for name, env in [('constant',[.9]*len(signal)), ('attack-release',envelope)]:
            self.write(signal, [(.8, -.5, e) for e in env])
            output = self.run_chain(out=self.root/name, gain='physical',
                                    **{'ota-level':0, 'filter-substeps':32}, **self.dynamic_options(controls))
            expected_lp = dynamic_lp(signal, controls, 48000, 64)
            expected = ac_oracle(expected_lp, 48000) if name == 'constant' else envelope_oracle(expected_lp, env, 48000)
            for actual, filtered, lp, value in zip(rows(output/'chain.csv'), rows(output/'filter/trace.csv'), expected_lp, expected):
                self.assertAlmostEqual(filtered['lp'], lp, delta=3e-12)
                self.assertAlmostEqual(actual['reference_output'], value, delta=3e-12)
        # This fixture distinguishes time-varying controls from the initial scalars.
        fixed = linear_lp(signal, 48000, 64, *controls[0])
        self.assertGreater(max(abs(a-b) for a,b in zip(fixed,expected_lp)), 1e-5)

    def test_constant_filter_controls_preserve_fixed_outputs(self):
        for ota in (0, 2.5):
            options = {'ota-level':ota, 'wav-gain':.1}
            if ota:
                options['compare-realtime'] = 'true'
            fixed = self.run_chain(out=self.root/('fixed'+str(ota)), **options)
            dynamic = self.run_chain(out=self.root/('dynamic'+str(ota)), **options,
                                     **self.dynamic_options([(200,2000,1)]*self.count))
            # Extra filter metadata/artifacts identify the trajectory. Numerical
            # fixed results and all downstream VCA files must remain identical.
            for path in fixed.rglob('*'):
                if path.is_file() and (path.parent.name.startswith('vca_') or path.name.endswith('.wav')
                        or path.name == 'chain.csv' or path == fixed/'filter/trace.csv'
                        or path == fixed/'filter/realtime.csv'):
                    self.assertEqual(path.read_bytes(), (dynamic/path.relative_to(fixed)).read_bytes())

    def test_routing_hashes_wav_and_determinism(self):
        self.check_routing_hashes_wav_and_determinism({})

    def test_dynamic_sweep_routing_hashes_wav_and_determinism(self):
        # A 0.1-second explicitly supplied waveform, cutoff/Q sweep and external
        # VCA attack/release. These are not a voice or hardware-envelope model.
        count, rate = 4801, 48000
        self.write([.2*math.sin(2*math.pi*440*i/rate) for i in range(count)],
                   [(.8,0,max(0,min(i/rate/.01,(.1-i/rate)/.03,1))) for i in range(count)])
        options = self.dynamic_options([(200,500+4500*i/(count-1),1+3*i/(count-1)) for i in range(count)], crlf=True)
        self.check_routing_hashes_wav_and_determinism(options)

    def check_routing_hashes_wav_and_determinism(self, options):
        a = self.run_chain(**options, **{'compare-realtime':'true', 'wav-gain': .1})
        b = self.run_chain(out=self.root/'again', **options, **{'compare-realtime':'true', 'wav-gain': .1})
        manifest = json.loads((a/'manifest.json').read_text())
        self.assertEqual((a/'manifest.json').read_bytes(), (b/'manifest.json').read_bytes())
        for path, expected in manifest['artifacts'].items():
            data = (a/path).read_bytes()
            self.assertEqual(len(data), expected['bytes'])
            self.assertEqual(hashlib.sha256(data).hexdigest(), expected['sha256'])
            self.assertEqual(data, (b/path).read_bytes())
        self.assertEqual((a/'inputs/signal.csv').read_bytes(), self.signal.read_bytes())
        self.assertEqual((a/'inputs/controls.csv').read_bytes(), self.controls.read_bytes())
        self.assertEqual((a/'tools/filter').read_bytes(), Path(FILTER).read_bytes())
        self.assertEqual((a/'tools/vca').read_bytes(), Path(VCA).read_bytes())
        if options:
            original = self.filter_controls.read_bytes()
            self.assertEqual((a/'inputs/filter_controls.csv').read_bytes(), original)
            self.assertEqual((a/'filter/controls.csv').read_bytes(), original)
            self.assertEqual(manifest['filter_controls']['sha256'], hashlib.sha256(original).hexdigest())
            self.assertEqual(manifest['filter_controls']['sample_count'], self.count)
            self.assertEqual(manifest['filter_controls']['rate_hz'], 48000)
            self.assertEqual(manifest['filter_controls']['bytes'], len(original))
            self.assertIn('piecewise linear', manifest['filter_controls']['interpolation'])
            self.assertIn('Initial endpoint only', manifest['filter_controls']['scalar_metadata_scope'])
            self.assertIn('not hardware', manifest['filter_controls']['scope'])
            command = manifest['commands']['filter']
            self.assertEqual(command[command.index('--controls')+1], 'inputs/filter_controls.csv')
            self.assertFalse(any(option in command for option in ('--hp-hz','--lp-hz','--q')))
            self.assertEqual(manifest['filter_controls']['endpoints'], 'filter/control_endpoints.csv')
            self.assertEqual(len(rows(a/'filter/control_endpoints.csv')), self.count)
        self.assertEqual(manifest['selected_outputs']['production']['trace'], 'vca_production_input/trace.csv')
        self.assertEqual(manifest['endpoint_span_s'], (self.count-1)/48000)
        self.assertEqual(manifest['playback_duration_s'], self.count/48000)
        chain = rows(a/'chain.csv')
        ref = rows(a/'vca_reference_input/trace.csv')
        prod = rows(a/'vca_production_input/trace.csv')
        actual_filter = rows(a/'filter/realtime.csv')
        for row, r, p, f in zip(chain, ref, prod, actual_filter):
            self.assertEqual(row['reference_output'], r['output'])
            self.assertEqual(row['production_output'], p['realtime_output'])
            self.assertEqual(row['production_minus_reference'], p['realtime_output']-r['output'])
            self.assertEqual(p['input'], f['lp_realtime'])
            self.assertEqual(F(p['input']), p['input'])  # Exact promoted float survives 17 digits.
        self.assertGreater(max(abs(p['realtime_output']-r['realtime_output']) for r,p in zip(ref,prod)), 1e-6)
        self.assertEqual((a/'reference.wav').read_bytes(), (a/'vca_reference_input/reference.wav').read_bytes())
        self.assertEqual((a/'production.wav').read_bytes(), (a/'vca_production_input/realtime.wav').read_bytes())
        for name, key in [('reference','reference_output'), ('production','production_output')]:
            payload = (a/(name+'.wav')).read_bytes()[58:]
            self.assertEqual(payload, b''.join(struct.pack('<f', r[key]*.1) for r in chain))

    def test_production_lp_mutation_changes_only_production_selection(self):
        self.check_production_lp_mutation({})

    def test_dynamic_production_lp_mutation_changes_only_production_selection(self):
        self.check_production_lp_mutation(self.dynamic_options())

    def check_production_lp_mutation(self, options):
        base = self.run_chain(out=self.root/'base', **options, **{'compare-realtime': 'true'})
        shim = self.shim('changed_filter', f'''import csv, pathlib, subprocess, sys, struct
subprocess.run([{FILTER!r}, *sys.argv[1:]], check=True)
p = pathlib.Path(sys.argv[sys.argv.index('--out')+1])/'realtime.csv'
with p.open() as f:
    reader=csv.DictReader(f); names=reader.fieldnames; data=list(reader)
for r in data[1:]:
    r['lp_realtime']=format(struct.unpack('f',struct.pack('f',float(r['lp_realtime'])+.125))[0],'.17g')
with p.open('w') as f:
    writer=csv.DictWriter(f,fieldnames=names,lineterminator='\\n');writer.writeheader();writer.writerows(data)
''')
        changed = self.run_chain(filter=shim, **options, **{'compare-realtime': 'true'})
        old, new = rows(base/'chain.csv'), rows(changed/'chain.csv')
        self.assertEqual([r['reference_output'] for r in old], [r['reference_output'] for r in new])
        self.assertGreater(max(abs(a['production_output']-b['production_output']) for a,b in zip(old,new)), .05)

    def test_dynamic_conflicts_malformed_bounds_and_clamps(self):
        options = self.dynamic_options()
        for key,value in [('hp-hz',200), ('lp-hz',2000), ('q',1)]:
            self.run_chain(False, **{**options,key:value})
            self.run_chain(False, **{key:None})  # Missing any fixed scalar fails too.
        args = self.args(**options)
        duplicate = subprocess.run([sys.executable,str(SCRIPT),*args,'--filter-controls',str(self.filter_controls)],capture_output=True)
        self.assertNotEqual(duplicate.returncode,0)
        self.assertFalse(self.out.exists())
        original = self.filter_controls.read_bytes()
        header, first, rest = original.split(b'\n',2)
        bad_rows = [b'00,200,2000,1', b'0,25,20001,1', b'0,24.999,2000,1', b'0,200,2000,.349',
                    b'0,200,2000,9.001', b'0,nan,2000,1', b'0,1e-999,2000,1', b'0,0x100,2000,1',
                    b'0,2_00,2000,1', b'0,200,2000, 1', b'0,200,2000,"1"', b'0,200,2000,1,1',
                    b'0,200,2000,1'+b'0'*128, b'0,200,2000,\xff']
        malformed = [original[:-1], original+b'81,200,2000,1\n', header+b'\n'+rest,
                     original.replace(header,b'sample,lp_hz,hp_hz,q'), original+b'\n']
        for bad in malformed + [header+b'\n'+row+b'\n'+rest for row in bad_rows]:
            self.filter_controls.write_bytes(bad)
            self.run_chain(False, **options)
        self.filter_controls.write_bytes(original)
        self.run_chain(False, **{**options,'filter-controls':self.root})
        fifo = self.root/'control-fifo'; os.mkfifo(fifo)
        self.run_chain(False, **{**options,'filter-controls':fifo})
        # A legal reference endpoint at the very end must not silently clamp in
        # production; both HP and LP final-endpoint clamps are rejected.
        self.write([0.,0.,0.], [(.8,0,0)]*3)
        for last in [(4000,2000,1),(200,4000,1)]:
            options = self.dynamic_options([(200,2000,1)]*2+[last])
            result = self.run_chain(False, rate=8000, **options, **{'compare-realtime':'true'})
            self.assertIn('clamped', result.stderr)
        self.assertEqual(list(self.root.glob('.cs80-chain-*')), [])

    def test_dynamic_child_control_integrity_and_failure_cleanup(self):
        options = self.dynamic_options()
        for name, mutation in [
            ('hash', "m['controls']['sha256']='0'*64"),
            ('count', "m['controls']['sample_count']-=1"),
            ('rate', "m['controls']['rate_hz']=8000"),
            ('file', "m['controls']['endpoint_file']='trace.csv'"),
            ('snapshot', "(out/'controls.csv').write_bytes(b'changed\\n')"),
            ('schema', "p=out/'control_endpoints.csv';p.write_text(p.read_text().replace('hp_hz','hp_wrong'))"),
            ('trace', "p=out/'control_endpoints.csv';p.write_text(p.read_text().replace('0,0,200,','0,0,201,',1))"),
            ('truncated', "p=out/'control_endpoints.csv';p.write_bytes(p.read_bytes().rsplit(b'\\n',2)[0]+b'\\n')"),
            ('partial', "(out/'control_endpoints.csv').unlink();sys.exit(7)"),
        ]:
            shim = self.shim(name, f'''import json,pathlib,subprocess,sys
subprocess.run([{FILTER!r},*sys.argv[1:]],check=True)
out=pathlib.Path(sys.argv[sys.argv.index('--out')+1])
p=out/'manifest.json';m=json.loads(p.read_text())
{mutation}
(out/'manifest.json').write_text(json.dumps(m))
''')
            with self.subTest(name=name):
                self.run_chain(False, filter=shim, **options)

    def test_dynamic_snapshot_survives_original_source_change(self):
        options = self.dynamic_options(crlf=True)
        original = self.filter_controls.read_bytes()
        shim = self.shim('source_changed', f'''import pathlib,subprocess,sys
pathlib.Path({str(self.filter_controls)!r}).write_text('changed after snapshot\\n')
subprocess.run([{FILTER!r},*sys.argv[1:]],check=True)
''')
        out = self.run_chain(filter=shim, **options)
        self.assertNotEqual(self.filter_controls.read_bytes(), original)
        self.assertEqual((out/'inputs/filter_controls.csv').read_bytes(), original)
        self.assertEqual((out/'filter/controls.csv').read_bytes(), original)
        manifest = json.loads((out/'manifest.json').read_text())
        self.assertEqual(manifest['filter_controls']['sha256'],hashlib.sha256(original).hexdigest())

    def test_crlf_exact_snapshots_and_initial_li(self):
        self.write([.1]*5, [(-.2, 0, .5)]*5, crlf=True)
        out = self.run_chain(**{'initial-li': -.3})
        for path, source in [('signal',self.signal),('controls',self.controls)]:
            self.assertEqual((out/('inputs/'+path+'.csv')).read_bytes(), source.read_bytes())
        self.assertTrue(all(row['reference_output'] == 0 for row in rows(out/'chain.csv')))

    def test_argument_and_input_rejection(self):
        for change in [{'rate': '48000.0'}, {'samples': '2'}, {'samples':100002}, {'rate':8000,'samples':80002},
                       {'samples':100001,'filter-substeps':256}, {'q':'nan'}, {'q':'1_0'}, {'vca-steps':0},
                       {'filter-tolerance':0}, {'wav-gain':-1}, {'gain':'implicit'}, {'compare-realtime':'false'},
                       {'compare-realtime':'true','gain':'physical'}, {'compare-realtime':'true','initial-li':0},
                       {'compare-realtime':'true','ota-level':0}, {'initial-li':9}]:
            with self.subTest(change=change):
                self.run_chain(False, **change)
        args = self.args()
        for tail in [['--rate','48000'], ['--unknown','1']]:
            p = subprocess.run([sys.executable,str(SCRIPT),*args,*tail],capture_output=True)
            self.assertNotEqual(p.returncode, 0)
        original = self.controls.read_bytes()
        header, _, rest = original.split(b'\n', 2)
        bad_rows = [b'00,0.8,0,0', b'0,nan,0,0', b'0,1e-999,0,0',
                    b'0,0.8,0,1.1', b'0,0.8,0, 0', b'0,0.8,0,0,0', b'0,0.8,0,0x0']
        for bad in [original[:-1], original+b'81,1,0,0\n'] + [header+b'\n'+row+b'\n'+rest for row in bad_rows]:
            self.controls.write_bytes(bad)
            self.run_chain(False)
        self.controls.write_bytes(original)
        self.run_chain(False, controls=self.root)
        fifo = self.root/'fifo'; os.mkfifo(fifo)
        self.run_chain(False, input=fifo)

    def test_new_output_and_owned_cleanup(self):
        self.out.mkdir()
        marker = self.out/'keep'; marker.write_text('original')
        p = subprocess.run([sys.executable,str(SCRIPT),*self.args()],capture_output=True)
        self.assertNotEqual(p.returncode,0)
        self.assertEqual(list(self.out.iterdir()),[marker])
        marker.unlink(); self.out.rmdir()
        self.out.symlink_to(self.root/'missing')
        p = subprocess.run([sys.executable,str(SCRIPT),*self.args()],capture_output=True)
        self.assertNotEqual(p.returncode,0)
        self.assertTrue(self.out.is_symlink()); self.out.unlink()
        real_link = CHAIN.os.link
        links = 0
        def fail_publication(source, target):
            nonlocal links
            links += 1
            if links == 3:
                (self.out/'unrelated').write_text('keep me')
                raise OSError('injected publication failure')
            real_link(source,target)
        with patch.object(CHAIN.os,'link',side_effect=fail_publication):
            self.assertEqual(CHAIN.main(self.args()),1)
        self.assertEqual(list(self.out.iterdir()), [self.out/'unrelated'])
        self.assertEqual((self.out/'unrelated').read_text(),'keep me')
        self.assertEqual(list(self.root.glob('.cs80-chain-*')),[])

    def test_child_failure_bad_schema_hash_and_range(self):
        false = self.shim('false', "import sys\nsys.stderr.write('x'*50000+'useful refinement failure')\nsys.exit(7)\n")
        failure = self.run_chain(False, vca=false)
        self.assertIn('useful refinement failure', failure.stderr)
        self.assertLess(len(failure.stderr), 17000)
        for name, mutation in [
            ('schema', "m['schema']=999"),
            ('hash', "m['input']['sha256']='0'*64"),
            ('count', "m['intervals']-=1"),
        ]:
            shim=self.shim(name, f'''import json,pathlib,subprocess,sys
subprocess.run([{FILTER!r},*sys.argv[1:]],check=True)
p=pathlib.Path(sys.argv[sys.argv.index('--out')+1])/'manifest.json'
m=json.loads(p.read_text());{mutation};p.write_text(json.dumps(m))
''')
            self.run_chain(False,filter=shim)
        range_shim=self.shim('range',f'''import csv,pathlib,subprocess,sys
subprocess.run([{FILTER!r},*sys.argv[1:]],check=True)
p=pathlib.Path(sys.argv[sys.argv.index('--out')+1])/'trace.csv'
with p.open() as f:
    r=csv.DictReader(f);names=r.fieldnames;data=list(r)
data[1]['lp']='8.00001'
with p.open('w') as f:
    w=csv.DictWriter(f,fieldnames=names,lineterminator='\\n');w.writeheader();w.writerows(data)
''')
        self.run_chain(False,filter=range_shim)
        slow=self.shim('slow','import time\ntime.sleep(2)\n')
        with patch.object(CHAIN,'CHILD_TIMEOUT',.02):
            self.assertEqual(CHAIN.main(self.args(filter=slow)),1)
        self.assertFalse(self.out.exists())
        self.assertEqual(list(self.root.glob('.cs80-chain-*')),[])

    def test_existing_stage_outputs_unchanged(self):
        self.check_existing_stage_outputs({})

    def test_existing_dynamic_stage_outputs_unchanged(self):
        self.check_existing_stage_outputs(self.dynamic_options())

    def check_existing_stage_outputs(self, options):
        bundle=self.run_chain(**options, **{'compare-realtime':'true','wav-gain':.1})
        manifest=json.loads((bundle/'manifest.json').read_text())
        for name, command in manifest['commands'].items():
            direct=self.root/('direct-'+name)
            executable = FILTER if name == 'filter' else VCA
            command=[executable,*command[1:]]
            index=command.index('--out'); command[index+1]=str(direct)
            result=subprocess.run(command,cwd=bundle,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            for path in direct.iterdir():
                self.assertEqual(path.read_bytes(),(bundle/name/path.name).read_bytes())


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--filter',required=True)
    parser.add_argument('--vca',required=True)
    args, rest=parser.parse_known_args()
    FILTER=str(Path(args.filter).resolve());VCA=str(Path(args.vca).resolve())
    unittest.main(argv=[__file__,*rest])
