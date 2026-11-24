#!/usr/bin/env python3
"""Fixed reviewed A48 qualification entry point for macos-arm64; actual original ee0dabca Apple Silicon evidence only."""
from pathlib import Path
import hashlib
import importlib.util
import subprocess
import sys
import zipfile
sys.dont_write_bytecode = True
ENGINE_SHA256 = '0b881f00d480ef32d31f6866eb03a3aa31a5ae809de05bafa9ad0d816f3b4fb1'
FIXED = {'reviewed_actual_inputs': True,
 'target': 'macos-arm64',
 'source': 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed',
 'tree': '17115913cbb29157bd00539485bcc5f38b57219c',
 'run': 45397189410,
 'attempt': 1,
 'job': 138455976227,
 'pins': {'docs/preset-evidence/2026.11.7/ci/source-ee0dabca/macos-arm64/retrieval-receipt.json': '1c8999da31d092f9bdb435eb3f76790e1f7314f040a397cd3676776d26a5ea6c',
          'docs/preset-evidence/2026.11.7/ci/source-ee0dabca/macos-arm64/install-acceptance.json': '24ac1d60ac214fe0ba76e2f13e64b97186ddf08b6a2cb1064b5dbaea04d24549',
          'docs/preset-evidence/2026.11.7/ci/source-ee0dabca/macos-arm64/archive-payload-binding.json': '5d4470627917cd47b9cbaf9493568f2327a3695fa6ae632a69d72dbf76eb6f57',
          'docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/native-contract.json': '342a00c271d95f0ed53b6dde684869b1a541419d3a61f0b15fda3499a7d8d95c',
          'docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/observations.json': '3d8163916dabc11c52efc45e92626e42df3a04d17be0b84ada0ff353c5369186',
          'docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/parser-archive-binding.json': '0034e472eb625d3042d780b2b28c77c3cad7a8b29b6e671eefacba68285c5358',
          'docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/skip-review.json': 'de1aca2f99da1ef1513a85a5e917d4afa68ea8c02b192bd158ad3f62aea61d66'},
 'documents_sha256': '258c2233eeb0723c2ac66513245920176ea9290d18fde315261abd756113b35a',
 'nested_python_skips': 5,
 'ctest_seconds': '1646.06',
 'artifacts': {'native': [14923599291,
                          'cs80-macos-15-test-evidence',
                          86866,
                          '93cf1ac090abc9c437dbef5fa279a6f5baad9d2e1b9cb6c21e900ffd70eee55a',
                          'Upload test and JUCE source evidence'],
               'validator': [14923963817,
                             'cs80-pluginval-macos-15',
                             12720,
                             'a8fd5db8bca67d76d7ae64e4354ecfa4245301859457e40dae64838f39b977d0',
                             'Upload pluginval reports'],
               'install': [14923244833,
                           'cs80-macos-15-install-bundle',
                           14989471,
                           '0427c77f8982d54563933cc244de4e6cb6c2cd60269afd3e7d9aef25c21648b6',
                           'Upload native macOS install bundle']}}

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
