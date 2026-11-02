#!/usr/bin/env python3
"""Label a macOS installer from actual slices in all three shipped binaries."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

PRODUCT = 'CS-80 Reference'


def classify_slices(outputs: list[str]) -> str:
    if len(outputs) != 3:
        raise ValueError('Expected VST3, AU and Standalone architecture observations')
    inventories = []
    for output in outputs:
        words = output.split()
        slices = set(words)
        if len(words) != len(slices) or slices not in ({'arm64'}, {'x86_64'}, {'arm64', 'x86_64'}):
            raise ValueError('Missing, duplicate or unsupported macOS binary architecture')
        inventories.append(slices)
    if any(slices != inventories[0] for slices in inventories[1:]):
        raise ValueError('The three product binaries do not have identical architecture slices')
    return 'universal' if len(inventories[0]) == 2 else next(iter(inventories[0]))


def inspect_build(build: Path) -> str:
    if sys.platform != 'darwin':
        raise ValueError('Actual macOS architecture inspection requires native lipo')
    base = build / 'CS80Reference_artefacts/Release'
    bundles = (base / f'VST3/{PRODUCT}.vst3', base / f'AU/{PRODUCT}.component',
               base / f'Standalone/{PRODUCT}.app')
    binaries = [bundle / 'Contents/MacOS' / PRODUCT for bundle in bundles]
    outputs = []
    for binary in binaries:
        if not binary.is_file() or binary.stat().st_size == 0:
            raise ValueError(f'Missing or empty macOS product binary: {binary}')
        outputs.append(subprocess.check_output(['lipo', '-archs', str(binary)], text=True).strip())
    return classify_slices(outputs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(inspect_build(args.build_dir))
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f'macOS package architecture inspection failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
