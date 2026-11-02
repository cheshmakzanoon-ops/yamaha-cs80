#!/usr/bin/env python3
"""Install pinned historical verifier fixtures, never synthetic acceptance data."""
from __future__ import annotations

import argparse
import base64
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import stat
import subprocess
import zipfile

AUDIT_COMMIT = '0151b8699febc47f07c3b4d8cc9ebbf642692a5c'
AUDIT_PATH = 'docs/evidence/qualification-2026-11-01/full-engine-audit-41336194253'
AUDIT_BYTES = 227720
AUDIT_SHA = 'a76dd6ee33dcfa4f14be4223ecb06331330d19d90a0baf84147745cef2c88f19'
AUDIT_MEMBERS = (
    'audit/latency.csv', 'audit/alias.csv', 'audit/fx.csv', 'audit/phrase.csv',
    'audit/click-causality.csv', 'audit-summary.md', 'audit-summary.json',
    'audit-provenance.json', 'audit.complete', 'audit-run.log',
)
CPU_COMMIT = '3a85d66aee90f8cfaee69d875ee21997b61ad940'
CPU_PATH = 'docs/evidence/qualification-2026-11-02/phase-engine-simd-rejected/raw/before-1'
CPU_FILES = {
    'metadata.json': 'f3eb6e84c50439e3fca730b5bbefe8f1ccd4ea2cb3f81b6009b4e4165df2d7e1',
    'completed.json': '1e7d20b1afb2139fe82f8f8dfa0dfd6facac4e104e7d800398fd3e0db241985c',
    'results.csv': 'b5a21b58e9af9a085de26775853d6cb85793221d554356a92d7399f51dc657ed',
}


def git_blob(repo: Path, commit: str, path: str) -> bytes:
    return subprocess.check_output(['git', '-C', str(repo), 'show', f'{commit}:{path}'])


def verify_bytes(data: bytes, digest: str, size: int | None, label: str) -> None:
    if size is not None and len(data) != size:
        raise ValueError(f'{label}: unexpected byte count')
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError(f'{label}: SHA-256 mismatch')


def validate_zip(data: bytes) -> list[zipfile.ZipInfo]:
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        entries = archive.infolist()
        names = [entry.filename for entry in entries]
        if len(names) != len(AUDIT_MEMBERS) or set(names) != set(AUDIT_MEMBERS):
            raise ValueError('Audit archive member inventory changed')
        if len(set(names)) != len(names):
            raise ValueError('Duplicate audit member')
        total = 0
        for entry in entries:
            path = PurePosixPath(entry.filename)
            if (path.is_absolute() or '..' in path.parts or '\\' in entry.filename
                    or ':' in entry.filename or entry.is_dir()):
                raise ValueError('Unsafe audit member path')
            kind = stat.S_IFMT(entry.external_attr >> 16)
            if kind not in (0, stat.S_IFREG):
                raise ValueError('Audit member is not a regular file')
            if entry.flag_bits & 1:
                raise ValueError('Encrypted audit evidence is unsupported')
            if entry.file_size > 8 * 1024 * 1024:
                raise ValueError('Audit member expanded size exceeds bound')
            total += entry.file_size
        if total > 16 * 1024 * 1024:
            raise ValueError('Audit expanded size exceeds bound')
        if archive.testzip() is not None:
            raise ValueError('Audit archive CRC failure')
        return entries


def materialize(repo: Path, destination: Path) -> dict:
    repo = Path(repo).resolve()
    destination = Path(destination).absolute()
    if destination.exists() or destination.is_symlink():
        raise FileExistsError(f'Fixture destination already exists: {destination}')
    # Whitespace separates the stored base64 shards. No missing or invalid byte
    # is replaced; the exact decoded archive digest remains the authority.
    encoded = b''.join(git_blob(repo, AUDIT_COMMIT,
                               f'{AUDIT_PATH}/archive.b64.part{index:02d}')
                       for index in range(1, 9))
    raw = base64.b64decode(b''.join(encoded.split()), validate=True)
    verify_bytes(raw, AUDIT_SHA, AUDIT_BYTES, 'original audit')
    entries = validate_zip(raw)
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        audit_files = {entry.filename: archive.read(entry) for entry in entries}
    cpu_files = {}
    for name, digest in CPU_FILES.items():
        data = git_blob(repo, CPU_COMMIT, f'{CPU_PATH}/{name}')
        verify_bytes(data, digest, None, f'original CPU {name}')
        cpu_files[name] = data
    # All source bytes have been verified before any output is created. Never
    # overwrite an existing directory, even if it appears between the checks.
    destination.mkdir(parents=True, exist_ok=False)
    audit_root = destination / 'original-audit'
    cpu_root = destination / 'original-cpu'
    for name, data in audit_files.items():
        target = audit_root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    cpu_root.mkdir()
    for name, data in cpu_files.items():
        (cpu_root / name).write_bytes(data)
    receipt = {
        'schema_version': 1,
        'qualification': 'Historical verifier fixtures; not current numerical release acceptance',
        'audit_commit': AUDIT_COMMIT,
        'audit_sha256': hashlib.sha256(raw).hexdigest(),
        'cpu_commit': CPU_COMMIT,
        'cpu_sha256': {name: hashlib.sha256(data).hexdigest() for name, data in cpu_files.items()},
        'artifact_root': str(audit_root),
        'cpu_root': str(cpu_root),
    }
    (destination / 'fixture-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(materialize(args.repo, args.out), indent=2))


if __name__ == '__main__':
    main()
