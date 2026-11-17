"""Exact unchanged Sub/Ring host fixtures and full release inventory guards.

No execution or test changes. Commands are verified before an orchestrator runs
original fixtures; capture comparison covers only these declared PCM outputs.
"""
from pathlib import Path
import hashlib


def require(ok, message):
    if not ok: raise ValueError(message)


def module_path(build):
    return Path(build) / 'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'


def selected_contract(repo, build):
    repo, build = Path(repo).resolve(), Path(build).resolve()
    tolerance = repo / 'tests/parity/tolerances-v2'
    result = {}
    for kind in ('sub', 'ring'):
        for rate in (44100, 48000, 96000):
            for mode in ('realtime', 'offline'):
                default = (kind, rate, mode) == ('sub', 48000, 'realtime')
                suffix = '' if default else f'_{rate}_{mode}'
                name = f'cs80_vst3_{kind}_automation{suffix}_tests'
                output_suffix = '' if default else f'-{rate}-{mode}'
                output = build / (kind + '-automation-capture' + output_suffix)
                command = [build / f'cs80_vst3_{kind}_automation_tests', module_path(build),
                           tolerance / 'host-automation.json', tolerance / f'host-automation-{kind}-v1.json', output]
                if not default: command += [str(rate), mode]
                result[name] = {'command': list(map(str, command)), 'timeout': 180,
                                'output': str(output), 'kind': kind, 'rate': rate, 'mode': mode}
    require(len(result) == 12, 'selected roster construction error')
    return result


def tests_by_name(document):
    tests = document.get('tests', [])
    result = {row['name']: row for row in tests}
    require(len(result) == len(tests) == 569, 'full unique 569-test roster required')
    return result


def validate_selected(document, repo, build):
    tests = tests_by_name(document)
    contract = selected_contract(repo, build)
    for name, expected in contract.items():
        require(name in tests, 'missing selected host test')
        actual = tests[name]
        require(actual.get('command') == expected['command'], 'original host command changed or not built: ' + name)
        props = {x['name']: x['value'] for x in actual.get('properties', [])}
        require(props.get('TIMEOUT') == expected['timeout'], 'host timeout changed')
        require(not props.get('DISABLED', False), 'host test disabled')
    return contract


def validate_full_inventory(before, after):
    a, b = tests_by_name(before), tests_by_name(after)
    require(a.keys() == b.keys(), 'full release roster changed')
    for name, old in a.items():
        new = b[name]
        require(old.get('properties', []) == new.get('properties', []), 'test properties changed: ' + name)
        require(isinstance(new.get('command'), list) and bool(new['command']), 'full test executable not built: ' + name)
        if old.get('command') is not None:
            require(old['command'] == new['command'], 'test command changed: ' + name)
        props = {x['name']: x['value'] for x in new.get('properties', [])}
        require(not props.get('DISABLED', False), 'release test disabled: ' + name)
    return sorted(b)


def capture_names(kind):
    if kind == 'sub':
        names = {f'{voices}-{source}-{lane}-{policy}.pcm'
                 for voices in (8, 16) for source in (13, 0, 11)
                 for lane in (20, 21) for policy in ('noqueue', 'policy0', 'policy2')}
        names |= {f'old20-{block}.pcm' for block in (7, 64, 256, 1024, 4096)}
        require(len(names) == 41, 'Sub capture roster')
    elif kind == 'ring':
        names = {f'{lane}-{voices}-{source}-{policy}.pcm'
                 for lane in (28, 29) for voices in (8, 16) for source in (13, 0, 11)
                 for policy in ('noqueue', 'policy0', 'policy2')}
        require(len(names) == 36, 'Ring capture roster')
    else: raise ValueError('unknown host capture kind')
    return names


def capture_manifest(directory, kind):
    directory = Path(directory)
    require(directory.is_dir() and not directory.is_symlink(), 'regular capture directory required')
    entries = list(directory.iterdir())
    require({x.name for x in entries} == capture_names(kind), 'capture roster missing/extra')
    result = {}
    for path in entries:
        require(path.is_file() and not path.is_symlink() and path.stat().st_size == 32768,
                'capture must contain 4096 stereo float frames')
        result[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def compare_captures(baseline, candidate, kind):
    a, b = capture_manifest(baseline, kind), capture_manifest(candidate, kind)
    require(a == b, 'host PCM capture mismatch')
    return {'kind': kind, 'files': len(a), 'bytes': len(a) * 32768, 'sha256': a,
            'scope': 'declared noqueue/flattened-policy and Sub historical-lane PCM only; active automation assertions remain separate'}


def definitions_manifest(build):
    build = Path(build)
    result = {str(path.relative_to(build)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted(build.rglob('CTestTestfile.cmake')) if path.is_file()}
    require(bool(result), 'raw configured CTest definitions required')
    return result


def verify_definitions(frozen, build):
    require(definitions_manifest(build) == frozen, 'raw CTest definitions changed after configuration/build')
