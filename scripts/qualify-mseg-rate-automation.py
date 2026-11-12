#!/usr/bin/env python3
"""Bounded normal-input qualification. Run only after source/script review."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RECOVERY = ROOT.parent
BASE = RECOVERY / 'mseg-rate-frozen89'
BUILD = ROOT / 'build-native'
OUT = ROOT / 'docs/experiments/mseg-rate-automation'
JUCE = RECOVERY / 'yamaha-cs80/build/_deps/juce-src'
SDK = JUCE / 'modules/juce_audio_processors_headless/format_types/VST3_SDK'
LIBXML = RECOVERY / 'yamaha-cs80/build-native/_deps/cs80_libxml2-src'
TOOLS = ROOT.parents[1] / 'cs80-build-tools/bin'
FROZEN_MODULE_SHA = '59ed49c6b64d28847b7ebe83ec60811c819c06d827679a15a72992818c93de4f'

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

freeze = json.loads((ROOT / 'tests/mseg-rate-source-freeze.json').read_text())
for path, expected in freeze.items():
    assert digest(ROOT / path) == expected, f'reviewed source changed: {path}'
assert digest(BASE / 'baseline89.so') == FROZEN_MODULE_SHA
inventory = json.loads((BASE / 'tracked-identities.json').read_text())
for path, expected in inventory.items():
    if path.startswith(('src/', 'tests/')):
        assert (BASE / path).is_file(), f'frozen89 source missing: {path}'
        assert digest(BASE / path) == expected, f'frozen89 source changed: {path}'
for name in ('Vst3MsegRateAutomationTests.cpp', 'Vst3Frozen89Roster.h'):
    # These are the only additions to the isolated original89 source tree.
    assert digest(BASE / 'tests' / name) == digest(ROOT / 'tests' / name)

allowed_old_changes = {'CMakeLists.txt', 'src/plugin/HostAutomationBridge.h',
                      'tests/Vst3LfoRateAutomationTests.cpp', 'tests/Vst3LfoNativeRandomTests.cpp',
                      'tests/Vst3LfoRateDepthCombinedTests.cpp', 'tests/Vst3LfoStaticShapesTests.cpp',
                      'tests/Vst3MsegRoutingProofTests.cpp'}
retained = {}
for path, expected in inventory.items():
    if path.startswith(('src/', 'tests/')) and path not in allowed_old_changes:
        assert (ROOT / path).is_file() and digest(ROOT / path) == expected, f'retained source changed: {path}'
        retained[path] = expected
for path, expected in inventory.items():
    if path.startswith('docs/experiments/mseg-routing-proof/canonical/'):
        assert digest(ROOT / path) == expected, f'canonical session changed: {path}'
        retained[path] = expected
# No overwrite of prior evidence and no deletion/retry hiding a failed run.
OUT.mkdir(parents=True, exist_ok=False)
(OUT / 'retained-identities.json').write_text(json.dumps(retained, indent=2) + '\n')
run([TOOLS / 'cmake', '-S', ROOT, '-B', BUILD, '-G', 'Ninja',
     '-DCMAKE_BUILD_TYPE=Release', '-DCS80_BUILD_PLUGIN=ON', '-DCS80_BUILD_TESTS=ON',
     '-DFETCHCONTENT_FULLY_DISCONNECTED=ON', f'-DFETCHCONTENT_SOURCE_DIR_JUCE={JUCE}',
     f'-DFETCHCONTENT_SOURCE_DIR_CS80_LIBXML2={LIBXML}'], OUT / 'configure.log')
run([TOOLS / 'cmake', '--build', BUILD, '--target', 'cs80_vst3_mseg_rate_automation_tests', '-j2'], OUT / 'build.log')
# Relative includes resolve INSIDE frozen89/tests, never candidate tests.
run(['/usr/bin/c++', '-std=c++20', '-O3', '-DNDEBUG', '-I' + str(BASE / 'src'), '-I' + str(SDK),
     BASE / 'tests/Vst3MsegRateAutomationTests.cpp', SDK / 'pluginterfaces/base/coreiids.cpp',
     SDK / 'pluginterfaces/base/funknown.cpp', SDK / 'public.sdk/source/vst/vstinitiids.cpp',
     '-ldl', '-o', BASE / 'mseg-rate-compat89'], OUT / 'baseline-build.log')
module = BUILD / 'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
runner = BUILD / 'cs80_vst3_mseg_rate_automation_tests'
host = ROOT / 'tests/parity/tolerances-v2/host-automation.json'
contract = ROOT / 'tests/parity/tolerances-v2/host-automation-mseg-rate-v1.json'
canonical = ROOT / 'docs/experiments/mseg-routing-proof/canonical'
proof = RECOVERY / 'yamaha-cs80-mseg-route-proof'
verifier = proof / 'build-mseg-routing-proof/canonical-state'
known_binaries = json.loads((proof / 'docs/experiments/mseg-routing-proof/binary-before.json').read_text())
assert digest(verifier) == known_binaries[str(verifier)], 'known canonical verifier'
binary_paths = [module, runner, BASE / 'baseline89.so', BASE / 'mseg-rate-compat89', verifier]
binary_before = {str(p): digest(p) for p in binary_paths}
(OUT / 'binary-before.json').write_text(json.dumps(binary_before, indent=2) + '\n')
pairs = []
state_hashes = {}
for rate in ('44100', '48000', '96000'):
    for mode in ('realtime', 'offline'):
        identity = rate + '-' + mode
        candidate = BUILD / ('mseg-rate-candidate-' + identity)
        baseline = BASE / ('mseg-rate-baseline-' + identity)
        assert not candidate.exists() and not baseline.exists(), 'capture paths must be fresh'
        full_log, base_log = OUT / ('candidate-' + identity + '.log'), OUT / ('baseline-' + identity + '.log')
        run([runner, module, canonical, candidate, host, contract, rate, mode], full_log)
        run([BASE / 'mseg-rate-compat89', BASE / 'baseline89.so', canonical, baseline, host, contract,
             rate, mode, '--compat-only'], base_log)
        assert 'compiled_lanes=92\n' in full_log.read_text()
        assert 'compiled_lanes=89\n' in base_log.read_text()
        assert 'scenarios=3 renders=51 captures=12 failures=0 mode=full' in full_log.read_text()
        assert 'scenarios=3 renders=21 captures=12 failures=0 mode=compat-only' in base_log.read_text()
        for route in range(3):
            for label, directory in [('candidate', candidate), ('baseline', baseline)]:
                readback = directory / (f'mseg-route{route}.state')
                log = OUT / (f'{label}-{identity}-route{route}-verify.log')
                run([verifier, 'verify', readback, str(route), '1'], log)
                assert 'canonical_native_readback=1 failures=0' in log.read_text()
                state_hashes[str(readback)] = digest(readback)
        a = sorted(p.name for p in candidate.glob('*.pcm'))
        b = sorted(p.name for p in baseline.glob('*.pcm'))
        assert a == b and len(a) == 12, 'complete matching capture sets'
        for name in a:
            actual, prior = candidate / name, baseline / name
            assert actual.stat().st_size == prior.stat().st_size == 131072, '16384 stereo float frames'
            assert actual.read_bytes() == prior.read_bytes(), f'PCM mismatch: {identity}/{name}'
            pairs.append({'case': identity + '/' + name, 'candidate_sha256': digest(actual),
                          'baseline_sha256': digest(prior), 'byte_equal': True})
assert len(pairs) == 72 and len(state_hashes) == 36
(OUT / 'state-hashes.json').write_text(json.dumps(state_hashes, indent=2) + '\n')
for p in binary_paths:
    assert digest(p) == binary_before[str(p)], f'binary changed during runs: {p}'
for path, expected in freeze.items():
    assert digest(ROOT / path) == expected, f'reviewed source changed during run: {path}'
assert digest(BASE / 'baseline89.so') == FROZEN_MODULE_SHA
for path, expected in inventory.items():
    if path.startswith(('src/', 'tests/')):
        assert (BASE / path).is_file() and digest(BASE / path) == expected, f'frozen89 source changed during run: {path}'
for name in ('Vst3MsegRateAutomationTests.cpp', 'Vst3Frozen89Roster.h'):
    assert digest(BASE / 'tests' / name) == digest(ROOT / 'tests' / name)
for path, expected in retained.items():
    assert (ROOT / path).is_file() and digest(ROOT / path) == expected, f'retained source changed during run: {path}'
(OUT / 'compatibility-pairs.json').write_text(json.dumps(pairs, indent=2) + '\n')
identities = {str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p): digest(p)
              for p in [module, runner, BASE / 'baseline89.so', BASE / 'mseg-rate-compat89',
                        ROOT / 'tests/mseg-rate-source-freeze.json', *sorted(OUT.glob('*.log')),
                        OUT / 'retained-identities.json', OUT / 'compatibility-pairs.json', OUT / 'binary-before.json', OUT / 'state-hashes.json']}
(OUT / 'sha256.json').write_text(json.dumps(identities, indent=2) + '\n')
(OUT / 'summary.json').write_text(json.dumps({'full_cases': 18, 'compat_cases': 18,
    'candidate_renders': 306, 'baseline_renders': 126, 'actual_pcm_pairs': 72,
    'capture_files': 144, 'native_config_readbacks': 36, 'forward_guard_tests': 'source-reviewed only, not runtime',
    'historical_suites': 'not rerun'}, indent=2) + '\n')
print('PASS: 18 full cases, 18 compatibility cases, 432 renders, 72 actual PCM pairs, 36 config readbacks')
