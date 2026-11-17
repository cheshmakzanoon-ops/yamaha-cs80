"""Draft GNU13.3 exact GCDA identity + bounded failure evidence. No execution."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat

VERSION = b'*33B'  # GCC13.3 marker independently present in retained production Engine.o.
MAX_FILE = 8 * 1024 * 1024
MAX_TOTAL = 32 * 1024 * 1024
MAX_ENTRIES = 128


def require(ok, message):
    if not ok: raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def absolute(value):
    path = PurePosixPath(value)
    require(path.is_absolute() and str(path) == str(value) and '..' not in path.parts,
            'canonical absolute POSIX path required')
    require('#' not in str(path) and '\x00' not in str(path), 'ambiguous compiler path rejected')
    return path


def option(argv, flag):
    require(argv.count(flag) == 1, 'one explicit ' + flag + ' required')
    i = argv.index(flag); require(i + 1 < len(argv), 'missing ' + flag + ' value')
    return argv[i + 1]


def make_contract(argv, cwd, source, engine_object, profile_root, instrumented_bytes, object_sha256):
    """Call BEFORE trainer execution, using recorded compiler argv/cwd and frozen object bytes."""
    cwd, source, engine_object, root = map(absolute, (cwd, source, engine_object, profile_root))
    argv = list(map(str, argv))
    require(sha(instrumented_bytes) == object_sha256, 'instrumented object hash differs from frozen build')
    require(instrumented_bytes[:6] == b'\x7fELF\x02\x01', 'expected little-endian ELF64 instrumented object')
    require(VERSION in instrumented_bytes, 'GNU13.3 gcov version marker absent')
    require(option(argv, '-c') == str(source), 'source invocation mismatch')
    require([x for x in argv if x.startswith('-fprofile-')] == ['-fprofile-generate=' + str(root)],
            'one exact generation-directory option required')
    output = option(argv, '-o'); output_path = PurePosixPath(output)
    require(str(output_path) == output and '..' not in output_path.parts and '#' not in output,
            'canonical unambiguous output spelling required')
    require((output_path if output_path.is_absolute() else cwd / output_path) == engine_object,
            'object invocation path mismatch')
    require(engine_object.name == 'Engine.cpp.o', 'exact CMake Engine object required')
    gcda_absolute = engine_object.with_suffix('.gcda')
    # GNU13.3 observed behavior differs by the ACTUAL -o spelling, not resolved output alone.
    # Preserve compiler bytes separately from canonical filesystem identity.
    # Absolute -o embeds root + '/' + absolute path, including the double slash.
    # Relative -o embeds root + '/' + absolute gcda path mangled /→#.
    # Never normalize an observed string to accept it: compare exact derived bytes first.
    relative = str(gcda_absolute)[1:] if output_path.is_absolute() else str(gcda_absolute).replace('/', '#')
    expected = root / relative
    embedded_expected = str(root) + "/" + (str(gcda_absolute) if output_path.is_absolute() else relative)
    embedded = [piece for piece in instrumented_bytes.split(b'\0') if piece.endswith(b'.gcda')]
    require(len(embedded) == 1, 'one embedded complete GCDA filename required')
    require(embedded[0] == embedded_expected.encode('ascii'), 'compiler-embedded GCDA path disagrees with invocation contract')
    return {'schema': 3, 'compiler_scope': 'GNU13.3.0 verified externally by pinned driver/frontend hashes',
            'argv': argv, 'cwd': str(cwd), 'source': str(source), 'engine_object': str(engine_object),
            'output_spelling': output, 'profile_root': str(root), 'relative_gcda': relative,
            'expected_gcda': str(expected), 'embedded_gcda': embedded_expected, 'object_sha256': object_sha256,
            'gcda_version_hex': VERSION.hex()}


def _regular_bytes(path, max_bytes):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1, 'regular single-link profile file required')
        require(before.st_size <= max_bytes, 'profile evidence byte limit')
        chunks = []; left = max_bytes + 1
        while left:
            part = os.read(fd, min(left, 65536))
            if not part: break
            chunks.append(part); left -= len(part)
        data = b''.join(chunks); after = os.fstat(fd)
        require(len(data) <= max_bytes and len(data) == before.st_size, 'profile read size mismatch')
        require((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) ==
                (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns),
                'profile changed during read')
        return data
    finally: os.close(fd)


def capture_unvalidated(root, evidence, contract):
    """Capture observed inventory/raw regular bytes BEFORE validation; persist rejection too.

    Caller guarantees the sole trainer has exited and the fresh owned tree is quiescent.
    Never follows symlinks. Unsafe/oversized entries are inventoried, not copied.
    Evidence must be a fresh non-overlapping directory owned by the caller.
    """
    root, evidence = Path(root), Path(evidence)
    require(root.is_absolute() and evidence.is_absolute(), 'absolute evidence/profile paths required')
    require(not evidence.exists() and not evidence.is_symlink(), 'fresh evidence directory required')
    require(not evidence.is_relative_to(root) and not root.is_relative_to(evidence), 'evidence/profile trees overlap')
    evidence.mkdir(parents=True)
    raw = evidence / 'raw'; raw.mkdir()
    report = {'schema': 3, 'profile_root': str(root), 'entries': [], 'bytes_copied': 0,
              'complete': True, 'accepted': False, 'validation': 'unvalidated', 'contract': contract}
    copied = {}
    try:
        require(str(root) == contract['profile_root'], 'wrong profile root')
        require(root.is_dir() and not root.is_symlink(), 'regular non-symlink profile root required')
        for directory, dirs, files in os.walk(root, topdown=True, followlinks=False):
            current = Path(directory)
            require(not current.is_symlink(), 'symlink directory changed during inventory')
            if len(current.relative_to(root).parts) > 32:
                report['complete'] = False; dirs[:] = []; continue
            for name in sorted(dirs + files):
                if len(report['entries']) >= MAX_ENTRIES:
                    raise ValueError('profile inventory entry limit')
                path = current / name; relative = str(path.relative_to(root))
                info = path.lstat()
                kind = ('symlink' if stat.S_ISLNK(info.st_mode) else 'directory' if stat.S_ISDIR(info.st_mode)
                        else 'file' if stat.S_ISREG(info.st_mode) else 'special')
                record = {'path': relative, 'kind': kind, 'bytes': info.st_size, 'links': info.st_nlink}
                report['entries'].append(record)
                if kind == 'symlink' and name in dirs: dirs.remove(name)
                if kind != 'file': continue
                try:
                    data = _regular_bytes(path, min(MAX_FILE, MAX_TOTAL - report['bytes_copied']))
                    destination = raw / relative; destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(data)
                    record['sha256'] = sha(data); record['copied'] = True
                    report['bytes_copied'] += len(data); copied[relative] = data
                except (OSError, ValueError) as error:
                    record['copied'] = False; record['error'] = repr(error); report['complete'] = False
    except (OSError, ValueError) as error:
        report['complete'] = False
        report['capture_error'] = repr(error)
    finally:
        (evidence / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def validate_evidence(evidence, contract):
    """Validate retained bytes only after successful training exit and CSV/work validation.

    This validates GCDA identity, NOT training completion. Caller must separately
    require the sole training command and exact training workload to have passed.
    """
    evidence = Path(evidence)
    report = json.loads((evidence / 'inventory.json').read_text())
    try:
        require(report['contract'] == contract and report['profile_root'] == contract['profile_root'],
                'evidence contract mismatch')
        expected = contract['relative_gcda']
        require(report['complete'], 'incomplete/unsafe profile evidence')
        files = [row for row in report['entries'] if row['kind'] != 'directory']
        require(len(files) == 1 and files[0]['kind'] == 'file' and files[0]['links'] == 1 and
                files[0]['path'] == expected and files[0].get('copied'), 'exact expected sole regular GCDA required')
        expected_parents = {str(p) for p in PurePosixPath(expected).parents if str(p) != '.'}
        require({row['path'] for row in report['entries'] if row['kind'] == 'directory'} == expected_parents,
                'unexpected profile directory inventory')
        data = _regular_bytes(evidence / 'raw' / expected, MAX_FILE)
        require(sha(data) == files[0]['sha256'] and len(data) == files[0]['bytes'], 'retained profile byte drift')
        require(len(data) > 16 and data[:4] == b'adcg' and data[4:8] == VERSION,
                'GNU13.3 little-endian GCDA header/size mismatch')
        report['accepted'] = True
        report['validation'] = 'gcda-identity-only'
        report['manifest'] = {expected: {'bytes': len(data), 'sha256': sha(data)}}
        return report['manifest']
    except (OSError, ValueError) as error:
        report['accepted'] = False
        report['validation'] = 'rejected'
        report['error'] = repr(error)
        raise
    finally:
        (evidence / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')


def capture_and_validate(root, evidence, contract):
    capture_unvalidated(root, evidence, contract)
    return validate_evidence(evidence, contract)


def complete_training_attempt(root, evidence, contract, train_once, validate_rows):
    """Integration seam: exactly one trainer call, evidence before CSV checks even on failure."""
    try:
        train_once()
    except BaseException as error:
        try:
            report = capture_unvalidated(root, evidence, contract)
            report['training_exit_success'] = False
            report['training_error'] = repr(error)
            (Path(evidence) / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')
        except BaseException as retention_error:
            error.add_note('Unvalidated profile retention also failed: ' + repr(retention_error))
        raise
    report = capture_unvalidated(root, evidence, contract)
    report['training_exit_success'] = True
    (Path(evidence) / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')
    validate_rows()
    return validate_evidence(evidence, contract)


def live_profile_manifest(root, contract):
    """Strict bounded live recheck. Caller captures failure evidence in its finalizer."""
    root = Path(root)
    require(str(root) == contract['profile_root'] and root.is_dir() and not root.is_symlink(),
            'exact regular live profile root required')
    files = []; directories = set(); count = 0
    for directory, dirs, names in os.walk(root, topdown=True, followlinks=False):
        current = Path(directory)
        require(not current.is_symlink() and len(current.relative_to(root).parts) <= 32, 'unsafe profile directory')
        for name in sorted(dirs + names):
            count += 1; require(count <= MAX_ENTRIES, 'profile inventory entry limit')
            path = current/name; info = path.lstat(); relative = str(path.relative_to(root))
            require(not stat.S_ISLNK(info.st_mode), 'profile symlink rejected')
            if stat.S_ISDIR(info.st_mode): directories.add(relative)
            else:
                require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1, 'regular single-link profile required')
                files.append(relative)
    expected = contract['relative_gcda']
    require(files == [expected], 'exact sole live GCDA required')
    require(directories == {str(p) for p in PurePosixPath(expected).parents if str(p) != '.'},
            'unexpected profile directories')
    data = _regular_bytes(root/expected, MAX_FILE)
    require(len(data)>16 and data[:4]==b'adcg' and data[4:8]==VERSION, 'GNU13.3 live GCDA header/size mismatch')
    return {expected: {'bytes':len(data),'sha256':sha(data)}}
