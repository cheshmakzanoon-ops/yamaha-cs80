#!/usr/bin/env python3
"""Bounded HP→LP→VCA endpoint diagnostic; Python 3.10+, stdlib only."""
import argparse
from array import array
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import selectors
import stat
import subprocess
import sys
import tempfile
import time

MAX_BYTES = 64 * 1024 * 1024
MAX_SAMPLES = 100001
MAX_STEPS = 10000000
CHILD_TIMEOUT = 120
DECIMAL = re.compile(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z')
FILTER_HEADER = 'sample,time_s,input,hp,lp,hp_vin,hp_x,hp_bp,hp_lp,lp_vin,lp_x,lp_bp,lp_lp'
REALTIME_HEADER = 'sample,time_s,input_float,hp_realtime,lp_realtime,hp_difference,lp_difference'
FILTER_CONTROL_HEADER = 'sample,time_s,hp_hz,lp_hz,q,hp_gm_siemens,lp_gm_siemens,hp_q_siemens,lp_q_siemens'
REALTIME_CONTROL_COLUMNS = ',hp_gm_cutoff_hz_float,lp_gm_cutoff_hz_float,hp_effective_total_hz,lp_effective_total_hz,q_float,hp_q_siemens_realtime,lp_q_siemens_realtime'
VCA_HEADER = 'sample,time,input,lc,ei,envelope,li,gain,coupling,output'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def decimal(text):
    require(len(text) <= 256 and DECIMAL.fullmatch(text), 'Expected bounded ASCII decimal')
    value = float(text)
    # Reject nonzero decimals that underflow to zero, like the component parsers.
    mantissa = re.split('[eE]', text)[0]
    require(math.isfinite(value) and (value != 0 or not any(c in '123456789' for c in mantissa)),
            'Unrepresentable decimal')
    return value


def integer(text, low, high):
    require(len(text) <= 10 and re.fullmatch('[0-9]+', text), 'Expected bounded unsigned integer')
    value = int(text)
    require(low <= value <= high, 'Integer outside bounds')
    return value


def snapshot(path, limit=MAX_BYTES):
    # O_NONBLOCK avoids hanging on a FIFO before checking the opened descriptor.
    fd = os.open(path, os.O_RDONLY | getattr(os, 'O_NONBLOCK', 0))
    with os.fdopen(fd, 'rb') as source:
        require(stat.S_ISREG(os.fstat(source.fileno()).st_mode), 'Expected regular file')
        data = source.read(limit + 1)
    require(len(data) <= limit, 'File exceeds byte budget')
    return data


def identity(path):
    s = path.lstat()
    return s.st_dev, s.st_ino


def digest(data):
    return hashlib.sha256(data).hexdigest()


def metadata(data):
    return {'bytes': len(data), 'sha256': digest(data)}


def endpoints(data, header, count, bounds, line_limit=256):
    """Strict bytes, not Python CSV's whitespace/quoting extensions."""
    lines = data.split(b'\n')
    require(lines[-1] == b'' and len(lines) == count + 2, 'Endpoint count or final newline mismatch')
    require(all(len(line) <= line_limit for line in lines), 'Endpoint line exceeds limit')
    lines = [line[:-1] if line.endswith(b'\r') else line for line in lines[:-1]]
    require(lines[0] == header.encode('ascii'), 'Unexpected endpoint header')
    columns = [array('d') for _ in bounds]
    for i, line in enumerate(lines[1:]):
        fields = line.decode('ascii').split(',')
        require(len(fields) == len(bounds) + 1 and fields[0] == str(i), 'Noncanonical index or columns')
        for values, text, (low, high) in zip(columns, fields[1:], bounds):
            value = decimal(text)
            require(low <= value <= high, 'Endpoint value outside bounds')
            values.append(value)
    return columns


class Owned:
    """Never recursively delete a directory or remove an untracked replacement."""
    def __init__(self):
        self.files = {}
        self.dirs = {}

    def mkdir(self, path):
        path.mkdir(mode=0o700)
        self.dirs[path] = identity(path)

    def write(self, path, data, executable=False):
        with path.open('xb') as out:
            self.files[path] = identity(path)
            out.write(data)
        if executable:
            path.chmod(0o500)

    def adopt(self, directory, names):
        # Private child output: only the documented filenames, never siblings.
        if directory.is_dir() and not directory.is_symlink():
            self.dirs[directory] = identity(directory)
            for name in names:
                path = directory / name
                if path.is_file() and not path.is_symlink():
                    self.files[path] = identity(path)

    def cleanup(self):
        for path, expected in reversed(list(self.files.items())):
            try:
                if identity(path) == expected:
                    path.unlink()
            except OSError:
                pass
        for path, expected in reversed(list(self.dirs.items())):
            try:
                if identity(path) == expected:
                    path.rmdir()  # Unexpected concurrent additions are retained.
            except OSError:
                pass


def parse(argv):
    p = argparse.ArgumentParser(description=__doc__, allow_abbrev=False,
        epilog='N endpoints / N-1 intervals. No resampling, normalization, engine or jointly integrated whole-note reference.')
    for name in ('filter', 'vca', 'input', 'controls', 'rate', 'samples', 'out', 'ota-level', 'gain'):
        p.add_argument('--' + name, required=True)
    p.add_argument('--filter-controls', help='sample,hp_hz,lp_hz,q endpoints; replaces all three scalar filter controls')
    for name in ('hp-hz', 'lp-hz', 'q'):
        p.add_argument('--' + name)
    for name, default in [('filter-substeps', '16'), ('vca-steps', '8'),
                          ('filter-tolerance', '.001'), ('vca-tolerance', '.001')]:
        p.add_argument('--' + name, default=default)
    p.add_argument('--initial-li')
    p.add_argument('--compare-realtime', choices=['true'])
    p.add_argument('--wav-gain')
    # argparse normally accepts duplicates with last-wins semantics; fail closed.
    options = [arg.split('=', 1)[0] for arg in argv if arg.startswith('--')]
    require(len(options) == len(set(options)), 'Duplicate option')
    a = p.parse_args(argv)
    scalars = (a.hp_hz, a.lp_hz, a.q)
    require(all(v is None for v in scalars) if a.filter_controls is not None
            else all(v is not None for v in scalars),
            'Supply either --filter-controls or all of --hp-hz, --lp-hz and --q; never combine them')
    a.rate = integer(a.rate, 8000, 384000)
    a.samples = integer(a.samples, 2, MAX_SAMPLES)
    a.filter_substeps = integer(a.filter_substeps, 1, 256)
    a.vca_steps = integer(a.vca_steps, 1, 128)
    a.compare_realtime = a.compare_realtime == 'true'
    require(a.samples - 1 <= 10 * a.rate, 'Duration exceeds ten seconds')
    a.integration_steps = 3 * (a.samples - 1) * (a.filter_substeps + a.vca_steps * (2 if a.compare_realtime else 1))
    require(a.integration_steps <= MAX_STEPS, 'Aggregate integration budget exceeds ten million')
    for name, low, high in [('hp_hz', 25, 20000), ('lp_hz', 25, 20000), ('q', .35, 9),
                            ('ota_level', 0, 10), ('filter_tolerance', 0, .1), ('vca_tolerance', 0, 1),
                            ('initial_li', -8, 8), ('wav_gain', 0, 1000000)]:
        if getattr(a, name) is not None:
            value = decimal(getattr(a, name))
            require(low <= value <= high, name + ' outside bounds')
            require('tolerance' not in name or value > 0, 'Tolerance must be positive')
            setattr(a, name, value)
    require(a.gain in ('unity', 'physical'), 'gain must be unity or physical')
    require(not a.compare_realtime or (a.gain == 'unity' and a.ota_level == 2.5),
            'Comparison requires gain unity and ota-level 2.5')
    return a


def read_json(path):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'Duplicate JSON key')
            result[key] = value
        return result
    def reject(value):
        raise ValueError('Nonfinite JSON value: ' + value)
    return json.loads(snapshot(path, 1024 * 1024), object_pairs_hook=pairs, parse_constant=reject)


