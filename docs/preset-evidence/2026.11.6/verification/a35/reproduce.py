#!/usr/bin/env python3
"""Rebuild A35 probes against the repository's already built current core."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
TARGET = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
LIBRARY = ROOT / 'build-preset/libcs80_core.a'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def run(command, log, expected=0):
    result = subprocess.run(command, cwd=ROOT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (HERE / log).write_text(result.stdout)
    if result.returncode != expected:
        raise RuntimeError(f'{log}: exit {result.returncode}, expected {expected}')
    return result.stdout


def main():
    source = (ROOT / 'src/core/OriginalCircuitLibrary.inc').read_text()
    start = source.index('                for (int k = 0; k < CP_count; ++k)',
                         source.index('if (row == 1)'))
    stop = source.index('p.values[P_detune] = 0.0f;', start) + len('p.values[P_detune] = 0.0f;')
    block = source[start:stop] + '\n'
    (HERE / 'rank-swap.inc').write_text(block)
    removed = '                std::swap(p.values[bodyMaterialParam(0)], p.values[bodyMaterialParam(1)]);\n'
    assert block.count(removed) == 1
    sources = {}
    for name in ['src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/Params.h',
                 'src/core/InstrumentBody.h', 'src/core/InstrumentBodyMaterials.h',
                 'src/core/InstrumentBodyVariants.h', 'src/core/OriginalCircuitLibrary.inc',
                 'src/core/FactoryPresets.cpp', 'src/core/FactoryPresets.h',
                 'src/plugin/MemoryEditing.h']:
        raw = (ROOT / name).read_bytes()
        committed = subprocess.check_output(['git', 'show', TARGET + ':' + name], cwd=ROOT)
        assert raw == committed, name
        sources[name] = {'bytes': len(raw), 'sha256': sha(raw), 'matches_target_commit': True}
    with tempfile.TemporaryDirectory(prefix='cs80-a35-') as temporary:
        temp = Path(temporary)
        executable = temp / 'routing-probe'
        compiler = ['c++', '-std=c++20', '-O2', '-I' + str(ROOT / 'src')]
        run(compiler + [str(HERE / 'routing-probe.cpp'), str(LIBRARY), '-o', str(executable)], 'compile.log')
        output = run([str(executable)], 'routing-probe.log')
        assert '303 checks, 0 failures' in output
        executable_hash = sha(executable.read_bytes())
        shutil.copyfile(HERE / 'routing-probe.cpp', temp / 'routing-probe.cpp')
        (temp / 'rank-swap.inc').write_text(block.replace(removed, ''))
        (HERE / 'negative-control.diff').write_text('Extracted production swap block: remove exactly this statement:\n-' + removed)
        negative = temp / 'missing-material-swap'
        run(compiler + [str(temp / 'routing-probe.cpp'), str(LIBRARY), '-o', str(negative)], 'negative-compile.log')
        negative_output = run([str(negative), 'swap-only'], 'missing-material-swap.log', 1)
        assert negative_output.count('FAIL:') == 4
    summary = {
        'target_source_commit': TARGET, 'source_sha256': sources,
        'library_sha256': sha(LIBRARY.read_bytes()),
        'executable_sha256': executable_hash,
        'checks': 303, 'failures': 0, 'maximum_absolute_sample': 0.0296440106,
        'sample_rate': 48000, 'render_block_frames': 113,
        'zero_mix_materials': 9, 'ranks': 2, 'fallback_models_including_off': 20,
        'next_note_controls': ['model', 'decay', 'size', 'variant', 'material'],
        'live_controls': ['mix', 'brightness'],
        'swap_negative_control_failures': 4,
        'host_runtime_test': False, 'release_qualification': False,
        'probe_sha256': sha((HERE / 'routing-probe.cpp').read_bytes()),
        'swap_block_sha256': sha(block.encode()),
    }
    (HERE / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({k: summary[k] for k in ['checks', 'failures', 'maximum_absolute_sample', 'swap_negative_control_failures']}))


if __name__ == '__main__':
    main()
