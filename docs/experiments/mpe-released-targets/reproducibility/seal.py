#!/usr/bin/env python3
"""Refresh only the bundle integrity index after an explained, reviewed change.

Does not modify historical evidence or source pins. It cannot authenticate a
change; inspect the diff before using this maintainer tool.
"""
import hashlib
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True


def seal(root):
    files = {}
    for path in sorted(root.rglob('*')):
        if not path.is_file() or path.name == 'manifest.json' or '__pycache__' in path.parts:
            continue
        if path.is_symlink():
            raise ValueError('Do not seal symbolic links: ' + str(path))
        files[str(path.relative_to(root))] = {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    (root/'manifest.json').write_text(json.dumps({'schema': 1, 'files': files}, indent=2, sort_keys=True) + '\n')
    return len(files)


if __name__ == '__main__':
    print(seal(Path(__file__).resolve().parents[1]))
