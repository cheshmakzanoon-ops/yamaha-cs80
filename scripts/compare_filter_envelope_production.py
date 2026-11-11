#!/usr/bin/env python3
"""Optional, descriptive comparison of analytic filter EG and actual FilterEnv.

The standalone renderer and its arbitrary exact event clock are unchanged.
This POSIX snapshot runner accepts only endpoint-aligned production events.
"""
import argparse
from fractions import Fraction
import importlib.util
import json
import math
import os
from pathlib import Path
import selectors
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'filter_reference', ROOT / 'scripts/render_filter_envelope_reference.py')
REF = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REF)
MAX_BYTES = 64 * 1024 * 1024
TIMEOUT = 60
NAMES = ('envelope.csv', 'events.json', 'transitions.json', 'input_events.csv',
         'renderer.py', 'render_amplitude_envelope_reference.py', 'manifest.json')
OPTIONAL_NAMES = ('filter_controls.csv',)
SOURCES = ('src/tools/filter_production_trace.cpp', 'src/core/Dsp.h',
           'src/tools/TruthInputSha256.h', 'src/tools/DecimalConversion.h', 'CMakeLists.txt')
HEADER = 'sample,time_s,envelope,stage,elapsed,gate,last_event'
STAGES = ('idle', 'attack', 'decay', 'sustain', 'release')
IO = REF.io
require = REF.require


def read_json(data):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'Duplicate JSON key')
            result[key] = value
        return result
    def reject(value):
        raise ValueError('Nonfinite JSON: ' + value)
    result = json.loads(data, object_pairs_hook=pairs, parse_constant=reject)
    def finite(value):
        if isinstance(value, float):
            require(math.isfinite(value), 'Nonfinite JSON number')
        elif isinstance(value, dict):
            for v in value.values():
                finite(v)
        elif isinstance(value, list):
            for v in value:
                finite(v)
    finite(result)
    return result


class Owned(IO.Owned):
    def adopt_reference(self, directory):
        # Child creates only this private directory and these documented names.
        # Never remove unknown additions, symlinks, or replacement inodes.
        if directory.is_dir() and not directory.is_symlink():
            self.dirs[directory] = IO.identity(directory)
            for name in NAMES + OPTIONAL_NAMES:
                path = directory / name
                if path.is_file() and not path.is_symlink():
                    self.files[path] = IO.identity(path)


def execute(command, directory, input_path=None):
    """Bound both output streams and wall time; stdin is an owned regular file."""
    output, error = bytearray(), bytearray()
    source = open(input_path, 'rb') if input_path is not None else open(os.devnull, 'rb')
    with source, subprocess.Popen(command, cwd=directory, stdin=source,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE) as child:
        deadline = time.monotonic() + TIMEOUT
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(child.stdout, selectors.EVENT_READ, output)
                selector.register(child.stderr, selectors.EVENT_READ, error)
                while selector.get_map() or child.poll() is None:
                    remaining = deadline - time.monotonic()
                    require(remaining > 0, 'Child execution timeout')
                    for key, _ in selector.select(min(.1, remaining)):
                        data = os.read(key.fd, 65536)
                        if not data:
                            selector.unregister(key.fileobj)
                        else:
                            key.data.extend(data)
                            require(len(output) <= MAX_BYTES and len(error) <= 16384,
                                    'Child output exceeds byte budget')
                code = child.wait(timeout=max(.001, deadline - time.monotonic()))
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
    require(code == 0, 'Child failed: ' + error.decode('utf-8', errors='replace').strip())
    return bytes(output)


def parse(argv):
    p = argparse.ArgumentParser(description=__doc__, allow_abbrev=False, add_help=False)
    p.add_argument('--production', required=True, help='Built cs80_filter_production_trace executable')
    # The independent renderer remains the authority for all reference options.
    options = [arg.split('=', 1)[0] for arg in argv if arg.startswith('--')]
    require(len(options) == len(set(options)), 'Duplicate option')
    extra, reference_argv = p.parse_known_args(argv)
    args = REF.parse(reference_argv)
    args.production, args.reference_argv = extra.production, reference_argv
    tokens = {}
    for i, arg in enumerate(reference_argv):
        if arg.startswith('--'):
            key, sep, value = arg.partition('=')
            tokens[key[2:]] = value if sep else reference_argv[i + 1]
    args.requested = {key: tokens[key] for key in ('il', 'al', 'attack', 'decay', 'release')}
    return args


