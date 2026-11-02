#!/usr/bin/env python3
"""Exact new namespace and read-only workflow registration, not capture evidence."""
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = json.loads((ROOT / 'tests/release/full-engine-audit-composed-default-v1-namespace.json').read_text())


class ComposedDefaultRegistrationTests(unittest.TestCase):
    def workflow(self):
        path = ROOT / CONTRACT['workflow_path']
        self.assertTrue(path.is_file(), 'separate composed-default V1 workflow not implemented')
        return path.read_text()

    def test_request_is_exact_frozen_composed_identity(self):
        raw = (ROOT / CONTRACT['request_path']).read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(), CONTRACT['request_sha256'])
        request = json.loads(raw)
        self.assertEqual((request['capture_family'], request['capture_version'], request['request_version']),
                         ('composed-default', 1, 2))
        self.assertEqual(request['source_commit'], CONTRACT['source_commit'])
        self.assertEqual(request['expected_src_tree'], CONTRACT['expected_src_tree'])
        self.assertEqual(sum(request['suite_files'].values()), 885)
        for relative, expected in CONTRACT['protected_files'].items():
            self.assertEqual(hashlib.sha256((ROOT / relative).read_bytes()).hexdigest(), expected, relative)

    def test_wrapper_keeps_exact_approved_guards_with_distinct_namespace(self):
        path = ROOT / CONTRACT['wrapper_path']
        self.assertTrue(path.is_file(), 'separate composed-default V1 wrapper not implemented')
        raw = path.read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(), CONTRACT['wrapper_sha256'],
                         'only the preregistered namespace translation of approved guards is admissible')
        self.assertIn(CONTRACT['registration_commit'].encode(), raw)
        self.assertNotIn(b'78da91e72c8780c0d60dfd079a88d6af2ea3b592', raw)
        self.assertIn(b'audit-composed-default-v1.complete', raw)
        self.assertNotIn(b'audit-v2.complete', raw)

    def test_workflow_has_distinct_branch_and_readonly_identity(self):
        text = self.workflow()
        self.assertEqual(text.count("      - '" + CONTRACT['branch'] + "'\n"), 1)
        self.assertIn('name: composed-default V1 full engine audit (capture, not numerical acceptance)\n', text)
        self.assertIn('permissions:\n  contents: read\n', text)
        self.assertNotRegex(text, r':\s*write\b')
        self.assertIn('group: full-engine-audit-composed-default-v1-${{ github.ref }}', text)
        self.assertIn('fetch-depth: 0', text)
        self.assertIn('timeout-minutes: 240', text)

    def test_workflow_has_complete_source_and_runner_triggers(self):
        match = re.search(r'    paths:\n((?:      - .+\n)+)', self.workflow())
        self.assertIsNotNone(match)
        self.assertEqual(match[1].strip().splitlines(), CONTRACT['workflow_paths'])

    def test_workflow_executes_all_guards_before_build(self):
        text = self.workflow()
        build = text.index('      - name: Build registered tracked core sources')
        commands = [
            'python tests/release/test_full_engine_audit_composed_default_v1_lock_timeout.py',
            'python tests/release/test_full_engine_audit_composed_default_v1.py',
            'python tests/release/test_full_engine_audit_composed_default_v1_completion_faults.py',
            'python ' + CONTRACT['registration_test'],
            'python scripts/full_engine_audit_gate_composed_default_v1.py --self-test',
            'python scripts/full_engine_audit_gate_composed_default_v1.py --preflight',
            'python tests/release/qualify_full_engine_audit_composed_default_v1_mutations.py --out artifacts/composed-default-v1-guard-mutations',
            'python ' + CONTRACT['registration_runner'] + ' --out artifacts/composed-default-v1-registration-controls',
        ]
        for command in commands:
            self.assertIn(command, text[:build], 'guard command must execute before expensive build: ' + command)

    def test_workflow_retains_original_build_and_complete_capture_command(self):
        text = self.workflow()
        self.assertIn('cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON', text)
        self.assertIn('cmake --build build-core --target cs80_engine_audit --parallel 2', text)
        request = json.loads((ROOT / CONTRACT['request_path']).read_text())
        self.assertIn(' '.join(request['command']) + ' 2>&1 | tee artifacts/audit-run.log', text)
        self.assertIn('run: python scripts/full_engine_audit_gate_composed_default_v1.py --finalize', text)

    def test_workflow_retains_both_provenance_layers_and_failures(self):
        text = self.workflow()
        self.assertIn('if: always()\n        uses: actions/upload-artifact@v5', text)
        self.assertIn('name: complete-composed-default-engine-audit-v1-${{ github.sha }}-${{ github.run_attempt }}', text)
        expected = ['artifacts/audit/', 'artifacts/audit-run.log', 'artifacts/audit.complete',
                    'artifacts/audit-provenance.json', 'artifacts/audit-summary.json', 'artifacts/audit-summary.md',
                    'artifacts/audit-composed-default-v1-provenance.json', 'artifacts/audit-composed-default-v1.complete',
                    'artifacts/composed-default-v1-guard-mutations/', 'artifacts/composed-default-v1-registration-controls/']
        body = text.split('          path: |\n')[1].split('          if-no-files-found:')[0]
        self.assertEqual(body.split(), expected)
        self.assertNotIn('artifacts/audit-v2', text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
