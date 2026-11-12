#!/usr/bin/env python3
"""Event-aware analytic envelope/conditional VCA composition; constant audio only."""
import argparse
from fractions import Fraction
import math
import os
from pathlib import Path
import struct
import sys

import render_amplitude_envelope_reference as envelope_reference

require = envelope_reference.require
TAU_LI = (1.0 / (1.0 / 180000.0 + 1.0 / 560000.0 + 3.0 / 270000.0)) * 1e-8
TAU_AC = 1e-6 * 100000.0
OFFSET = struct.unpack('f', struct.pack('f', 1.5e-3))[0]
FEEDTHROUGH = struct.unpack('f', struct.pack('f', 1e-3))[0]
MAX_SAMPLES = 20001
MAX_EVENTS = 1024
MAX_BUNDLE_BYTES = 32 * 1024 * 1024


def convolution_kernel(rate, dt, q=1.0 / TAU_AC):
    """Integral exp(-q*(dt-s))*exp(-rate*s) ds; no positive exponent."""
    delta = abs(q - rate)
    if delta == 0:
        return dt * math.exp(-q * dt)
    return math.exp(-min(q, rate) * dt) * (-math.expm1(-delta * dt)) / delta


def _short_convolution(li, dl, value, de, a, b, dt, q):
    """Local power basis avoids subtracting nearly equal kernels at tiny dt.

    Expand LI and E on u=t/dt, then integrate each power against exp(-q*dt*(1-u)).
    Used only when every dimensionless rate <= .5. Forty terms are well beyond
    binary64 precision here; this is roundoff-aware evaluation, not certification.
    """
    x, y, z = a * dt, b * dt, q * dt
    left, right = [li], [value]
    lx = ry = 1.0
    for n in range(1, 41):
        lx *= -x / n
        ry *= -y / n
        left.append(-dl * lx)
        right.append(-de * ry)
    terms = []
    for n in range(41):
        coefficient = math.fsum(left[j] * right[n-j] for j in range(n+1))
        term, moments = 1.0, [1.0 / (n + 1)]
        for k in range(1, 41):
            term *= z / k
            moments.append(term / (n + k + 1))
        terms.append(coefficient * math.exp(-z) * math.fsum(moments))
    return q * dt * math.fsum(terms)


def advance_segment(li, coupling, lc, scale, envelope_start, envelope_target,
                    envelope_rate, dt):
    """Return (LI,coupling) for one smooth phase, dt seconds from its anchor."""
    require(dt >= 0 and math.isfinite(dt), 'Invalid segment duration')
    if dt == 0:
        return li, coupling
    a, b, q = 1.0 / TAU_LI, envelope_rate, 1.0 / TAU_AC
    dl, de = lc - li, envelope_target - envelope_start
    next_li = li + dl * (-math.expm1(-a * dt))
    if max(a, b, q) * dt <= .5:
        forcing = _short_convolution(li, dl, envelope_start, de, a, b, dt, q)
    else:
        # (lc-dl*exp(-a*t)) * (target-de*exp(-b*t)). fsum preserves
        # cancellation of coefficient contributions; kernel handles q==rate.
        forcing = q * math.fsum((
            lc * envelope_target * convolution_kernel(0.0, dt, q),
            -dl * envelope_target * convolution_kernel(a, dt, q),
            -lc * de * convolution_kernel(b, dt, q),
            dl * de * convolution_kernel(a + b, dt, q)))
    next_coupling = math.fsum((coupling * math.exp(-q * dt), scale * forcing))
    require(math.isfinite(next_li) and next_li >= 0 and math.isfinite(next_coupling),
            'Nonfinite or negative LI state')
    return next_li, next_coupling


