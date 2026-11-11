#!/usr/bin/env python3
"""Independent fixed IL/AL/A/D/R filter-EG analytic endpoints (Python 3.10+)."""
import argparse
from fractions import Fraction
import math
import os
from pathlib import Path
import struct
import sys
import types

# Reuse only input, serialization, and output-ownership helpers, not the amplitude
# model. Keeping the existing module unchanged preserves its CLI and artifacts.
HELPER = Path(__file__).resolve().with_name('render_amplitude_envelope_reference.py')
with HELPER.open('rb') as source:
    HELPER_BYTES = source.read(1024 * 1024 + 1)
if len(HELPER_BYTES) > 1024 * 1024:
    raise ValueError('Helper source exceeds byte budget')
io = types.ModuleType('_filter_reference_io')
io.__file__ = str(HELPER)
# Compile exactly these bytes without creating an unowned __pycache__ artifact.
exec(compile(HELPER_BYTES, str(HELPER), 'exec'), io.__dict__)
require, number, integer = io.require, io.number, io.integer
MAX_SAMPLES, MAX_EVENTS = io.MAX_SAMPLES, io.MAX_EVENTS
MAX_INPUT_BYTES, MAX_BUNDLE_BYTES = io.MAX_INPUT_BYTES, io.MAX_BUNDLE_BYTES
MAX_WORK = MAX_SAMPLES + 4 * MAX_EVENTS + 3
# The declared production literal is 0.0008f, promoted without a decimal change.
SETTLE_EPSILON = struct.unpack('<f', struct.pack('<f', .0008))[0]
ACTIONS = ('fresh_on', 'retained_retrigger', 'note_off', 'reset')
MAPPING = ('hp_base_hz', 'lp_base_hz', 'hp_depth', 'lp_depth', 'q')


def parse(argv):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False,
        epilog='Inferred model, not hardware truth or production equivalence. N endpoints at i/rate; no t=0 tick.')
    for name in ('events', 'il', 'al', 'attack', 'decay', 'release', 'rate', 'samples', 'out'):
        parser.add_argument('--' + name, required=True)
    for name in MAPPING:
        parser.add_argument('--' + name.replace('_', '-'))
    options = [arg.split('=', 1)[0] for arg in argv if arg.startswith('--')]
    require(len(options) == len(set(options)), 'Duplicate option')
    args = parser.parse_args(argv)
    args.rate = integer(args.rate, 8000, 384000)
    args.samples = integer(args.samples, 2, MAX_SAMPLES)
    require(Fraction(args.samples - 1, args.rate) <= 10, 'Endpoint span exceeds ten seconds')
    mapping_count = sum(getattr(args, name) is not None for name in MAPPING)
    require(mapping_count in (0, len(MAPPING)), 'Supply all five mapping options, or none')
    ranges = [('il', -8, 8), ('al', -8, 8), ('attack', Fraction(1, 1000), 1),
              ('decay', Fraction(1, 100), 10), ('release', Fraction(1, 100), 10)]
    if mapping_count:
        ranges += [('hp_base_hz', 25, 20000), ('lp_base_hz', 25, 20000),
                   ('hp_depth', -8, 8), ('lp_depth', -8, 8), ('q', Fraction(35, 100), 9)]
    for name, low, high in ranges:
        value = number(getattr(args, name))
        require(low <= value <= high, name + ' outside bounds')
        setattr(args, name, value)
    return args


def parse_events(data, span):
    require(len(data) <= MAX_INPUT_BYTES, 'Event input exceeds byte budget')
    lines = data.split(b'\n')
    require(lines[-1] == b'', 'Event file must end in a newline')
    require(2 <= len(lines) <= MAX_EVENTS + 2, 'Event count outside bounds')
    require(all(len(line) <= 128 for line in lines), 'Event line exceeds 128 bytes')
    lines = [line[:-1] if line.endswith(b'\r') else line for line in lines[:-1]]
    require(lines[0] == b'event,time_s,action', 'Unexpected event header')
    result, previous = [], Fraction(0)
    for index, line in enumerate(lines[1:]):
        fields = line.decode('ascii').split(',')
        require(len(fields) == 3 and fields[0] == str(index), 'Noncanonical event index or columns')
        t = number(fields[1])
        require(previous <= t <= span, 'Events must be ordered and inside the endpoint span')
        require(fields[2] in ACTIONS, 'Unknown event action')
        result.append((t, fields[2]))
        previous = t
    return result


