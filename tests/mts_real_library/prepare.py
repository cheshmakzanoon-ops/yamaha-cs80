from pathlib import Path
import subprocess, hashlib, argparse, json, shutil, sys
if sys.flags.optimize:
    raise SystemExit('REFUSE optimized Python: run without -O/PYTHONOPTIMIZE')

def require(condition, message):
    if not condition:
        raise SystemExit(message)

parser = argparse.ArgumentParser(description='Prepare isolated official MTS IPC qualification, without modifying production source')
parser.add_argument('--repo', required=True, type=Path)
parser.add_argument('--upstream', required=True, type=Path)
parser.add_argument('--library', type=Path)
parser.add_argument('--evidence', required=True, type=Path)
a = parser.parse_args()
r, u, e = a.repo.resolve(), a.upstream.resolve(), a.evidence.resolve()
harness = Path(__file__).resolve().parent
lib = (a.library or u/'libMTS/Linux/x86_64/libMTS.so').resolve()
for path in [r, u, e, harness, lib]:
    require('\n' not in str(path) and '\r' not in str(path), 'REFUSE newline-containing evidence paths')
for source in [r, u, harness]:
    require(e != source and source not in e.parents and e not in source.parents,
            'REFUSE evidence directory overlapping repository, upstream or harness: '+str(e))
require(not e.exists() or (e.is_dir() and not any(e.iterdir())), 'REFUSE nonempty evidence output: '+str(e))
require(subprocess.check_output(['git', '-C', str(u), 'rev-parse', 'HEAD'], text=True).strip() ==
        'f214739b8832e7f297cb9970d0c0efbf783f1462', 'Unexpected upstream commit')
pinned = {
    'Master/libMTSMaster.h': '124caf7eb967c68c221cd0632b8ead09d143456ec3f72be304007daf5ebfb186',
    'Master/libMTSMaster.cpp': '5bad1805125c86b8e710113ff367823fe3615774734b82709cb88cff69293db2',
    'Client/libMTSClient.h': '5c89f524b41f732b955e092c35522eb007dcb3a23db7631e9f9f75f4ffcf958f',
    'Client/libMTSClient.cpp': 'b854e7c2b657a660431839fb1c413522fa2877296404cee2a36db55d8813c4b3',
}
for relative, expected in pinned.items():
    require(hashlib.sha256((u/relative).read_bytes()).hexdigest() == expected, 'Unexpected upstream file: '+relative)
require(hashlib.sha256(lib.read_bytes()).hexdigest() == '03d23cb8d64ae246ceb416a7323cb230c5748c4b58d52a7c288d2c93ea3b7de5', 'Unexpected official library hash')
require((r/'src/vendor/mts-esp/Client/libMTSClient.cpp').read_bytes() == (u/'Client/libMTSClient.cpp').read_bytes(), 'Production vendor client differs from pinned upstream')
# Encode arbitrary filesystem path bytes as an unambiguous C++ narrow string.
# Printable ASCII uses JSON-compatible quote/backslash escaping; other bytes
# use exactly three octal digits, preventing escape continuation ambiguity.
literal = '"' + ''.join(json.dumps(chr(b))[1:-1] if 32 <= b < 127 else '\\%03o' % b
                        for b in bytes(lib)) + '"'
e.mkdir(parents=True, exist_ok=True)
for name in ['master.cpp', 'client.cpp', 'prepare.py', 'run.py', 'build.sh']:
    shutil.copyfile(harness/name, e/name)
(e/'config.json').write_text(json.dumps({'repo': str(r), 'upstream': str(u), 'library': str(lib)}, indent=2)+'\n')
for kind in ['Master', 'Client']:
    d = e/kind
    d.mkdir()
    for name in [f'libMTS{kind}.h', f'libMTS{kind}.cpp']:
        content = (u/kind/name).read_bytes()
        if name.endswith('.cpp'):
            old = b'"/usr/local/lib/libMTS.so"'
            require(content.count(old) == 1, 'Unexpected loader literal count')
            content = content.replace(old, literal.encode('ascii'))
        (d/name).write_bytes(content)
    with (e/f'{kind}-loader.patch').open('w') as f:
        result = subprocess.run(['diff', '-u', str(u/kind/f'libMTS{kind}.cpp'), str(d/f'libMTS{kind}.cpp')], stdout=f)
        require(result.returncode in [0, 1], 'Failed to record loader diff')
with (e/'source-hashes.txt').open('w') as f:
    for p in [lib, r/'src/plugin/MtsEspClient.cpp', r/'src/plugin/MtsEspClient.h', r/'src/core/Tuning.h',
              *(u/relative for relative in pinned), *e.glob('*/*')]:
        f.write(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p)+'\n')