class Composition:
    """Queries never replace anchors; only actual phase/event boundaries do."""
    def __init__(self, attack, decay, sustain, release, audio_input, lc, ei,
                 initial_li, initial_coupling):
        self.env = envelope_reference.Envelope(Fraction(attack), decay, sustain, release)
        self.audio_input, self.lc, self.ei = map(float, (audio_input, lc, ei))
        self.li, self.coupling = map(float, (initial_li, initial_coupling))
        require(all(math.isfinite(v) for v in (self.audio_input, self.lc, self.ei, self.li, self.coupling)),
                'Finite composition parameters required')
        require(self.lc >= 0 and self.li >= 0, 'Nonnegative LC and initial LI required')
        self.scale = (2.0 ** self.ei) * (self.audio_input + OFFSET + FEEDTHROUGH)
        self.now = self.anchor = Fraction(0)
        self.anchor_li, self.anchor_coupling = self.li, self.coupling
        self.event_audit, self.transition_audit, self.segments = [], [], []
        self._phase()

    def _phase(self):
        self.e0 = self.env.value(self.anchor)
        if self.env.stage == 'attack':
            self.target = self.env.origin + 1.5 * (1.0 - self.env.origin)
            self.rate = math.log(3.0) / float(self.env.attack)
        elif self.env.stage == 'decay':
            self.target, self.rate = self.env.sustain, math.log(1000.0) / self.env.decay
        elif self.env.stage == 'release':
            self.target, self.rate = 0.0, math.log(1000.0) / self.env.release
        else:
            self.target, self.rate = self.e0, 0.0

    def _evaluate(self, t):
        return advance_segment(self.anchor_li, self.anchor_coupling, self.lc, self.scale,
                               self.e0, self.target, self.rate, float(t - self.anchor))

    def _state(self, value=None):
        if value is None:
            value = self.env.value(self.now)
        gain = self.li * (2.0 ** self.ei) * value
        raw = self.scale * self.li * value
        return {'time': envelope_reference.exact_time(self.now), 'stage': self.env.stage,
                'gate': self.env.gate, 'envelope': value, 'li': self.li,
                'coupling': self.coupling, 'gain': gain, 'raw': raw,
                'output': raw - self.coupling}

    def state(self):
        return self._state()

    def _anchor_at(self, t, reason):
        if t != self.anchor:
            self.segments.append({'start': envelope_reference.exact_time(self.anchor),
                                  'end': envelope_reference.exact_time(t), 'stage': self.env.stage,
                                  'envelope_start': self.e0, 'envelope_target': self.target,
                                  'envelope_rate': self.rate, 'li_start': self.anchor_li,
                                  'coupling_start': self.anchor_coupling, 'li_end': self.li,
                                  'coupling_end': self.coupling, 'end_reason': reason})
        self.anchor = t
        self.anchor_li, self.anchor_coupling = self.li, self.coupling

    def advance(self, t):
        t = Fraction(t)
        require(t >= self.now, 'Composition clock moved backwards')
        while self.env.end is not None and self.env.end <= t:
            boundary = self.env.end
            self.li, self.coupling = self._evaluate(boundary)
            self.now = boundary
            left = self._state(self.env.value(boundary))
            self._anchor_at(boundary, 'autonomous phase boundary')
            first = len(self.env.transitions)
            self.env.advance(boundary)
            self._phase()
            self.transition_audit.append({'time': envelope_reference.exact_time(boundary),
                                          'before': left, 'after': self.state(),
                                          'envelope_transitions': self.env.transitions[first:]})
        self.li, self.coupling = self._evaluate(t)
        self.env.advance(t)
        self.now = t
        return self.state()

    def event(self, t, action):
        t = Fraction(t)
        self.advance(t)
        before = self.state()
        self._anchor_at(t, 'event: ' + action)
        audit = self.env.event(t, action)
        self._phase()
        result = {'event': len(self.event_audit), 'time': envelope_reference.exact_time(t),
                  'action': action, 'before': before, 'after': self.state(),
                  'envelope_event': audit}
        self.event_audit.append(result)
        return result


def parse(argv):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    names = ('events', 'attack', 'decay', 'sustain', 'release', 'audio-input', 'lc', 'ei',
             'initial-li', 'initial-coupling', 'rate', 'samples', 'out')
    for name in names:
        parser.add_argument('--' + name, required=True)
    options = [x.split('=', 1)[0] for x in argv if x.startswith('--')]
    require(len(options) == len(set(options)), 'Duplicate option')
    args = parser.parse_args(argv)
    args.rate = envelope_reference.integer(args.rate, 8000, 384000)
    args.samples = envelope_reference.integer(args.samples, 2, MAX_SAMPLES)
    require(Fraction(args.samples - 1, args.rate) <= 2, 'Endpoint span exceeds two seconds')
    for name, low, high in [('attack', Fraction(1,1000), 1), ('decay', Fraction(1,100), 10),
                           ('sustain', 0, 1), ('release', Fraction(1,100), 10),
                           ('audio_input', -8, 8), ('lc', 0, 8), ('ei', -8, 8),
                           ('initial_li', 0, 8), ('initial_coupling', -8, 8)]:
        value = envelope_reference.number(getattr(args, name))
        require(low <= value <= high, name + ' outside bounds')
        setattr(args, name, value)
    return args


def solve(args, events):
    require(len(events) <= MAX_EVENTS, 'Event count exceeds composition budget')
    model = Composition(*(getattr(args, name) for name in
        ('attack', 'decay', 'sustain', 'release', 'audio_input', 'lc', 'ei', 'initial_li', 'initial_coupling')))
    rows, cursor = [], 0
    for i in range(args.samples):
        t = Fraction(i, args.rate)
        while cursor < len(events) and events[cursor][0] <= t:
            model.event(*events[cursor]); cursor += 1
        row = {'sample': i, 'time_s': float(t), **model.advance(t), 'last_event': cursor - 1}
        rows.append(row)
    require(cursor == len(events), 'Unconsumed event')
    segments = list(model.segments)
    if model.now != model.anchor:
        # An audit of the final partial phase, not a new integration anchor.
        segments.append({'start': envelope_reference.exact_time(model.anchor),
                         'end': envelope_reference.exact_time(model.now), 'stage': model.env.stage,
                         'envelope_start': model.e0, 'envelope_target': model.target,
                         'envelope_rate': model.rate, 'li_start': model.anchor_li,
                         'coupling_start': model.anchor_coupling, 'li_end': model.li,
                         'coupling_end': model.coupling, 'end_reason': 'final output endpoint (query only)'})
    return {'rows': rows, 'event_audit': model.event_audit,
            'transitions': model.transition_audit, 'segments': segments,
            'envelope_transitions': model.env.transitions}