def trace(path, header, count, rate, selected):
    data = snapshot(path)
    require(data.endswith(b'\n'), 'Incomplete stage trace')
    reader = csv.reader(io.StringIO(data.decode('ascii')), strict=True)
    require(next(reader, None) == header.split(','), 'Unexpected stage trace schema')
    names = header.split(',')
    result = {key: array('d') for key in selected}
    n = 0
    for n, fields in enumerate(reader, 1):
        require(n <= count and len(fields) == len(names) and fields[0] == str(n - 1), 'Invalid stage row/index')
        values = [decimal(v) for v in fields[1:]]
        require(values[0] == (n - 1) / rate, 'Stage endpoint clock mismatch')
        for key in result:
            result[key].append(values[names.index(key) - 1])
    require(n == count, 'Stage endpoint count mismatch')
    return result


def execute(command, root):
    """Drain stderr continuously, retaining only its last 16 KiB; no log files."""
    tail = bytearray()
    with subprocess.Popen(command, cwd=root, stdin=subprocess.DEVNULL,
                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE) as child:
        deadline = time.monotonic() + CHILD_TIMEOUT
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(child.stderr, selectors.EVENT_READ)
                while selector.get_map() or child.poll() is None:
                    remaining = deadline - time.monotonic()
                    require(remaining > 0, 'Stage timeout after ' + str(CHILD_TIMEOUT) + ' seconds')
                    for key, _ in selector.select(min(remaining, .1)):
                        data = os.read(key.fd, 4096)
                        if data:
                            tail.extend(data)
                            del tail[:-16384]
                        else:
                            selector.unregister(key.fileobj)
                code = child.wait(timeout=max(.001, deadline - time.monotonic()))
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
        return code, tail.decode('utf-8', errors='replace').strip()


