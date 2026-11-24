#!/usr/bin/env python3
"""Inert provenance fixtures; no historical or actual ee0 product is qualified."""
from __future__ import annotations
import copy
from datetime import datetime, timedelta, timezone
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
spec = importlib.util.spec_from_file_location('a46_originals', HERE / 'bind-windows-originals.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)
WORKSPACE = r'D:\a\yamaha-cs80\yamaha-cs80'


def zipped(members):
    raw = io.BytesIO()
    with zipfile.ZipFile(raw, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, payload in members.items():
            archive.writestr(name, payload)
    return raw.getvalue()


def pinned_fetch():
    digest = 'c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab'
    return dict(repository='Tracktion/pluginval', version='1.0.4', release_id=188928656, asset_id=210999198,
                asset_name='pluginval_Windows.zip', expected_size=2408590, actual_size=2408590,
                expected_sha256=digest, actual_sha256=digest, verified=True,
                attempts=[dict(number=1, source_url='https://api.github.com/repos/Tracktion/pluginval/releases/assets/210999198',
                               transfer_exit=0, reason='downloaded', bytes=2408590, final_host='release-assets.githubusercontent.com')])


class OriginalBindingTests(unittest.TestCase):
    rejected_mutations = 0

    @classmethod
    def setUpClass(cls):
        cls.workflow = subprocess.check_output(['git', '-C', str(REPO), 'show', checker.SOURCE + ':.github/workflows/build.yml']).decode()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='a46-original-inert-')
        self.root = Path(self.temp.name)
        self.manifest = self.build_fixture()
        self.inputs = self.root / 'inputs.json'
        self.write('inputs.json', self.manifest)

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name, value):
        raw = value if isinstance(value, bytes) else (json.dumps(value, indent=2) + '\n').encode()
        (self.root / name).write_bytes(raw)

    def read(self, name):
        return json.loads((self.root / name).read_bytes())

    def verify(self):
        return checker.verify(REPO, self.inputs)

    def rejects(self, function=None):
        with self.assertRaises((ValueError, KeyError, OSError)):
            (function or self.verify)()
        type(self).rejected_mutations += 1

    def change_job(self, label, change):
        job = self.read(label + '-api.json')
        change(job)
        self.write(label + '-api.json', job)
        jobs = self.read('jobs-api.json')
        jobs['jobs'] = [job if entry['id'] == job['id'] else entry for entry in jobs['jobs']]
        self.write('jobs-api.json', jobs)

    def replace_log(self, label, before, after):
        path = self.root / (label + '-job.log')
        raw = path.read_bytes()
        self.assertIn(before.encode(), raw)
        path.write_bytes(raw.replace(before.encode(), after.encode(), 1))

    def build_fixture(self):
        start = datetime(2026, 11, 24, 10, 0, tzinfo=timezone.utc)

        def iso(time):
            return time.isoformat().replace('+00:00', 'Z')

        def job(identity, name, names, offset):
            steps = [dict(name=name, number=index + 1, status='completed', conclusion='success',
                          started_at=iso(start + timedelta(seconds=offset + index * 10)),
                          completed_at=iso(start + timedelta(seconds=offset + index * 10 + 9)))
                     for index, name in enumerate(names)]
            return dict(id=identity, name=name, run_id=checker.RUN, run_attempt=1, head_sha=checker.SOURCE,
                        status='completed', conclusion='success', steps=steps,
                        url=f'https://api.github.com/repos/{checker.REPOSITORY}/actions/jobs/{identity}',
                        run_url=f'https://api.github.com/repos/{checker.REPOSITORY}/actions/runs/{checker.RUN}')

        producer = job(checker.PRODUCER, 'Windows / VST3 + Standalone', checker.PRODUCER_STEPS, 0)
        consumer = job(900000000001, 'pluginval / Windows VST3 (strictness 10)', checker.CONSUMER_STEPS, 100)
        repo = dict(id=12345, full_name=checker.REPOSITORY)
        run = dict(id=checker.RUN, head_sha=checker.SOURCE, run_attempt=1, repository=repo, head_repository=repo,
                   path='.github/workflows/build.yml', event='push', status='in_progress', conclusion=None)
        source = dict(expected_sha=checker.JUCE, actual_sha=checker.JUCE, expected_version='8.0.11', actual_version='8.0.11',
                      source_clean='true', verification='PASS', workflow_commit=checker.SOURCE, workflow_run=str(checker.RUN),
                      configured_source=WORKSPACE + r'\build\_deps\juce-src', expected_source=WORKSPACE + r'\build\_deps\juce-src', source_override='')
        native = zipped({'ctest-results.xml': b'INERT; native content deliberately outside this helper\n',
                         'Testing/Temporary/LastTest.log': b'INERT; native content deliberately outside this helper\n',
                         'ci-juce-provenance.txt': ''.join(key + '=' + value + '\n' for key, value in source.items()).encode()})
        reports, consoles = {}, {}
        selected = WORKSPACE + r'\vst3\CS-80 Reference.vst3'
        for lane, name, console, report, seed in checker.LANES:
            report_text = '\n'.join(['Random seed: ' + (seed or '0x1709'), 'Validation started', 'Strictness level: 10',
                                     'Num programs: 523', 'SUCCESS']) + '\n'
            console_text = 'Started validating: ' + selected + '\n' + report_text
            reports[report], reports[console], consoles[name] = report_text.encode(), console_text.encode(), console_text.splitlines()
        fetch = pinned_fetch()
        reports.update({'pluginval-download.json': json.dumps(fetch).encode(), 'pluginval-version.txt': b'pluginval - 1.0.4\n'})
        raw_archives = {'native': native, 'validator': zipped(reports),
                        'vst3': zipped({'CS-80 Reference.vst3/Contents/x86_64-win/CS-80 Reference.vst3': b'INERT NOT A NATIVE IMAGE'}),
                        'install': zipped({'inert.txt': b'INERT NOT AN INSTALLABLE PRODUCT'})}
        apis, artifact_items = {}, {}
        for index, (role, (name, upload_name)) in enumerate(checker.ARTIFACTS.items()):
            raw = raw_archives[role]
            owner = consumer if role == 'validator' else producer
            upload = checker.step(owner, upload_name)
            identity = 800000000001 + index
            url = f'https://api.github.com/repos/{checker.REPOSITORY}/actions/artifacts/{identity}'
            api = dict(id=identity, name=name, size_in_bytes=len(raw), digest='sha256:' + checker.sha(raw), expired=False,
                       expires_at='2099-01-01T00:00:00Z', created_at=upload['started_at'], updated_at=upload['started_at'],
                       workflow_run=dict(id=checker.RUN, head_sha=checker.SOURCE, repository_id=repo['id'], head_repository_id=repo['id']),
                       url=url, archive_download_url=url + '/zip')
            apis[role] = api
            self.write(role + '.zip', raw)
            self.write(role + '-api.json', api)
            artifact_items[role] = dict(archive=role + '.zip', api=role + '-api.json')

        def emit_step(entry, messages):
            time = checker.stamp(entry['started_at'])
            return ''.join(iso(time + timedelta(milliseconds=100 + index)) + ' ' + message + '\n'
                           for index, message in enumerate(messages))

        def log(owner, source_part):
            output = ''
            for entry in owner['steps']:
                name = entry['name']
                if name == 'Run actions/checkout@v5':
                    messages = ['##[group]Run actions/checkout@v5', '  clean: true', '##[endgroup]',
                                "Working directory is '" + WORKSPACE + "'", "Deleting the contents of '" + WORKSPACE + "'",
                                'Initialized empty Git repository in INERT/.git/',
                                '[command]git.exe checkout --progress --force -B main refs/remotes/origin/main', checker.SOURCE]
                elif name in [value[1] for value in checker.ARTIFACTS.values()]:
                    role = next(role for role, (_, step_name) in checker.ARTIFACTS.items() if step_name == name)
                    api = apis[role]
                    messages = ['##[group]Run actions/upload-artifact@v5', '  name: ' + api['name'],
                                *checker.UPLOAD_PATHS[role], '  overwrite: false', '##[endgroup]',
                                'Uploaded bytes ' + str(api['size_in_bytes']),
                                'SHA256 digest of uploaded artifact zip is ' + api['digest'].split(':')[1],
                                f'Artifact {api["name"]} has been successfully uploaded! Final size is {api["size_in_bytes"]} bytes. Artifact ID is {api["id"]}']
                elif name == 'Download Windows VST3':
                    api = apis['vst3']
                    messages = ['##[group]Run actions/download-artifact@v5', '  name: cs80-windows-vst3', '  path: vst3',
                                '  repository: ' + checker.REPOSITORY, '  run-id: ' + str(checker.RUN), '##[endgroup]',
                                f'- cs80-windows-vst3 (ID: {api["id"]}, Size: {api["size_in_bytes"]}, Expected Digest: {api["digest"]})',
                                'Starting download of artifact to: ' + WORKSPACE + '\\vst3',
                                'SHA256 digest of downloaded artifact is ' + api['digest'].split(':')[1],
                                'Artifact download completed successfully.']
                else:
                    commands = checker.run_commands(source_part, name, WORKSPACE)
                    messages = ['##[group]Run ' + commands[0], *['\x1b[36;1m' + line + '\x1b[0m' for line in commands],
                                'shell: C:\\Program Files\\PowerShell\\7\\pwsh.EXE -command "fixture"', '##[endgroup]']
                    if name == 'Verify configured JUCE source':
                        messages += ['-- Verified clean JUCE 8.0.11 at ' + checker.JUCE + '; evidence: INERT']
                    elif name == 'Test':
                        messages += ['100% tests passed, 0 tests failed out of 255']
                    elif name == 'Fetch pluginval 1.0.4':
                        messages += ['Verified official pluginval 1.0.4 Windows asset 210999198: 2408590 bytes, SHA256 ' + fetch['actual_sha256'],
                                     'pluginval - 1.0.4']
                    elif name in consoles:
                        messages += consoles[name]
                output += emit_step(entry, messages)
            return output.encode()

        native_part = self.workflow.split('\n  windows:\n', 1)[1].split('\n  pluginval-windows:\n', 1)[0]
        consumer_part = self.workflow.split('\n  pluginval-windows:\n', 1)[1]
        self.write('producer-job.log', log(producer, native_part))
        self.write('consumer-job.log', log(consumer, consumer_part))
        self.write('run-api.json', run)
        self.write('jobs-api.json', dict(jobs=[producer, consumer]))
        self.write('artifacts-api.json', dict(artifacts=list(apis.values())))
        self.write('producer-api.json', producer)
        self.write('consumer-api.json', consumer)
        self.write('inert-acquisition.json', dict(synthetic=True, authenticated=False))
        return dict(provenance=checker.CONTEXT, run_api='run-api.json', jobs_api='jobs-api.json', artifacts_api='artifacts-api.json',
                    producer_api='producer-api.json', consumer_api='consumer-api.json', producer_log='producer-job.log',
                    consumer_log='consumer-job.log', artifacts=artifact_items, acquisition_receipts=['inert-acquisition.json'])

    def test_complete_inert_original_binding_preserves_scope(self):
        result = self.verify()
        self.assertEqual(len(result['artifact_bindings']), 4)
        self.assertEqual(result['validators']['required_lanes'], 2)
        self.assertEqual(result['validators']['actual_invocations'], 2)
        self.assertEqual(result['validators']['validation_retries'], 0)
        self.assertIs(result['consumer_download_binding']['independent_of_compiler_sidecar_presence'], True)
        for key in ('api_authenticity_independently_established_by_this_offline_helper', 'native_results_accepted',
                    'platform_qualified', 'release_qualified'):
            self.assertIs(result[key], False)

    def test_failed_job_or_original_step_cannot_be_repaired_by_success_text(self):
        original = self.read('consumer-api.json')
        for kind in ('job', 'step'):
            self.change_job('consumer', lambda job: job.update(conclusion='failure') if kind == 'job'
                            else next(step for step in job['steps'] if step['name'] == 'Validate VST3 headless').update(conclusion='failure'))
            self.rejects()
            self.change_job('consumer', lambda job: (job.clear(), job.update(copy.deepcopy(original))))

    def test_wrong_run_source_attempt_and_collection_are_rejected(self):
        original = self.read('run-api.json')
        for key, value in (('head_sha', 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'), ('id', checker.RUN + 1), ('run_attempt', 2), ('event', 'workflow_dispatch')):
            bad = dict(original)
            bad[key] = value
            self.write('run-api.json', bad)
            self.rejects()
        self.write('run-api.json', original)
        collection = self.read('jobs-api.json')
        collection['jobs'].append(copy.deepcopy(collection['jobs'][1]))
        self.write('jobs-api.json', collection)
        self.rejects()

    def test_product_digest_receipt_and_selection_required_without_sidecars(self):
        original = (self.root / 'consumer-job.log').read_bytes()
        api = self.read('vst3-api.json')
        for before, after in ((f'ID: {api["id"]},', f'ID: {api["id"] + 1},'),
                              (f'Size: {api["size_in_bytes"]},', f'Size: {api["size_in_bytes"] + 1},'),
                              ('SHA256 digest of downloaded artifact is ' + api['digest'].split(':')[1], 'SHA256 digest of downloaded artifact is ' + '0' * 64),
                              ('Starting download of artifact to: ' + WORKSPACE + '\\vst3', 'Starting download of artifact to: ' + WORKSPACE + '\\other'),
                              ('Started validating: ' + WORKSPACE + '\\vst3\\CS-80 Reference.vst3', 'Started validating: ' + WORKSPACE + '\\vst3\\OTHER.vst3')):
            (self.root / 'consumer-job.log').write_bytes(original)
            self.replace_log('consumer', before, after)
            self.rejects()

    def test_changed_commands_and_post_success_crash_rejected(self):
        original = (self.root / 'consumer-job.log').read_bytes()
        for before, after in (('--strictness-level 10', '--strictness-level 9'),
                              ('if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }', 'Write-Output ignored'),
                              ('--random-seed 0x5037a0c', '--random-seed 0x5037a0d'),
                              ('--skip-gui-tests', '--random-seed 0x1709')):
            (self.root / 'consumer-job.log').write_bytes(original)
            self.replace_log('consumer', before, after)
            self.rejects()
        (self.root / 'consumer-job.log').write_bytes(original.replace(b'SUCCESS\n', b'SUCCESS\n2026-11-24T10:02:10.900Z ##[error]Process completed with exit code 3221225477.\n', 1))
        self.rejects()

    def test_archive_upload_identity_drift_is_rejected(self):
        path = self.root / 'vst3.zip'
        path.write_bytes(path.read_bytes() + b'CHANGED')
        self.rejects()

    def test_pinned_download_fields_and_retry_policy(self):
        checker.check_download_receipt(pinned_fetch())
        for key, value in (('version', '1.0.5'), ('release_id', 188928657), ('asset_id', 210999199),
                           ('expected_size', 1), ('actual_size', 1), ('expected_sha256', '0' * 64),
                           ('actual_sha256', '0' * 64), ('verified', False), ('repository', 'other/pluginval'),
                           ('asset_name', 'other.zip')):
            bad = pinned_fetch()
            bad[key] = value
            self.rejects(lambda: checker.check_download_receipt(bad))
        bad = pinned_fetch()
        bad['attempts'][0]['source_url'] = 'https://example.com/pluginval.zip'
        self.rejects(lambda: checker.check_download_receipt(bad))

    def test_unframed_runtime_and_source_failure_are_not_accepted(self):
        self.rejects(lambda: checker.log_rows(b'2026-11-24T10:00:00Z SUCCESS\nunframed runtime\n'))
        raw = (REPO / 'docs/preset-evidence/2026.11.7/ci/source-c5549f7d/windows/producer-job-api.json').read_bytes()
        original_failure = json.loads(raw)
        self.assertEqual(original_failure['conclusion'], 'failure')
        self.rejects(lambda: checker.check_job(original_failure, checker.PRODUCER, 'Windows / VST3 + Standalone', checker.PRODUCER_STEPS))
        self.assertEqual((REPO / 'docs/preset-evidence/2026.11.7/ci/source-c5549f7d/windows/producer-job-api.json').read_bytes(), raw)


if __name__ == '__main__':
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(OriginalBindingTests))
    print(json.dumps(dict(scope='inert original-binding fixtures; no product executed and no actual ee0 archive accepted',
                          methods=result.testsRun, failures=len(result.failures), errors=len(result.errors),
                          rejected_mutations=OriginalBindingTests.rejected_mutations,
                          actual_artifacts_checked=False, platform_qualified=False, release_qualified=False)))
    raise SystemExit(0 if result.wasSuccessful() else 1)
