#!/usr/bin/env python3
"""Fresh-directory, test-only fault controls. Never modifies shipping source."""
from pathlib import Path
import json, re, shutil, subprocess, sys

root = Path(__file__).resolve().parents[3]
out = Path(sys.argv[1]).resolve()
library = Path(sys.argv[2]).resolve(strict=True)
out.mkdir(parents=True, exist_ok=False)
flags = ['c++', '-std=c++20', '-O3', '-DNDEBUG', '-DCS80_DISABLE_AVX2_FILTER_BATCH=1',
         '-Wall', '-Wextra', '-Werror']

def run(args, name, status=0):
    argv = ['taskset', '-c', '0-3'] + [str(a) for a in args]
    (out / (name + '.command.json')).write_text(json.dumps(argv, indent=2) + '\n')
    p = subprocess.run(argv, text=True, capture_output=True, timeout=180)
    (out / (name + '.log')).write_text(p.stdout + p.stderr)
    if p.returncode != status:
        raise RuntimeError(f'{name}: expected exit {status}, got {p.returncode}')
    return p.stdout

def result_summary(text, name):
    pattern = r'^(\d+) scenarios, (\d+) checks, (\d+) failures$'
    matches = re.findall(pattern, text, flags=re.MULTILINE)
    final = re.fullmatch(pattern, text.strip().splitlines()[-1]) if text.strip() else None
    if len(matches) != 1 or final is None:
        raise RuntimeError(f'{name}: missing unique terminal numerical summary')
    scenarios, checks, failures = map(int, final.groups())
    if scenarios != 54 or checks != 1958109 or not (0 < failures <= checks):
        raise RuntimeError(f'{name}: incomplete numerical rejection')
    return {'scenarios': scenarios, 'checks': checks, 'failures': failures}

engine = (root / 'src/core/Engine.cpp').read_text()
dsp = (root / 'src/core/Dsp.h').read_text()
call = 'subValue = subLfo.tick(subSpeedSmoothed, sr, global.subWave, modRng);'
coefficient = '* (usesSourceModels(global.renderPolicy) ? subSpeedSmoothing : 0.0015f);'
mutations = {
    'coefficient': ('engine', coefficient, coefficient.replace('0.0015f', '0.0016f')),
    'extra-rng': ('engine', call, '(void) modRng.nextUInt();\n        ' + call),
    'skipped-wrap-draw': ('lfo', 'if (! sampleHoldStarted || wrapped)',
                         'if (! sampleHoldStarted || (wrapped && false))'),
    'phase-reset': ('engine', call, 'subLfo.reset();\n        ' + call),
    'missing-wrap': ('lfo', 'phase -= std::floor(phase);', 'phase -= 0.0f;'),
    'omitted-lfo-tick': ('engine', call,
                        'static bool runTick = false; runTick = !runTick;\n        if (runTick) ' + call),
}
summary = {}
for name, (where, before, after) in mutations.items():
    folder = out / name
    shutil.copytree(root / 'src', folder / 'src')
    if where == 'engine':
        if engine.count(before) != 1: raise RuntimeError(f'{name}: nonunique mutation')
        (folder / 'src/core/Engine.cpp').write_text(engine.replace(before, after))
    else:
        begin = dsp.index('class Lfo\n')
        end = dsp.index('class DelayLine\n', begin)
        part = dsp[begin:end]
        if part.count(before) != 1: raise RuntimeError(f'{name}: nonunique mutation')
        (folder / 'src/core/Dsp.h').write_text(dsp[:begin] + part.replace(before, after) + dsp[end:])
    obj = folder / 'Engine.o'
    run(flags + ['-I' + str(folder/'src'), '-c', folder/'src/core/Engine.cpp', '-o', obj], name + '-compile')
    binary = folder / 'probe'
    run(flags + ['-I' + str(folder/'src'), root/'tests/LegacySubLfoOracleTests.cpp', obj, library,
                 '-o', binary], name + '-link')
    text = run([binary], name, 1)
    summary[name] = result_summary(text, name)

# The test validator must reject reviving/per-observation track selection too.
folder = out / 'revive-eligibility'
folder.mkdir()
header = (root/'tests/LegacySubLfoOracle.h').read_text()
needle = 'bool observe(Observation actual) noexcept\n    {\n'
if header.count(needle) != 1: raise RuntimeError('nonunique validator mutation')
(folder/'LegacySubLfoOracle.h').write_text(header.replace(needle, needle+'        eligible = 3;\n'))
shutil.copyfile(root/'tests/LegacySubLfoOracleTests.cpp', folder/'probe.cpp')
binary = folder/'probe'
run(flags+['-I'+str(root/'src'), folder/'probe.cpp',library,'-o',binary], 'revive-eligibility-build')
text = run([binary], 'revive-eligibility', 1)
summary['revive-eligibility'] = result_summary(text, 'revive-eligibility')
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary), flush=True)
