#!/usr/bin/env python3
"""Bounded normal-input qualification. Run only after source/script review."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RECOVERY = ROOT.parent
BASE = RECOVERY / 'lfo-depth-frozen83'
BUILD = ROOT / 'build-native'
OUT = ROOT / 'docs/experiments/lfo-depth-automation'
JUCE = RECOVERY / 'yamaha-cs80/build/_deps/juce-src'
SDK = JUCE / 'modules/juce_audio_processors_headless/format_types/VST3_SDK'
LIBXML = RECOVERY / 'yamaha-cs80/build-native/_deps/cs80_libxml2-src'
TOOLS = ROOT.parents[1] / 'cs80-build-tools/bin'
FROZEN_MODULE_SHA = '04adbf33704b1b053847d4e6f5532b9d75b0b960584f087f1a9864338b0f4182'

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def run(args, log):
    with log.open('w') as stream:
        stream.write('COMMAND ' + repr([str(x) for x in args]) + '\n')
        stream.flush()
        subprocess.run([str(x) for x in args], cwd=ROOT, env=ENV,
                       stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=1200)

# Env comes only from the established official-dependency recovery setup.
ENV = os.environ.copy()
env_file = RECOVERY / 'yamaha-cs80-tuning-ref-automation/build/recovery-env.sh'
raw = subprocess.check_output(['bash', '-c', 'source "$1"; env -0', 'bash', str(env_file)])
ENV.update(dict(item.decode().split('=', 1) for item in raw.split(b'\0') if item))

freeze = json.loads((ROOT / 'tests/lfo-depth-source-freeze.json').read_text())
for path, expected in freeze.items():
    assert digest(ROOT / path) == expected, f'reviewed source changed: {path}'
assert digest(BASE / 'baseline83.so') == FROZEN_MODULE_SHA
inventory = json.loads((BASE / 'tracked-identities.json').read_text())
for path, expected in inventory.items():
    if path.startswith(('src/', 'tests/')):
        assert (BASE / path).is_file(), f'frozen83 source missing: {path}'
        assert digest(BASE / path) == expected, f'frozen83 source changed: {path}'
for name in ('Vst3LfoDepthAutomationTests.cpp', 'Vst3Frozen83Roster.h'):
    # These are the only additions to the isolated original83 source tree.
    assert digest(BASE / 'tests' / name) == digest(ROOT / 'tests' / name)

allowed_old_changes = {'CMakeLists.txt', 'src/plugin/HostAutomationBridge.h',
                      'tests/Vst3ModWheelAutomationTests.cpp'}
retained = {}
for path, expected in inventory.items():
    if path.startswith(('src/', 'tests/')) and path not in allowed_old_changes:
        assert (ROOT / path).is_file() and digest(ROOT / path) == expected, f'retained source changed: {path}'
        retained[path] = expected
# No overwrite of prior evidence and no deletion/retry hiding a failed run.
OUT.mkdir(parents=True, exist_ok=False)
(OUT / 'retained-identities.json').write_text(json.dumps(retained, indent=2) + '\n')
run([TOOLS / 'cmake', '-S', ROOT, '-B', BUILD, '-G', 'Ninja',
     '-DCMAKE_BUILD_TYPE=Release', '-DCS80_BUILD_PLUGIN=ON', '-DCS80_BUILD_TESTS=ON',
     '-DFETCHCONTENT_FULLY_DISCONNECTED=ON', f'-DFETCHCONTENT_SOURCE_DIR_JUCE={JUCE}',
     f'-DFETCHCONTENT_SOURCE_DIR_CS80_LIBXML2={LIBXML}'], OUT / 'configure.log')
run([TOOLS / 'cmake', '--build', BUILD, '--target', 'cs80_vst3_lfo_depth_automation_tests', '-j2'], OUT / 'build.log')
# Relative includes resolve INSIDE frozen83/tests, never candidate tests.
run(['/usr/bin/c++', '-std=c++20', '-O3', '-DNDEBUG', '-I' + str(BASE / 'src'), '-I' + str(SDK),
     BASE / 'tests/Vst3LfoDepthAutomationTests.cpp', SDK / 'pluginterfaces/base/coreiids.cpp',
     SDK / 'pluginterfaces/base/funknown.cpp', SDK / 'public.sdk/source/vst/vstinitiids.cpp',
     '-ldl', '-o', BASE / 'lfo-depth-compat83'], OUT / 'baseline-build.log')
module = BUILD / 'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
runner = BUILD / 'cs80_vst3_lfo_depth_automation_tests'
host = ROOT / 'tests/parity/tolerances-v2/host-automation.json'
contract = ROOT / 'tests/parity/tolerances-v2/host-automation-lfo-depth-v1.json'
pairs = []
for rate in ('44100', '48000', '96000'):
    for mode in ('realtime', 'offline'):
        identity = rate + '-' + mode
        candidate = BUILD / ('lfo-depth-candidate-' + identity)
        baseline = BASE / ('lfo-depth-baseline-' + identity)
        assert not candidate.exists() and not baseline.exists(), 'capture paths must be fresh'
        full_log, base_log = OUT / ('candidate-' + identity + '.log'), OUT / ('baseline-' + identity + '.log')
        run([runner, module, host, contract, candidate, rate, mode], full_log)
        run([BASE / 'lfo-depth-compat83', BASE / 'baseline83.so', host, contract,
             baseline, rate, mode, '--compat-only'], base_log)
        assert 'compiled_lanes=86\n' in full_log.read_text()
        assert 'compiled_lanes=83\n' in base_log.read_text()
        assert 'scenarios=16 renders=224 captures=64 failures=0 mode=full' in full_log.read_text()
        assert 'scenarios=16 renders=112 captures=64 failures=0 mode=compat-only' in base_log.read_text()
        a = sorted(p.name for p in candidate.glob('*.pcm'))
        b = sorted(p.name for p in baseline.glob('*.pcm'))
        assert a == b and len(a) == 64, 'complete matching capture sets'
        for name in a:
            actual, prior = candidate / name, baseline / name
            assert actual.stat().st_size == prior.stat().st_size == 32768, '4096 stereo float frames'
            assert actual.read_bytes() == prior.read_bytes(), f'PCM mismatch: {identity}/{name}'
            pairs.append({'case': identity + '/' + name, 'candidate_sha256': digest(actual),
                          'baseline_sha256': digest(prior), 'byte_equal': True})
assert len(pairs) == 384
for path, expected in freeze.items():
    assert digest(ROOT / path) == expected, f'reviewed source changed during run: {path}'
assert digest(BASE / 'baseline83.so') == FROZEN_MODULE_SHA
for path, expected in inventory.items():
    if path.startswith(('src/', 'tests/')):
        assert (BASE / path).is_file() and digest(BASE / path) == expected, f'frozen83 source changed during run: {path}'
for name in ('Vst3LfoDepthAutomationTests.cpp', 'Vst3Frozen83Roster.h'):
    assert digest(BASE / 'tests' / name) == digest(ROOT / 'tests' / name)
for path, expected in retained.items():
    assert (ROOT / path).is_file() and digest(ROOT / path) == expected, f'retained source changed during run: {path}'
(OUT / 'compatibility-pairs.json').write_text(json.dumps(pairs, indent=2) + '\n')
identities = {str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p): digest(p)
              for p in [module, runner, BASE / 'baseline83.so', BASE / 'lfo-depth-compat83',
                        ROOT / 'tests/lfo-depth-source-freeze.json', *sorted(OUT.glob('*.log')),
                        OUT / 'retained-identities.json', OUT / 'compatibility-pairs.json']}
(OUT / 'sha256.json').write_text(json.dumps(identities, indent=2) + '\n')
(OUT / 'summary.json').write_text(json.dumps({'full_cases': 96, 'compat_cases': 96,
    'candidate_renders': 1344, 'baseline_renders': 672, 'actual_pcm_pairs': 384,
    'capture_files': 768, 'forward_guard_tests': 'source-reviewed only, not runtime',
    'historical_suites': 'not rerun'}, indent=2) + '\n')
print('PASS: 96 full cases, 96 compatibility cases, 2016 renders, 384 actual PCM pairs')
