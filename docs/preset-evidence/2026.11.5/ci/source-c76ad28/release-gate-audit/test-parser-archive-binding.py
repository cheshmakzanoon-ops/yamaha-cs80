#!/usr/bin/env python3
"""Small inert-fixture probe for A58's discovered parser-input binding gap.

No fixture is an actual GitHub artifact, native test result or validation run.
"""
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import unittest
import zipfile

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('a58_binding', ROOT / 'bind-parser-archives.py')
binding = importlib.util.module_from_spec(spec)
spec.loader.exec_module(binding)
SOURCE = 'a' * 40
RUN = 77
TARGET = 'linux-x86_64'


def zipped(files):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr(name, data)
    return stream.getvalue()


def api(raw, name, number):
    route = 'https://api.github.com/repos/' + binding.REPOSITORY + '/actions/artifacts/' + str(number)
    return {'id': number, 'name': name, 'url': route, 'archive_download_url': route + '/zip',
            'size_in_bytes': len(raw), 'digest': 'sha256:' + binding.digest(raw),
            'workflow_run': {'id': RUN, 'head_sha': SOURCE}, '_synthetic': True}


def fixture():
    native_files = {'ctest-results.xml': b'<synthetic-native/>',
                    'Testing/Temporary/LastTest.log': b'synthetic native output\n',
                    'ci-juce-provenance.txt': b'synthetic provenance\n'}
    validator_files = {'pluginval-version.txt': b'synthetic version\n',
                       'pluginval-headless.log': b'synthetic final console\n',
                       'pluginval-reports/cs80-headless': b'synthetic final report\n',
                       'pluginval-headless-core.log': b'synthetic child termination\n',
                       'pluginval-headless-attempt1.log': b'synthetic preserved failed attempt\n'}
    nr, vr = zipped(native_files), zipped(validator_files)
    rec = binding.record
    native = {'target': TARGET, 'archive': rec(nr, '/synthetic/native.zip'),
              'inputs': {'junit': rec(native_files['ctest-results.xml'], 'ctest-results.xml'),
                         'lasttest': rec(native_files['Testing/Temporary/LastTest.log'], 'Testing/Temporary/LastTest.log')},
              'juce_provenance': rec(native_files['ci-juce-provenance.txt'], 'ci-juce-provenance.txt')}
    validator = {'target': TARGET, 'archive': rec(vr, '/synthetic/validator.zip'),
                 'version': {'record': rec(validator_files['pluginval-version.txt'], 'pluginval-version.txt')},
                 'lanes': [{'lane': 'headless',
                            'console': rec(validator_files['pluginval-headless.log'], 'pluginval-headless.log'),
                            'report': rec(validator_files['pluginval-reports/cs80-headless'], 'pluginval-reports/cs80-headless'),
                            'core_log': rec(validator_files['pluginval-headless-core.log'], 'pluginval-headless-core.log'),
                            'preserved_failed_attempts': [{'member': 'pluginval-headless-attempt1.log', **rec(validator_files['pluginval-headless-attempt1.log'], 'pluginval-headless-attempt1.log')}]}]}
    observations = {'release_qualified': False, 'native': [native], 'validators': [validator], '_synthetic': True}
    return {'observations': observations, 'native_raw': nr, 'native_api': api(nr, binding.ARTIFACTS[TARGET][0], 1),
            'validator_raw': vr, 'validator_api': api(vr, binding.ARTIFACTS[TARGET][1], 2),
            'native_files': native_files, 'validator_files': validator_files}


def run(data):
    return binding.bind(data['observations'], TARGET, data['native_raw'], data['native_api'],
                        data['validator_raw'], data['validator_api'], SOURCE, RUN)


