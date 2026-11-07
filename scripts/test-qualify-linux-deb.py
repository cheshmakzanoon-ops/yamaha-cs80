#!/usr/bin/env python3
"""Offline metadata/archive/package tests; synthetic payloads are never executed."""
from __future__ import annotations

import copy
from contextlib import redirect_stderr
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location('qualifier', Path(__file__).with_name('qualify-linux-deb.py'))
q = importlib.util.module_from_spec(spec)
spec.loader.exec_module(q)
SHA = 'a' * 40
RUN = '12345'
MERGE = 'c' * 40
BASE = 'd' * 40


def successful_run():
    repo = {'full_name': q.REPOSITORY, 'id': q.REPOSITORY_ID}
    return {'id': int(RUN), 'head_sha': SHA, 'repository': repo, 'head_repository': repo,
            'name': 'build', 'path': '.github/workflows/build.yml', 'event': 'push',
            'status': 'completed', 'conclusion': 'success', 'run_attempt': 1}


def artifact_record():
    return {'id': 6789, 'name': q.ARTIFACT_NAME, 'expired': False, 'size_in_bytes': 123,
            'digest': 'sha256:' + 'b' * 64,
            'workflow_run': {'id': int(RUN), 'head_sha': SHA, 'repository_id': q.REPOSITORY_ID,
                             'head_repository_id': q.REPOSITORY_ID}}


def write_evidence(directory):
    directory.mkdir()
    payload = {}
    for name in q.EVIDENCE_FILES:
        if name == 'verification.json':
            data = json.dumps({'package_uploaded': False, 'deb_sha256': 'a' * 64}).encode()
        elif name.endswith('.json'):
            data = b'{}\n'
        else:
            data = b'Scoped text evidence about a package.deb; not package bytes.\n'
        (directory / name).write_bytes(data)
        payload[name] = data
    return payload


def event_fixture(manual=False):
    repo = {'full_name': q.REPOSITORY, 'id': q.REPOSITORY_ID, 'fork': False}
    environment = {'GITHUB_REPOSITORY': q.REPOSITORY, 'GITHUB_REPOSITORY_ID': str(q.REPOSITORY_ID),
                   'GITHUB_EVENT_NAME': 'workflow_dispatch' if manual else 'pull_request',
                   'GITHUB_SHA': MERGE, 'GITHUB_REF': 'refs/heads/main' if manual else 'refs/pull/84/merge',
                   'GITHUB_RUN_ID': '98765', 'GITHUB_RUN_ATTEMPT': '1'}
    event = {'repository': copy.deepcopy(repo)}
    if manual:
        event.update(ref='refs/heads/main', inputs={'source_run_id': RUN, 'source_sha': SHA})
    else:
        event.update(action='synchronize', number=84, pull_request={
            'number': 84, 'state': 'open', 'head': {'sha': SHA, 'repo': copy.deepcopy(repo)},
            'base': {'sha': BASE, 'ref': 'main', 'repo': copy.deepcopy(repo)}})
    return environment, event


