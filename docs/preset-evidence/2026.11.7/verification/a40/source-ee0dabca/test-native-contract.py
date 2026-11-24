#!/usr/bin/env python3
"""Inert acceptance/rejection fixtures; these are not native product executions."""
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[5]
spec = importlib.util.spec_from_file_location('a29_contract', HERE / 'verify-native-contract.py')
contract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)


class NativeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.parser, cls.binder = contract.load_helpers(REPO)
        cls.roster = contract.derive_contract(REPO)
        cls.target = 'macos-arm64'
        cls.names = cls.roster['expected_names'][cls.target]

    def setUp(self):
        root = Path('/tmp/cs80-a40-1cee5a')
        root.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=root)
        self.base = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)

    def fixture(self, names=None, outcome=None, footer=True, body=True, juce_source=None):
        names = list(names or self.names)
        outcome = dict(outcome or {})
        outcome.setdefault('cs80_filter_batch_cap_tests', 'formal_skipped')
        outcome.setdefault('cs80_stereo_pra_exact_tests', 'formal_skipped')
        counts = {kind: sum(outcome.get(n) == kind for n in names)
                  for kind in ('failed', 'formal_skipped', 'disabled')}
        root = ET.Element('testsuite', tests=str(len(names)), failures=str(counts['failed']),
                          skipped=str(counts['formal_skipped']), disabled=str(counts['disabled']), time='1')
        log = ['Start testing: synthetic fixture only']
        for index, name in enumerate(names, 1):
            result = outcome.get(name, 'passed')
            case = ET.SubElement(root, 'testcase', name=name, time='0.1',
                                 status='disabled' if result == 'disabled' else 'unknown' if result == 'unknown' else 'run')
            if result == 'failed':
                ET.SubElement(case, 'failure', message='synthetic failure')
            elif result in ('formal_skipped', 'disabled'):
                ET.SubElement(case, 'skipped', message='disabled' if result == 'disabled' else 'SKIP_RETURN_CODE=77')
            output = 'synthetic fixture only\n'
            if name == 'cs80_ci_bundle_verification_tests':
                output += "test_platform ... skipped 'synthetic platform condition'\nRan 2 tests in 0.01s\n\nOK (skipped=1)\n"
            if name == 'cs80_ui_smoke':
                output += 'UI SMOKE START: processor=1 bytes engine=1 bytes\n'
                if body:
                    output += 'UI SMOKE PHASE: instrument body host identity and recall\n'
                    output += 'UI SMOKE TIMING: phase=instrument body host identity and recall seconds=1 total_seconds=2\n'
                output += 'UI SMOKE TEST PASSED\n'
            ET.SubElement(case, 'system-out').text = output
            log += [f'{index}/{len(names)} Test: {name}', 'Output:', '--------', output.rstrip(),
                    '<end of output>', 'Test Failed.' if result == 'failed' else 'Test Passed.']
        if footer:
            log.append('End testing: synthetic fixture only')
        juce = dict(expected_sha='c352e2489000ffeb9e397051377d75eef2efe641',
                    actual_sha='c352e2489000ffeb9e397051377d75eef2efe641', expected_version='8.0.11',
                    actual_version='8.0.11', source_clean='true', verification='PASS',
                    workflow_commit=juce_source or contract.SOURCE, workflow_run=str(contract.CONTEXT['run_id']))
        path = self.base / 'native.zip'
        with zipfile.ZipFile(path, 'w') as archive:
            archive.writestr('ctest-results.xml', ET.tostring(root))
            archive.writestr('Testing/Temporary/LastTest.log', '\n'.join(log) + '\n')
            archive.writestr('ci-juce-provenance.txt', '\n'.join(k + '=' + v for k, v in juce.items()) + '\n')
        observed = self.parser.native(dict(target=self.target, archive=str(path)), self.base)
        raw = path.read_bytes()
        route = 'https://api.github.com/repos/' + self.binder.REPOSITORY + '/actions/artifacts/1'
        api = dict(id=1, name=self.binder.ARTIFACTS[self.target][0], url=route, archive_download_url=route + '/zip',
                   size_in_bytes=len(raw), digest='sha256:' + contract.sha(raw), expired=False,
                   expires_at='2100-01-01T00:00:00Z', workflow_run=dict(id=contract.CONTEXT['run_id'], head_sha=contract.SOURCE))
        review = dict(provenance=contract.CONTEXT, target=self.target, reviewed=True,
                      review_note='Synthetic rejection-test fixture only; no actual skip approval.',
                      formal_skips=observed['formal_skips'], nested_python_skips=observed['nested_python_skips'])
        (self.base / 'api.json').write_text(json.dumps(api))
        (self.base / 'review.json').write_text(json.dumps(review))
        return dict(target=self.target, archive='native.zip', api='api.json', skip_review='review.json')

    def verify(self, item):
        return contract.check_target(item, self.base, self.parser, self.binder, self.roster)

    def edit_json(self, name, edit):
        path = self.base / name
        data = json.loads(path.read_text())
        edit(data)
        path.write_text(json.dumps(data))

    def test_complete_content_and_separate_skip_counts(self):
        result = self.verify(self.fixture())
        self.assertEqual(result['counts']['registered'], 277)
        self.assertEqual(result['counts']['formal_skipped'], 2)
        self.assertEqual(result['nested_python_skips']['total'], 1)
        self.assertTrue(result['ui_body_identity_phase_complete'])
        self.assertFalse(result['native_results_accepted'])
        self.assertFalse(result['release_qualified'])

    def test_same_count_name_substitution_rejected(self):
        names = [n if n != 'cs80_piano_partial_decay_tests' else 'cs80_accidental_replacement_tests' for n in self.names]
        with self.assertRaisesRegex(ValueError, 'Full native roster differs'):
            self.verify(self.fixture(names=names))

    def test_failed_disabled_unknown_and_new_test_skip_rejected(self):
        for result in ('failed', 'disabled', 'unknown', 'formal_skipped'):
            with self.subTest(result=result), self.assertRaises(ValueError):
                self.verify(self.fixture(outcome={'cs80_body_noise_rate_tests': result}))

    def test_incomplete_lasttest_and_missing_body_phase_rejected(self):
        for changes in (dict(footer=False), dict(body=False)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.verify(self.fixture(**changes))

    def test_skip_review_must_match_formal_and_nested_observations(self):
        for edit in (lambda d: d.update(reviewed=False), lambda d: d.update(formal_skips=[]),
                     lambda d: d.update(nested_python_skips=dict(total=0, suites=[]))):
            item = self.fixture()
            self.edit_json('review.json', edit)
            with self.assertRaises(ValueError):
                self.verify(item)

    def test_wrong_source_run_hash_or_expired_api_rejected(self):
        edits = (lambda d: d['workflow_run'].update(head_sha=contract.PREVIOUS),
                 lambda d: d['workflow_run'].update(id=45366603800),
                 lambda d: d.update(digest='sha256:' + '0' * 64),
                 lambda d: d.update(expired=True),
                 lambda d: d.update(expires_at='2000-01-01T00:00:00Z'))
        for edit in edits:
            item = self.fixture()
            self.edit_json('api.json', edit)
            with self.assertRaises(ValueError):
                self.verify(item)

    def test_wrong_source_in_juce_evidence_rejected(self):
        with self.assertRaisesRegex(ValueError, 'JUCE/source provenance text differs'):
            self.verify(self.fixture(juce_source=contract.PREVIOUS))

    def test_junit_skip_cannot_hide_lasttest_failure(self):
        item = self.fixture()
        path = self.base / 'native.zip'
        with zipfile.ZipFile(path) as archive:
            members = {name: archive.read(name) for name in archive.namelist()}
        name = 'Testing/Temporary/LastTest.log'
        text = members[name].decode()
        text, count = re.subn(r'(Test: cs80_filter_batch_cap_tests\nOutput:.*?<end of output>\n)Test Passed\.',
                             r'\1Test Failed.', text, count=1, flags=re.S)
        self.assertEqual(count, 1)
        members[name] = text.encode()
        with zipfile.ZipFile(path, 'w') as archive:
            for member, raw in members.items():
                archive.writestr(member, raw)
        raw = path.read_bytes()
        self.edit_json('api.json', lambda d: d.update(size_in_bytes=len(raw), digest='sha256:' + contract.sha(raw)))
        with self.assertRaisesRegex(ValueError, 'LastTest contains a failed or unknown result'):
            self.verify(item)

    def test_context_rejects_previous_source_and_noninteger_attempt(self):
        for changes in (dict(source=contract.PREVIOUS), dict(attempt=True), dict(run_id=45366603800)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                contract.check_context(dict(contract.CONTEXT, **changes))

    def test_cli_requires_all_four_unless_explicit_target_and_preserves_output(self):
        item = self.fixture()
        manifest = self.base / 'inputs.json'
        manifest.write_text(json.dumps(dict(provenance=contract.CONTEXT, native=[item])))
        output = self.base / 'output.json'
        command = [sys.executable, str(HERE / 'verify-native-contract.py'), '--repo', str(REPO),
                   '--inputs', str(manifest), '--out', str(output)]
        rejected = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(rejected.returncode, 2, rejected.stderr)
        self.assertFalse(output.exists())
        accepted = subprocess.run(command + ['--target', self.target], capture_output=True, text=True)
        self.assertEqual(accepted.returncode, 0, accepted.stderr)
        before = output.read_bytes()
        refused = subprocess.run(command + ['--target', self.target], capture_output=True, text=True)
        self.assertEqual(refused.returncode, 2)
        self.assertEqual(before, output.read_bytes())


if __name__ == '__main__':
    unittest.main(verbosity=2)
