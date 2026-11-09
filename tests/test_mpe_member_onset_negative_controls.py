#!/usr/bin/env python3
"""Private fresh-directory MPE onset fault controls; never edit working source."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def summary(text, required_failures):
    pattern = r'^MPE member onset: (\d+) checks, (\d+) failures$'
    found = re.findall(pattern, text, re.MULTILINE)
    last = re.fullmatch(pattern, text.strip().splitlines()[-1]) if text.strip() else None
    if len(found) != 1 or last is None:
        raise RuntimeError('missing unique terminal numerical summary')
    checks, failures = map(int, last.groups())
    if checks != 2501 or not (0 <= failures <= checks) or bool(failures) != required_failures:
        raise RuntimeError('wrong coverage or numerical outcome')
    return {'checks': checks, 'failures': failures}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--baseline-root', type=Path, required=True)
    parser.add_argument('--baseline-library', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    library = args.library.resolve(strict=True)
    baseline = args.baseline_root.resolve(strict=True)
    baseline_library = args.baseline_library.resolve(strict=True)
    flags = ['c++', '-std=c++20', '-O3', '-DNDEBUG', '-DCS80_DISABLE_AVX2_FILTER_BATCH=1', '-Wall', '-Wextra', '-Werror']
    commands = []
    def run(name, argv, expected):
        argv = [str(x) for x in argv]
        try:
            result = subprocess.run(argv, capture_output=True, text=True, timeout=180)
        except subprocess.TimeoutExpired as ex:
            (out/(name+'.timeout.txt')).write_text(str(ex))
            raise RuntimeError(f'{name}: timeout is not a behavioral rejection') from ex
        text = result.stdout + result.stderr
        (out/(name+'.log')).write_text(text)
        commands.append({'name': name, 'argv': argv, 'returncode': result.returncode})
        (out/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        if result.returncode != expected:
            raise RuntimeError(f'{name}: expected exit {expected}, got {result.returncode}')
        return text
    results = {}
    for name, includes, archive in [('candidate', root, library), ('baseline', baseline, baseline_library)]:
        binary = out/name
        run(name+'-build', flags+['-I'+str(includes/'src'),root/'tests/MpeMemberOnsetTests.cpp',archive,'-o',binary],0)
        results[name] = summary(run(name,[binary],0 if name=='candidate' else 1),name!='candidate')
    engine = (root/'src/core/Engine.cpp').read_text()
    mutations = {
        'omit-bend': ('initial->bend = value;', 'initial->bend = 0.0f;'),
        'omit-pressure': ('initial->pressure = pressure;', 'initial->pressure = 0.0f;'),
        'omit-timbre': ('initial->timbre = value;', 'initial->timbre = 0.0f;'),
        'omit-reset': ('sustainPedals.fill(false);\n    memberExpression.fill({});', 'sustainPedals.fill(false);'),
        'omit-mode-edge': ('if (wasMpe != global.mpe)\n        memberExpression.fill({});', 'if (wasMpe != global.mpe) {}'),
        'omit-role-change': ('        discardChangedMemberExpression();', '        // fault: no role invalidation'),
        'broaden-mode-scope': ('if (!global.mpe || midiChannel < 1 || midiChannel > 16)', 'if (midiChannel < 1 || midiChannel > 16)'),
        'cache-curved-pressure': ('initial->pressure = pressure;', 'initial->pressure = applyPressureCurve(pressure, global.pressureCurve, global.pressureThreshold);'),
    }
    for name, (before, after) in mutations.items():
        if engine.count(before) != 1: raise RuntimeError(f'{name}: mutation is not unique')
        folder = out/name; shutil.copytree(root/'src',folder/'src')
        (folder/'src/core/Engine.cpp').write_text(engine.replace(before,after))
        obj=folder/'Engine.o'; binary=folder/'probe'
        run(name+'-compile',flags+['-I'+str(folder/'src'),'-c',folder/'src/core/Engine.cpp','-o',obj],0)
        run(name+'-link',flags+['-I'+str(folder/'src'),root/'tests/MpeMemberOnsetTests.cpp',obj,library,'-o',binary],0)
        results[name] = summary(run(name,[binary],1),True)
    (out/'summary.json').write_text(json.dumps(results,indent=2)+'\n')
    pins = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [library,baseline_library,root/'src/core/Engine.cpp',root/'src/core/Engine.h',root/'tests/MpeMemberOnsetTests.cpp',Path(__file__).resolve()]}
    (out/'input-hashes.json').write_text(json.dumps(pins,indent=2)+'\n')
    print(json.dumps(results),flush=True)

if __name__ == '__main__':
    main()
