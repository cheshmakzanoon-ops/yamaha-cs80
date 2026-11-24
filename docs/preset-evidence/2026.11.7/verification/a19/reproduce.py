#!/usr/bin/env python3
"""A19 frozen-source actual-preset probe. All PCM/build products stay in --work.

Run with --rebuild to reproduce without the session's supplied core archives.
Both versions always use a complete matching src header snapshot.
"""
import argparse, concurrent.futures, hashlib, io, json, pathlib, subprocess, tarfile

HERE = pathlib.Path(__file__).resolve().parent
REVISIONS = {'before': '18711f93a39aa49f3d6f9a41784be456224facbe',
             'after': 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--repo', type=pathlib.Path, default=HERE.parents[4])
p.add_argument('--work', type=pathlib.Path, default=pathlib.Path('/tmp/cs80-a19'))
p.add_argument('--output', type=pathlib.Path, default=HERE)
p.add_argument('--current-lib', type=pathlib.Path, default=pathlib.Path('/tmp/cs80-build-1cee5a/libcs80_core.a'))
p.add_argument('--baseline-lib', type=pathlib.Path, default=pathlib.Path('/tmp/cs80-reference-1cee5a/libcs80_core.a'))
p.add_argument('--rebuild', action='store_true')
a = p.parse_args(); a.work.mkdir(parents=True, exist_ok=True); a.output.mkdir(parents=True, exist_ok=True)
commands = []
def run(command, **kw):
    command = list(map(str, command)); commands.append(command)
    subprocess.run(command, check=True, **kw)
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def capture(kind):
    root = a.work / kind; root.mkdir(exist_ok=True)
    archive = subprocess.check_output(['git', 'archive', REVISIONS[kind], 'src'], cwd=a.repo)
    with tarfile.open(fileobj=io.BytesIO(archive)) as t: t.extractall(root, filter='data')
    inc = root / 'src'
    library = a.current_lib if kind == 'after' else a.baseline_lib
    if a.rebuild:
        objects = []
        for name in ('Engine', 'EngineWscHeld', 'FactoryPresets', 'Ig00156Batch'):
            obj = root / (name + '.o')
            flags = ['-DCS80_ENABLE_BORROWED_HELD=1'] if name in ('Engine', 'EngineWscHeld') else ['-ffp-contract=off'] if name == 'Ig00156Batch' else []
            run(['c++', '-std=gnu++20', '-O3', '-DNDEBUG', '-fPIC', '-I', inc, *flags, '-c', inc / 'core' / (name + '.cpp'), '-o', obj])
            objects.append(obj)
        library = root / 'libcs80_core.a'
        if library.exists(): library.unlink()
        run(['ar', 'rcs', library, *objects])
    exe = root / 'winds_probe'
    run(['c++', '-std=c++20', '-O2', '-I', inc, HERE / 'winds_probe.cpp', library, '-o', exe])
    with (a.output / (kind + '-render.csv')).open('w') as log:
        run([exe, root / 'pcm', 'baseline' if kind == 'before' else 'current'], stdout=log)
    if kind == 'after':
        inventory_exe = root / 'preset_capture'
        run(['c++', '-std=c++20', '-O2', '-I', inc, inc / 'tools/preset_capture.cpp', library, '-o', inventory_exe])
        run([inventory_exe, '--inventory', a.work / 'current-inventory.json', '--library', 'all'])
    return {'kind': kind, 'source_commit': REVISIONS[kind], 'library_path': str(library),
            'library_sha256': sha(library), 'executable_sha256': sha(exe),
            'source_sha256': {name: sha(inc / 'core' / name) for name in ('Engine.h', 'Engine.cpp', 'InstrumentBody.h', 'curated/Winds.inc')}}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    bindings = list(pool.map(capture, ('before', 'after')))
provenance = {'source_bindings': bindings, 'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
              'probe_sha256': sha(HERE / 'winds_probe.cpp'), 'reproduce_sha256': sha(HERE / 'reproduce.py'),
              'analysis_sha256': sha(HERE / 'analyze.py'),
              'current_inventory_sha256': sha(a.work / 'current-inventory.json'), 'commands': commands,
              'method': 'Unmodified full program vectors; only oversampling selected. Complete matching headers exported via git archive for both versions.',
              'raw_pcm_directory': str(a.work), 'rebuild': a.rebuild}
(a.output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
run(['python3', HERE / 'analyze.py', '--work', a.work, '--output', a.output, '--baseline-inventory', a.repo / 'docs/preset-evidence/2026.11.7/development/baseline-inventory.json'])