def aligned_events(events, rate):
    lines = ['event,sample,action\n']
    for i, (t, action) in enumerate(events):
        sample = t * rate
        require(sample.denominator == 1,
                f'Event {i} is not exactly endpoint-aligned; no production event quantization is allowed')
        lines.append(f'{i},{sample.numerator},{action}\n')
    return ''.join(lines).encode('ascii')


def passed_float(value):
    """Nearest-even binary32 from an exact decimal rational, without double rounding."""
    sign = -1 if value < 0 else 1
    value = abs(value)
    bits = struct.unpack('>I', struct.pack('>f', float(value)))[0]
    candidates = [(struct.unpack('>f', struct.pack('>I', b))[0], b)
                  for b in range(max(0, bits - 1), bits + 2)]
    return sign * min(candidates, key=lambda item: (abs(Fraction(item[0]) - value), item[1] % 2))[0]


def trace_rows(data, args, events):
    lines = data.decode('ascii').splitlines(keepends=True)
    require(len(lines) == args.samples + 1 and lines[0] == HEADER + '\n', 'Production trace count/header mismatch')
    result, cursor, gate = [], 0, False
    for i, line in enumerate(lines[1:]):
        require(line.endswith('\n') and len(line) <= 256, 'Incomplete/oversized production row')
        fields = line[:-1].split(',')
        require(len(fields) == 7 and fields[0] == str(i), 'Production row index/columns mismatch')
        values = [float(REF.number(fields[k])) for k in (1, 2, 4)]
        require(values[0] == float(Fraction(i, args.rate)), 'Production clock mismatch')
        require(-8 <= values[1] <= 8 and 0 <= values[2] <= 10 and fields[3] in STAGES,
                'Production state outside bounds')
        while cursor < len(events) and events[cursor][0] * args.rate == i:
            gate = events[cursor][1] in ('fresh_on', 'retained_retrigger')
            cursor += 1
        require(fields[5] == str(int(gate)) and fields[6] == str(cursor - 1), 'Production event ordering mismatch')
        result.append((values[1], fields[3], values[2]))
    return result


def compare(reference, production, args):
    lines = reference.decode('ascii').splitlines()
    require(len(lines) == args.samples + 1 and lines[0] == 'sample,time_s,envelope,stage,gate,last_event',
            'Reference trace mismatch')
    errors, values, mismatches = [], [], 0
    csv = ['sample,time_s,reference,production,difference,reference_stage,production_stage,production_elapsed\n']
    for i, (line, (actual, stage, elapsed)) in enumerate(zip(lines[1:], production)):
        fields = line.split(',')
        require(len(fields) == 6 and fields[0] == str(i) and fields[3] in STAGES, 'Reference row mismatch')
        require(float(REF.number(fields[1])) == float(Fraction(i, args.rate)), 'Reference clock mismatch')
        value = float(REF.number(fields[2]))
        require(-8 <= value <= 8, 'Reference level outside bounds')
        error = actual - value
        errors.append(error)
        values.append(value)
        mismatches += fields[3] != stage
        csv.append(f'{i},{fields[1]},{value:.17g},{actual:.17g},{error:.17g},{fields[3]},{stage},{elapsed:.17g}\n')
    rms = math.sqrt(math.fsum(x * x for x in errors) / args.samples)
    reference_rms = math.sqrt(math.fsum(x * x for x in values) / args.samples)
    metrics = {'rows': args.samples, 'maximum_absolute_error': max(map(abs, errors)),
               'rms_error': rms, 'reference_rms': reference_rms,
               'relative_rms_error': rms / reference_rms if reference_rms else None,
               'stage_mismatch_rows': mismatches,
               'definition': 'All N rows including t=0, events, snaps, and final endpoint; difference=production-reference. No alignment, exclusions, fitted tolerance, or pass/fail.'}
    return ''.join(csv).encode('ascii'), metrics


