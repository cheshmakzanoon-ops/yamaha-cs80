#!/usr/bin/env python3
"""Reference-only endpoint-clock sensitivity study; Python 3.10+, stdlib only."""
import csv
import json
import math
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import tempfile
import time
import types


def load_workflow(data, filename):
    """Execute the retained source bytes, without importing or writing pycache."""
    module = types.ModuleType('chain_workflow_snapshot')
    module.__file__ = str(filename)
    exec(compile(data, str(filename), 'exec'), module.__dict__)
    return module


WORKFLOW_PATH = Path(__file__).resolve().with_name('render_reference_chain.py')
# This is repository code, not a user-supplied module. Retain exactly the bytes
# loaded here; the copies used for all three runs are loaded from these bytes.
with WORKFLOW_PATH.open('rb') as stream:
    WORKFLOW_BYTES = stream.read(1024 * 1024 + 1)
if len(WORKFLOW_BYTES) > 1024 * 1024:
    raise ValueError('Chain workflow exceeds source byte budget')
CHAIN = load_workflow(WORKFLOW_BYTES, WORKFLOW_PATH)
require = CHAIN.require
MAX_STEPS = 10000000
MAX_SNAPSHOT_BYTES = 64 * 1024 * 1024
MAX_BUNDLE_BYTES = 512 * 1024 * 1024
TOTAL_TIMEOUT = 240
RUNS = ('baseline', 'matched_step', 'finer_step')
DIFFERENCES = (('matched_step_minus_baseline', 'matched_step', 'baseline'),
               ('finer_step_minus_baseline', 'finer_step', 'baseline'),
               ('finer_step_minus_matched_step', 'finer_step', 'matched_step'))


def parse(argv):
    options = {arg.split('=', 1)[0] for arg in argv if arg.startswith('--')}
    require(not options.intersection(('--compare-realtime', '--wav-gain')),
            'Clock study is reference-only; production and WAV options are not supported')
    a = CHAIN.parse(argv)
    require(a.rate <= 192000, 'Doubled rate must not exceed 384000 Hz')
    require(2 * a.samples - 1 <= CHAIN.MAX_SAMPLES, 'Refined endpoint count exceeds 100001')
    require(a.filter_substeps % 2 == 0 and a.vca_steps % 2 == 0,
            'Both filter-substeps and vca-steps must be even (at least two)')
    a.study_integration_steps = 12 * (a.samples - 1) * (a.filter_substeps + a.vca_steps)
    require(a.study_integration_steps <= MAX_STEPS,
            'Combined three-run integration budget exceeds ten million')
    return a


def refine(columns, header):
    """Insert requested-value midpoints, not mapped conductance/gain midpoints.

    Original double endpoints survive round trips through .17g exactly. PWL
    trajectories are preserved up to finite-precision interpolation/roundoff.
    """
    count = len(columns[0])
    require(count >= 2 and all(len(c) == count for c in columns), 'Invalid endpoint columns')
    lines = [header + '\n']
    for i in range(2 * count - 1):
        left = i // 2
        values = [c[left] if i % 2 == 0 else c[left] + (c[left + 1] - c[left]) * .5
                  for c in columns]
        require(all(math.isfinite(v) for v in values), 'Nonfinite refined endpoint')
        lines.append(str(i) + ',' + ','.join(format(v, '.17g') for v in values) + '\n')
    return ''.join(lines).encode('ascii')


def metrics(candidate, reference):
    require(len(candidate) == len(reference) and len(reference) > 0, 'Comparison endpoint count mismatch')
    delta = [v - r for v, r in zip(candidate, reference)]
    error = math.fsum(v * v for v in delta)
    energy = math.fsum(v * v for v in reference)
    require(math.isfinite(error) and math.isfinite(energy), 'Nonfinite comparison energy')
    return {'max_abs': max(abs(v) for v in delta), 'rms_abs': math.sqrt(error / len(reference)),
            'relative_rms': math.sqrt(error / energy) if energy else None,
            'sample_count': len(reference)}


