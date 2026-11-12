#!/usr/bin/env python3
"""Independent constant-sine/PWL converter-voltage ring time reference."""
import argparse
from bisect import bisect_right
from fractions import Fraction
import math
import os
from pathlib import Path
import sys

import render_amplitude_envelope_reference as helpers

require = helpers.require
MAX_SAMPLES = 20001
MAX_KNOTS = 1024
MAX_BUNDLE_BYTES = 32 * 1024 * 1024
K = math.log(30.0) / 5.0
SIGNAL_OFFSET, CARRIER_OFFSET, OUTPUT_OFFSET = .0005, .005, .001
PARAMETERS = ('input_amplitude', 'input_frequency', 'input_phase', 'amount', 'carrier_initial_phase')


def hz_for_volts(volts):
    return (1.0 / .150) * math.exp(K * (float(volts) - 5.0))


def phase_increment(v_start, v_end, duration, elapsed=None):
    """Cycles integrated from a voltage segment's fixed left endpoint."""
    duration = Fraction(duration)
    elapsed = duration if elapsed is None else Fraction(elapsed)
    require(duration > 0 and 0 <= elapsed <= duration, 'Invalid phase interval')
    # Form the bounded voltage change before conversion: no huge reciprocal
    # slope at closely spaced exact-rational knots.
    z = K * float((Fraction(v_end) - Fraction(v_start)) * elapsed / duration)
    ratio = 1.0 if z == 0 else math.expm1(z) / z
    return hz_for_volts(v_start) * float(elapsed) * ratio


def parse_knots(data, span):
    require(len(data) <= helpers.MAX_INPUT_BYTES, 'Knot input exceeds byte budget')
    lines = data.split(b'\n')
    require(lines[-1] == b'', 'Knot file must end in a newline')
    require(4 <= len(lines) <= MAX_KNOTS + 2, 'Knot count outside bounds')
    require(all(len(line) <= 160 for line in lines), 'Knot line exceeds budget')
    lines = [line[:-1] if line.endswith(b'\r') else line for line in lines[:-1]]
    require(lines[0] == b'knot,time_s,volts', 'Unexpected knot header')
    knots = []
    for i, line in enumerate(lines[1:]):
        fields = line.decode('ascii').split(',')
        require(len(fields) == 3 and fields[0] == str(i), 'Noncanonical knot index or columns')
        t, v = map(helpers.number, fields[1:])
        require(0 <= t <= span and 0 <= v <= 10, 'Knot outside bounds')
        require(not knots or knots[-1][0] < t, 'Knot times must strictly increase')
        knots.append((t, v))
    require(knots[0][0] == 0 and knots[-1][0] == span, 'Knots must cover exact endpoint span')
    return knots


class Composition:
    """Fixed knot anchors; sampling order/density never changes accumulated phase."""
    def __init__(self, knots, input_amplitude, input_frequency, input_phase,
                 amount, carrier_initial_phase):
        self.knots = [(Fraction(t), Fraction(v)) for t, v in knots]
        require(2 <= len(self.knots) <= MAX_KNOTS and self.knots[0][0] == 0,
                'Bounded knots starting at zero required')
        require(0 < self.knots[-1][0] <= 2, 'Span outside bounds')
        require(all(0 <= v <= 10 for _, v in self.knots) and
                all(a[0] < b[0] for a,b in zip(self.knots,self.knots[1:])), 'Invalid knots')
        values = tuple(map(float, (input_amplitude,input_frequency,input_phase,amount,carrier_initial_phase)))
        require(all(math.isfinite(x) for x in values), 'Finite parameters required')
        a, f, p, m, c = values
        require(0 <= a <= 1 and 0 < f <= 2000 and 0 <= p < 1 and 0 <= m <= 1 and 0 <= c < 1,
                'Parameters outside bounds')
        self.amplitude,self.frequency,self.input_phase,self.amount = a,f,p,m
        self.times = [t for t,_ in self.knots]
        self.phases = [c]
        for (t,v),(end,w) in zip(self.knots,self.knots[1:]):
            self.phases.append(math.fsum((self.phases[-1],phase_increment(v,w,end-t))))
        self.segments = [dict(start=helpers.exact_time(t),end=helpers.exact_time(end),
            start_volts=float(v),end_volts=float(w),start_cycles=self.phases[i],end_cycles=self.phases[i+1])
            for i,((t,v),(end,w)) in enumerate(zip(self.knots,self.knots[1:]))]

    def advance(self, t):
        t = Fraction(t)
        require(0 <= t <= self.times[-1], 'Query outside endpoint span')
        i = bisect_right(self.times,t)-1
        start,v = self.knots[i]
        if i == len(self.knots)-1:
            volts,cycles = float(v),self.phases[i]
        else:
            end,w = self.knots[i+1]
            volts = float(v+(w-v)*(t-start)/(end-start))
            cycles = math.fsum((self.phases[i],phase_increment(v,w,end-start,t-start)))
        carrier = math.sin(math.tau * (cycles % 1.0))
        audio = self.amplitude * math.sin(math.tau * ((self.input_phase+self.frequency*float(t)) % 1.0))
        wet = (audio+SIGNAL_OFFSET)*(carrier+CARRIER_OFFSET)+OUTPUT_OFFSET
        return dict(time=helpers.exact_time(t),volts=volts,carrier_hz=hz_for_volts(volts),
                    carrier_cycles=cycles,carrier=carrier,input=audio,wet=wet,
                    output=(1-self.amount)*audio+self.amount*wet)