def render(args):
    require(os.name == 'posix', 'Snapshot execution currently requires POSIX')
    out = Path(args.out).absolute()
    require(not os.path.lexists(out) and out.parent.is_dir(), 'Output must be new and parent must exist')
    event_bytes = IO.snapshot(args.events)
    events = REF.parse_events(event_bytes, Fraction(args.samples - 1, args.rate))
    normalized = aligned_events(events, args.rate)
    snapshots = {'input_events.csv': event_bytes, 'production_events.csv': normalized,
                 'reference_renderer.py': IO.snapshot(ROOT / 'scripts/render_filter_envelope_reference.py'),
                 'render_amplitude_envelope_reference.py': IO.snapshot(ROOT / 'scripts/render_amplitude_envelope_reference.py'),
                 'comparison_renderer.py': IO.snapshot(__file__),
                 'production_executable': IO.snapshot(args.production, MAX_BYTES)}
    for path in SOURCES:
        snapshots['sources/' + path] = IO.snapshot(ROOT / path)
    owned = Owned()
    temp = Path(tempfile.mkdtemp(prefix='cs80-filter-comparison-'))
    owned.dirs[temp] = IO.identity(temp)
    try:
        for name in ('input_events.csv', 'production_events.csv', 'reference_renderer.py',
                     'render_amplitude_envelope_reference.py', 'production_executable'):
            owned.write(temp / name, snapshots[name])
        (temp / 'production_executable').chmod(0o500)
        reference_dir = temp / 'reference'
        # These bounded rationals originate as decimals; retain original CLI tokens
        # by replacing only the two paths in the already validated invocation.
        reference_args = list(args.reference_argv)
        for key, value in (('events', str(temp / 'input_events.csv')), ('out', str(reference_dir))):
            index = next(i for i, token in enumerate(reference_args) if token.split('=', 1)[0] == '--' + key)
            if '=' in reference_args[index]:
                reference_args[index] = '--' + key + '=' + value
            else:
                reference_args[index + 1] = value
        try:
            require(not execute([sys.executable, str(temp / 'reference_renderer.py'), *reference_args], temp),
                    'Unexpected reference stdout')
        finally:
            owned.adopt_reference(reference_dir)
        names = NAMES + (OPTIONAL_NAMES if args.q is not None else ())
        require(set(p.name for p in reference_dir.iterdir()) == set(names), 'Unexpected reference artifacts')
        reference = {name: IO.snapshot(reference_dir / name, MAX_BYTES) for name in names}
        manifest = read_json(reference['manifest.json'])
        require(manifest['complete'] is True and manifest['schema'] == 'cs80-filter-envelope-reference-v1',
                'Incomplete reference manifest')
        require(manifest['artifacts'] == {name: IO.metadata(data) for name, data in reference.items() if name != 'manifest.json'},
                'Reference artifact hash mismatch')
        require(reference['input_events.csv'] == event_bytes and reference['renderer.py'] == snapshots['reference_renderer.py'],
                'Reference input/source snapshot mismatch')
        require(reference['render_amplitude_envelope_reference.py'] == snapshots['render_amplitude_envelope_reference.py'],
                'Reference IO helper snapshot mismatch')
        raw = execute([str(temp / 'production_executable'), str(args.rate), str(args.samples),
                       *args.requested.values()], temp, temp / 'production_events.csv')
        meta_bytes, separator, trace = raw.partition(b'\n')
        require(separator, 'Missing production metadata')
        meta = read_json(meta_bytes)
        require(meta['schema'] == 'cs80-filter-production-trace-v1', 'Production schema mismatch')
        require(meta['source_sha256'] == {path: IO.metadata(snapshots['sources/' + path])['sha256'] for path in SOURCES},
                'Production build/source mismatch; rebuild the helper for this checkout')
        require(meta['input_sha256'] == IO.metadata(normalized)['sha256'] and meta['input_bytes'] == len(normalized)
                and meta['samples'] == args.samples and meta['events'] == len(events),
                'Production input provenance mismatch')
        parameters = meta['parameters']
        require(set(parameters) == {'il', 'al', 'attack', 'decay', 'release', 'rate_hz'}, 'Production parameter schema mismatch')
        require(parameters['rate_hz'] == args.rate, 'Production sample rate mismatch')
        for key in args.requested:
            require(type(parameters[key]) in (float, int) and parameters[key] == passed_float(getattr(args, key)),
                    'Production parameter quantization mismatch')
        production = trace_rows(trace, args, events)
        differences, metrics = compare(reference['envelope.csv'], production, args)
        artifacts = {**snapshots, **{'reference/' + name: data for name, data in reference.items()},
                     'production.csv': trace, 'production_metadata.json': IO.json_bytes(meta),
                     'comparison.csv': differences}
        report = {
            'schema': 'cs80-filter-production-comparison-v1', 'complete': True,
            'scope': 'Diagnostic difference between inferred continuous reference and actual reconstruction FilterEnv; not hardware accuracy, whole-voice or legacy parity',
            'rate_hz': args.rate, 'samples': args.samples, 'intervals': args.samples - 1,
            'endpoint_span': IO.exact_time(Fraction(args.samples - 1, args.rate)),
            'requested_parameters': {key: {'decimal': args.requested[key], 'decimal_rational': str(getattr(args, key)),
                                         'reference_binary64': float(getattr(args, key)), 'production_float': parameters[key]}
                                     for key in args.requested},
            'production_rate_float': parameters['rate_hz'], 'metrics': metrics,
            'event_semantics': 'fresh_on: noteOn(IL,AL,A,D,R,0,false); retained_retrigger: same with true; note_off: noteOff unless Idle (native no-op); reset: reset. Initial/reset Idle is 0, completed-release Idle is IL. No live changes or engine lifecycle.',
            'mapping_scope': 'Optional cutoff/Q mapping is preserved only in standalone reference artifacts, never compared with production filters.',
            'clock': 'At i=0 apply events in file order without a tick. At i>0 tick(rate,true,true) for preceding interval, then apply events at i in file order, then export. All event times times rate must be exact integers.',
            'differences': ['Reference resolves continuous crossings and advances fractional remainder; production checks once per sample, clamps, and discards boundary remainder.',
                            'Production receives float IL/AL/A/D/R and rate. Fresh onset assigns float IL; retained retrigger preserves private double level, not a public-float round trip. Reference retains binary64 level and exact rational attack clock.',
                            'Both use the promoted binary32 0.0008f settle threshold, but production checks strict < once per tick and reference snaps at continuous equality. Zero-span attack remains Attack until a tick; reference completes it immediately. Stage mismatch is diagnostic, not automatically an error.',
                            'Production elapsed is the actual public float diagnostic, not a reconstructed phase age; decay/sustain and release/idle transitions do not reset that clock.'],
            'limits': {'max_bundle_bytes': MAX_BYTES, 'child_timeout_seconds': TIMEOUT},
            'provenance': 'Reference executed from frozen source and production from frozen binary; exact inputs, binary, compile-bound source snapshots and hashes included. Hashes identify bytes, not authenticated builds or bundled Python/compiler/libm runtime. Comparison source snapshot is not an authenticated execution image.',
            'artifacts': {name: IO.metadata(data) for name, data in sorted(artifacts.items())}}
        artifacts['manifest.json'] = IO.json_bytes(report)
        require(sum(map(len, artifacts.values())) <= MAX_BYTES, 'Comparison bundle exceeds byte budget')
        owned.mkdir(out)
        for name, data in artifacts.items():
            path = out / name
            missing = []
            parent = path.parent
            while parent != out and not parent.exists():
                missing.append(parent)
                parent = parent.parent
            for parent in reversed(missing):
                owned.mkdir(parent)
            owned.write(path, data)  # Completion marker is last.
    except BaseException:
        owned.cleanup()
        raise
    # Keep published artifacts, but remove only tracked temporary inodes.
    owned.files = {p: inode for p, inode in owned.files.items() if temp in p.parents}
    owned.dirs = {p: inode for p, inode in owned.dirs.items() if p == temp or temp in p.parents}
    owned.cleanup()


def main(argv=None):
    try:
        render(parse(sys.argv[1:] if argv is None else argv))
    except (ValueError, OSError, UnicodeError, KeyError, TypeError) as error:
        print('filter comparison: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
