#!/usr/bin/env python3
"""Archive retained evidence, never overwrite previously archived raw evidence.

Maintainer tool, not runtime reproduction. Run only after writers have stopped.
Historical absolute paths are provenance, not portable dependencies.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True

BASE = '2ade213bc4dec34b04352063a72e97dac4cc5667'
PRODUCTION = 'c0219bd0de8559d72fce4194a849414de5ac7e61'
FOCUSED = '300356958b3cde471f8dc14a7b505070a4cffc8c'
TEXT_EXTENSIONS = {'.cpp', '.h', '.inc', '.py', '.sh', '.log', '.json', '.txt', '.md', '.diff', '.map', '.exit', '.sha256', '.status', '.ninja', '.cmake'}
PENDING = {'profiles', 'full-suite', 'fault-controls', 'profiles-final', 'full-suite-final', 'strict-compatibility'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def record(path):
    return {'bytes': path.stat().st_size, 'sha256': sha(path)}


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def strings(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for k, v in value.items():
            yield from strings(k)
            yield from strings(v)
    elif isinstance(value, list):
        for v in value:
            yield from strings(v)


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def tree(repo, commit):
    entries = {}
    for raw in git(repo, 'ls-tree', '-rz', commit).split(b'\0'):
        if raw:
            meta, name = raw.decode().split('\t', 1)
            mode, kind, obj = meta.split()
            entries[name] = {'mode': mode, 'kind': kind, 'object': obj}
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--review', type=Path, required=True)
    parser.add_argument('--final-freeze', default=FOCUSED)
    parser.add_argument('--include-completed-late-runs', action='store_true')
    parser.add_argument('--completed-group', action='append', default=[], help='Stopped late evidence subdirectory to include during staging')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    repo = root.parents[2]
    external = {}
    origins = {}
    prior = root / 'evidence/archive-origins.json'
    if prior.exists():
        origins.update(json.loads(prior.read_text()))
    external_path = root / 'external-inputs.json'
    if external_path.exists():
        external.update(json.loads(external_path.read_text())['files'])
    referenced = set()
    indexed_external = {}
    for source_root, destination in [(args.source.resolve(), root/'evidence'), (args.review.resolve(), root/'evidence/independent-review')]:
        for source in sorted(source_root.rglob('*')):
            if not source.is_file():
                continue
            rel = source.relative_to(source_root)
            if not args.include_completed_late_runs and ((rel.parts[0] in PENDING and rel.parts[0] not in args.completed_group) or (rel.name.startswith('profiles') and rel.suffix in {'.log', '.status'})):
                continue
            private_tree = 'fault-controls' in rel.parts and any(p in {'src', 'tests'} for p in rel.parts[2:])
            include = source.suffix in TEXT_EXTENSIONS and not private_tree
            identity = record(source)
            if not include:
                reason = 'private mutation source tree' if private_tree else 'raw capture, executable, object, archive or other non-text artifact'
                entry = dict(identity, reason=reason)
                old = external.get(str(source))
                require(old is None or old == entry, 'External bytes changed since archive: ' + str(source))
                external[str(source)] = entry
                continue
            # Every retained evidence file must be actual UTF-8 text.
            source.read_text(encoding='utf-8')
            target = destination/rel
            if target.exists():
                require(record(target) == identity, 'Refusing to overwrite historical evidence: ' + str(target))
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
            origins[str(target.relative_to(root))] = dict(identity, original_path=str(source))
            if source.suffix == '.json':
                parsed = json.loads(source.read_text())
                def harvest(value):
                    if isinstance(value, dict):
                        for key, item in value.items():
                            if key.startswith('/workspace/') and isinstance(item, dict) and 'sha256' in item and ('bytes' in item or 'size' in item):
                                indexed_external[key] = {'sha256': item['sha256'], 'bytes': item.get('bytes', item.get('size'))}
                            harvest(item)
                    elif isinstance(value, list):
                        for item in value: harvest(item)
                harvest(parsed)
                for value in strings(parsed):
                    if value.startswith('/workspace/') and '\n' not in value:
                        referenced.add(value)
    # Supplement omitted artifacts with retained binary/library inputs referenced
    # by exact JSON paths. This records present local bytes, not executed provenance.
    for value in sorted(referenced):
        path = Path(value)
        if value in external or not path.is_file():
            continue
        with path.open('rb') as f:
            magic = f.read(8)
        if path.suffix not in {'.a', '.o', '.bin', '.f32'} and not (magic.startswith(b'\x7fELF') or magic == b'!<arch>\n'):
            continue
        external[value] = dict(record(path), reason='retained local binary/library referenced by archived JSON; execution identity is separately recorded in original provenance')
    for name, expected in indexed_external.items():
        path = Path(name)
        require(path.is_file() and record(path) == expected, 'Indexed external artifact differs: ' + name)
        if name not in external:
            external[name] = dict(expected, reason='retained local artifact with exact archived size/hash record')
    write(prior, origins)
    write(external_path, {'schema': 1, 'scope': 'Optional historical workspace inputs, not distributed or required for bundled verification. Private mutation trees are hash-indexed, not copied.', 'files': external})
    baseline, production, focused, final = (tree(repo, c) for c in [BASE, PRODUCTION, FOCUSED, args.final_freeze])
    runtime = lambda entries: {n: r for n, r in entries.items() if n.startswith('src/') and n != 'src/tools/ui_snapshot.cpp'}
    require(runtime(production) == runtime(focused) == runtime(final), 'Production source changed across test freezes')
    core_delta = {n for n in set(baseline)|set(production) if n.startswith('src/') and baseline.get(n) != production.get(n)}
    require(core_delta == {'src/core/Engine.cpp', 'src/core/Engine.h'}, 'Production delta exceeds bounded files')
    source_names = {n for n in final if n.startswith(('src/', 'tests/', 'cmake/')) or n in {'CMakeLists.txt', 'CMakePresets.json'}}
    source_inputs = {}
    for n in sorted(source_names):
        content = git(repo, 'show', args.final_freeze + ':' + n)
        path = repo/n
        require(path.read_bytes() == content, 'Working source differs from final pin: ' + n)
        source_inputs[n] = dict(final[n], bytes=len(content), sha256=hashlib.sha256(content).hexdigest())
    # Whole exact subtrees and selected files protect all unaffected product paths.
    preserved_prefixes = ['src/plugin/', 'src/experimental/', 'src/vendor/', 'src/reference/', 'assets/', 'artifacts/', 'installer/', 'scripts/', '.github/', 'docs/screenshots/']
    preserved = {n: r for n, r in baseline.items() if n.startswith(tuple(preserved_prefixes)) or (n.startswith('src/core/') and n not in {'src/core/Engine.cpp', 'src/core/Engine.h'})}
    actual_preserved = {n: r for n, r in final.items() if n.startswith(tuple(preserved_prefixes)) or (n.startswith('src/core/') and n not in {'src/core/Engine.cpp', 'src/core/Engine.h'})}
    require(preserved == actual_preserved, 'Preserved exact tree differs')
    for n, r in preserved.items():
        content = git(repo, 'show', BASE + ':' + n)
        require((repo/n).read_bytes() == content, 'Preserved working bytes changed: ' + n)
        r.update(bytes=len(content), sha256=hashlib.sha256(content).hexdigest())
    changes = {n: {'baseline': baseline.get(n), 'final': final.get(n)} for n in sorted(set(baseline)|set(final)) if baseline.get(n) != final.get(n)}
    write(root/'evidence/source-pins.json', {
        'baseline': BASE, 'production': PRODUCTION, 'focused_source_test_freeze': FOCUSED,
        'final_neighbor_test_freeze': args.final_freeze,
        'focused_to_final_changes': [n for n in sorted(set(focused)|set(final)) if focused.get(n) != final.get(n)],
        'baseline_to_final_changes': changes,
        'preserved_prefixes': preserved_prefixes,
        'preserved_tree_entries': preserved,
        'source_inputs': source_inputs,
        'qualification_status': 'awaiting final qualification' if not args.include_completed_late_runs else 'completed bounded qualification',
        'production_source_equal_across_freezes': True,
        'plan_clarification': {'commit': FOCUSED, 'before': 'active and physically held. pedalHeld is not a physical hold.', 'after': 'active with its accepted engine gate held (held=true); generated ARP gates use the same field', 'scope': 'Terminology clarification consistent with the original accepted KAS/ARP boundary; see recorded Git history.'},
        'note': 'Source pins identify original execution checkpoints separately from later neighboring-test corrections. Only Engine.cpp/h differ in production core; ui_snapshot.cpp is test dispatch.'
    })
    # PLAN, scripts and index files are covered; the manifest cannot hash itself.
    files = {str(p.relative_to(root)): record(p) for p in sorted(root.rglob('*')) if p.is_file() and p.name != 'manifest.json' and '__pycache__' not in p.parts}
    write(root/'manifest.json', {'schema': 1, 'files': files})
    print(json.dumps({'bundled_files': len(files), 'external_files': len(external), 'preserved_tree_entries': len(preserved)}))


if __name__ == '__main__':
    main()
