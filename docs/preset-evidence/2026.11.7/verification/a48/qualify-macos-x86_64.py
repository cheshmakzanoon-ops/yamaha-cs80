#!/usr/bin/env python3
"""Fixed reviewed A48 qualification entry point for macos-x86_64; prepared only until actual pins are reviewed."""
from pathlib import Path
import hashlib
import importlib.util
import subprocess
import sys
import zipfile
sys.dont_write_bytecode = True
ENGINE_SHA256 = '0b881f00d480ef32d31f6866eb03a3aa31a5ae809de05bafa9ad0d816f3b4fb1'
FIXED = {'reviewed_actual_inputs': False,
 'target': 'macos-x86_64',
 'source': 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed',
 'tree': '17115913cbb29157bd00539485bcc5f38b57219c',
 'run': 45397189410,
 'attempt': 1,
 'job': 138455976307}

def main():
    engine = Path(__file__).resolve().parent / 'macos-qualification.py'
    if hashlib.sha256(engine.read_bytes()).hexdigest() != ENGINE_SHA256:
        raise ValueError('Reviewed A48 macOS engine changed')
    spec = importlib.util.spec_from_file_location('a48_macos_qualification', engine)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.main(FIXED, Path(__file__).resolve())

if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print('Mac qualification refused: ' + str(error), file=sys.stderr)
        sys.exit(2)
