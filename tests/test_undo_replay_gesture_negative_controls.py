#!/usr/bin/env python3
"""Build private UndoHistory object faults and relink the completed native gate.

Unchanged native objects and the working source/build remain untouched. Detection
requires expected normal exit, a completion summary and behavioral failures;
compile/link errors, signals and missing summaries do not count as detections.
"""
import argparse
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
    build = args.build.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    entry = next(x for x in json.loads((build / 'compile_commands.json').read_text())
                 if x['file'] == str(ROOT / 'src/plugin/UndoHistory.cpp') and 'CS80Snapshots.dir' in x['command'])
    compile_args = shlex.split(entry['command'])
    old_object = compile_args[compile_args.index('-o') + 1]
    commands = subprocess.run([os.environ.get('NINJA', 'ninja'), '-C', str(build), '-t', 'commands', 'CS80Snapshots'],
                              check=True, text=True, capture_output=True).stdout.splitlines()
    tokens = shlex.split(commands[-1])
    assert tokens[:2] == [':', '&&'] and tokens[-2:] == ['&&', ':']
    link_args = tokens[2:-2]
    assert old_object in link_args and '-o' in link_args
    old_executable = link_args[link_args.index('-o') + 1]
    source = (ROOT / 'src/plugin/UndoHistory.cpp').read_text()
    variants = {}
    for name, old, new in [
        ('omit-replay-gestures', 'if (owner.history.isPerformingUndoRedo() && target.hasType("PARAM")',
         'if (false && owner.history.isPerformingUndoRedo() && target.hasType("PARAM")'),
        ('omit-replay-listener-guard', 'if (! onMessageThread() || history.isPerformingUndoRedo())',
         'if (! onMessageThread())'),
        ('omit-active-edit-guard', '!onMessageThread() || openGestures != 0 || userScopes != 0 || writing',
         '!onMessageThread() || writing'),
        ('lose-first-coalesced-value', 'new ParameterValueAction(owner, target, before, next->after, true)',
         'new ParameterValueAction(owner, target, after, next->after, true)'),
        ('lose-missing-property', 'bool undo() override { return apply(before, hadProperty); }',
         'bool undo() override { return apply(before, true); }'),
        ('record-external-automation', 'write(i, nullptr);', 'write(i, &history);'),
        ('omit-recording-write-guard', '|| userScopes != 0 || writing', '|| userScopes != 0'),
        ('omit-thread-guard', 'if (!onMessageThread() || openGestures != 0', 'if (openGestures != 0')]:
        assert source.count(old) == 1, (name, 'unexpected source match count')
        variants[name] = source.replace(old, new)
    results = {}

    def probe(name, exe):
        result = subprocess.run([str(exe), '--undo-replay-gesture-tests'], text=True, capture_output=True, timeout=120)
        (args.output / (name + '.log')).write_text(result.stdout + result.stderr)
        summaries = re.findall(r'Undo replay gesture tests: (\d+) checks, (\d+) failures', result.stdout)
        assert len(summaries) == 1, (name, 'no normal completion', result.returncode)
        checks, failures = map(int, summaries[0])
        assert result.returncode == (0 if name == 'candidate' else 1), (name, result.returncode)
        assert (failures == 0) == (name == 'candidate'), (name, failures)
        results[name] = {'checks': checks, 'failures': failures, 'exit': result.returncode}
        print(name, results[name], flush=True)

    probe('candidate', build / old_executable)
    with tempfile.TemporaryDirectory(prefix='cs80-undo-replay-negative-') as directory:
        directory = Path(directory)
        for name, changed in variants.items():
            file = directory / (name + '.cpp'); file.write_text(changed)
            obj, exe = directory / (name + '.o'), directory / name
            cc = [str(obj) if x == old_object else str(file) if x == entry['file'] else x for x in compile_args]
            cc += ['-I' + str(ROOT / 'src/plugin')]
            subprocess.run(cc, cwd=entry['directory'], check=True)
            link = [str(obj) if x == old_object else str(exe) if x == old_executable else x for x in link_args]
            subprocess.run(link, cwd=build, check=True)
            probe(name, exe)
    (args.output / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