def execute(command, root, deadline):
    """Bound the whole study and each stage; drain and cap diagnostic stderr."""
    deadline = min(deadline, time.monotonic() + CHAIN.CHILD_TIMEOUT)
    require(deadline > time.monotonic(), 'Study timeout before stage launch')
    tail = bytearray()
    with subprocess.Popen(command, cwd=root, stdin=subprocess.DEVNULL,
                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                          start_new_session=True) as child:
        complete = False
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(child.stderr, selectors.EVENT_READ)
                while selector.get_map() or child.poll() is None:
                    remaining = deadline - time.monotonic()
                    require(remaining > 0, 'Study or component timeout')
                    for key, _ in selector.select(min(remaining, .1)):
                        data = os.read(key.fd, 4096)
                        if data:
                            tail.extend(data)
                            del tail[:-16384]
                        else:
                            selector.unregister(key.fileobj)
                code = child.wait(timeout=max(.001, deadline - time.monotonic()))
            complete = True
        finally:
            if not complete:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.wait()
    return code, tail.decode('utf-8', errors='replace').strip()


def adopt_run(owned, directory, dynamic):
    # A successful chain render owns exactly this public contract. Never walk
    # arbitrary artifacts or adopt an output from a failed render.
    owned.adopt(directory, ['chain.csv', 'manifest.json'])
    owned.adopt(directory / 'inputs', ['signal.csv', 'controls.csv', 'vca_reference_input.csv']
                + (['filter_controls.csv'] if dynamic else []))
    owned.adopt(directory / 'tools', ['filter', 'vca', 'workflow.py'])
    owned.adopt(directory / 'filter', ['manifest.json', 'stimulus.csv', 'trace.csv']
                + (['controls.csv', 'control_endpoints.csv'] if dynamic else []))
    owned.adopt(directory / 'vca_reference_input', ['manifest.json', 'stimulus.csv', 'trace.csv'])


def verify_run(directory, expected, workflow_bytes, tools):
    manifest = CHAIN.read_json(directory / 'manifest.json')
    require(manifest['schema'] == 'cs80-filter-vca-chain-v1' and manifest['complete'] is True
            and manifest['compare_realtime'] is False and manifest['wav_gain'] is None
            and manifest['rate_hz'] == expected.rate and manifest['samples'] == expected.samples,
            'Chain manifest schema/clock/scope mismatch')
    require(manifest['selected_outputs'] == {'reference': {'trace': 'vca_reference_input/trace.csv', 'column': 'output'}},
            'Unexpected selected chain output')
    for path, data in [('tools/workflow.py', workflow_bytes), ('tools/filter', tools['filter']), ('tools/vca', tools['vca'])]:
        require(CHAIN.snapshot(directory / path) == data, 'Executed chain snapshot mismatch')
    # Restrict hashes to the known contract, without trusting manifest paths.
    expected_paths = {'inputs/signal.csv', 'inputs/controls.csv', 'inputs/vca_reference_input.csv',
                      'tools/filter', 'tools/vca', 'tools/workflow.py', 'chain.csv',
                      'filter/manifest.json', 'filter/stimulus.csv', 'filter/trace.csv',
                      'vca_reference_input/manifest.json', 'vca_reference_input/stimulus.csv',
                      'vca_reference_input/trace.csv'}
    if expected.filter_controls is not None:
        expected_paths.update(('inputs/filter_controls.csv', 'filter/controls.csv', 'filter/control_endpoints.csv'))
    require(set(manifest['artifacts']) == expected_paths, 'Unexpected chain artifact inventory')
    for path in expected_paths:
        require(CHAIN.metadata(CHAIN.snapshot(directory / path)) == manifest['artifacts'][path],
                'Chain artifact hash mismatch: ' + path)
    return manifest