def parse(argv):
    parser = argparse.ArgumentParser(description=__doc__,allow_abbrev=False)
    for name in ('knots', *[x.replace('_','-') for x in PARAMETERS], 'rate','samples','out'):
        parser.add_argument('--'+name,required=True)
    options = [x.split('=',1)[0] for x in argv if x.startswith('--')]
    require(len(options)==len(set(options)), 'Duplicate option')
    args = parser.parse_args(argv)
    args.rate = helpers.integer(args.rate,8000,384000)
    args.samples = helpers.integer(args.samples,2,MAX_SAMPLES)
    require(Fraction(args.samples-1,args.rate)<=2, 'Endpoint span exceeds two seconds')
    for name in PARAMETERS:
        value = helpers.number(getattr(args,name))
        high = 2000 if name=='input_frequency' else 1
        require(0 <= value <= high, name+' outside bounds')
        if name=='input_frequency': require(value>0, 'Positive input frequency required')
        if name in ('input_phase','carrier_initial_phase'): require(value<1, 'Phase must be below one cycle')
        setattr(args,name,value)
    return args


def solve(args, knots):
    require(knots[-1][0]==Fraction(args.samples-1,args.rate), 'Knot/sample span mismatch')
    model = Composition(knots,*(getattr(args,name) for name in PARAMETERS))
    return dict(rows=[dict(sample=i,time_s=float(Fraction(i,args.rate)),**model.advance(Fraction(i,args.rate)))
                      for i in range(args.samples)],segments=model.segments,
                knots=[dict(knot=i,**model.advance(t)) for i,(t,_) in enumerate(knots)])


def render(args):
    out = Path(args.out).absolute()
    require(not os.path.lexists(out), 'Output path must be new (including symlinks)')
    require(out.parent.is_dir(), 'Output parent must exist')
    data = helpers.snapshot(args.knots)
    result = solve(args,parse_knots(data,Fraction(args.samples-1,args.rate)))
    columns = ('sample','time_s','volts','carrier_hz','carrier_cycles','carrier','input','wet','output')
    csv = [','.join(columns)+'\n']
    for row in result['rows']:
        csv.append(','.join(format(row[k],'.17g') if isinstance(row[k],float) else str(row[k]) for k in columns)+'\n')
    artifacts = {'trace.csv': ''.join(csv).encode('ascii'),'input_knots.csv':data,
        'knots.json':helpers.json_bytes(result['knots']),'segments.json':helpers.json_bytes(result['segments']),
        'renderer.py':helpers.snapshot(__file__),
        'render_amplitude_envelope_reference.py':helpers.snapshot(helpers.__file__),
        'RingModulator.h':helpers.snapshot(Path(__file__).resolve().parents[1]/'src/reference/RingModulator.h')}
    manifest = dict(schema='cs80-ring-time-reference-v1',complete=True,rate_hz=args.rate,
        samples=args.samples,intervals=args.samples-1,endpoint_span=helpers.exact_time(Fraction(args.samples-1,args.rate)),
        parameters={name:dict(decimal_rational=str(getattr(args,name)),binary64=float(getattr(args,name))) for name in PARAMETERS},
        constants=dict(anchor_volts=5,anchor_period_seconds=.150,inferred_max_volts=10,max_period_seconds=.005,
                       signal_offset=SIGNAL_OFFSET,carrier_offset=CARRIER_OFFSET,output_offset=OUTPUT_OFFSET),
        equations=['f(V)=(1/.150)*exp(log(30)*(V-5)/5)', 'phase_increment=fstart*dt*expm1(z)/z; z=log(30)/5*(Vend-Vstart)*dt/duration',
                   'wet=(input+.0005)*(carrier+.005)+.001','output=(1-amount)*input+amount*wet'],
        clock='Exact Fraction sample/knot clock; continuous PWL voltage; phase anchored only at actual knots; no time-zero tick',
        scope='Independent inferred exponential-converter/offset-multiplier reference; fixed sine input and amount, explicit initial phases; no envelope generation or smoothing',
        limitations=['No hardware calibration','No production equivalence','No alias/antialias qualification','No audio listening or WAV','No whole voice'],
        units='Converter-node volts; frequencies Hz; phases cycles; input/output uncalibrated engine units',
        limits=dict(max_samples=MAX_SAMPLES,max_knots=MAX_KNOTS,max_seconds=2,max_input_bytes=helpers.MAX_INPUT_BYTES,max_bundle_bytes=MAX_BUNDLE_BYTES),
        provenance='RingModulator.h algebraic model; docs/workstreams/ws4-circuits.md Step 2 evidence classes. Header snapshot is provenance only, not executed. Direct converter voltage, not SPEED/depth/envelope composition or production smoothing. Saved source hashes identify bytes, not authenticated execution; libm bits may vary.',
        artifacts={name:helpers.metadata(value) for name,value in sorted(artifacts.items())})
    artifacts['manifest.json'] = helpers.json_bytes(manifest)
    require(sum(map(len,artifacts.values()))<=MAX_BUNDLE_BYTES, 'Bundle exceeds byte budget')
    owned = helpers.Owned()
    try:
        owned.mkdir(out)
        for name,value in artifacts.items(): owned.write(out/name,value)
    except BaseException:
        owned.cleanup()
        raise
    return result


def main(argv=None):
    try:
        render(parse(sys.argv[1:] if argv is None else argv))
    except (ValueError,OSError,UnicodeError,OverflowError) as error:
        print('ring time reference: '+str(error),file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':
    sys.exit(main())