class Envelope:
    """Independent closed forms, with phase changes on an exact rational clock."""
    def __init__(self, il, al, attack, decay, release):
        self.il, self.al = float(il), float(al)
        self.attack, self.decay, self.release = attack, float(decay), float(release)
        self.stage, self.origin = 'idle', 0.0
        self.start = self.now = Fraction(0)
        self.end, self.gate, self.transitions = None, False, []

    def value(self, t):
        elapsed = t - self.start
        if self.stage == 'attack':
            span = self.al - self.origin
            if 2 * elapsed <= self.attack:
                return self.origin + 1.5 * span * -math.expm1(-math.log(3.0) * float(elapsed / self.attack))
            # Works in both directions, including a negative AL. A rounded AL
            # before the boundary never changes phase: the rational clock does.
            remaining = float((self.end - t) / self.attack)
            return self.al - .5 * span * math.expm1(math.log(3.0) * remaining)
        if self.stage in ('decay', 'release'):
            target = 0.0 if self.stage == 'decay' else self.il
            seconds = self.decay if self.stage == 'decay' else self.release
            return target + (self.origin - target) * math.exp(-math.log(1000.0) * float(elapsed) / seconds)
        return self.origin

    def begin(self, stage, t, value):
        self.stage, self.start, self.origin, self.end = stage, t, value, None
        if stage == 'attack':
            self.end = t + self.attack if value != self.al else t
        elif stage in ('decay', 'release'):
            target = 0.0 if stage == 'decay' else self.il
            span = abs(value - target)
            seconds = self.decay if stage == 'decay' else self.release
            # log1p preserves a positive duration for adjacent representable
            # spans above epsilon. There is no tolerance bucket or time clamp.
            duration = (seconds / math.log(1000.0) * math.log1p((span - SETTLE_EPSILON) / SETTLE_EPSILON)
                        if span > SETTLE_EPSILON else 0.0)
            self.end = t + Fraction(duration)

    def advance(self, t):
        require(t >= self.now, 'Envelope clock moved backwards')
        while self.end is not None and self.end <= t:
            boundary, before = self.end, self.stage
            if before == 'attack':
                next_stage, target, before_value = 'decay', self.al, self.al
            else:
                next_stage, target = ('sustain', 0.0) if before == 'decay' else ('idle', self.il)
                before_value = target + math.copysign(SETTLE_EPSILON, self.origin - target)
            if boundary == self.start:
                before_value = self.origin
            self.begin(next_stage, boundary, target)
            self.transitions.append({'time': io.exact_time(boundary), 'before_stage': before,
                                     'after_stage': next_stage, 'before_value': before_value, 'after_value': target})
        self.now = t
        value = self.value(t)
        require(math.isfinite(value) and -8 <= value <= 8, 'Envelope result outside finite [-8,8]')
        return value

    def state(self):
        return {'stage': self.stage, 'envelope': self.value(self.now), 'gate': self.gate,
                'phase_start': io.exact_time(self.start)}

    def event(self, t, action):
        self.advance(t)
        before = self.state()
        if action in ('fresh_on', 'retained_retrigger'):
            self.gate = True
            self.begin('attack', t, self.il if action == 'fresh_on' else before['envelope'])
        elif action == 'note_off':
            self.gate = False
            if self.stage != 'idle':
                self.begin('release', t, before['envelope'])
        elif action == 'reset':
            self.gate = False
            self.begin('idle', t, 0.0)
        else:
            raise ValueError('Unknown event action')
        # Expose the fresh-on IL assignment even when zero-span phases and an
        # immediate decay snap follow it at the same time.
        action_state = self.state()
        self.advance(t)
        return {'time': io.exact_time(t), 'action': action, 'before': before,
                'after_action': action_state, 'after': self.state()}


def mapped_controls(args, value):
    hp = float(args.hp_base_hz) * 2.0 ** (float(args.hp_depth) * value)
    lp = float(args.lp_base_hz) * 2.0 ** (float(args.lp_depth) * value)
    q = float(args.q)
    require(all(math.isfinite(x) for x in (hp, lp, q)) and 25 <= hp <= 20000 and 25 <= lp <= 20000
            and .35 <= q <= 9, 'Mapped endpoint outside finite chain bounds; no clamping')
    return hp, lp, q


def generate(args, events):
    require(args.samples + 4 * len(events) + 3 <= MAX_WORK, 'Aggregate work budget exceeded')
    env = Envelope(args.il, args.al, args.attack, args.decay, args.release)
    lines = ['sample,time_s,envelope,stage,gate,last_event\n']
    controls = ['sample,hp_hz,lp_hz,q\n'] if args.q is not None else None
    audit, cursor = [], 0
    for i in range(args.samples):
        t = Fraction(i, args.rate)
        while cursor < len(events) and events[cursor][0] <= t:
            audit.append({'event': cursor, **env.event(*events[cursor])})
            cursor += 1
        value = env.advance(t)
        lines.append(f'{i},{float(t):.17g},{value:.17g},{env.stage},{int(env.gate)},{cursor - 1}\n')
        if controls is not None:
            hp, lp, q = mapped_controls(args, value)
            controls.append(f'{i},{hp:.17g},{lp:.17g},{q:.17g}\n')
    require(cursor == len(events), 'Unconsumed event')
    result = {'envelope.csv': ''.join(lines).encode('ascii'), 'events.json': io.json_bytes(audit),
              'transitions.json': io.json_bytes(env.transitions)}
    if controls is not None:
        result['filter_controls.csv'] = ''.join(controls).encode('ascii')
    return result