def render(args):
    out = Path(args.out).absolute()
    require(not os.path.lexists(out), 'Output path must be new (including symlinks)')
    require(out.parent.is_dir(), 'Output parent must exist')
    event_bytes = envelope_reference.snapshot(args.events)
    events = envelope_reference.parse_events(event_bytes, Fraction(args.samples - 1, args.rate))
    result = solve(args, events)
    columns = ('sample', 'time_s', 'envelope', 'stage', 'gate', 'li', 'gain', 'raw', 'coupling', 'output', 'last_event')
    lines = [','.join(columns) + '\n']
    for row in result['rows']:
        lines.append(','.join(format(row[k], '.17g') if isinstance(row[k], float) else
                              str(int(row[k])) if isinstance(row[k], bool) else str(row[k]) for k in columns) + '\n')
    artifacts = {'trace.csv': ''.join(lines).encode('ascii'), 'input_events.csv': event_bytes,
                 'events.json': envelope_reference.json_bytes(result['event_audit']),
                 'transitions.json': envelope_reference.json_bytes(result['transitions']),
                 'segments.json': envelope_reference.json_bytes(result['segments']),
                 'envelope_transitions.json': envelope_reference.json_bytes(result['envelope_transitions']),
                 'renderer.py': envelope_reference.snapshot(__file__),
                 'render_amplitude_envelope_reference.py': envelope_reference.snapshot(envelope_reference.__file__)}
    manifest = {'schema': 'cs80-event-aware-envelope-vca-v1', 'complete': True,
        'scope': 'Constant-audio conditional envelope/VCA reference; inferred laws, not hardware, production equivalence, or a whole voice',
        'rate_hz': args.rate, 'samples': args.samples, 'intervals': args.samples-1,
        'endpoint_span': envelope_reference.exact_time(Fraction(args.samples-1,args.rate)),
        'parameters': {name: {'decimal_rational': str(getattr(args,name)), 'binary64': float(getattr(args,name))}
                       for name in ('attack','decay','sustain','release','audio_input','lc','ei','initial_li','initial_coupling')},
        'constants': {'tau_li_seconds': TAU_LI, 'tau_ac_seconds': TAU_AC,
                      'offset_promoted_float32': OFFSET, 'feedthrough_promoted_float32': FEEDTHROUGH},
        'equations': ['LI=LC+(LI0-LC)*exp(-t/tauLI)', 'raw=2**EI*(input+offset+feedthrough)*LI*envelope',
                      'dCoupling/dt=(raw-Coupling)/tauAC', 'output=raw-Coupling'],
        'clock': 'Exact Fraction samples/events; autonomous boundaries before same-time events in file order; snapshots after events; no t=0 tick',
        'integration': 'Closed exponential convolution; anchor only actual stage/event boundaries. Samples query anchors, never feed states back. Short-interval cancellation uses 40-term local series; binary64 evaluation is not a rigorous error certificate.',
        'phase_boundaries': 'Existing envelope exact rational attack duration; binary64 logarithmic decay/release durations converted once to Fraction; existing threshold snaps retained',
        'jump_semantics': 'LI/coupling continuous through envelope events/snaps; raw/output right-continuous jumps retained with no fictitious pre-event forcing',
        'units': 'Uncalibrated engine units for input/LC/LI/coupling/output; EI octaves; envelope unitless',
        'limits': {'max_samples': MAX_SAMPLES, 'max_events': MAX_EVENTS, 'max_seconds': 2,
                   'max_bundle_bytes': MAX_BUNDLE_BYTES, 'max_input_bytes': envelope_reference.MAX_INPUT_BYTES},
        'not_modelled': ['time-varying audio/LC/EI','negative LI or zero-floor crossings','filter/oscillator','voice allocation','pedals','live ADSR parameters','hardware calibration','production comparison'],
        'artifacts': {name: envelope_reference.metadata(data) for name,data in sorted(artifacts.items())},
        'provenance': 'Source/event snapshots identify bytes, not an authenticated runtime. Existing IG00151Reference.h equations and promoted float offsets; libm final bits can vary.'}
    artifacts['manifest.json'] = envelope_reference.json_bytes(manifest)
    require(sum(map(len,artifacts.values())) <= MAX_BUNDLE_BYTES, 'Bundle byte budget exceeded')
    owned = envelope_reference.Owned()
    try:
        owned.mkdir(out)
        for name,data in artifacts.items():
            owned.write(out/name,data)
    except BaseException:
        owned.cleanup()
        raise
    return result


def main(argv=None):
    try:
        render(parse(sys.argv[1:] if argv is None else argv))
    except (ValueError,OSError,UnicodeError,OverflowError) as error:
        print('event-aware envelope/VCA: '+str(error),file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