def run_stage(root, owned, name, command, compare, wav, filter_controls=False):
    output = root / name
    names = ['manifest.json', 'stimulus.csv', 'trace.csv']
    if name == 'filter' and filter_controls:
        names += ['controls.csv', 'control_endpoints.csv']
    if name == 'filter' and compare:
        names.append('realtime.csv')
    if wav:
        names += ['reference.wav'] + (['realtime.wav'] if compare else [])
    # Full numerical diagnostics stay in component manifests; failure text is capped.
    try:
        code, error = execute(command + ['--out', name], root)
        require(code == 0, name + ' exited ' + str(code) + '; no bundle published' + (': ' + error if error else ''))
    finally:
        owned.adopt(output, names)
    require(set(p.name for p in output.iterdir()) == set(names), 'Unexpected stage artifacts')
    for file in output.iterdir():
        require(file.is_file() and not file.is_symlink(), 'Stage artifacts must be regular files')
    return read_json(output / 'manifest.json')


def check_input(meta, expected, count, rate):
    require(meta['sha256'] == digest(expected) and meta['bytes'] == len(expected)
            and meta['sample_count'] == count and meta['rate_hz'] == rate, 'Stage input provenance mismatch')


def vca_input(lp, controls):
    lines = ['sample,input,lc,ei,envelope\n']
    for i, (signal, lc, ei, env) in enumerate(zip(lp, *controls)):
        require(-8 <= signal <= 8, 'Filter LP exceeds VCA input [-8,8]; choose an explicit upstream fixture, never normalize')
        lines.append(str(i) + ',' + ','.join(format(v, '.17g') for v in (signal, lc, ei, env)) + '\n')
    return ''.join(lines).encode('ascii')


def verify_wav(path, values, rate, gain):
    import struct
    data = snapshot(path)
    n = len(values)
    expected_header = (b'RIFF' + struct.pack('<I', 50 + 4*n) + b'WAVEfmt ' +
        struct.pack('<IHHIIHHH', 18, 3, 1, rate, rate*4, 4, 32, 0) +
        b'fact' + struct.pack('<II', 4, n) + b'data' + struct.pack('<I', 4*n))
    require(data[:58] == expected_header and len(data) == 58 + 4*n, 'Unexpected WAV schema/count')
    for i, value in enumerate(values):
        require(data[58 + 4*i:62 + 4*i] == struct.pack('<f', value * gain), 'WAV samples do not match selected trace')


