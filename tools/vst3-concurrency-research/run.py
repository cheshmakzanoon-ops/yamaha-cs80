#!/usr/bin/env python3
"""Frozen 12-case diagnostic only; never changes production CTest scheduling."""
import argparse, concurrent.futures, hashlib, json, os, pathlib, subprocess, time
BASELINE = 'f9dc1586bf8bfbecd51d776f97c6ed0100750d20'
TIMEOUT = 180

def cases():
    result = []
    for family in ('generic', 'pwm'):
        for rate in (44100, 48000, 96000):
            for mode in ('realtime', 'offline'):
                name = (f'cs80_vst3_automation_{rate}_{mode}_tests' if family == 'generic'
                        else f'cs80_vst3_pwm_automation_{rate}_{mode}_tests')
                if family == 'generic' and rate == 48000 and mode == 'realtime':
                    name = 'cs80_vst3_automation_tests'
                result.append(dict(name=name, family=family, rate=rate, mode=mode))
    return result

def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()

def validate_inventory(inventory, repo, build):
    tests = inventory['tests']
    names = [t['name'] for t in tests]
    if len(names) != len(set(names)):
        raise ValueError('duplicate CTest names')
    contracts = repo / 'tests/parity/tolerances-v2'
    module = build / 'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
    selected = []
    for case in cases():
        matches = [t for t in tests if t['name'] == case['name']]
        if len(matches) != 1:
            raise ValueError('missing or duplicate frozen case: ' + case['name'])
        test = matches[0]
        props = {p['name']: p['value'] for p in test.get('properties', [])}
        if set(props) - {'TIMEOUT', 'WORKING_DIRECTORY'}:
            raise ValueError('unhandled CTest properties: ' + case['name'])
        if props.get('TIMEOUT') != TIMEOUT or pathlib.Path(props.get('WORKING_DIRECTORY', str(build))).resolve() != build:
            raise ValueError('timeout/working directory drift')
        executable = build / ('cs80_vst3_automation_tests' if case['family'] == 'generic' else 'cs80_vst3_pwm_automation_tests')
        expected = [str(executable), str(module), str(contracts / 'host-automation.json')]
        capture = None
        if case['family'] == 'pwm':
            capture = build / f'pwm-automation-capture-{case["rate"]}-{case["mode"]}'
            expected += [str(contracts / 'host-automation-pwm-v1.json'), str(capture), str(case['rate']), case['mode']]
        elif case['name'] != 'cs80_vst3_automation_tests':
            expected += [str(case['rate']), case['mode']]
        if test.get('command') != expected:
            raise ValueError('frozen CTest command drift: ' + case['name'])
        selected.append({**case, 'command': expected, 'capture': str(capture) if capture else None})
    return selected

def execute(case, phase, out, build):
    folder = out / phase / case['name']
    folder.mkdir(parents=True, exist_ok=False)
    home = out / 'homes' / case['name']
    home.mkdir(parents=True, exist_ok=True)
    capture = pathlib.Path(case['capture']) if case['capture'] else None
    if capture is not None and capture.exists():
        raise ValueError('refuse existing capture directory')
    if any(home.rglob('*')):
        raise ValueError('home is not pristine')
    env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / 'config'), XDG_CACHE_HOME=str(home / 'cache'))
    start = time.monotonic()
    with (folder / 'stdout.log').open('wb') as log:
        try:
            code = subprocess.run(case['command'], cwd=build, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=TIMEOUT).returncode
        except subprocess.TimeoutExpired:
            code = 124
    result = {**case, 'phase': phase, 'exit': code, 'seconds': time.monotonic() - start,
              'timeout': TIMEOUT, 'log_sha256': digest(folder / 'stdout.log'), 'captures': {},
              'home_entries': [str(f.relative_to(home)) for f in home.rglob('*')]}
    if capture is not None and capture.exists():
        capture.rename(folder / 'capture')  # Preserve raw output; no deletion/overwrite.
        result['captures'] = {str(f.relative_to(folder / 'capture')): {'size': f.stat().st_size, 'sha256': digest(f)}
                              for f in sorted((folder / 'capture').rglob('*')) if f.is_file()}
    log_text = (folder / 'stdout.log').read_text(errors='replace')
    result['pwm_full_gate'] = case['family'] != 'pwm' or ('gate=full-pwm' in log_text and '24 native pwm scenarios, 0 failures' in log_text)
    (folder / 'result.json').write_text(json.dumps(result, indent=2))
    return result

