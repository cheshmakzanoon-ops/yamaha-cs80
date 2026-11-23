#!/usr/bin/env python3
"""Download a supplied artifact URI; never print or persist the temporary URI."""
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import urllib.request

target = Path(sys.argv[2])
try:
    api = json.loads(Path(sys.argv[3]).read_text())
    assert api['workflow_run']['id'] == 45246863788
    assert api['workflow_run']['head_sha'] == '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
    assert api['expired'] is False
    expected_size = api['size_in_bytes']
    expected_digest = api['digest'].removeprefix('sha256:')
    assert 0 < expected_size <= 512 * 1024 * 1024
    assert len(expected_digest) == 64
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        raw = target.read_bytes()
        assert len(raw) == expected_size and hashlib.sha256(raw).hexdigest() == expected_digest
        status = 'existing_matching_archive'
    else:
        handle, temporary = tempfile.mkstemp(prefix=target.name + '.', suffix='.part', dir=target.parent)
        total = 0
        digest = hashlib.sha256()
        with os.fdopen(handle, 'wb') as output, urllib.request.urlopen(sys.argv[1], timeout=45) as response:
            while block := response.read(1024 * 1024):
                total += len(block)
                assert total <= expected_size
                digest.update(block)
                output.write(block)
        assert total == expected_size and digest.hexdigest() == expected_digest
        try:
            os.link(temporary, target)
        except FileExistsError:
            raw = target.read_bytes()
            assert len(raw) == expected_size and hashlib.sha256(raw).hexdigest() == expected_digest
        Path(temporary).unlink()
        status = 'downloaded_and_verified'
    print(json.dumps({'status': status, 'path': str(target), 'artifact_id': api['id'],
                      'bytes': expected_size, 'sha256': expected_digest, 'api_size_digest_match': True}))
except Exception as error:
    # Exception messages and tracebacks could disclose a signed temporary URI.
    print(json.dumps({'status': 'download_failed', 'path': str(target), 'error_type': type(error).__name__}))
    sys.exit(1)