class EventAuthorizationTests(unittest.TestCase):
    def validate(self, environment=None, event=None, candidate=MERGE, parents=None, run=None, sha=None):
        default_environment, default_event = event_fixture()
        return q.validate_event(environment or default_environment, event or default_event, candidate,
                                [BASE, SHA] if parents is None else parents,
                                q.PR_SOURCE_RUN if run is None else run,
                                q.PR_SOURCE_COMMIT if sha is None else sha)

    def test_pr_context_records_synthetic_merge_separately_from_branch_head(self):
        for action in ('opened', 'synchronize', 'reopened'):
            environment, event = event_fixture(); event['action'] = action
            context = self.validate(environment, event)
            self.assertEqual(context['event'], 'pull_request')
        self.assertEqual(context['checkout_commit'], MERGE)
        self.assertEqual(context['pr_head_commit'], SHA)
        self.assertEqual(context['run_head_commit'], SHA)
        self.assertEqual(context['base_commit'], BASE)
        self.assertEqual(context['checkout_kind'], 'synthetic PR merge')
        self.assertNotEqual(context['checkout_commit'], context['run_head_commit'])

    def test_manual_context_binds_explicit_dispatch_inputs_and_checkout(self):
        environment, event = event_fixture(manual=True)
        for ref in ('main', 'refs/heads/main'):
            event['ref'] = ref
            context = self.validate(environment, event, parents=[], run=RUN, sha=SHA)
            self.assertEqual(context['run_head_commit'], MERGE)
            self.assertEqual(context['checkout_kind'], 'dispatched branch')
            self.assertNotIn('pr_head_commit', context)

    def test_unsupported_or_privileged_events_and_pr_actions_are_rejected(self):
        for name in ('pull_request_target', 'workflow_run', 'push', 'issue_comment', ''):
            environment, event = event_fixture()
            environment['GITHUB_EVENT_NAME'] = name
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'Unsupported qualification event'):
                self.validate(environment, event)
        for action in ('closed', 'edited', 'labeled', 'ready_for_review', ''):
            environment, event = event_fixture(); event['action'] = action
            with self.subTest(action=action), self.assertRaisesRegex(ValueError, 'Unsupported PR action'):
                self.validate(environment, event)

    def test_fork_or_mismatched_event_base_and_head_repository_are_rejected(self):
        for target in ('repository', 'base', 'head'):
            for field, value in (('full_name', 'fork/repo'), ('id', 123), ('fork', True), ('fork', None)):
                environment, event = event_fixture()
                repo = event['repository'] if target == 'repository' else event['pull_request'][target]['repo']
                repo[field] = value
                with self.subTest(target=target, field=field), self.assertRaises(ValueError):
                    self.validate(environment, event)

    def test_invalid_pr_base_ref_identity_state_or_merge_parents_are_rejected(self):
        for field, value in (('number', 85), ('state', 'closed')):
            environment, event = event_fixture(); event['pull_request'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.validate(environment, event)
        environment, event = event_fixture(); event['pull_request']['base']['ref'] = 'other'
        with self.assertRaisesRegex(ValueError, 'main-targeted'):
            self.validate(environment, event)
        for parents in ([], [SHA], [SHA, BASE], [BASE, MERGE], [BASE, SHA, MERGE]):
            with self.subTest(parents=parents), self.assertRaisesRegex(ValueError, 'merge parents'):
                self.validate(parents=parents)

    def test_unpinned_or_event_supplied_pr_source_inputs_are_rejected(self):
        for run, sha in ((RUN, q.PR_SOURCE_COMMIT), (q.PR_SOURCE_RUN, SHA)):
            with self.subTest(run=run, sha=sha), self.assertRaisesRegex(ValueError, 'fixed approved source inputs'):
                self.validate(run=run, sha=sha)
        for inputs in ({'source_run_id': q.PR_SOURCE_RUN, 'source_sha': q.PR_SOURCE_COMMIT}, [], 'untrusted'):
            environment, event = event_fixture(); event['inputs'] = inputs
            with self.subTest(inputs=inputs), self.assertRaisesRegex(ValueError, 'fixed approved source inputs'):
                self.validate(environment, event)

    def test_invalid_checkout_environment_and_run_identity_are_rejected(self):
        for key, value in (('GITHUB_REPOSITORY', 'fork/repo'), ('GITHUB_REPOSITORY_ID', '1'),
                           ('GITHUB_SHA', SHA), ('GITHUB_REF', 'refs/heads/main'),
                           ('GITHUB_RUN_ID', '1;bad'), ('GITHUB_RUN_ATTEMPT', '0')):
            environment, event = event_fixture(); environment[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.validate(environment, event)

    def test_manual_ref_or_input_mismatch_is_rejected(self):
        for change in ({'ref': 'other'}, {'ref': 'refs/tags/v1'}, {'inputs': {}},
                       {'inputs': {'source_run_id': q.PR_SOURCE_RUN, 'source_sha': SHA}}):
            environment, event = event_fixture(manual=True); event.update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.validate(environment, event, parents=[], run=RUN, sha=SHA)

    def test_current_run_metadata_is_bound_to_branch_head_not_merge_checkout(self):
        context = self.validate()
        repo = event_fixture()[1]['repository']
        metadata = {'id': 98765, 'run_attempt': 1, 'head_sha': SHA, 'event': 'pull_request',
                    'path': '.github/workflows/qualify-linux-deb.yml', 'repository': repo, 'head_repository': repo}
        q.validate_qualification_run(metadata, context)
        for change in ({'head_sha': MERGE}, {'id': 98766}, {'run_attempt': 2}, {'event': 'pull_request_target'},
                       {'path': '.github/workflows/other.yml'}, {'head_repository': dict(repo, fork=True)}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                q.validate_qualification_run(dict(metadata, **change), context)
        environment, event = event_fixture(manual=True)
        manual = self.validate(environment, event, parents=[], run=RUN, sha=SHA)
        q.validate_qualification_run(dict(metadata, head_sha=MERGE, event='workflow_dispatch'), manual)

    def test_pr_artifact_id_digest_size_and_source_pins_are_exact(self):
        self.assertEqual(q.PR_SOURCE_RUN, '42351562098')
        self.assertEqual(q.PR_SOURCE_COMMIT, '635ad6e8f24978040b02a4a10e646d7e3c592ce4')
        self.assertEqual(q.PR_SOURCE_COMMIT, q.PACKAGER_COMMIT)
        context = self.validate()
        artifact = {'id': 13566771073, 'size_in_bytes': 12413145,
                    'digest': 'sha256:01b00fc1e2c32c840507ed27052e9cb9cbf67cf3cc0149df920233be3bb3a29d'}
        q.validate_pr_artifact(artifact, context)
        for change in ({'id': 13566771074}, {'size_in_bytes': 1}, {'digest': 'sha256:' + '0' * 64}):
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, 'fixed approved source artifact'):
                q.validate_pr_artifact(dict(artifact, **change), context)
        # Manual sources are still subject to validate_run/select_artifact/read_bundle.
        q.validate_pr_artifact(artifact_record(), {'event': 'workflow_dispatch'})

    def test_main_rejects_unauthorized_event_before_network_or_materialization(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); environment, event = event_fixture()
            event['pull_request']['head']['repo']['fork'] = True
            event_path = root / 'event.json'; event_path.write_text(json.dumps(event))
            environment['GITHUB_EVENT_PATH'] = str(event_path)
            work = root / 'must-not-exist'
            argv = ['qualifier', '--source-run-id', q.PR_SOURCE_RUN, '--source-sha', q.PR_SOURCE_COMMIT,
                    '--work-dir', str(work), '--packager-root', str(root / 'packager')]
            headers = f'tree {SHA}\nparent {BASE}\nparent {SHA}\n\nSynthetic test commit\n'.encode()
            with patch.dict(os.environ, environment, clear=True), patch('sys.argv', argv), \
                    patch.object(q, 'run', side_effect=[MERGE.encode(), headers]), patch.object(q, 'api_json') as api:
                with self.assertRaisesRegex(ValueError, 'non-fork'):
                    q.main()
                api.assert_not_called()
            self.assertFalse(work.exists())


class EvidenceOutputTests(unittest.TestCase):
    def test_unexpected_package_product_archive_or_other_file_fails_closed(self):
        for name in ('package.deb', 'plugin.so', 'source.tar.gz', 'extra.txt'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temp:
                root = Path(temp); evidence = root / 'evidence'; qualified = root / 'qualified'
                write_evidence(evidence)
                (evidence / name).write_bytes(b'not eligible for output')
                with self.assertRaisesRegex(ValueError, 'Unexpected or missing evidence files'):
                    q.stage_qualified_evidence(evidence, qualified)
                self.assertFalse(qualified.exists())

    def test_binary_or_link_disguised_as_allowed_text_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); evidence = root / 'evidence'; qualified = root / 'qualified'
            write_evidence(evidence)
            path = evidence / 'standalone-elf.txt'
            path.write_bytes(b'\x7fELF\x02\x01\x01\x00binary canary')
            with self.assertRaisesRegex(ValueError, 'Binary evidence is forbidden'):
                q.stage_qualified_evidence(evidence, qualified)
            self.assertFalse(qualified.exists())
            path.unlink(); path.symlink_to(root / 'outside.txt')
            (root / 'outside.txt').write_text('not an eligible source')
            with self.assertRaisesRegex(ValueError, 'Evidence must be regular files'):
                q.stage_qualified_evidence(evidence, qualified)
            self.assertFalse(qualified.exists())

    def test_output_report_must_explicitly_state_package_not_uploaded(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); evidence = root / 'evidence'; qualified = root / 'qualified'
            write_evidence(evidence)
            for report in ({}, {'package_uploaded': True}):
                (evidence / 'verification.json').write_text(json.dumps(report))
                with self.assertRaisesRegex(ValueError, 'package_uploaded=false'):
                    q.stage_qualified_evidence(evidence, qualified)
                self.assertFalse(qualified.exists())


class NativeDiversionTests(unittest.TestCase):
    """Synthetic native metadata/filesystem; system ELF bytes are inspected, never run."""
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cs80-native-diversion-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.target = self.root / q.LOADER_TARGET.lstrip('/')
        self.target.parent.mkdir(parents=True)
        # Existing system ELF is only copied/read by readelf, never executed.
        self.target.write_bytes(Path('/bin/true').read_bytes())
        self.target.chmod(0o755)
        (self.root / 'usr/lib64').mkdir()
        (self.root / 'lib64').symlink_to('usr/lib64')
        self.alias = self.root / q.LOADER_ALIAS.lstrip('/')
        self.alias.symlink_to('../lib/x86_64-linux-gnu/ld-linux-x86-64.so.2')
        self.diverted = self.root / q.LOADER_DIVERSION.lstrip('/')
        self.products = self.root / 'products'
        self.evidence = self.root / 'evidence'; self.evidence.mkdir()
        self.distro = {'ID': 'ubuntu', 'VERSION_ID': '24.04'}
        self.owner = 'libc6'; self.truename = q.LOADER_DIVERSION
        self.installed = 'install ok installed\tamd64\t2.39-0ubuntu8.9'
        self.package_owner = f'libc6:amd64: {q.LOADER_TARGET}'
        self.md5 = q.hashlib.md5(self.target.read_bytes(), usedforsecurity=False).hexdigest()
        self.header = None
        self.interpreter = f'[Requesting program interpreter: {q.LOADER_ALIAS}]\n 0x0000000000000001 (NEEDED) Shared library: [ld-linux-x86-64.so.2]\n'
        self.uid = 0
        self.extra_mode = 0
        original_lstat = Path.lstat
        def fixture_lstat(path):
            value = original_lstat(path)
            if path == self.target:
                fields = list(value); fields[4] = self.uid; fields[0] |= self.extra_mode
                return os.stat_result(fields)
            return value
        for patcher in (patch.object(Path, 'lstat', fixture_lstat),
                        patch.object(q.platform, 'freedesktop_os_release', lambda: self.distro),
                        patch.object(q, 'run', self.native_read)):
            patcher.start(); self.addCleanup(patcher.stop)

    def native_read(self, *args, **kwargs):
        self.assertEqual(kwargs['env']['LC_ALL'], 'C')
        if args == ('dpkg-divert', '--listpackage', q.LOADER_ALIAS):
            return self.owner.encode()
        if args == ('dpkg-divert', '--truename', q.LOADER_ALIAS):
            return self.truename.encode()
        if args == ('dpkg-query', '-W', '-f=${Status}\t${Architecture}\t${Version}\n', 'libc6:amd64'):
            return self.installed.encode()
        if args == ('dpkg-query', '--search', '--', q.LOADER_TARGET):
            return self.package_owner.encode()
        if args == ('dpkg-query', '--control-show', 'libc6:amd64', 'md5sums'):
            return f'{self.md5}  {q.LOADER_TARGET.lstrip("/")}\n'.encode()
        if args == ('readelf', '-h', str(self.target)):
            return self.header.encode() if self.header is not None else subprocess.check_output(args, env=kwargs['env'])
        if args == ('readelf', '-l', '-d', str(self.products / q.STANDALONE)):
            return self.interpreter.encode()
        self.fail('Unexpected native probe: ' + repr(args))

    def verify(self):
        return q.verify_native_loader_diversion(self.products, self.root)

    def test_exact_native_diversion_and_package_file_are_verified(self):
        facts = self.verify()
        self.assertEqual(facts['canonical_loader'], q.LOADER_TARGET)
        self.assertEqual(facts['diversion_owner'], 'libc6')
        self.assertTrue(facts['installed_loader_md5_verified'])
        self.assertEqual(facts['loader_sha256'], q.digest(self.target.read_bytes()))
        self.assertEqual(facts['standalone_interpreter'], q.LOADER_ALIAS)
        self.assertFalse(facts['diverted_payload_present'])

    def test_exact_warning_retained_and_labeled_verified_not_warning_free(self):
        verifier = q.verify_native_loader_diversion
        result = subprocess.CompletedProcess([], 0, stdout='package built', stderr=q.LOADER_WARNING)
        with patch.object(q, 'verify_native_loader_diversion', lambda products: verifier(products, self.root)), redirect_stderr(io.StringIO()):
            report = q.check_packaging_result(result, self.evidence, self.products)
        self.assertEqual(report['status'], 'verified native diversion warning')
        self.assertEqual(report['warning_text'], q.LOADER_WARNING)
        self.assertEqual((self.evidence / 'packaging.stderr.txt').read_text(), q.LOADER_WARNING)
        self.assertTrue(report['native_diversion']['installed_loader_md5_verified'])
        with redirect_stderr(io.StringIO()), self.assertRaisesRegex(ValueError, 'warnings require review'):
            q.check_packaging_result(result, self.evidence)

    def test_unknown_mutated_duplicate_or_partial_warnings_are_fatal(self):
        for text in (q.LOADER_WARNING + 'another tool: warning: extra\n', q.LOADER_WARNING * 2,
                     q.LOADER_WARNING.replace('libc6 from', 'other from'),
                     q.LOADER_WARNING.replace('.usr-is-merged', '.other'),
                     q.LOADER_WARNING.splitlines()[0] + '\n', 'other tool: WARNING: unknown\n'):
            result = subprocess.CompletedProcess([], 0, stdout='', stderr=text)
            with self.subTest(text=text), patch.object(q, 'verify_native_loader_diversion') as verify, \
                    redirect_stderr(io.StringIO()), self.assertRaisesRegex(ValueError, 'warnings require review'):
                q.check_packaging_result(result, self.evidence, self.products)
            verify.assert_not_called()

    def test_nonzero_exit_remains_fatal_before_known_warning_verification(self):
        result = subprocess.CompletedProcess([], 1, stdout='', stderr=q.LOADER_WARNING)
        with patch.object(q, 'verify_native_loader_diversion') as verify, redirect_stderr(io.StringIO()), \
                self.assertRaisesRegex(ValueError, 'Debian packaging failed'):
            q.check_packaging_result(result, self.evidence, self.products)
        verify.assert_not_called()

    def test_warning_free_result_does_not_invoke_diversion_exception(self):
        with patch.object(q, 'verify_native_loader_diversion') as verify:
            report = q.check_packaging_result(subprocess.CompletedProcess([], 0, stdout='', stderr=''), self.evidence, self.products)
        verify.assert_not_called()
        self.assertEqual(report['status'], 'no dependency-scanner warnings')

    def test_other_or_missing_distro_version_is_rejected(self):
        for self.distro in ({}, {'ID': 'debian', 'VERSION_ID': '24.04'}, {'ID': 'ubuntu', 'VERSION_ID': '22.04'}):
            with self.subTest(distro=self.distro), self.assertRaisesRegex(ValueError, 'Ubuntu 24.04'):
                self.verify()

    def test_absent_mutated_diversion_owner_or_path_is_rejected(self):
        for self.owner, self.truename in (('', q.LOADER_DIVERSION), ('LOCAL', q.LOADER_DIVERSION),
                                        ('other', q.LOADER_DIVERSION), ('libc6', ''), ('libc6', q.LOADER_TARGET)):
            with self.subTest(owner=self.owner, path=self.truename), self.assertRaisesRegex(ValueError, 'owner/path mismatch'):
                self.verify()

    def test_uninstalled_wrong_arch_version_or_file_owner_is_rejected(self):
        good = self.installed
        for self.installed in ('', 'deinstall ok config-files\tamd64\t2.39-0ubuntu8.9',
                               'install ok installed\tarm64\t2.39-0ubuntu8.9', 'install ok installed\tamd64\t2.40-1'):
            with self.subTest(installed=self.installed), self.assertRaisesRegex(ValueError, 'libc6 installation'):
                self.verify()
        self.installed = good
        for self.package_owner in ('', f'other:amd64: {q.LOADER_TARGET}', f'libc6:amd64: {q.LOADER_ALIAS}'):
            with self.subTest(owner=self.package_owner), self.assertRaisesRegex(ValueError, 'owned by native libc6'):
                self.verify()

    def test_mismatched_missing_or_redirected_loader_target_is_rejected(self):
        self.alias.unlink(); self.alias.symlink_to('../lib/x86_64-linux-gnu/other')
        with self.assertRaises(FileNotFoundError):
            self.verify()
        other = self.target.with_name('other'); other.write_bytes(self.target.read_bytes())
        with self.assertRaisesRegex(ValueError, 'canonical target'):
            self.verify()
        self.alias.unlink(); self.alias.symlink_to('../lib/x86_64-linux-gnu/ld-linux-x86-64.so.2')
        self.target.unlink(); self.target.symlink_to('other')
        with self.assertRaisesRegex(ValueError, 'canonical target'):
            self.verify()

    def test_diverted_payload_bad_permissions_or_package_digest_is_rejected(self):
        self.diverted.write_text('unexpected')
        with self.assertRaisesRegex(ValueError, 'diverted loader payload'):
            self.verify()
        self.diverted.unlink(); self.diverted.symlink_to('absent')
        with self.assertRaisesRegex(ValueError, 'diverted loader payload'):
            self.verify()
        self.diverted.unlink()
        for self.uid, mode in ((1000, 0o755), (0, 0o777), (0, 0o644)):
            self.target.chmod(mode)
            with self.subTest(uid=self.uid, mode=mode), self.assertRaisesRegex(ValueError, 'root-owned regular'):
                self.verify()
        self.uid = 0; self.target.chmod(0o755)
        for self.extra_mode in (0o4000, 0o2000, 0o1000):
            with self.subTest(extra_mode=self.extra_mode), self.assertRaisesRegex(ValueError, 'root-owned regular'):
                self.verify()
        self.extra_mode = 0
        for self.md5 in ('', 'invalid', '0' * 32):
            with self.subTest(md5=self.md5), self.assertRaisesRegex(ValueError, 'installed libc6 file metadata'):
                self.verify()

    def test_wrong_loader_elf_or_product_interpreter_is_rejected(self):
        for self.header in ('Type: REL\nMachine: Advanced Micro Devices X86-64\n', 'Type: DYN\nMachine: AArch64\n'):
            with self.subTest(header=self.header), self.assertRaisesRegex(ValueError, 'dynamic ELF'):
                self.verify()
        self.header = None
        for self.interpreter in ('', '[Requesting program interpreter: /tmp/loader]\n[ld-linux-x86-64.so.2]',
                                  f'[Requesting program interpreter: {q.LOADER_ALIAS}]\n',
                                  f'[Requesting program interpreter: {q.LOADER_ALIAS}]\n 0x1d (RUNPATH) Library runpath: [ld-linux-x86-64.so.2]\n'):
            with self.subTest(interpreter=self.interpreter), self.assertRaisesRegex(ValueError, 'interpreter/dependency'):
                self.verify()
        self.target.write_bytes(b'not an ELF loader')
        self.md5 = q.hashlib.md5(self.target.read_bytes(), usedforsecurity=False).hexdigest()
        with self.assertRaisesRegex(ValueError, 'unexpected ELF identity'):
            self.verify()


class MetadataTests(unittest.TestCase):
    def test_valid_inputs_and_successful_same_repo_run(self):
        q.validate_inputs(q.REPOSITORY, RUN, SHA)
        q.validate_run(successful_run(), RUN, SHA)
        a = artifact_record()
        self.assertEqual(q.select_artifact({'total_count': 1, 'artifacts': [a]}, RUN, SHA), a)

    def test_reject_shell_metacharacters_or_partial_input(self):
        for repository, run, sha in [(q.REPOSITORY, '1; echo bad', SHA), (q.REPOSITORY, '-1', SHA),
                                      (q.REPOSITORY, '../1', SHA), (q.REPOSITORY, RUN, 'a' * 7),
                                      (q.REPOSITORY, RUN, SHA.upper()), ('other/repo', RUN, SHA)]:
            with self.subTest(values=(repository, run, sha)), self.assertRaises(ValueError):
                q.validate_inputs(repository, run, sha)

    def test_reject_unqualified_or_mismatched_runs(self):
        changes = [{'status': 'in_progress'}, {'conclusion': 'failure'}, {'event': 'pull_request'},
                   {'name': 'other'}, {'path': '.github/workflows/other.yml'}, {'run_attempt': 2},
                   {'id': 1}, {'head_sha': 'b' * 40},
                   {'head_repository': {'full_name': 'fork/repo', 'id': 1}},
                   {'repository': {'full_name': q.REPOSITORY, 'id': 1}}]
        for change in changes:
            with self.subTest(change=change), self.assertRaises(ValueError):
                q.validate_run(dict(successful_run(), **change), RUN, SHA)

    def test_reject_ambiguous_expired_wrong_or_oversized_artifacts(self):
        a = artifact_record()
        responses = [{'total_count': 2, 'artifacts': [a]}, {'total_count': 2, 'artifacts': [a, a]},
                     {'total_count': 0, 'artifacts': []}]
        for change in ({'expired': True}, {'digest': ''}, {'size_in_bytes': q.MAX_ZIP + 1},
                       {'id': '1; bad'}, {'workflow_run': {'id': int(RUN), 'head_sha': 'b' * 40}}):
            responses.append({'total_count': 1, 'artifacts': [dict(a, **change)]})
        for response in responses:
            with self.subTest(response=response), self.assertRaises(ValueError):
                q.select_artifact(response, RUN, SHA)

    def test_source_identity_allows_only_documentation_and_scoped_packaging(self):
        same = {'src/a.cpp': ('100644', 'blob', 'a'), 'CMakeLists.txt': ('100644', 'blob', 'b')}
        self.assertEqual(q.source_identity(same, same), [])
        candidate = dict(same, **{'docs/release/check.md': ('100644', 'blob', 'c'),
                                  'scripts/qualify-linux-deb.py': ('100644', 'blob', 'd')})
        self.assertEqual(len(q.source_identity(same, candidate)), 2)
        for name in ('src/a.cpp', 'CMakeLists.txt', 'cmake/check.cmake', '.github/workflows/build.yml', 'scripts/other.py'):
            with self.subTest(path=name), self.assertRaises(ValueError):
                q.source_identity(same, dict(same, **{name: ('100644', 'blob', 'changed')}))

    def test_corrected_packager_pin_rejects_baseline_dirty_or_wrong_blob(self):
        correct = [q.PACKAGER_COMMIT.encode(), q.PACKAGER_BLOB.encode(), b'']
        with patch.object(q, 'run', side_effect=correct):
            q.validate_packager(Path('/synthetic/packager'))
        for response in ([b'185e04c'], [q.PACKAGER_COMMIT.encode(), b'wrong'],
                         [q.PACKAGER_COMMIT.encode(), q.PACKAGER_BLOB.encode(), b' M installer/linux/make-deb.sh']):
            with patch.object(q, 'run', side_effect=response), self.assertRaises(ValueError):
                q.validate_packager(Path('/synthetic/packager'))

    def test_native_packaging_failure_and_warning_diagnostics_survive_safely(self):
        for code, stderr in [(1, 'dpkg-shlibdeps: error: no dependency information'),
                             (0, 'dpkg-shlibdeps: warning: missing symbol\n::error::untrusted text')]:
            with tempfile.TemporaryDirectory() as temp:
                evidence = Path(temp)
                result = subprocess.CompletedProcess(['packaging-fixture'], code, stdout='fixture output', stderr=stderr)
                log = io.StringIO()
                with redirect_stderr(log), self.assertRaises(ValueError):
                    q.check_packaging_result(result, evidence)
                self.assertIn('dpkg-shlibdeps', log.getvalue())
                self.assertNotIn('::error::', log.getvalue())
                self.assertEqual((evidence / 'packaging.stderr.txt').read_text(), stderr)
                self.assertFalse((evidence / 'qualified').exists())

    def test_truncated_or_duplicate_git_metadata_rejected(self):
        item = {'path': 'src/a.cpp', 'type': 'blob', 'mode': '100644', 'sha': SHA}
        for response in ({'truncated': True, 'tree': [item]}, {'truncated': False, 'tree': [item, item]}):
            with self.assertRaises(ValueError):
                q.github_tree(response)


class BundleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cs80-qualification-fixtures-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.payload = {q.STANDALONE: b'never execute this standalone fixture', q.PLUGIN: b'never execute this plugin fixture',
                        'LICENSE': b'project notice', 'licenses/JUCE-LICENSE.md': b'JUCE notice',
                        'SOURCE.txt': f'Source commit: {SHA}\nhttps://github.com/{q.REPOSITORY}/tree/{SHA}\n'.encode(),
                        'evidence/juce-provenance.txt': '\n'.join(f'{k}={v}' for k, v in {
                            'verification': 'PASS', 'source_clean': 'true', 'actual_sha': q.JUCE_SHA,
                            'expected_sha': q.JUCE_SHA, 'actual_version': '8.0.11', 'expected_version': '8.0.11',
                            'workflow_commit': SHA, 'workflow_run': RUN}.items()).encode()}
        self.number = 0

    def make_bundle(self, payload_change=None, manifest_change=None, tar_change=None,
                    embedded_change=None, sums_change=None, zip_duplicate=False):
        payload = dict(self.payload)
        if payload_change:
            payload_change(payload)
        manifest = {'schema_version': 1, 'product': q.PRODUCT, 'commit': SHA, 'workflow_run_id': RUN,
                    'platform': 'linux', 'architecture': 'x86_64', 'formats': ['VST3', 'Standalone'],
                    'juce_sha': q.JUCE_SHA, 'juce_version': '8.0.11',
                    'files': [{'path': name, 'type': 'file', 'mode': '0o755' if name in (q.STANDALONE, q.PLUGIN) else '0o644',
                               'size': len(data), 'sha256': q.digest(data)} for name, data in sorted(payload.items())]}
        if manifest_change:
            manifest_change(manifest)
        raw_manifest = (json.dumps(manifest) + '\n').encode()
        stem = f'cs80-reference-linux-x86_64-{SHA[:12]}'
        members = []
        for item in manifest['files']:
            m = tarfile.TarInfo(stem + '/' + item['path']); m.mode = int(item['mode'], 8)
            data = payload.get(item['path'], b'')
            m.size = len(data); members.append((m, data))
        embedded = copy.deepcopy(manifest)
        if embedded_change:
            embedded_change(embedded)
        data = json.dumps(embedded).encode()
        m = tarfile.TarInfo(stem + '/manifest.json'); m.size = len(data); m.mode = 0o644
        members.append((m, data))
        if tar_change:
            tar_change(members)
        compressed = io.BytesIO()
        with tarfile.open(fileobj=compressed, mode='w:gz') as tar:
            for member, data in members:
                tar.addfile(member, io.BytesIO(data))
        compressed = compressed.getvalue()
        sums = q.digest(raw_manifest) + '  ' + stem + '.manifest.json\n' + q.digest(compressed) + '  ' + stem + '.tar.gz\n'
        if sums_change:
            sums = sums_change(sums)
        self.number += 1
        path = self.root / f'bundle-{self.number}.zip'
        with zipfile.ZipFile(path, 'w') as archive:
            archive.writestr('SHA256SUMS', sums)
            archive.writestr(stem + '.manifest.json', raw_manifest)
            archive.writestr(stem + '.tar.gz', compressed)
            if zip_duplicate:
                archive.writestr('SHA256SUMS', sums)
        record = artifact_record()
        record.update(size_in_bytes=path.stat().st_size, digest='sha256:' + q.digest(path.read_bytes()))
        return path, record, manifest, payload

    def load(self, **kwargs):
        path, record, manifest, payload = self.make_bundle(**kwargs)
        return q.read_bundle(path, record, SHA, RUN)

    def test_verified_bundle_materializes_only_required_paths(self):
        manifest, payload = self.load()
        build = self.root / 'build'
        products = q.materialize(build, manifest, payload)
        self.assertEqual((products / q.PLUGIN).read_bytes(), self.payload[q.PLUGIN])
        self.assertEqual((products / q.STANDALONE).stat().st_mode & 0o777, 0o755)
        self.assertEqual((build / '_deps/juce-src/LICENSE.md').read_bytes(), b'JUCE notice')
        self.assertFalse((products / 'SOURCE.txt').exists())
        with self.assertRaises(ValueError):
            q.materialize(build, manifest, payload)

    def test_outer_size_and_digest_mismatch_rejected(self):
        path, record, _, _ = self.make_bundle()
        for changed in (dict(record, size_in_bytes=1), dict(record, digest='sha256:' + '0' * 64)):
            with self.assertRaises(ValueError):
                q.read_bundle(path, changed, SHA, RUN)

    def test_duplicate_outer_zip_member_rejected(self):
        with self.assertWarns(UserWarning), self.assertRaises(ValueError):
            self.load(zip_duplicate=True)

    def test_manifest_source_platform_and_juce_mismatches_rejected(self):
        for field, value in [('commit', 'b' * 40), ('workflow_run_id', '9'), ('platform', 'windows'),
                             ('architecture', 'arm64'), ('juce_sha', 'c' * 40)]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.load(manifest_change=lambda m: m.update({field: value}))

    def test_unsafe_manifest_paths_rejected(self):
        for path in ('../escape', '/absolute', 'a/../escape', 'a//b', 'a/./b', 'C:/drive', 'a\\b', 'a\ncommand'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.load(manifest_change=lambda m: m['files'][0].update(path=path))
        self.assertFalse((self.root.parent / 'escape').exists())

    def test_duplicate_or_linked_manifest_entries_rejected(self):
        for change in (lambda m: m['files'].append(dict(m['files'][0])),
                       lambda m: m['files'][0].update(type='symlink'),
                       lambda m: m['files'][0].update(mode='0o4755')):
            with self.assertRaises(ValueError):
                self.load(manifest_change=change)

    def test_duplicate_missing_and_unmanifested_tar_members_rejected(self):
        for change in (lambda members: members.append(members[0]), lambda members: members.pop(0),
                       lambda members: setattr(members[0][0], 'name', '../escape')):
            with self.assertRaises(ValueError):
                self.load(tar_change=change)

    def test_link_device_and_permission_tar_members_rejected(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.FIFOTYPE, tarfile.CHRTYPE):
            def change(members):
                m = members[0][0]; m.type = kind; m.linkname = '/outside'; m.size = 0
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                self.load(tar_change=change)
        with self.assertRaises(ValueError):
            self.load(tar_change=lambda members: setattr(members[0][0], 'mode', 0o4755))

    def test_payload_hash_checksums_and_embedded_manifest_mismatch_rejected(self):
        with self.assertRaises(ValueError):
            self.load(manifest_change=lambda m: m['files'][0].update(sha256='0' * 64))
        with self.assertRaises(ValueError):
            self.load(sums_change=lambda sums: '0' + sums[1:])
        with self.assertRaises(ValueError):
            self.load(embedded_change=lambda m: m.update(commit='b' * 40))

    def test_source_and_juce_provenance_mismatch_rejected(self):
        for name, data in [('SOURCE.txt', b'Source commit: wrong\n'),
                           ('evidence/juce-provenance.txt', self.payload['evidence/juce-provenance.txt'] + b'\nverification=PASS'),
                           ('evidence/juce-provenance.txt', self.payload['evidence/juce-provenance.txt'].replace(b'source_clean=true', b'source_clean=false'))]:
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.load(payload_change=lambda payload: payload.update({name: data}))

    def test_bounded_archive_and_member_sizes(self):
        with patch.object(q, 'MAX_TAR', 100), self.assertRaises(ValueError):
            self.load()
        with patch.object(q, 'MAX_FILE', 10), self.assertRaises(ValueError):
            self.load()


@unittest.skipUnless(shutil.which('dpkg-deb'), 'dpkg-deb required for inert archive tests')
class DebianInspectionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cs80-deb-inspection-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'; self.source.mkdir()
        for name in q.NOTICE_SOURCES.values():
            p = self.source / name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(name.encode())
        self.payload = {q.STANDALONE: b'inert standalone, not executable code',
                        q.PLUGIN: b'inert plugin, not executable code', 'licenses/JUCE-LICENSE.md': b'JUCE notice'}
        self.depends = ('libc6 (>= 2.38), libstdc++6 (>= 13.1), libasound2t64 | libasound2, '
                        'libfreetype6, libfontconfig1, libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1')
        self.stage = self.root / 'stage'; (self.stage / 'DEBIAN').mkdir(parents=True)
        files = {'usr/bin/cs80-reference': self.payload[q.STANDALONE],
                 'usr/lib/vst3/' + q.PLUGIN.removeprefix('VST3/'): self.payload[q.PLUGIN],
                 'usr/share/doc/cs80-reference/JUCE-LICENSE.md': b'JUCE notice'}
        files.update({'usr/share/doc/cs80-reference/' + name: (self.source / path).read_bytes()
                      for name, path in q.NOTICE_SOURCES.items()})
        for name, data in files.items():
            p = self.stage / name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(data)
            p.chmod(0o644 if name.startswith('usr/share/doc') else 0o755)

    def check(self):
        (self.stage / 'DEBIAN/control').write_text('Package: cs80-reference\nVersion: 1\nArchitecture: amd64\n'
                'Maintainer: Fixture <test@example.invalid>\nDescription: Inert fixture\nDepends: ' + self.depends + '\n')
        deb = self.root / 'package.deb'
        subprocess.run(['dpkg-deb', '--build', '--root-owner-group', str(self.stage), str(deb)], check=True, capture_output=True)
        return q.verify_deb(deb, self.payload, self.source, self.root / 'extracted')

    def test_valid_package_bytes_modes_notices_and_dependency_floors(self):
        depends, hashes = self.check()
        self.assertEqual(depends, self.depends)
        self.assertEqual(hashes['usr/bin/cs80-reference'], q.digest(self.payload[q.STANDALONE]))
        self.assertEqual((self.root / 'extracted/usr/bin/cs80-reference').stat().st_mode & 0o777, 0o755)

    def test_qualified_output_excludes_actual_deb_elf_and_archive_bytes(self):
        self.check()  # Build/inspect an actual .deb containing inert fixture bytes.
        elf = self.root / 'fixture.so'
        elf.write_bytes(b'\x7fELF\x02\x01\x01\x00never execute this binary canary')
        archive = self.root / 'source-bundle.zip'
        with zipfile.ZipFile(archive, 'w') as stream:
            stream.writestr('source.c', 'Never compile or distribute this archive fixture.\n')
        forbidden = [self.root / 'package.deb', elf, archive]
        evidence = self.root / 'evidence'; qualified = self.root / 'qualified'
        expected = write_evidence(evidence)

        q.stage_qualified_evidence(evidence, qualified)

        actual = {p.relative_to(qualified).as_posix(): p.read_bytes()
                  for p in qualified.rglob('*') if p.is_file()}
        self.assertEqual(actual, expected)  # Inspect actual staged files and bytes.
        self.assertEqual(set(actual), q.EVIDENCE_FILES)
        for path in forbidden:
            self.assertTrue(path.is_file())  # Inputs remain confined to runner temp.
            self.assertFalse((qualified / path.name).exists())
            for data in actual.values():
                self.assertNotIn(path.read_bytes(), data)
        self.assertFalse(json.loads(actual['verification.json'])['package_uploaded'])

    def test_missing_versioned_abi_floor_rejected(self):
        for package in ('libc6', 'libstdc++6'):
            original = self.depends
            self.depends = original.replace(package + (' (>= 2.38)' if package == 'libc6' else ' (>= 13.1)'), package)
            with self.subTest(package=package), self.assertRaises(ValueError):
                self.check()
            self.depends = original

    def test_explicit_dependency_name_must_match_not_just_a_substring(self):
        self.depends = self.depends.replace('libxcursor1', 'not-libxcursor1')
        with self.assertRaisesRegex(ValueError, 'Missing explicit dynamic-library dependency: libxcursor1'):
            self.check()

    def test_missing_notice_or_changed_payload_rejected(self):
        notice = self.stage / 'usr/share/doc/cs80-reference/JUCE-LICENSE.md'
        notice.unlink()
        with self.assertRaises(ValueError):
            self.check()
        notice.write_bytes(b'JUCE notice')
        (self.stage / 'usr/bin/cs80-reference').write_bytes(b'altered bytes')
        with self.assertRaises(ValueError):
            self.check()

    def test_control_script_symlink_or_lost_execute_bits_rejected(self):
        preinst = self.stage / 'DEBIAN/preinst'; preinst.write_text('#!/bin/sh\nexit 0\n'); preinst.chmod(0o755)
        with self.assertRaisesRegex(ValueError, 'Unexpected package control scripts'):
            self.check()
        preinst.unlink()
        binary = self.stage / 'usr/bin/cs80-reference'; binary.chmod(0o644)
        with self.assertRaisesRegex(ValueError, 'Packaged product execute bits changed'):
            self.check()
        binary.unlink(); binary.symlink_to('/external')
        with self.assertRaises(ValueError):
            self.check()


if __name__ == '__main__':
    unittest.main(verbosity=2)
