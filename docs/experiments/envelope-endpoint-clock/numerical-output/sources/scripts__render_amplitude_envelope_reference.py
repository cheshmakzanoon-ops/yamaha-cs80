#!/usr/bin/env python3
"""Independent fixed-ADSR analytic endpoint reference (Python 3.10+, stdlib)."""
import argparse
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import sys

MAX_SAMPLES = 100001
MAX_EVENTS = 4096
MAX_INPUT_BYTES = 1024 * 1024
MAX_BUNDLE_BYTES = 64 * 1024 * 1024
MAX_WORK = MAX_SAMPLES + 4 * MAX_EVENTS + 3
DECIMAL = re.compile(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z')
ACTIONS = ('note_on', 'note_off', 'retrigger', 'reset')
DECAY_EPSILON = .0005
RELEASE_EPSILON = .0002


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(text):
    """Bound representation work as well as magnitude, before Fraction parsing."""
    require(len(text) <= 64 and DECIMAL.fullmatch(text), 'Expected bounded ASCII decimal')
    pieces = re.split('[eE]', text)
    if len(pieces) == 2:
        require(len(pieces[1].lstrip('+-')) <= 3 and abs(int(pieces[1])) <= 300,
                'Decimal exponent outside [-300,300]')
    value = float(text)
    require(math.isfinite(value) and (value != 0 or not any(c in '123456789' for c in pieces[0])),
            'Unrepresentable decimal')
    return Fraction(text)


def integer(text, low, high):
    require(len(text) <= 10 and re.fullmatch(r'(?:0|[1-9][0-9]*)', text),
            'Expected canonical unsigned integer')
    value = int(text)
    require(low <= value <= high, 'Integer outside bounds')
    return value


def snapshot(path, limit=MAX_INPUT_BYTES):
    fd = os.open(path, os.O_RDONLY | getattr(os, 'O_NONBLOCK', 0))
    with os.fdopen(fd, 'rb') as source:
        require(stat.S_ISREG(os.fstat(source.fileno()).st_mode), 'Expected regular file')
        data = source.read(limit + 1)
    require(len(data) <= limit, 'File exceeds byte budget')
    return data


def metadata(data):
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def exact_time(t):
    return {'numerator': t.numerator, 'denominator': t.denominator, 'seconds': float(t)}


def json_bytes(value):
    return (json.dumps(value, sort_keys=True, indent=2, allow_nan=False) + '\n').encode('ascii')


def parse(argv):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False,
        epilog='Inferred software model, not hardware truth or production equivalence. N endpoints at i/rate, no t=0 tick.')
    for name in ('events', 'attack', 'decay', 'sustain', 'release', 'lc', 'ei', 'rate', 'samples', 'out'):
        parser.add_argument('--' + name, required=True)
    options = [arg.split('=', 1)[0] for arg in argv if arg.startswith('--')]
    require(len(options) == len(set(options)), 'Duplicate option')
    args = parser.parse_args(argv)
    args.rate = integer(args.rate, 8000, 384000)
    args.samples = integer(args.samples, 2, MAX_SAMPLES)
    require(Fraction(args.samples - 1, args.rate) <= 10, 'Endpoint span exceeds ten seconds')
    for name, low, high in [('attack', Fraction(1, 1000), 1), ('decay', Fraction(1, 100), 10),
                          ('sustain', 0, 1), ('release', Fraction(1, 100), 10), ('lc', -8, 8), ('ei', -8, 8)]:
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
    result = []
    previous = Fraction(0)
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
    """Phase-anchored closed forms; never uses production or sampled recurrences."""
    def __init__(self, attack, decay, sustain, release):
        self.attack = attack
        self.decay = float(decay)
        self.sustain = float(sustain)
        self.release = float(release)
        self.stage = 'idle'
        self.start = Fraction(0)
        self.origin = 0.0
        self.end = None
        self.now = Fraction(0)
        self.gate = False
        self.transitions = []

    def value(self, t):
        elapsed = float(t - self.start)
        if self.stage == 'attack':
            # Equivalent forms near each endpoint avoid cancellation near zero
            # and overshoot rounding near one. The exact rational remainder,
            # not float(elapsed)==float(A), controls the late-attack evaluation.
            # Neither form changes stage; only advance's exact end clock does.
            if 2 * (t - self.start) <= self.attack:
                phase = float((t - self.start) / self.attack)
                return self.origin + 1.5 * (1.0 - self.origin) * -math.expm1(-math.log(3.0) * phase)
            remaining = float((self.end - t) / self.attack)
            return 1.0 - .5 * (1.0 - self.origin) * math.expm1(math.log(3.0) * remaining)
        if self.stage == 'decay':
            return self.sustain + (self.origin - self.sustain) * math.exp(-math.log(1000.0) * elapsed / self.decay)
        if self.stage == 'release':
            return self.origin * math.exp(-math.log(1000.0) * elapsed / self.release)
        return self.origin

    def begin(self, stage, t, value):
        self.stage, self.start, self.origin = stage, t, value
        self.end = None
        if stage == 'attack':
            self.end = t + self.attack if value < 1.0 else t
        elif stage in ('decay', 'release'):
            span = value - self.sustain if stage == 'decay' else value
            epsilon = DECAY_EPSILON if stage == 'decay' else RELEASE_EPSILON
            seconds = self.decay if stage == 'decay' else self.release
            duration = seconds / math.log(1000.0) * math.log(span / epsilon) if span > epsilon else 0.0
            # Declare the binary64 logarithmic duration once, then compare clocks
            # exactly; no epsilon/tolerance buckets or accumulated sample times.
            self.end = t + Fraction(duration)

    def advance(self, t):
        require(t >= self.now, 'Envelope clock moved backwards')
        while self.end is not None and self.end <= t:
            boundary, before = self.end, self.stage
            value_before = 1.0 if before == 'attack' else (
                self.sustain + DECAY_EPSILON if before == 'decay' else RELEASE_EPSILON)
            if boundary == self.start:
                value_before = self.origin
            next_stage, after = {'attack': ('decay', 1.0), 'decay': ('sustain', self.sustain),
                                 'release': ('idle', 0.0)}[before]
            self.begin(next_stage, boundary, after)
            self.transitions.append({'time': exact_time(boundary), 'before_stage': before,
                                     'after_stage': next_stage, 'before_value': value_before, 'after_value': after})
        self.now = t
        value = self.value(t)
        require(math.isfinite(value) and 0 <= value <= 1, 'Envelope result outside finite [0,1]')
        return value

    def state(self):
        return {'stage': self.stage, 'envelope': self.value(self.now), 'gate': self.gate,
                'phase_start': exact_time(self.start)}

    def event(self, t, action):
        self.advance(t)
        before = self.state()
        if action in ('note_on', 'retrigger'):
            self.gate = True
            self.begin('attack', t, before['envelope'])
        elif action == 'note_off':
            self.gate = False
            if self.stage != 'idle':
                self.begin('release', t, before['envelope'])
        elif action == 'reset':
            self.gate = False
            self.begin('idle', t, 0.0)
        else:
            raise ValueError('Unknown event action')
        self.advance(t)  # Resolve zero-duration boundaries before the after audit.
        return {'time': exact_time(t), 'action': action, 'before': before, 'after': self.state()}


