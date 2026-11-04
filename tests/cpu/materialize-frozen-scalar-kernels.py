#!/usr/bin/env python3
"""Materialize a namespaced oracle from registered verbatim scalar-kernel blobs.

Only the cs80 namespace declaration changes. This script reads no live production
header and evaluates no numerical candidate. Local includes stay within output.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture-root', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'fixtures/exact_scalar_kernels')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    fixture = args.fixture_root.resolve()
    destination = args.output.resolve()
    if destination == fixture or fixture in destination.parents:
        raise RuntimeError('Generated files must not overwrite the frozen fixture')
    manifest = json.loads((fixture / 'manifest.json').read_text())
    destination.mkdir(parents=True, exist_ok=True)
    records = {}
    for registered, entry in manifest['files'].items():
        name = entry['generated_basename']
        if Path(name).name != name:
            raise RuntimeError('Frozen generated filename must be a basename')
        source = (fixture / 'raw' / name).read_bytes().replace(b'\r\n', b'\n')
        if hashlib.sha256(source).hexdigest() != entry['sha256']:
            raise RuntimeError('Frozen source hash mismatch: ' + registered)
        changed, count = re.subn(rb'(?m)^namespace cs80(?=[\s:{])',
                                 b'namespace cs80::scalar_kernel_frozen', source)
        if count != 1 or hashlib.sha256(changed).hexdigest() != entry['generated_sha256']:
            raise RuntimeError('Unexpected namespace transform: ' + registered)
        (destination / name).write_bytes(changed)
        records[name] = entry['generated_sha256']
    (destination / 'oracle-source-manifest.json').write_text(json.dumps({
        'baseline_commit': manifest['baseline_commit'], 'generated_sha256': records,
        'transform': 'One namespace declaration replacement; no live production dependency or arithmetic edit.'
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