class BindingTests(unittest.TestCase):
    def setUp(self):
        self.data = fixture()

    def test_binds_both_archives_and_every_recorded_member(self):
        result = run(self.data)
        self.assertEqual([x['kind'] for x in result], ['native', 'validator'])
        self.assertEqual([len(x['parsed_member_records_bound']) for x in result], [3, 5])

    def test_rejects_stale_native_archive_digest(self):
        self.data['observations']['native'][0]['archive']['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'native parser archive: SHA-256 differs'):
            run(self.data)

    def test_rejects_stale_native_archive_size(self):
        self.data['observations']['native'][0]['archive']['bytes'] += 1
        with self.assertRaisesRegex(ValueError, 'native parser archive: byte count differs'):
            run(self.data)

    def test_rejects_substituted_validator_with_coherent_fresh_api(self):
        self.data['validator_files']['pluginval-headless.log'] = b'different synthetic console\n'
        self.data['validator_raw'] = zipped(self.data['validator_files'])
        self.data['validator_api'] = api(self.data['validator_raw'], binding.ARTIFACTS[TARGET][1], 2)
        with self.assertRaisesRegex(ValueError, 'validator parser archive:'):
            run(self.data)

    def test_rejects_stale_member_after_coherent_outer_zip_update(self):
        self.data['native_files']['ctest-results.xml'] = b'<changed-synthetic-native/>'
        self.data['native_raw'] = zipped(self.data['native_files'])
        self.data['native_api'] = api(self.data['native_raw'], binding.ARTIFACTS[TARGET][0], 1)
        self.data['observations']['native'][0]['archive'] = binding.record(self.data['native_raw'], '/synthetic/native.zip')
        with self.assertRaisesRegex(ValueError, 'junit: (byte count|SHA-256) differs'):
            run(self.data)

    def test_rejects_wrong_source_api(self):
        self.data['native_api']['workflow_run']['head_sha'] = 'b' * 40
        with self.assertRaisesRegex(ValueError, 'API source/run differs'):
            run(self.data)

    def test_rejects_wrong_artifact_name(self):
        self.data['native_api']['name'] = 'cs80-windows-test-evidence'
        with self.assertRaisesRegex(ValueError, 'Artifact name/route differs'):
            run(self.data)

    def test_rejects_duplicate_target_observations(self):
        self.data['observations']['native'].append(copy.deepcopy(self.data['observations']['native'][0]))
        with self.assertRaisesRegex(ValueError, 'missing or duplicate target'):
            run(self.data)

    def test_rejects_missing_archive_identity(self):
        self.data['observations']['native'][0]['archive'] = None
        with self.assertRaisesRegex(ValueError, 'native parser archive: missing record'):
            run(self.data)

    def test_rejects_changed_raw_parser_release_scope(self):
        self.data['observations']['release_qualified'] = True
        with self.assertRaisesRegex(ValueError, 'Raw parser release scope changed'):
            run(self.data)

    def test_rejects_missing_juce_member_record(self):
        del self.data['observations']['native'][0]['juce_provenance']
        with self.assertRaisesRegex(ValueError, 'Required JUCE provenance record absent'):
            run(self.data)

    def test_rejects_stale_preserved_attempt_member_digest(self):
        self.data['observations']['validators'][0]['lanes'][0]['preserved_failed_attempts'][0]['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'prior_attempt: SHA-256 differs'):
            run(self.data)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(BindingTests)
    with (ROOT / 'synthetic-binding-probe.log').open('x') as stream:
        result = unittest.TextTestRunner(stream=stream, verbosity=2).run(suite)
    out = {'scope': 'Synthetic inert input-identity fixtures only; no native or release acceptance.',
           'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
           'skipped': len(result.skipped), 'positive_methods': 1, 'negative_methods': 11,
           'release_qualified': False,
           'checker_sha256': hashlib.sha256((ROOT / 'bind-parser-archives.py').read_bytes()).hexdigest(),
           'probe_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    with (ROOT / 'synthetic-binding-probe-results.json').open('x') as stream:
        json.dump(out, stream, indent=2)
        stream.write('\n')
    print(json.dumps(out))
    raise SystemExit(0 if result.wasSuccessful() else 1)
