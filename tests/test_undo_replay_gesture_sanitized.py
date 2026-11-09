#!/usr/bin/env python3
"""Instrument changed history/processor code and the focused fixture, not all JUCE.

Requires a completed GCC/Clang Ninja CS80Snapshots build from this checkout.
Reuses unchanged native objects, replacing UndoHistory.cpp, PluginProcessor.cpp
and the snapshot main with private ASan/UBSan objects. This is intentionally
partial instrumentation, not a whole-plugin/framework sanitizer qualification.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    entries = json.loads((build / 'compile_commands.json').read_text())
    command_lines = subprocess.run([os.environ.get('NINJA', 'ninja'), '-C', str(build), '-t', 'commands', 'CS80Snapshots'],
                                   check=True, text=True, capture_output=True).stdout.splitlines()
    tokens = shlex.split(command_lines[-1])
    assert tokens[:2] == [':', '&&'] and tokens[-2:] == ['&&', ':']
    link = tokens[2:-2]
    exe = output / 'undo-replay-sanitized'
    link[link.index('-o') + 1] = str(exe)
    main_cpp = output / 'main.cpp'
    main_cpp.write_text('''#include "UndoReplayGestureTests.h"
#include <cstdio>
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int checks = 0, failures = 0;
    cs80::tests::testUndoReplayGestures([&](bool condition, const char* what)
    { ++checks; if (!condition) { ++failures; std::printf("FAIL: %s\\n", what); } });
    std::printf("Undo replay gesture tests: %d checks, %d failures\\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
''')
    flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-fno-pie']
    executed = []
    with tempfile.TemporaryDirectory(prefix='cs80-undo-replay-sanitized-') as temporary:
        temporary = Path(temporary)
        for source in ['src/plugin/UndoHistory.cpp', 'src/plugin/PluginProcessor.cpp', 'src/tools/ui_snapshot.cpp']:
            entry = next(x for x in entries if x['file'] == str(ROOT / source) and 'CS80Snapshots.dir' in x['command'])
            cc = shlex.split(entry['command'])
            old_object = cc[cc.index('-o') + 1]
            assert old_object in link
            new_object = temporary / (Path(source).stem + '.o')
            cc = [str(new_object) if x == old_object else str(main_cpp) if source.endswith('ui_snapshot.cpp') and x == entry['file'] else x for x in cc]
            cc += flags + ['-I' + str(ROOT / 'tests')]
            subprocess.run(cc, cwd=entry['directory'], check=True)
            executed.append(cc)
            link = [str(new_object) if x == old_object else x for x in link]
        link += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie']
        subprocess.run(link, cwd=build, check=True)
        executed.append(link)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result = subprocess.run([str(exe)], env=env, text=True, capture_output=True, timeout=120)
    (output / 'run.log').write_text(result.stdout + result.stderr)
    summaries = re.findall(r'Undo replay gesture tests: (\d+) checks, (\d+) failures', result.stdout)
    assert result.returncode == 0 and len(summaries) == 1 and int(summaries[0][1]) == 0, (result.returncode, summaries)
    report = {'scope': __doc__, 'checks': int(summaries[0][0]), 'failures': 0, 'exit': result.returncode,
              'ASAN_OPTIONS': env['ASAN_OPTIONS'], 'UBSAN_OPTIONS': env['UBSAN_OPTIONS'],
              'instrumented': ['src/plugin/UndoHistory.cpp', 'src/plugin/PluginProcessor.cpp', 'focused fixture main including UndoReplayGestureTests.h'],
              'commands': executed, 'binary_sha256': hashlib.sha256(exe.read_bytes()).hexdigest()}
    (output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    print(result.stdout, end='')
    print('Partial ASan/UBSan: changed history/processor code and focused fixture; unchanged native/JUCE objects are not instrumented.')


if __name__ == '__main__':
    main()
