#!/usr/bin/env python3
"""Standalone correctness oracle; never applies patches or runs CPU timing."""
from pathlib import Path
import argparse, hashlib, json, re, shutil, subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--cpu', type=int)
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--unsafe', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parent / 'focused'
    for name in ['Probe.cpp', 'StereoFir.h', 'Dsp.h', 'BaselineDsp.h']:
        shutil.copy2(source / name, out / name)
    commands, results = [], []
    prefix = ['taskset', '-c', str(args.cpu)] if args.cpu is not None else []
    def execute(command, stem):
        commands.append(prefix + command)
        (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        with (out / (stem + '.log')).open('w') as log:
            process = subprocess.run(prefix + command, stdout=log, stderr=subprocess.STDOUT,
                                     timeout=300, check=False)
        (out / (stem + '.exit')).write_text(str(process.returncode) + '\n')
        if process.returncode:
            raise RuntimeError('Failed ' + stem + '; original logs preserved')
    execute([args.cxx, '--version'], 'compiler')
    profiles = [('generic', []), ('off', ['-DCS80_DISABLE_AVX2_FILTER_BATCH=1'])]
    if args.native:
        profiles.append(('native', ['-march=native']))
    if args.unsafe:
        profiles.append(('unsafe', ['-ffast-math']))
    for profile, flags in profiles:
        for arm in ['baseline', 'candidate']:
            name = profile + '-' + arm
            command = [args.cxx, '-O3', '-DNDEBUG', '-std=c++20', *flags]
            if arm == 'candidate':
                command.append('-DCANDIDATE=1')
            command += [str(out / 'Probe.cpp'), '-o', str(out / name)]
            execute(command, name + '-build')
            execute([str(out / name), str(out / (name + '.bin'))], name)
        a = (out / (profile + '-baseline.bin')).read_bytes()
        b = (out / (profile + '-candidate.bin')).read_bytes()
        if a != b:
            mismatch = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            (out / 'FIRST-MISMATCH.json').write_text(json.dumps({'profile': profile, 'byte': mismatch,
                'baseline_bytes': len(a), 'candidate_bytes': len(b)}, indent=2))
            raise RuntimeError('Exact output/status mismatch; stop, do not tune tolerances')
        match = re.search(r'PASS calls=(\d+) accelerated=(\d+)',
                          (out / (profile + '-candidate.log')).read_text())
        if not match:
            raise RuntimeError('Missing fixture receipt')
        calls, accelerated = map(int, match.groups())
        if calls != 115520 or len(a) != calls * 20:
            raise RuntimeError('Unexpected fixture inventory')
        if profile == 'generic' and accelerated == 0:
            raise RuntimeError('Generic accelerated route not exercised; unsupported build')
        if profile in ['off', 'unsafe'] and accelerated:
            raise RuntimeError('Required fallback not taken')
        # Native admission depends on the host's compiler feature flags, notably FMA.
        results.append({'profile': profile, 'calls': calls, 'accelerated': accelerated,
                        'sha256': hashlib.sha256(a).hexdigest()})
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
              for p in out.iterdir() if p.is_file()}
    (out / 'artifact-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
    (out / 'complete.txt').write_text('Focused correctness passed; no Engine or CPU performance claim.\n')

if __name__ == '__main__':
    main()
