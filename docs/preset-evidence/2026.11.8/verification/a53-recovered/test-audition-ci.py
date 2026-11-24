#!/usr/bin/env python3
"""Synthetic identity and upload negative controls; no actual artifact is qualified."""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('binder', HERE / 'bind-audition-ci.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


def fixture():
    report = dict(accepted_archive_content=True, status='accepted_actual_archive_content',
        verifier_sha256=b.content.file_sha(HERE / 'verify-audition-archive.py'),
        library_revision=b.content.REVISION, repository=b.REPO, release_qualified=False,
        roster_sha256=b.content.ROSTER_SHA256, expected_inventory_sha256=b.INVENTORY_SHA256,
        expected=dict(**b.content.EXPECTED, folder_count=60, regular_files=321, checksums=320,
                      parameters=204, programs=523, stereo_frames=71385600, raw_audio_bytes=571084800),
        source_commit=b.SOURCE, source_tree=b.TREE, run_id=str(b.RUN), run_attempt='1',
        expected_ci_identity_checked=True, wav_count=256, selected_program_count=95,
        actual_audio_seconds=1487.2, actual_raw_audio_bytes=571084800, regular_files=321,
        checksum_count=320, actual_stereo_frames=71385600, nonfinite_samples=0, samples_over_full_scale=0,
        inventory_match=dict(programs=523, parameters=204, values=106692,
                             all_ordered_identities_identical=True, all_binary32_values_identical=True,
                             expected_sha256=b.INVENTORY_SHA256), zip_sha256='a'*64, zip_bytes=1234,
        capture_binary_sha256='b'*64, recovery_edition=b.content.RECOVERY_EDITION, independent_inventory_binding=dict(
            receipt_sha256='c'*64, inventory_sha256=b.INVENTORY_SHA256,
            source_commit=b.SOURCE, source_tree=b.TREE, independently_observed_capture_sha256='d'*64,
            inventory_normal_exit_code=0, original_build={'run_id':b.RUN,'run_attempt':b.ATTEMPT,'producer_job_id':b.JOB},
            producer_sha256=b.content.file_sha(HERE/'produce-independent-inventory.py'),
            verifier_sha256=b.content.file_sha(HERE/'verify-audition-archive.py'),
            execution={'kind':'local_recovery','platform':'Linux-fixture','python_version':'3.12'},
            native_capture_binary_hash_equality_expected=False))
    run = dict(id=b.RUN, run_attempt=1, head_sha=b.SOURCE, repository={'full_name':b.REPO,'id':1436742091},workflow_id=399616500)
    names = ['Run actions/checkout@v5','Configure','Verify configured JUCE source','Build','Test','Capture curated preset auditions','Upload curated preset auditions']
    steps = [dict(name=name, number=i, status='completed', conclusion='success',
                  started_at=f'2026-11-24T10:0{i}:00Z', completed_at=f'2026-11-24T10:0{i}:50Z') for i,name in enumerate(names,1)]
    job = dict(id=b.JOB, run_id=b.RUN, run_attempt=1, head_sha=b.SOURCE, name='Linux / VST3 + Standalone',
               status='completed', conclusion='success', completed_at='2026-11-24T10:08:00Z', steps=steps)
    name = 'cs80-curated-preset-audition-' + b.SOURCE
    artifact = dict(id=12345, name=name, expired=False, workflow_run={'id':b.RUN,'head_sha':b.SOURCE,'repository_id':1436742091,'head_repository_id':1436742091},
        digest='sha256:'+'a'*64, size_in_bytes=1234, url=f'https://api.github.com/repos/{b.REPO}/actions/artifacts/12345',
        created_at='2026-11-24T10:07:41Z', expires_at='2027-02-01T00:00:00Z')
    log = '\n'.join([
        '2026-11-24T10:06:00.001Z python3 scripts/capture-factory-auditions.py --capture build/cs80_preset_capture --out preset-audition',
        '2026-11-24T10:07:40.010Z SHA256 digest of uploaded artifact zip is '+'a'*64,
        f'2026-11-24T10:07:40.020Z Artifact {name} has been successfully uploaded! Final size is 1234 bytes. Artifact ID is 12345',
        f'2026-11-24T10:07:40.030Z Artifact download URL: https://github.com/{b.REPO}/actions/runs/{b.RUN}/artifacts/12345',
        '2026-11-24T10:06:00.002Z shell: /usr/bin/bash -e {0}'])
    return [report, artifact, run, job, log]


class BindingTests(unittest.TestCase):
    def test_synthetic_control_has_bounded_claims(self):
        result = b.bind(*fixture())
        self.assertFalse(result['release_qualified'])
        self.assertFalse(result['capture_binary_bytes_independently_available'])
        self.assertEqual(result['wav_count'],256)
        self.assertEqual(result['expected_inventory_sha256'], b.INVENTORY_SHA256)

    def test_content_mismatches(self):
        for key,value in [('accepted_archive_content',False),('verifier_sha256','0'*64),('source_commit','0'*40),
                          ('status','prepared_pending_actual_archive'),('library_revision','2026.11.6'),
                          ('repository','other/repository'),('release_qualified',True),
                          ('roster_sha256','0'*64),('expected_inventory_sha256','0'*64),
                          ('source_tree','0'*40),('run_id','45366603800'),('run_attempt','2'),
                          ('expected_ci_identity_checked',False),('wav_count',238),('actual_stereo_frames',71385599),('independent_inventory_binding',{}),('recovery_edition','lost-local-version'),('wav_count',171),('wav_count',225),
                          ('selected_program_count',89),('regular_files',276),('checksum_count',275),
                          ('actual_audio_seconds',1276.2),('actual_raw_audio_bytes',490060800),
                          ('capture_binary_sha256','invalid'),('expected',{}),
                          ('samples_over_full_scale',1),('inventory_match',{'values':105646})]:
            with self.subTest(key=key):
                args=fixture();args[0][key]=value
                with self.assertRaises(ValueError):b.bind(*args)

    def test_current_inventory_count_does_not_hide_wrong_values_or_identity(self):
        for key,value in [('programs',522),('parameters',202),('all_ordered_identities_identical',False),
                          ('all_binary32_values_identical',False),('expected_sha256','0'*64)]:
            with self.subTest(key=key):
                args=fixture();args[0]['inventory_match'][key]=value
                with self.assertRaises(ValueError):b.bind(*args)

    def test_artifact_mismatches(self):
        for key,value in [('id',0),('expired',True),('name','old-audition'),('digest','sha256:'+'0'*64),
                          ('size_in_bytes',1233),('created_at','2026-11-24T09:00:00Z'),
                          ('url','https://api.github.com/repos/other/repo/actions/artifacts/12345'),
                          ('workflow_run',{'id':b.RUN,'head_sha':'0'*40,'repository_id':1436742091,'head_repository_id':1436742091})]:
            with self.subTest(key=key):
                args=fixture();args[1][key]=value
                with self.assertRaises(ValueError):b.bind(*args)

    def test_run_and_producer_mismatches(self):
        for index,key,value in [(2,'id',45366603800),(2,'run_attempt',2),(2,'head_sha','0'*40),(2,'workflow_id',399935756),
                                (3,'id',138349816508),(3,'head_sha','0'*40),(3,'run_attempt',2),
                                (3,'run_id',45366603800),(3,'name','macOS / VST3 + AU + Standalone'),
                                (3,'conclusion','failure'),(3,'status','in_progress')]:
            with self.subTest(index=index,key=key):
                args=fixture();args[index][key]=value
                with self.assertRaises(ValueError):b.bind(*args)

    def test_capture_and_upload_steps_must_succeed_once_in_order(self):
        for fault in ['failed_capture','skipped_upload','missing_capture','duplicate_capture','reordered','overlap']:
            with self.subTest(fault=fault):
                args=fixture();steps=args[3]['steps']
                if fault=='failed_capture':steps[-2]['conclusion']='failure'
                elif fault=='skipped_upload':steps[-1]['conclusion']='skipped'
                elif fault=='missing_capture':steps.pop(-2)
                elif fault=='duplicate_capture':steps.append(copy.deepcopy(steps[-2]))
                elif fault=='overlap':steps[-1]['started_at']=steps[-2]['started_at']
                else:steps[-1]['number']=0
                with self.assertRaises(ValueError):b.bind(*args)

    def test_original_upload_must_bind_hash_id_size_time_and_command(self):
        for fault in ['digest','id','size','old_time','duplicate_digest','duplicate_success','wrong_url','missing_command','no_exit_propagation','command_outside_step','duplicate_command']:
            with self.subTest(fault=fault):
                args=fixture();log=args[-1]
                if fault=='digest':log=log.replace('a'*64,'c'*64)
                elif fault=='id':log=log.replace('ID is 12345','ID is 99999')
                elif fault=='size':log=log.replace('size is 1234','size is 1233')
                elif fault=='old_time':log=log.replace('10:07:40.','09:05:40.')
                elif fault=='duplicate_digest':log+='\n'+log.splitlines()[1]
                elif fault=='duplicate_success':log+='\n'+log.splitlines()[2]
                elif fault=='wrong_url':log=log.replace('/artifacts/12345','/artifacts/99999')
                elif fault=='no_exit_propagation':log=log.replace('bash -e {0}', 'bash {0}')
                elif fault=='command_outside_step':log=log.replace('10:06:00.001Z','10:00:00.001Z')
                elif fault=='duplicate_command':log+='\n'+log.splitlines()[0]
                else:log='\n'.join(log.splitlines()[1:])
                args[-1]=log
                with self.assertRaises(ValueError):b.bind(*args)


if __name__=='__main__': unittest.main(verbosity=2)