def generate(args, events):
    require(args.samples + 4 * len(events) + 3 <= MAX_WORK, 'Aggregate work budget exceeded')
    env = Envelope(args.attack, args.decay, args.sustain, args.release)
    envelope_lines = ['sample,time_s,envelope,stage,gate,last_event\n']
    control_lines = ['sample,lc,ei,envelope\n']
    event_audit, cursor = [], 0
    lc, ei = format(float(args.lc), '.17g'), format(float(args.ei), '.17g')
    for i in range(args.samples):
        t = Fraction(i, args.rate)
        while cursor < len(events) and events[cursor][0] <= t:
            event_audit.append({'event': cursor, **env.event(*events[cursor])})
            cursor += 1
        value = format(env.advance(t), '.17g')
        envelope_lines.append(f'{i},{float(t):.17g},{value},{env.stage},{int(env.gate)},{cursor - 1}\n')
        control_lines.append(f'{i},{lc},{ei},{value}\n')
    require(cursor == len(events), 'Unconsumed event')
    return {'envelope.csv': ''.join(envelope_lines).encode('ascii'),
            'controls.csv': ''.join(control_lines).encode('ascii'),
            'events.json': json_bytes(event_audit), 'transitions.json': json_bytes(env.transitions)}


def identity(path):
    info = path.lstat()
    return info.st_dev, info.st_ino


