#!/usr/bin/env python3
"""Build isolated negative controls; never edit the production checkout.

Usage: python3 scripts/test-ig00156-top-octave-mutations.py NEW_OUTPUT_DIRECTORY
Requires a GNU-compatible C++20 compiler, selected with CXX if needed. No external packages.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

if len(sys.argv) != 2:
    raise SystemExit('Usage: test-ig00156-top-octave-mutations.py NEW_OUTPUT_DIRECTORY')

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=False)
compiler = shlex.split(os.environ.get('CXX', 'c++'))
header = (repo / 'src/core/Ig00156Filter.h').read_text()
source = repo / 'tests/Ig00156TopOctaveTests.cpp'
limits = repo / 'tests/parity/known-deviations/ig00156-top-octave.json'
results = []


def build(name, changed_header=None):
    directory = out / name
    directory.mkdir()
    if changed_header is not None:
        (directory / 'core').mkdir()
        (directory / 'core/Ig00156Filter.h').write_text(changed_header)
    executable = directory / 'guard'
    command = compiler + ['-std=c++20', '-O3', '-DNDEBUG',
        '-I' + str(directory), '-I' + str(repo / 'src'),
        '-I' + str(repo / 'src/core'), str(source), '-o', str(executable)]
    run = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (directory / 'build.log').write_text(run.stdout)
    if run.returncode:
        raise RuntimeError(f'{name}: build failed, not a detected mutation')
    return executable


def run_case(name, executable, configuration, expected_code, marker):
    run = subprocess.run([str(executable), str(configuration)], stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True)
    (out / f'{name}.log').write_text(run.stdout)
    summary = next((line for line in run.stdout.splitlines() if 'checks,' in line), '')
    passed = run.returncode == expected_code and marker in run.stdout
    results.append(dict(name=name, exit_code=run.returncode, verified=passed, summary=summary))
    print(name + ': ' + summary)
    if not passed:
        raise RuntimeError(f'{name}: unexpected outcome, see saved log')


baseline = build('baseline')
run_case('baseline', baseline, limits, 0, '0 failures.')

needle = 'kHalf = 0.5 * (fastmath::tan(theta) / theta) / fs;'
assert header.count(needle) == 1
unwarped = build('unwarped', header.replace(needle, 'kHalf = 0.5 / fs;'))
run_case('unwarped', unwarped, limits, 1,
         'FAIL: actual high-cutoff output matches independent discrete nodal truth')

needle = 'return { static_cast<float>(lp), static_cast<float>(hp), static_cast<float>(-bp) };'
assert header.count(needle) == 1
changed = header.replace(needle, '''const Output current { static_cast<float>(lp), static_cast<float>(hp), static_cast<float>(-bp) };
        const Output previous = delayedOutput;
        delayedOutput = current;
        return previous;''')
changed = changed.replace('private:\n    Ig00156Network net;', 'private:\n    Output delayedOutput {};\n    Ig00156Network net;')
assert 'Output delayedOutput {};' in changed
delayed = build('delayed', changed)
run_case('delayed', delayed, limits, 1,
         'FAIL: actual high-cutoff output matches independent discrete nodal truth')

narrowed = json.loads(limits.read_text())
narrowed['ig00156_top_octave_regressions']['limits_88200_12000'] = [0.03, 0.08]
narrowed_path = out / 'original-acceptance-in-known-deviation-slot.json'
narrowed_path.write_text(json.dumps(narrowed, indent=2) + '\n')
run_case('original_acceptance_is_still_missed', baseline, narrowed_path, 1,
         'FAIL: high-cutoff core error does not exceed declared known-deviation ceiling')

(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print('Verified baseline and all three negative controls. No shipping code was edited.')
