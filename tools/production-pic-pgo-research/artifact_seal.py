"""Seal and restore only a generated Linux VST3 bundle; never build or load it.

Seals are content/mode manifests, not filesystem write protection. The caller
must keep sealed arms and ordinary binary manifests immutable and verify them
before/after host runs. All writers must be quiescent during these operations.
"""
import hashlib
import os
from pathlib import Path
import shutil
import stat
import tempfile

CANONICAL = Path('CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3')
BINARY = 'Contents/x86_64-linux/CS-80 Reference.so'
MODULEINFO = 'Contents/Resources/moduleinfo.json'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def checked_path(value, *, existing=True):
    raw = Path(value)
    require('..' not in raw.parts, 'parent traversal forbidden')
    path = Path(os.path.abspath(raw))
    for part in [*reversed(path.parents), path]:
        require(not part.is_symlink(), 'symlink path/ancestor forbidden')
    if existing:
        require(path.exists(), 'required path missing')
    return path


def manifest_bundle(bundle):
    root = checked_path(bundle)
    require(root.is_dir(), 'bundle must be a directory')
    result = {}

    def visit(path):
        info = path.lstat()
        mode = stat.S_IMODE(info.st_mode)
        require(not stat.S_ISLNK(info.st_mode), 'bundle symlink forbidden')
        require(not mode & 0o7000, 'special permission bits forbidden')
        key = '.' if path == root else path.relative_to(root).as_posix()
        if stat.S_ISDIR(info.st_mode):
            require(mode & 0o700 == 0o700, 'bundle directories require owner rwx')
            result[key] = {'kind': 'directory', 'mode': mode}
            for child in sorted(path.iterdir()):
                visit(child)
        else:
            require(stat.S_ISREG(info.st_mode), 'nonregular bundle entry forbidden')
            require(info.st_nlink == 1, 'hard-linked bundle file forbidden')
            require(mode & 0o400, 'bundle files require owner read')
            content = path.read_bytes()
            require(len(content) == info.st_size, 'bundle file changed while reading')
            result[key] = {'kind': 'file', 'mode': mode, 'bytes': len(content),
                           'sha256': hashlib.sha256(content).hexdigest()}
    visit(root)
    for name in (BINARY, MODULEINFO):
        require(name in result and result[name]['kind'] == 'file' and result[name]['bytes'] > 0,
                'required nonempty VST3 binary/moduleinfo missing')
    require(result[BINARY]['mode'] & 0o111, 'VST3 binary executable mode missing')
    return result


def verify_bundle(bundle, manifest):
    require(manifest_bundle(bundle) == manifest, 'sealed bundle content/mode/inventory changed')
    return manifest


def seal_bundle(source_bundle, sealed_dir):
    source = checked_path(source_bundle)
    destination = checked_path(sealed_dir, existing=False)
    require(not destination.exists(), 'sealed destination already exists')
    require(destination.parent.is_dir(), 'sealed destination parent must already exist')
    require(source not in destination.parents and destination not in source.parents,
            'source/seal paths overlap')
    manifest = manifest_bundle(source)
    # Copy symlinks as links, never follow a newly introduced link. Verification
    # rejects them. Concurrent mutation is unsupported and must stop the trial.
    shutil.copytree(source, destination, symlinks=True, copy_function=shutil.copy2)
    verify_bundle(source, manifest)
    verify_bundle(destination, manifest)
    return manifest


def restore_bundle(sealed_dir, canonical_dir, manifest, *, build_root):
    """Restore a sealed arm at the sole generated destination, without relinking.

    A fresh sibling container holds staging and the previous generated bundle.
    Validation failure rolls back the old canonical bundle when possible. Only
    that owned container is removed; no caller-supplied arbitrary tree is deleted.
    """
    build = checked_path(build_root)
    require(build.is_dir() and build != Path('/'), 'explicit non-root build directory required')
    canonical = checked_path(canonical_dir)
    require(canonical == build / CANONICAL, 'destination is not canonical generated VST3 bundle')
    sealed = checked_path(sealed_dir)
    require(sealed != canonical and sealed not in canonical.parents and canonical not in sealed.parents,
            'sealed/canonical paths overlap')
    verify_bundle(sealed, manifest)
    manifest_bundle(canonical)  # Require an existing valid generated bundle.
    container = Path(tempfile.mkdtemp(prefix='.cs80-bundle-restore-', dir=canonical.parent))
    staged, previous = container / 'staged', container / 'previous'
    moved = False
    safe_cleanup = True
    try:
        shutil.copytree(sealed, staged, symlinks=True, copy_function=shutil.copy2)
        verify_bundle(staged, manifest)
        verify_bundle(sealed, manifest)
        # Recheck ancestors immediately before the two same-filesystem renames.
        require(checked_path(canonical) == build / CANONICAL, 'canonical path changed')
        canonical.rename(previous)
        moved = True
        staged.rename(canonical)
        verify_bundle(canonical, manifest)
        verify_bundle(sealed, manifest)
        return manifest
    except BaseException:
        if moved:
            try:
                if canonical.exists():
                    canonical.rename(container / 'failed')
                previous.rename(canonical)
            except BaseException:
                safe_cleanup = False  # Preserve old bundle for manual recovery.
                raise
        raise
    finally:
        if safe_cleanup:
            shutil.rmtree(container)
