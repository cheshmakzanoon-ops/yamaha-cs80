#!/usr/bin/env python3
"""Read-only Windows installer payload preflight; never load or run products.

Invoked synchronously by Inno Setup before it packages its [Files] sources.
Uses the same bundle containment and x64 PE-role checks as CI archives.
"""
import argparse
import importlib.util
import os
import stat
from pathlib import Path
import sys

sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location('cs80_packager', Path(__file__).with_name('package-ci-artifacts.py'))
packager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packager)


def reject_link(path):
    info = path.lstat()
    # Windows junctions/mount points are reparse points, not Python symlinks.
    # Inspect before descending so external trees are never scanned through one.
    if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
        raise ValueError(f'Linked/reparse installer payload is unsupported: {path}')


def inspect_tree(path):
    reject_link(path)
    if path.is_dir():
        with os.scandir(path) as entries:
            for entry in entries:
                inspect_tree(Path(entry.path))


def check_products(base):
    for folder in ('VST3', 'Standalone'):
        reject_link(base / folder)
    inspect_tree(base / 'VST3' / (packager.PRODUCT + '.vst3'))
    inspect_tree(base / 'Standalone' / (packager.PRODUCT + '.exe'))
    return packager.collect_products(base, 'windows', 'x86_64')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True,
                        help='CS80Reference_artefacts/Release directory passed to Inno Setup')
    args = parser.parse_args()
    try:
        entries = check_products(args.build_dir)
        print(f'Windows installer preflight passed: {len(entries)} entries; x64 VST3 DLL and Standalone EXE present.')
        return 0
    except (ValueError, OSError) as error:
        print(f'Windows installer preflight failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