def render(a):
    require(os.name == 'posix', 'Clock study requires a POSIX host')
    deadline = time.monotonic() + TOTAL_TIMEOUT
    out = Path(a.out).absolute()
    require(out.name not in ('', '.', '..') and not os.path.lexists(out), 'Output must be a new path, including symlinks')
    require(out.parent.is_dir(), 'Output parent must already exist')
    snapshot_bytes = 0
    def capture(path, limit=CHAIN.MAX_BYTES):
        nonlocal snapshot_bytes
        data = CHAIN.snapshot(path, min(limit, MAX_SNAPSHOT_BYTES - snapshot_bytes))
        snapshot_bytes += len(data)
        return data

    inputs = {'signal': capture(a.input), 'controls': capture(a.controls)}
    specifications = {'signal': ('sample,value', [(-8, 8)], 128),
                      'controls': ('sample,lc,ei,envelope', [(-8, 8), (-8, 8), (0, 1)], 256)}
    if a.filter_controls is not None:
        inputs['filter_controls'] = capture(a.filter_controls)
        specifications['filter_controls'] = ('sample,hp_hz,lp_hz,q', [(25, 20000), (25, 20000), (.35, 9)], 128)
    refined = {}
    for name, data in inputs.items():
        header, bounds, limit = specifications[name]
        columns = CHAIN.endpoints(data, header, a.samples, bounds, limit)
        refined[name] = refine(columns, header)
        snapshot_bytes += len(refined[name])
        require(snapshot_bytes <= MAX_SNAPSHOT_BYTES, 'Aggregate snapshot byte budget exceeded')
        # Preflight the actual serialized data using the component-input bounds.
        check = CHAIN.endpoints(refined[name], header, 2 * a.samples - 1, bounds, limit)
        require(all(c == r[::2] for c, r in zip(columns, check)), 'Original endpoint preservation failed')
    snapshot_bytes += len(WORKFLOW_BYTES)
    require(snapshot_bytes <= MAX_SNAPSHOT_BYTES, 'Aggregate snapshot byte budget exceeded')
    tools = {'filter': capture(a.filter), 'vca': capture(a.vca),
             'render_reference_chain.py': WORKFLOW_BYTES,
             'study_reference_chain_clock.py': capture(__file__, 1024 * 1024)}
    snapshots = {'inputs/' + name + '.csv': data for name, data in inputs.items()}
    snapshots.update({'inputs/refined_' + name + '.csv': data for name, data in refined.items()})
    snapshots.update({'tools/' + name: data for name, data in tools.items()})
    require(snapshot_bytes == sum(len(data) for data in snapshots.values()), 'Snapshot byte accounting mismatch')
    require(snapshot_bytes <= MAX_SNAPSHOT_BYTES, 'Aggregate snapshot byte budget exceeded')
    require(time.monotonic() < deadline, 'Study timeout during preflight')
    owned, published = CHAIN.Owned(), CHAIN.Owned()
    root = Path(tempfile.mkdtemp(prefix='.cs80-clock-', dir=out.parent))
    owned.dirs[root] = CHAIN.identity(root)
    try:
        for directory in ('inputs', 'tools', 'runs'):
            owned.mkdir(root / directory)
        for name, data in snapshots.items():
            owned.write(root / name, data, executable=name in ('tools/filter', 'tools/vca'))
        workflow = load_workflow(WORKFLOW_BYTES, root / 'tools/render_reference_chain.py')
        workflow.execute = lambda command, directory: execute(command, directory, deadline)
        runs, common, run_specs, run_manifests = {}, {}, {}, {}
        for name in RUNS:
            require(time.monotonic() < deadline, 'Study timeout before chain run')
            refined_clock = name != 'baseline'
            options = vars(a).copy()
            options.update(rate=a.rate * (2 if refined_clock else 1),
                           samples=2 * a.samples - 1 if refined_clock else a.samples,
                           filter_substeps=a.filter_substeps // (2 if name == 'matched_step' else 1),
                           vca_steps=a.vca_steps // (2 if name == 'matched_step' else 1),
                           out=str(root / 'runs' / name), filter=str(root / 'tools/filter'), vca=str(root / 'tools/vca'))
            prefix = 'inputs/' + ('refined_' if refined_clock else '')
            options.update(input=str(root / (prefix + 'signal.csv')), controls=str(root / (prefix + 'controls.csv')))
            if a.filter_controls is not None:
                options['filter_controls'] = str(root / (prefix + 'filter_controls.csv'))
            options['integration_steps'] = 3 * (options['samples'] - 1) * (options['filter_substeps'] + options['vca_steps'])
            parameters = types.SimpleNamespace(**options)
            workflow.render(parameters)
            directory = root / 'runs' / name
            adopt_run(owned, directory, a.filter_controls is not None)
            component = verify_run(directory, parameters, WORKFLOW_BYTES, tools)
            run_specs[name] = parameters
            run_manifests[name] = CHAIN.metadata(CHAIN.snapshot(directory / 'manifest.json'))
            values = CHAIN.trace(directory / 'chain.csv', 'sample,time_s,reference_output',
                                 parameters.samples, parameters.rate, ['reference_output'])['reference_output']
            common[name] = values[::2] if refined_clock else values
            require(len(common[name]) == a.samples, 'Common-clock endpoint count mismatch')
            runs[name] = {'manifest': 'runs/' + name + '/manifest.json', 'rate_hz': parameters.rate,
                          'samples': parameters.samples, 'filter_substeps': parameters.filter_substeps,
                          'vca_steps': parameters.vca_steps, 'integration_steps': parameters.integration_steps,
                          'selected_filter_substeps': 2 * parameters.filter_substeps,
                          'selected_vca_steps': 2 * parameters.vca_steps,
                          'component_diagnostics': {key: 'runs/' + name + '/' + path
                                                    for key, path in component['component_diagnostics'].items()}}
        comparisons = {name: metrics(common[candidate], common[reference])
                       for name, candidate, reference in DIFFERENCES}
        header = ('sample,time_s,baseline_index,matched_step_index,finer_step_index,'
                  'baseline_output,matched_step_output,finer_step_output,'
                  + ','.join(name for name, _, _ in DIFFERENCES) + '\n')
        lines = [header]
        for i in range(a.samples):
            values = [common[name][i] for name in RUNS]
            differences = [common[candidate][i] - common[reference][i] for _, candidate, reference in DIFFERENCES]
            lines.append(','.join([str(i), format(i / a.rate, '.17g'), str(i), str(2*i), str(2*i)]
                                 + [format(v, '.17g') for v in values + differences]) + '\n')
        owned.write(root / 'common_clock.csv', ''.join(lines).encode('ascii'))
        for path, data in snapshots.items():
            require(CHAIN.snapshot(root / path) == data, 'Study snapshot changed during render')
        # Later stages must not change an earlier bundle after its common-clock
        # values were selected. Recheck the retained manifest and its inventory.
        for name in RUNS:
            directory = root / 'runs' / name
            require(CHAIN.metadata(CHAIN.snapshot(directory / 'manifest.json')) == run_manifests[name],
                    'Completed chain manifest changed during study')
            verify_run(directory, run_specs[name], WORKFLOW_BYTES, tools)
        artifacts, total_bytes = {}, 0
        for path in sorted(owned.files):
            data = CHAIN.snapshot(path)
            total_bytes += len(data)
            require(total_bytes <= MAX_BUNDLE_BYTES, 'Aggregate artifact byte budget exceeded')
            artifacts[str(path.relative_to(root))] = CHAIN.metadata(data)
        require(time.monotonic() < deadline, 'Study timeout before publication')
        manifest = {
            'schema': 'cs80-chain-clock-study-v1', 'complete': True,
            'scope': 'Reference-only endpoint-clock sensitivity diagnostic; not a runtime DSP, hardware, CPU, engine or whole-instrument qualification',
            'interpretation': 'Differences describe coupled endpoint reconstruction and numerical sensitivity. They are not interstage-only errors, rigorous bounds, Richardson estimates, convergence orders or production-equivalence claims. No generic pass gate.',
            'pwl_preservation': 'Midpoints inserted in source and requested Hz/Q, LC/EI/envelope values before nonlinear mappings. Original double endpoints are preserved exactly; between-endpoint functions are preserved up to floating-point interpolation and roundoff.',
            'matched_step': 'Doubled endpoint rate and halved even substeps preserve nominal component integration time steps. Segmentation/interpolation, rounding and LI-floor crossings can still affect results.',
            'initialization': 'Each run uses the existing zero filter/coupling states and the same explicit initial LI, or the same first LC if omitted; no invented prehistory.',
            'units': 'Unchanged uncalibrated numeric LP-to-VCA units, OTA level and gain; no conversion, normalization, alignment, production path or WAV export.',
            'clock': {'rate_hz': a.rate, 'samples': a.samples, 'intervals': a.samples - 1,
                      'endpoint_span_s': (a.samples - 1) / a.rate,
                      'comparison_indices': {'baseline': 'i', 'matched_step': '2*i', 'finer_step': '2*i'},
                      'comparison_range': 'i = 0 .. samples-1, including initial and final endpoints',
                      'trace': 'common_clock.csv'},
            'runs': runs, 'comparisons': comparisons,
            'comparison_definition': {name: {'candidate': candidate, 'reference': reference,
                                            'difference': 'candidate minus reference',
                                            'relative_rms': 'sqrt(sum(difference^2)/sum(reference^2)); null for zero reference energy'}
                                      for name, candidate, reference in DIFFERENCES},
            'limits': {'max_original_rate_hz': 192000, 'max_refined_rate_hz': 384000,
                       'max_refined_samples': CHAIN.MAX_SAMPLES, 'max_aggregate_integration_steps': MAX_STEPS,
                       'requested_integration_steps': a.study_integration_steps,
                       'max_snapshot_bytes': MAX_SNAPSHOT_BYTES, 'max_bundle_bytes': MAX_BUNDLE_BYTES,
                       'snapshot_bytes': snapshot_bytes, 'artifact_bytes_excluding_study_manifest': total_bytes,
                       'max_input_or_artifact_bytes': CHAIN.MAX_BYTES,
                       'child_timeout_seconds': CHAIN.CHILD_TIMEOUT, 'study_timeout_seconds': TOTAL_TIMEOUT},
            'artifacts': artifacts,
            'provenance': 'Each chain run executes copies of the retained binary bytes and the retained chain workflow source loaded in-process. Each run retains its own executable snapshots, commands and component manifests. Study source is retained as file bytes. Interpreter, shared libraries, OS and runtime are not bundled.'}
        manifest_bytes = (json.dumps(manifest, sort_keys=True, indent=2, allow_nan=False) + '\n').encode('utf-8')
        require(total_bytes + len(manifest_bytes) <= MAX_BUNDLE_BYTES, 'Aggregate artifact byte budget exceeded')
        owned.write(root / 'manifest.json', manifest_bytes)
        # New destination only; no overwrite/rename window and completion marker last.
        published.mkdir(out)
        for path in sorted(owned.dirs):
            if path != root:
                published.mkdir(out / path.relative_to(root))
        ordered = sorted(path for path in owned.files if path != root / 'manifest.json') + [root / 'manifest.json']
        for path in ordered:
            dest = out / path.relative_to(root)
            os.link(path, dest)
            published.files[dest] = CHAIN.identity(dest)
    except BaseException:
        published.cleanup()
        raise
    finally:
        owned.cleanup()


def main(argv=None):
    try:
        render(parse(sys.argv[1:] if argv is None else argv))
        print('Wrote reference-only endpoint-clock sensitivity study. Descriptive differences, not an interstage error bound.')
        return 0
    except (OSError, ValueError, KeyError, TypeError, UnicodeError, csv.Error, subprocess.SubprocessError) as error:
        print('study_reference_chain_clock: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