def expected_pcm_names():
    return {f"pwm_lane{lane}-{voices}-{source}-{suffix}.pcm"
            for lane in (50, 51, 52, 53) for voices in (8, 16)
            for source in (13, 0, 11) for suffix in ("noqueue", "policy0", "policy2")}

def valid(result):
    captures = result['captures']
    return (result['exit'] == 0 and not result['home_entries'] and result['pwm_full_gate']
            and (result['family'] != 'pwm' or (set(captures) == expected_pcm_names() and all(v['size'] == 32768 for v in captures.values()))))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=pathlib.Path, required=True)
    parser.add_argument('--build', type=pathlib.Path, required=True)
    parser.add_argument('--out', type=pathlib.Path, required=True)
    args = parser.parse_args()
    repo, build, out = args.repo.resolve(), args.build.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    result = {'baseline': BASELINE, 'concurrency': 2, 'serial': [], 'parallel': [], 'accepted': False}
    try:
        subprocess.run(['git', '-C', str(repo), 'diff', '--exit-code', BASELINE, 'HEAD', '--', 'src', 'tests', 'CMakeLists.txt', 'cmake'], check=True)
        if subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain', '--', 'src', 'tests', 'CMakeLists.txt', 'cmake'], text=True).strip():
            raise ValueError('dirty source inputs')
        for external in ('/usr/local/lib/libMTS.so', '/Library/Application Support/MTS-ESP/libMTS.dylib'):
            if pathlib.Path(external).exists():
                raise ValueError('external MTS library present; frozen environment assumption fails')
        result['commit'] = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
        inventory = json.loads(subprocess.check_output(['ctest', '--test-dir', str(build), '--show-only=json-v1'], text=True))
        (out / 'ctest-inventory.json').write_text(json.dumps(inventory, indent=2))
        selected = validate_inventory(inventory, repo, build)
        binaries = {c['command'][0] for c in selected} | {c['command'][1] for c in selected}
        result['binaries'] = {p: digest(p) for p in sorted(binaries)}
        result['cases'] = selected
        start = time.monotonic()
        for case in selected:
            record = execute(case, 'serial', out, build)
            result['serial'].append(record)
            if not valid(record):
                raise ValueError('serial preflight failed: ' + case['name'])
        result['serial_wall'] = time.monotonic() - start
        start = time.monotonic()
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            result['parallel'] = list(pool.map(lambda c: execute(c, 'parallel', out, build), selected))
        result['parallel_wall'] = time.monotonic() - start
        result['binaries_unchanged'] = all(digest(p) == h for p, h in result['binaries'].items())
        result['accepted'] = (result['binaries_unchanged'] and len(result['serial']) == len(result['parallel']) == 12
                              and all(valid(r) for r in result['parallel'])
                              and all(a['log_sha256'] == b['log_sha256'] and a['captures'] == b['captures']
                                      for a, b in zip(result['serial'], result['parallel'])))
        if not result['accepted']:
            raise ValueError('parallel result/equality gate failed')
        print('PASS: 24 cases, exact logs/captures. Single-order diagnostic; no production scheduling change.')
    except Exception as error:
        result['error'] = str(error)
        print('FAIL:', error)
    finally:
        (out / 'results.json').write_text(json.dumps(result, indent=2))
    return 0 if result['accepted'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
