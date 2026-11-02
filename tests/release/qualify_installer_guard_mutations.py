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
import test_installer_evidence_workflow as workflow

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
    if not sys.platform.startswith('linux'):
        raise ValueError('Complete guard replay requires native Linux Debian fixture; no skipped tests may qualify')
    args.out.mkdir(parents=True, exist_ok=False)
    original_loader, original_controller = payload.load_module, native.controller
    original_workflow = workflow.WORKFLOW_PATH
    for label, case in [('payload', payload.InstalledDistributionDocsTests), ('native', native.NativeInstallerPayloadTests),
                        ('workflow', workflow.InstallerEvidenceWorkflowTests)]:
        result = execute(case, args.out / f'{label}-unmodified.log')
        if not result.wasSuccessful() or result.skipped:
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
                if result.testsRun != 16 or result.errors or result.skipped or not positive or len(result.failures) != failures:
                    raise RuntimeError(f'{name}: expected {failures} ordinary failures and passing positive')
                records.append({'mutant': name, 'ordinary_failures': len(result.failures), 'errors': 0, 'positive_passed': positive})
        controller_path = ROOT / 'scripts/qualify_native_installer.py'
        controller_source = controller_path.read_text()
        controller_mutants = [
            ('partial_install_cleanup', 'if uninstaller.is_file():',
             'if uninstaller.is_file() and primary_error is None:',
             ['test_partial_windows_install_is_cleaned_up', 'test_cleanup_failure_preserves_original_install_failure',
              'test_windows_binary_mismatch_is_rejected_and_cleaned', 'test_input_transformation_after_snapshot_is_not_silently_recaptured']),
            ('missing_uninstaller', 'elif primary_error is None:', 'elif False:',
             ['test_successful_windows_install_without_uninstaller_is_not_qualified']),
            ('silent_binary_recapture',
             'verify_installed_payload(platform, binary_inputs, build, expanded)',
             'verify_installed_payload(platform, capture_build_payload(platform, build), build, expanded)',
             ['test_input_transformation_after_snapshot_is_not_silently_recaptured']),
            ('trust_binary_manifest',
             'binary_receipt = verify_installed_payload(platform, binary_inputs, build, expanded)',
             "binary_receipt = {'verified_components': sorted(binary_inputs['components']), 'platform_limit': 'VST3 not installed'}",
             ['test_input_transformation_after_snapshot_is_not_silently_recaptured', 'test_windows_binary_mismatch_is_rejected_and_cleaned']),
            ('unbound_binary_receipt',
             "            'binary_verification_sha256': hashlib.sha256((out / 'binary-verification.json').read_bytes()).hexdigest(),\n",
             '', ['test_windows_receipt_hashes_actual_binary_verification']),
        ]
        for name, old, new, expected in controller_mutants:
            if controller_source.count(old) != 1:
                raise RuntimeError('Controller mutation anchor absent/ambiguous: ' + name)
            with tempfile.TemporaryDirectory(prefix='cs80-controller-mutant-') as temp:
                native.controller = imported(controller_source.replace(old, new), Path(temp) / 'mutant.py')
                result = execute(native.NativeInstallerPayloadTests, args.out / (name + '.log'))
                failed = [test._testMethodName for test, _ in result.failures]
                if result.testsRun != 11 or result.errors or result.skipped or sorted(failed) != sorted(expected):
                    raise RuntimeError(name + ': expected exact ordinary failures: ' + str(expected))
                records.append({'mutant': name, 'ordinary_failures': len(result.failures),
                                'failure_ids': failed, 'errors': 0, 'positive_passed': True})
        # Demonstrate that an import-successful runtime crash cannot masquerade
        # as a semantic kill. This probe is explicitly not a production defect count.
        old = '    binary_inputs = capture_build_payload(platform, build)'
        if controller_source.count(old) != 1:
            raise RuntimeError('Controller runtime probe anchor absent/ambiguous')
        with tempfile.TemporaryDirectory(prefix='cs80-controller-crash-') as temp:
            native.controller = imported(controller_source.replace(old, "    raise RuntimeError('deliberate unqualified crash')"), Path(temp) / 'mutant.py')
            result = execute(native.NativeInstallerPayloadTests, args.out / 'runtime-error-refused.log')
            if result.testsRun != 11 or not result.errors or result.wasSuccessful():
                raise RuntimeError('Runtime crash did not demonstrate harness refusal')
        workflow_source = original_workflow.read_text()
        workflow_mutants = [
            ('omit_binary_input_evidence', '            packages/native-installer-ci/binary-input-manifest.json\n', '',
             'test_frozen_binary_input_manifest_is_retained'),
            ('omit_binary_verification_evidence', '            packages/native-installer-ci/binary-verification.json\n', '',
             'test_binary_verification_receipt_is_retained'),
            ('success_only_evidence_upload',
             '      - name: Upload test and JUCE source evidence\n        if: always()',
             '      - name: Upload test and JUCE source evidence\n        if: success()',
             'test_native_evidence_uploads_run_even_after_failure'),
            ('drop_native_golden_evidence', '            build/ui-smoke/golden/*.png\n', '',
             'test_prior_test_notice_and_visual_evidence_is_preserved'),
        ]
        for name, old, new, expected in workflow_mutants:
            if workflow_source.count(old) != 3:
                raise RuntimeError('Workflow mutation must affect exactly three native steps: ' + name)
            with tempfile.TemporaryDirectory(prefix='cs80-evidence-workflow-mutant-') as temp:
                workflow.WORKFLOW_PATH = Path(temp) / 'build.yml'
                workflow.WORKFLOW_PATH.write_text(workflow_source.replace(old, new))
                result = execute(workflow.InstallerEvidenceWorkflowTests, args.out / (name + '.log'))
                failed = [test.id().split(' (', 1)[0].rsplit('.', 1)[-1] for test, _ in result.failures]
                if result.testsRun != 4 or result.errors or result.skipped or failed != [expected] * 3:
                    raise RuntimeError(name + ': expected three intended ordinary failures')
                records.append({'mutant': name, 'ordinary_failures': 3, 'failure_ids': failed,
                                'errors': 0, 'positive_passed': True})
    finally:
        payload.load_module, native.controller = original_loader, original_controller
        workflow.WORKFLOW_PATH = original_workflow
    for label, case in [('payload', payload.InstalledDistributionDocsTests), ('native', native.NativeInstallerPayloadTests),
                        ('workflow', workflow.InstallerEvidenceWorkflowTests)]:
        result = execute(case, args.out / f'{label}-restored.log')
        if not result.wasSuccessful() or result.skipped:
            raise RuntimeError('Restored control failed')
    report = {'scope': 'Synthetic payload/source-guard controls; native macOS/Windows execution is not established',
              'sources': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (source_path, controller_path, original_workflow, ROOT / "tests/release/test_installer_evidence_workflow.py")},
              'mutations': records, 'restored_original_passed': True,
              'runtime_error_control_refused_not_semantic_kill': True,
              'logs': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(args.out.glob('*.log'))}}
    (args.out / 'qualification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'pass': True, 'mutants': len(records), 'out': str(args.out)}))


if __name__ == '__main__':
    main()
