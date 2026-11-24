#!/usr/bin/env python3
"""Acquire one connector-returned ZIP and check its authentic API identity.

The short-lived download URL is received only as an argument and is neither
printed nor retained. API authenticity and original upload binding are separate
checks performed by the retained supporting-workflow inspector.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import urllib.request


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--url', required=True)
    p.add_argument('--api', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    a = p.parse_args()
    meta = json.loads(a.api.read_text())
    expected = meta['size_in_bytes']
    if not isinstance(expected, int) or not 0 < expected <= 256 * 1024 * 1024:
        raise ValueError('Invalid API size')
    if a.out.exists() or a.out.is_symlink():
        raise ValueError('Refusing to overwrite an existing archive')
    a.out.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        digest, size = hashlib.sha256(), 0
        with tempfile.NamedTemporaryFile(prefix='.acquiring-', dir=a.out.parent, delete=False) as stream:
            temporary = Path(stream.name)
            with urllib.request.urlopen(a.url, timeout=60) as response:
                while block := response.read(min(1024 * 1024, expected - size + 1)):
                    size += len(block)
                    if size > expected:
                        raise ValueError('Download exceeds API size')
                    digest.update(block)
                    stream.write(block)
        if size != expected or 'sha256:' + digest.hexdigest() != meta['digest']:
            raise ValueError('Download digest/size differs from API')
        os.link(temporary, a.out)
        print(json.dumps({'artifact_id': meta['id'], 'path': str(a.out),
                          'bytes': size, 'sha256': digest.hexdigest()}))
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        # urllib errors may contain a signed URL; do not echo exception text.
        print(type(e).__name__ + ': supporting artifact acquisition failed')
        raise SystemExit(1)