class Owned:
    """Cleanup only this invocation's inodes; never recursively remove a tree."""
    def __init__(self):
        self.files, self.dirs = {}, {}

    def mkdir(self, path):
        path.mkdir(mode=0o700)
        self.dirs[path] = identity(path)

    def write(self, path, data):
        with path.open('xb') as out:
            self.files[path] = identity(path)
            out.write(data)

    def cleanup(self):
        for paths, remove in ((self.files, Path.unlink), (self.dirs, Path.rmdir)):
            for path, expected in reversed(list(paths.items())):
                try:
                    if identity(path) == expected:
                        remove(path)
                except OSError:
                    pass


def render(args):
    out = Path(args.out).absolute()
    require(not os.path.lexists(out), 'Output path must be new (including symlinks)')
    require(out.parent.is_dir(), 'Output parent must exist')
    event_bytes = snapshot(args.events)
    events = parse_events(event_bytes, Fraction(args.samples - 1, args.rate))
    artifacts = generate(args, events)
    artifacts['input_events.csv'] = event_bytes
    artifacts['renderer.py'] = snapshot(__file__)
    manifest = {
        'schema': 'cs80-amplitude-envelope-reference-v1', 'complete': True,
        'scope': 'Independent fixed-ADSR inferred RC trajectory, not measured hardware, production equivalence, or a whole voice',
        'rate_hz': args.rate, 'samples': args.samples, 'intervals': args.samples - 1,
        'endpoint_span': exact_time(Fraction(args.samples - 1, args.rate)), 'event_count': len(events),
        'parameters': {name: {'decimal_rational': str(getattr(args, name)), 'binary64': float(getattr(args, name))}
                       for name in ('attack', 'decay', 'sustain', 'release', 'lc', 'ei')},
        'initial_state': 'Idle, envelope 0, gate false; no tick at t=0',
        'clock': 'Exact rational i/rate samples and decimal event times; same-time events in file order before sample, after autonomous boundaries',
        'phase_boundaries': 'Exact rational attack duration; binary64 log threshold duration converted once to exact rational, right-continuous completion with remaining time advanced',
        'attack': 'start + 1.5*(1-start)*(1-exp(-ln(3)*elapsed/A)); comparator at elapsed A; start 1 completes immediately',
        'decay_release': 'target + span*exp(-ln(1000)*elapsed/T); T is -60 dB remaining span, not completion time',
        'snap_policy': {'decay_absolute_threshold': DECAY_EPSILON, 'release_absolute_threshold': RELEASE_EPSILON,
                        'equality': 'Snap at equality; immediately complete spans already <= threshold'},
        'event_semantics': 'note_on and retrigger retain current binary64 level and restart attack; note_off restarts release unless Idle; reset sets Idle/0',
        'control_export': {'path': 'controls.csv', 'schema': 'sample,lc,ei,envelope',
                           'lc': 'Explicit constant uncalibrated LC', 'ei': 'Explicit constant EI in octaves',
                           'envelope': 'Unitless [0,1]; existing chain reconstructs these samples piecewise linearly, not as this analytic curve'},
        'not_modelled': ['live parameter changes', 'pedals', 'voice allocation or stealing', 'filter envelope', 'production float round trips', 'audio'],
        'limits': {'max_samples': MAX_SAMPLES, 'max_events': MAX_EVENTS, 'max_input_bytes': MAX_INPUT_BYTES,
                   'max_bundle_bytes': MAX_BUNDLE_BYTES, 'max_work_units': MAX_WORK,
                   'requested_work_bound': args.samples + 4 * len(events) + 3},
        'artifacts': {name: metadata(data) for name, data in sorted(artifacts.items())},
        'provenance': 'Exact event and source snapshots read for this run. Source hash is a snapshot identity, not an authenticated execution/runtime image. Same runtime is deterministic; libm last bits can vary across platforms.'}
    artifacts['manifest.json'] = json_bytes(manifest)
    require(sum(map(len, artifacts.values())) <= MAX_BUNDLE_BYTES, 'Bundle byte budget exceeded')
    owned = Owned()
    try:
        owned.mkdir(out)
        for name, data in artifacts.items():  # Completion marker is always last.
            owned.write(out / name, data)
    except BaseException:
        owned.cleanup()
        raise


def main(argv=None):
    try:
        render(parse(sys.argv[1:] if argv is None else argv))
    except (ValueError, OSError, UnicodeError) as error:
        print('amplitude reference: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
