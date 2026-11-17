"""Pure cross-arm admission. No build, execution, linking or qualification claim."""
import hashlib
from pathlib import Path, PurePosixPath
import re
import stat

FILTER = 'src/core/Ig00156Batch.cpp'
KINDS = ('source', 'objects', 'compile_commands', 'dependencies', 'test_definitions')

def require(ok, why):
    if not ok:
        raise ValueError(why)

def hashes(values):
    require(isinstance(values, dict) and bool(values), 'nonempty identity map required')
    for key, value in values.items():
        require(isinstance(key, str) and bool(key) and isinstance(value, str)
                and re.fullmatch('[0-9a-f]{64}', value), 'invalid identity')

def changed(before, after):
    hashes(before); hashes(after)
    require(before.keys() == after.keys(), 'identity inventory changed')
    return {key for key in before if before[key] != after[key]}

def inventory(values):
    require(isinstance(values, list) and bool(values), 'nonempty frozen inventory required')
    require(all(isinstance(v, str) and bool(v) for v in values), 'invalid inventory entry')
    require(len(set(values)) == len(values), 'duplicate frozen inventory entry')
    return set(values)

def verify_pair(stock, candidate, *, filter_object, candidate_source, expected_inventories):
    """Expected inventories must be independently frozen from full source/link rosters.

    Cross-arm equality cannot establish that the same omitted input is absent.
    Objects are all compiler inputs, not final archives/executables. This checks
    evidence maps, NOT the actual linker or native-module bytes; those are separate
    mandatory gates. Caller must bind maps to bytes with capture_files or equivalent.
    """
    require(set(stock) == set(candidate) == set(expected_inventories) == set(KINDS),
            'exact evidence kinds required')
    require(isinstance(candidate_source, str) and re.fullmatch('[0-9a-f]{64}', candidate_source),
            'invalid frozen candidate digest')
    for kind in KINDS:
        expected = inventory(expected_inventories[kind])
        hashes(stock[kind]); hashes(candidate[kind])
        require(set(stock[kind]) == set(candidate[kind]) == expected,
                'evidence differs from frozen complete inventory: ' + kind)
    require(changed(stock['source'], candidate['source']) == {FILTER},
            'only filter source may change')
    require(candidate['source'][FILTER] == candidate_source, 'candidate source identity differs')
    require(changed(stock['objects'], candidate['objects']) == {filter_object},
            'only filter input object may change')
    for kind in ('compile_commands', 'dependencies', 'test_definitions'):
        require(not changed(stock[kind], candidate[kind]), kind + ' changed')
    return {'identity_admission': 'PASS', 'production_qualified': False}

def capture_files(root, relative_paths):
    """Hash exact already-frozen owned inputs; reject traversal, symlinks and missing files.

    This is a quiescent-root check, not protection against an adversarial concurrent
    filesystem writer. Orchestration must stop builds and freeze ownership first.
    """
    root = Path(root).absolute()
    inventory(relative_paths)
    for parent in (root, *root.parents):
        require(not parent.is_symlink(), 'symlink in root')
    require(root.is_dir(), 'root missing')
    result = {}
    for value in relative_paths:
        pure = PurePosixPath(value)
        require(not pure.is_absolute() and pure.as_posix() == value
                and all(part not in ('.', '..') for part in pure.parts)
                and '\\' not in value and value not in ('', '.'), 'unsafe relative path')
        path = root.joinpath(*pure.parts)
        for parent in (path, *path.parents):
            require(not parent.is_symlink(), 'symlink input')
            if parent == root:
                break
        require(path.is_file() and stat.S_ISREG(path.stat().st_mode), 'missing regular input')
        with path.open('rb') as f:
            result[value] = hashlib.file_digest(f, 'sha256').hexdigest()
    return result