def render(a):
    require(os.name == 'posix', 'This snapshot-execution workflow currently requires a POSIX host')
    out = Path(a.out).absolute()
    require(out.name not in ('', '.', '..') and not os.path.lexists(out), 'Output must be a new path, including symlinks')
    require(out.parent.is_dir(), 'Output parent must already exist')
    signal = snapshot(a.input)
    control_bytes = snapshot(a.controls)
    source = endpoints(signal, 'sample,value', a.samples, [(-8, 8)], 128)[0]
    controls = endpoints(control_bytes, 'sample,lc,ei,envelope', a.samples, [(-8, 8), (-8, 8), (0, 1)])
    filter_bytes = snapshot(a.filter_controls) if a.filter_controls is not None else None
    filter_controls = (endpoints(filter_bytes, 'sample,hp_hz,lp_hz,q', a.samples,
                                [(25, 20000), (25, 20000), (.35, 9)], 128)
                       if filter_bytes is not None else None)
    initial = controls[0][0] if a.initial_li is None else a.initial_li
    require(not a.compare_realtime or (source[0] == 0 and controls[2][0] == 0 and initial == controls[0][0]),
            'Comparison requires first input/envelope zero and initial LI equal first LC')
    snapshots = {'inputs/signal.csv': signal, 'inputs/controls.csv': control_bytes,
                 'tools/filter': snapshot(a.filter), 'tools/vca': snapshot(a.vca),
                 'tools/workflow.py': snapshot(__file__)}
    if filter_bytes is not None:
        snapshots['inputs/filter_controls.csv'] = filter_bytes
    owned, published = Owned(), Owned()
    root = Path(tempfile.mkdtemp(prefix='.cs80-chain-', dir=out.parent))
    owned.dirs[root] = identity(root)
    try:
        for folder in ('inputs', 'tools'):
            owned.mkdir(root / folder)
        for name, data in snapshots.items():
            owned.write(root / name, data, executable=name in ('tools/filter', 'tools/vca'))
        commands = {}
        filter_options = (['--controls', 'inputs/filter_controls.csv'] if filter_bytes is not None else
                          ['--hp-hz', str(a.hp_hz), '--lp-hz', str(a.lp_hz), '--q', str(a.q)])
        commands['filter'] = ['./tools/filter', '--input', 'inputs/signal.csv', '--input-rate', str(a.rate),
            '--input-samples', str(a.samples)] + filter_options + [
            '--ota-level', str(a.ota_level), '--gain', a.gain, '--substeps', str(a.filter_substeps),
            '--tolerance', str(a.filter_tolerance)]
        comparison = ['--compare-realtime', 'true'] if a.compare_realtime else []
        audio = ['--wav-gain', str(a.wav_gain)] if a.wav_gain is not None else []
        commands['filter'] += comparison + (['--realtime-rate', str(a.rate)] if a.compare_realtime else []) + audio
        fm = run_stage(root, owned, 'filter', commands['filter'], a.compare_realtime, bool(audio), filter_bytes is not None)
        require(fm['schema'] == 1 and fm['model'] == 'IG00156 conditional HP-to-LP filter reference'
                and fm['rate_hz'] == a.rate and fm['intervals'] == a.samples - 1 and fm['newton_certified'] is True,
                'Filter schema/clock/solver mismatch')
        check_input(fm['input'], signal, a.samples, a.rate)
        require(snapshot(root / 'filter/stimulus.csv') == signal, 'Filter input snapshot mismatch')
        if filter_bytes is not None:
            check_input(fm['controls'], filter_bytes, a.samples, a.rate)
            require(fm['controls']['file'] == 'controls.csv'
                    and fm['controls']['endpoint_file'] == 'control_endpoints.csv', 'Filter control artifact names mismatch')
            require(snapshot(root / 'filter/controls.csv') == filter_bytes, 'Filter control snapshot mismatch')
            header = FILTER_CONTROL_HEADER + (REALTIME_CONTROL_COLUMNS if a.compare_realtime else '')
            requested = trace(root / 'filter/control_endpoints.csv', header, a.samples, a.rate, ['hp_hz', 'lp_hz', 'q'])
            require(all(requested[key] == series for key, series in zip(('hp_hz', 'lp_hz', 'q'), filter_controls)),
                    'Filter requested-control trace mismatch')
        lp = trace(root / 'filter/trace.csv', FILTER_HEADER, a.samples, a.rate, ['input', 'lp'])
        require(lp['input'] == source, 'Filter input trace mismatch')
        productions = None
        if a.compare_realtime:
            require(fm['realtime_comparison']['internal_rate_hz'] == a.rate
                    and fm['realtime_comparison']['sample_count'] == a.samples, 'Production filter clock mismatch')
            productions = trace(root / 'filter/realtime.csv', REALTIME_HEADER, a.samples, a.rate, ['lp_realtime'])['lp_realtime']
        outputs = {}
        stage_manifests = {'filter': fm}
        for name, signal_values in [('vca_reference_input', lp['lp'])] + (
                [('vca_production_input', productions)] if a.compare_realtime else []):
            data = vca_input(signal_values, controls)
            input_name = 'inputs/' + name + '.csv'
            owned.write(root / input_name, data)
            command = ['./tools/vca', '--input', input_name, '--rate', str(a.rate), '--samples', str(a.samples),
                       '--steps', str(a.vca_steps), '--tolerance', str(a.vca_tolerance), '--initial-li', str(initial)] + comparison + audio
            commands[name] = command
            vm = run_stage(root, owned, name, command, a.compare_realtime, bool(audio))
            require(vm['schema'] == 'cs80-ig00151-reference-v1' and vm['rate'] == a.rate and vm['samples'] == a.samples
                    and vm['intervals'] == a.samples - 1 and vm['compare_realtime'] == a.compare_realtime
                    and vm['initial_li'] == initial, 'VCA schema/clock/initialization mismatch')
            require(vm['stimulus_sha256'] == digest(data) and vm['stimulus_bytes'] == len(data)
                    and snapshot(root / name / 'stimulus.csv') == data, 'VCA input provenance mismatch')
            header = VCA_HEADER + (',realtime_li,realtime_output' if a.compare_realtime else '')
            values = trace(root / name / 'trace.csv', header, a.samples, a.rate,
                           ['input', 'lc', 'ei', 'envelope', 'output'] + (['realtime_output'] if a.compare_realtime else []))
            require(values['input'] == signal_values and all(values[key] == series for key, series in zip(('lc', 'ei', 'envelope'), controls)),
                    'VCA drive trace mismatch')
            outputs[name] = values
            stage_manifests[name] = vm
        reference = outputs['vca_reference_input']['output']
        production = outputs['vca_production_input']['realtime_output'] if a.compare_realtime else None
        lines = ['sample,time_s,reference_output' + (',production_output,production_minus_reference' if a.compare_realtime else '') + '\n']
        for i, value in enumerate(reference):
            row = [i / a.rate, value] + ([production[i], production[i] - value] if a.compare_realtime else [])
            lines.append(str(i) + ',' + ','.join(format(v, '.17g') for v in row) + '\n')
        owned.write(root / 'chain.csv', ''.join(lines).encode('ascii'))
        selected = {'reference': {'trace': 'vca_reference_input/trace.csv', 'column': 'output'}}
        if a.compare_realtime:
            selected['production'] = {'trace': 'vca_production_input/trace.csv', 'column': 'realtime_output'}
        if audio:
            for key, name, values in [('reference', 'vca_reference_input/reference.wav', reference)] + (
                    [('production', 'vca_production_input/realtime.wav', production)] if a.compare_realtime else []):
                verify_wav(root / name, values, a.rate, a.wav_gain)
                owned.write(root / (key + '.wav'), snapshot(root / name))
                selected[key]['wav'] = key + '.wav'
        # Verify the exact executed snapshots are still the hashed bytes.
        for name, data in snapshots.items():
            require(snapshot(root / name) == data, 'Input/tool snapshot changed during render')
        difference = None
        if a.compare_realtime:
            deltas = [p-r for p, r in zip(production, reference)]
            energy = math.fsum(v*v for v in reference)
            error = math.fsum(v*v for v in deltas)
            difference = {'max_abs': max(abs(v) for v in deltas), 'rms_abs': math.sqrt(error / a.samples),
                          'relative_rms': math.sqrt(error / energy) if energy else None}
        artifacts = {str(path.relative_to(root)): metadata(snapshot(path)) for path in sorted(owned.files)}
        manifest = {
            'schema': 'cs80-filter-vca-chain-v1', 'complete': True,
            'scope': ('Endpoint-controlled' if filter_bytes is not None else 'Fixed-control') + ' filter to externally controlled VCA diagnostic; not engine, hardware or whole-note truth',
            'clock': 'One declared endpoint/internal stage rate; no oversampling, decimator, lifecycle, resampling or alignment',
            'interstage': 'Fine reference filter LP endpoints reconstructed piecewise linearly for the VCA; not jointly integrated; no interstage error bound',
            'units': 'Direct numeric LP to VCA input in uncalibrated engine units; no conversion, normalization or implicit gain',
            'rate_hz': a.rate, 'samples': a.samples, 'intervals': a.samples - 1,
            'endpoint_span_s': (a.samples - 1) / a.rate, 'playback_duration_s': a.samples / a.rate,
            'compare_realtime': a.compare_realtime, 'selected_outputs': selected,
            'comparison': difference, 'comparison_definition': 'All endpoints; production-filter to production-VCA minus reference-filter to reference-VCA; descriptive only, no compounded error or parity gate',
            'component_diagnostics': {name: name + '/manifest.json' for name in stage_manifests},
            'hybrid_diagnostics': 'VCA on reference LP has optional production columns; VCA on production LP has a reference trace. Neither hybrid is selected as the full production chain.',
            'limits': {'max_samples': MAX_SAMPLES, 'max_input_or_artifact_bytes': MAX_BYTES, 'max_aggregate_integration_steps': MAX_STEPS,
                       'requested_integration_steps': a.integration_steps, 'child_timeout_seconds': CHILD_TIMEOUT},
            'commands': {name: command + ['--out', name] for name, command in commands.items()},
            'wav_gain': a.wav_gain, 'artifacts': artifacts,
            'provenance': 'Hashes describe exact snapshots, executed binary bytes and artifacts, not source-path assumptions. Component source hashes are retained, not reinterpreted. Dynamic libraries and runtime are not bundled.'}
        if filter_bytes is not None:
            manifest['filter_controls'] = {
                'input': 'inputs/filter_controls.csv', 'snapshot': 'filter/controls.csv',
                'endpoints': 'filter/control_endpoints.csv', **metadata(filter_bytes),
                'sample_count': a.samples, 'rate_hz': a.rate,
                'interpolation': fm['controls']['interpolation'],
                'scalar_metadata_scope': fm['controls']['scalar_metadata_scope'],
                'scope': fm['controls']['scope']}
        owned.write(root / 'manifest.json', (json.dumps(manifest, sort_keys=True, indent=2, allow_nan=False) + '\n').encode('utf-8'))
        # Exclusive final path creation after every child/check/export succeeds.
        # Hard links never overwrite; manifest is installed last as a completion marker.
        published.mkdir(out)
        for directory in sorted(owned.dirs):
            if directory != root:
                published.mkdir(out / directory.relative_to(root))
        for path in list(sorted(p for p in owned.files if p.name != 'manifest.json')) + sorted(p for p in owned.files if p.name == 'manifest.json' and p.parent != root) + [root / 'manifest.json']:
            dest = out / path.relative_to(root)
            os.link(path, dest)
            published.files[dest] = identity(dest)
    except BaseException:
        published.cleanup()
        raise
    finally:
        owned.cleanup()


def main(argv=None):
    try:
        a = parse(sys.argv[1:] if argv is None else argv)
        render(a)
        print('Wrote ' + ('endpoint-controlled filter' if a.filter_controls is not None else 'fixed-filter')
              + ' to VCA endpoint diagnostic. Not a jointly integrated whole-note reference.')
        if a.wav_gain is not None:
            print('Start playback at low volume; above-full-scale samples are retained and players may clip.')
        return 0
    except (OSError, ValueError, KeyError, TypeError, UnicodeError, csv.Error, subprocess.SubprocessError) as error:
        print('render_reference_chain: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