def render(args):
    out = Path(args.out).absolute()
    require(not os.path.lexists(out), 'Output path must be new (including symlinks)')
    require(out.parent.is_dir(), 'Output parent must exist')
    event_bytes = io.snapshot(args.events)
    events = parse_events(event_bytes, Fraction(args.samples - 1, args.rate))
    artifacts = generate(args, events)
    artifacts.update({'input_events.csv': event_bytes, 'renderer.py': io.snapshot(__file__),
                      'render_amplitude_envelope_reference.py': HELPER_BYTES})
    parameters = ('il', 'al', 'attack', 'decay', 'release') + (MAPPING if args.q is not None else ())
    manifest = {
        'schema': 'cs80-filter-envelope-reference-v1', 'complete': True,
        'scope': 'Independent fixed filter-EG inferred RC trajectory; not measured hardware, production equivalence, or a whole voice',
        'rate_hz': args.rate, 'samples': args.samples, 'intervals': args.samples - 1,
        'endpoint_span': io.exact_time(Fraction(args.samples - 1, args.rate)), 'event_count': len(events),
        'parameters': {name: {'decimal_rational': str(getattr(args, name)), 'binary64': float(getattr(args, name))}
                       for name in parameters},
        'initial_state': 'Idle, envelope 0, gate false; no tick at t=0; fresh_on assigns IL',
        'sustain_target': 0,
        'clock': 'Exact rational i/rate samples and decimal event times; same-time events in file order before sample, after autonomous boundaries',
        'phase_boundaries': 'Exact rational attack duration; binary64 log threshold duration converted once to exact rational, right-continuous completion with remainder advanced',
        'attack': 'start + 1.5*(AL-start)*(1-exp(-ln(3)*elapsed/A)); bidirectional comparator at elapsed A; start equal AL completes immediately',
        'decay_release': 'target + (start-target)*exp(-ln(1000)*elapsed/T); targets 0 / IL; T means -60 dB remaining span',
        'snap_policy': {'absolute_threshold': SETTLE_EPSILON, 'threshold_binary64_hex': SETTLE_EPSILON.hex(),
                        'source_literal': '0.0008f (IEEE-754 binary32 promoted to binary64)',
                        'equality': 'Snap at equality; immediately complete absolute spans already <= threshold'},
        'event_semantics': 'fresh_on assigns IL then attacks AL; retained_retrigger attacks from current binary64 level; note_off releases to IL unless Idle; reset sets Idle/0',
        'audit': 'before follows autonomous boundaries; after_action precedes zero-duration boundaries; after includes them; transition before_value declares threshold left limit or immediate origin',
        'control_export': ({'path': 'filter_controls.csv', 'schema': 'sample,hp_hz,lp_hz,q',
                           'kind': 'Declared endpoint mapping, not measured control-voltage calibration',
                           'law': 'HP=hp_base_hz*2^(hp_depth*EG); LP=lp_base_hz*2^(lp_depth*EG); explicit constant Q',
                           'units': 'EG dimensionless signed control; depth octaves per EG unit; base/cutoff Hz; Q dimensionless',
                           'interpolation': 'Downstream PWL cutoff-Hz endpoints are not the continuous nonlinear map of the analytic EG',
                           'bounds': 'Each emitted HP/LP 25..20000 Hz and Q .35..9; reject, never clamp; unsampled mappings are not exported or bounded'}
                          if args.q is not None else None),
        'not_modelled': ['live parameter changes', 'pedals', 'voice allocation or stealing', 'production float round trips', 'hardware control calibration', 'audio'],
        'limits': {'max_samples': MAX_SAMPLES, 'max_events': MAX_EVENTS, 'max_input_bytes': MAX_INPUT_BYTES,
                   'max_bundle_bytes': MAX_BUNDLE_BYTES, 'max_work_units': MAX_WORK,
                   'requested_work_bound': args.samples + 4 * len(events) + 3},
        'artifacts': {name: io.metadata(data) for name, data in sorted(artifacts.items())},
        'provenance': 'Exact event, renderer, and helper source snapshots read for this run. Helper supplies bounded IO/serialization/ownership only; its amplitude Envelope is unused. Source hashes are snapshot identities, not authenticated execution images. Same runtime is deterministic; libm last bits may vary.'}
    artifacts['manifest.json'] = io.json_bytes(manifest)
    require(sum(map(len, artifacts.values())) <= MAX_BUNDLE_BYTES, 'Bundle byte budget exceeded')
    owned = io.Owned()
    try:
        owned.mkdir(out)
        for name, data in artifacts.items():
            owned.write(out / name, data)  # Completion manifest last.
    except BaseException:
        owned.cleanup()
        raise


def main(argv=None):
    try:
        render(parse(sys.argv[1:] if argv is None else argv))
    except (ValueError, OSError, UnicodeError, OverflowError) as error:
        print('filter envelope reference: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
