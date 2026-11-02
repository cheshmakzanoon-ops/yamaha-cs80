#!/usr/bin/env python3
"""Mutate actual guard source in temporary modules; never claim native audio proof."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
import test_installed_distribution_docs as payload
import test_native_installer_payload as native

MUTANTS = (
    ('source_commit', "require(manifest.get('source_commit') == expected_source,", 'require(True,', 1),
    ('source_tree', "require(manifest.get('source_tree') == tree,", 'require(True,', 1),
    ('source_url', "require(manifest.get('source_url') == source_url,", 'require(True,', 1),
    ('independent_notice_bytes', 'require(actual == expected[name],', 'require(True,', 2),
    ('manifest_hash', "require(row.get('sha256') == hashlib.sha256(actual).hexdigest(),", 'require(True,', 1),
    ('honest_scope', "require(manifest.get('scope') ==", "require(True or manifest.get('scope') ==", 1),
    ('extra_documents', "require(set(expected) | {'distribution-manifest.json'} <= observed and observed <= allowed,", 'require(True,', 1),
)


def execute(case, output):
    stream = io.StringIO()
    result = unittest.TextTestRunner(stream=stream, verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(case))
    output.write_text(stream.getvalue())
    return result


def imported(source, path):
    path.write_text(source)
    spec = importlib.util.spec_from_file_location('isolated_installer_guard_mutant', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    original_loader, original_controller = payload.load_module, native.controller
    for label, case in [('payload', payload.InstalledDistributionDocsTests), ('native', native.NativeInstallerPayloadTests)]:
        result = execute(case, args.out / f'{label}-unmodified.log')
        if not result.wasSuccessful():
            raise RuntimeError('Unmodified control failed before mutation')
    source_path = ROOT / 'scripts/verify_distribution_docs.py'
    source = source_path.read_text()
    records = []
    try:
        for name, old, new, failures in MUTANTS:
            if source.count(old) != 1:
                raise RuntimeError(f'Mutation anchor absent/ambiguous: {name}')
            with tempfile.TemporaryDirectory(prefix='cs80-installer-mutant-') as temp:
                module = imported(source.replace(old, new), Path(temp) / 'mutant.py')
                payload.load_module = lambda module_name, path: module
                result = execute(payload.InstalledDistributionDocsTests, args.out / f'{name}.log')
                positive = not any('test_exact_extracted_payload' in str(test) for test, _ in result.failures + result.errors)
                if result.testsRun != 16 or result.errors or not positive or len(result.failures) != failures:
                    raise RuntimeError(f'{name}: expected {failures} ordinary failures and passing positive')
                records.append({'mutant': name, 'ordinary_failures': len(result.failures), 'errors': 0, 'positive_passed': positive})
        controller_path = ROOT / 'scripts/qualify_native_installer.py'
        controller_source = controller_path.read_text()
        old = 'if uninstaller.is_file():'
        if controller_source.count(old) != 1:
            raise RuntimeError('Cleanup mutation anchor absent/ambiguous')
        with tempfile.TemporaryDirectory(prefix='cs80-cleanup-mutant-') as temp:
            native.controller = imported(controller_source.replace(old, 'if False:'), Path(temp) / 'mutant.py')
            result = execute(native.NativeInstallerPayloadTests, args.out / 'partial_install_cleanup.log')
            positive = not any('test_linux_actual_container' in str(test) for test, _ in result.failures + result.errors)
            if result.testsRun != 6 or result.errors or not positive or len(result.failures) != 2:
                raise RuntimeError('Cleanup mutant must produce two ordinary failures without breaking the positive')
            records.append({'mutant': 'partial_install_cleanup', 'ordinary_failures': 2, 'errors': 0, 'positive_passed': positive})
    finally:
        payload.load_module, native.controller = original_loader, original_controller
    for label, case in [('payload', payload.InstalledDistributionDocsTests), ('native', native.NativeInstallerPayloadTests)]:
        if not execute(case, args.out / f'{label}-restored.log').wasSuccessful():
            raise RuntimeError('Restored control failed')
    report = {'scope': 'Synthetic payload/source-guard controls; native macOS/Windows execution is not established',
              'sources': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (source_path, controller_path)},
              'mutations': records, 'restored_original_passed': True}
    (args.out / 'qualification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'pass': True, 'mutants': len(records), 'out': str(args.out)}))


if __name__ == '__main__':
    main()
